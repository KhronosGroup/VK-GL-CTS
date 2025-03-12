#ifndef _VULKAN_JSON_PARSER_HPP
#define _VULKAN_JSON_PARSER_HPP

/*
 * Copyright 2024 The Khronos Group Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Defines JSON generators for Vulkan structures
 */

/********************************************************************************************/
/** This code is generated. To make changes, please modify the scripts or the relevant xml **/
/********************************************************************************************/

#pragma once
#include <iostream>
#include <map>
#include <cinttypes>
#include <algorithm>
#include <bitset>
#include <functional>
#include <sstream>
#include <cassert>
#include <limits>
#include <json/json.h>

namespace vk_json_parser
{

template <typename T1, typename T2>
class GlobalMem
{
    static constexpr size_t MAX_ALIGNMENT = alignof(std::max_align_t);

    void grow(T1 size = 0)
    {
        //push_back new single vector of size m_tabSize onto vec
        void *p = calloc(size > m_tabSize ? size : m_tabSize, sizeof(T2));
        assert(p);
        m_vec.push_back(p);
        m_pointer = 0U;
    }
    void *alloc(T1 size)
    {
        // Align to the next multiple of MAX_ALIGNMENT.
        size = (size + static_cast<T1>(MAX_ALIGNMENT) - 1) & ~(static_cast<T1>(MAX_ALIGNMENT) - 1);

        void *result = static_cast<uint8_t *>(m_vec.back()) + m_pointer;
        m_pointer += size;
        return result;
    }

public:
    GlobalMem(T1 tabSize_ = 32768U) : m_tabSize(tabSize_), m_pointer(0U)
    {
    }

    void *allocate(T1 size)
    {
        if (m_vec.empty() || m_pointer + size >= m_tabSize)
        {
            grow();
        }
        return alloc(size);
    }

    void *allocate(T1 count, T1 size)
    {
        T1 totalSize = count * size;
        if (m_vec.empty() || m_pointer + totalSize >= m_tabSize)
        {
            grow(totalSize);
        }
        return alloc(totalSize);
    }
    // deallocates all memory. Any use of earlier allocated elements is forbidden
    void clear()
    {
        // remove all vectors from vec excluding the one with index 0
        for (size_t i = 1; i < m_vec.size(); i++)
        {
            free(m_vec[i]);
        }
        if (!m_vec.empty())
        {
            m_vec.resize(1);
        }
        m_pointer = 0;
    }

    ~GlobalMem()
    {
        clear();
        if (!m_vec.empty())
        {
            free(m_vec[0]);
        }
    }

private:
    std::vector<void *> m_vec;
    T1 m_tabSize;
    T1 m_pointer;
};

static thread_local GlobalMem<uint32_t, uint8_t> s_globalMem(32768U);

// To make sure the generated data is consistent across platforms,
// we typecast to 32-bit.
static void parse_size_t(const Json::Value &obj, size_t &o)
{
    uint32_t _res = static_cast<uint32_t>(obj.asUInt());
    o             = _res;
}

static void parse_char(const Json::Value &obj, char o[])
{
    const std::string &_res = obj.asString();
    memcpy((void *)o, _res.c_str(), static_cast<uint32_t>(_res.size()));
    o[_res.size()] = '\0';
}
static void parse_char(const Json::Value &obj, const char *const *)
{
}
static void parse_char(const Json::Value &obj, const char **o)
{
    const std::string &_res = obj.asString();
    char *writePtr          = (char *)s_globalMem.allocate(static_cast<uint32_t>(_res.size()) + 1);
    memcpy((void *)writePtr, _res.c_str(), _res.size());
    writePtr[_res.size()] = '\0';
    *o                    = writePtr;
}

static void parse_int32_t(const Json::Value &obj, int32_t &o)
{
    o = obj.asInt();
}

static void parse_uint32_t(const Json::Value &obj, uint32_t &o)
{
    if (obj.isString())
        if (obj.asString() == "VK_MAX_PHYSICAL_DEVICE_NAME_SIZE")
            o = 256;
        else if (obj.asString() == "VK_UUID_SIZE")
            o = 16;
        else if (obj.asString() == "VK_LUID_SIZE")
            o = 8;
        else if (obj.asString() == "VK_MAX_EXTENSION_NAME_SIZE")
            o = 256;
        else if (obj.asString() == "VK_MAX_DESCRIPTION_SIZE")
            o = 256;
        else if (obj.asString() == "VK_MAX_MEMORY_TYPES")
            o = 32;
        else if (obj.asString() == "VK_MAX_MEMORY_HEAPS")
            o = 16;
        else if (obj.asString() == "VK_REMAINING_MIP_LEVELS")
            o = (~0U);
        else if (obj.asString() == "VK_REMAINING_ARRAY_LAYERS")
            o = (~0U);
        else if (obj.asString() == "VK_REMAINING_3D_SLICES_EXT")
            o = (~0U);
        else if (obj.asString() == "VK_ATTACHMENT_UNUSED")
            o = (~0U);
        else if (obj.asString() == "VK_TRUE")
            o = 1;
        else if (obj.asString() == "VK_FALSE")
            o = 0;
        else if (obj.asString() == "VK_QUEUE_FAMILY_IGNORED")
            o = (~0U);
        else if (obj.asString() == "VK_QUEUE_FAMILY_EXTERNAL")
            o = (~1U);
        else if (obj.asString() == "VK_QUEUE_FAMILY_FOREIGN_EXT")
            o = (~2U);
        else if (obj.asString() == "VK_SUBPASS_EXTERNAL")
            o = (~0U);
        else if (obj.asString() == "VK_MAX_DEVICE_GROUP_SIZE")
            o = 32;
        else if (obj.asString() == "VK_MAX_DRIVER_NAME_SIZE")
            o = 256;
        else if (obj.asString() == "VK_MAX_DRIVER_INFO_SIZE")
            o = 256;
        else if (obj.asString() == "VK_SHADER_UNUSED_KHR")
            o = (~0U);
        else if (obj.asString() == "VK_MAX_GLOBAL_PRIORITY_SIZE_KHR")
            o = 16;
        else if (obj.asString() == "VK_MAX_SHADER_MODULE_IDENTIFIER_SIZE_EXT")
            o = 32;
        else if (obj.asString() == "VK_MAX_VIDEO_AV1_REFERENCES_PER_FRAME_KHR")
            o = 7;
        else if (obj.asString() == "VK_SHADER_INDEX_UNUSED_AMDX")
            o = (~0U);
        else
            assert(false);
    else
        o = obj.asUInt();
}

static void parse_uint8_t(const Json::Value &obj, uint8_t &o)
{
    o = static_cast<uint8_t>(obj.asUInt());
}

static void parse_uint64_t(const Json::Value &obj, uint64_t &o)
{
    if (obj.isString())
        if (obj.asString() == "VK_WHOLE_SIZE")
            o = (~0ULL);
        else
            assert(false);
    else
        o = obj.asUInt64();
}

static void parse_float(const Json::Value &obj, float &o)
{
    if (obj.isString())
        if (obj.asString() == "VK_LOD_CLAMP_NONE")
            o = 1000.0F;
        else if (obj.asString() == "NaN")
            o = std::numeric_limits<float>::quiet_NaN();
        else
            assert(false);
    else
        o = obj.asFloat();
}

static void parse_int(const Json::Value &obj, int &o)
{
    o = obj.asInt();
}

static void parse_double(const Json::Value &obj, double &o)
{
    o = obj.asDouble();
}

static void parse_int64_t(const Json::Value &obj, int64_t &o)
{
    o = obj.asInt64();
}

static void parse_uint16_t(const Json::Value &obj, uint16_t &o)
{
    o = static_cast<uint16_t>(obj.asUInt());
}

static void parse_NvSciBufAttrList(const Json::Value &obj, vk::pt::NvSciBufAttrList &o)
{
    o = static_cast<vk::pt::NvSciBufAttrList>(obj.asInt());
}

static void parse_NvSciBufObj(const Json::Value &obj, vk::pt::NvSciBufObj &o)
{
    o = static_cast<vk::pt::NvSciBufObj>(obj.asInt());
}

static void parse_NvSciSyncAttrList(const Json::Value &obj, vk::pt::NvSciSyncAttrList &o)
{
    o = static_cast<vk::pt::NvSciSyncAttrList>(obj.asInt());
}

static void parse_NvSciSyncObj(const Json::Value &obj, vk::pt::NvSciSyncObj &o)
{
    o = static_cast<vk::pt::NvSciSyncObj>(obj.asInt());
}

static void *parsePNextChain(const Json::Value &obj);

// base64 encoder taken from executor/xeTestResultParser.cpp

static std::vector<uint8_t> base64decode(const std::string &encoded)
{
    int base64DecodeOffset = 0;
    std::vector<uint8_t> result;

    for (std::size_t inNdx = 0; inNdx < encoded.size(); inNdx++)
    {
        uint8_t byte        = encoded[inNdx];
        uint8_t decodedBits = 0;

        if (de::inRange<uint8_t>(byte, 'A', 'Z'))
            decodedBits = (uint8_t)(byte - 'A');
        else if (de::inRange<uint8_t>(byte, 'a', 'z'))
            decodedBits = (uint8_t)(('Z' - 'A' + 1) + (byte - 'a'));
        else if (de::inRange<uint8_t>(byte, '0', '9'))
            decodedBits = (uint8_t)(('Z' - 'A' + 1) + ('z' - 'a' + 1) + (byte - '0'));
        else if (byte == '+')
            decodedBits = ('Z' - 'A' + 1) + ('z' - 'a' + 1) + ('9' - '0' + 1);
        else if (byte == '/')
            decodedBits = ('Z' - 'A' + 1) + ('z' - 'a' + 1) + ('9' - '0' + 2);
        else
            continue; // Not an B64 input character.

        int phase = base64DecodeOffset % 4;

        if (phase == 0)
            result.resize(result.size() + 3, 0);

        //        if ((int)image->data.size() < (base64DecodeOffset >> 2) * 3 + 3)
        // throw TestResultParseError("Malformed base64 data");
        uint8_t *outPtr = result.data() + (base64DecodeOffset >> 2) * 3;

        switch (phase)
        {
        case 0:
            outPtr[0] |= (uint8_t)(decodedBits << 2);
            break;
        case 1:
            outPtr[0] = (uint8_t)(outPtr[0] | (uint8_t)(decodedBits >> 4));
            outPtr[1] = (uint8_t)(outPtr[1] | (uint8_t)((decodedBits & 0xF) << 4));
            break;
        case 2:
            outPtr[1] = (uint8_t)(outPtr[1] | (uint8_t)(decodedBits >> 2));
            outPtr[2] = (uint8_t)(outPtr[2] | (uint8_t)((decodedBits & 0x3) << 6));
            break;
        case 3:
            outPtr[2] |= decodedBits;
            break;
        default:
            DE_ASSERT(false);
        }

        base64DecodeOffset++;
    }
    return result;
}

static void parse_void_data(const Json::Value &obj, void *o, int oSize)
{
    std::vector<uint8_t> data;
    if (obj.isString())
    {
        data = base64decode(obj.asString());
    }
    else
    {
        data.resize(oSize);
        for (int i = 0; i < std::min(oSize, (int)obj.size()); i++)
        {
            parse_uint8_t(obj[i], const_cast<uint8_t &>(data[i]));
        }
    }
    memcpy(o, data.data(), oSize);
}

static void parse_VkBool32(const Json::Value &obj, VkBool32 &o)
{
    const std::string &_res = obj.asString();
    //VkBool is represented as VK_TRUE and VK_FALSE in the json
    o = (_res == "VK_TRUE") ? (1) : (0);
}

static void parse_VkDeviceAddress(const Json::Value &obj, VkDeviceAddress &o)
{
    const std::string &_res = obj.asString();
    sscanf(_res.c_str(), "%" SCNu64, &o);
}

static void parse_VkDeviceSize(const Json::Value &obj, VkDeviceSize &o)
{
    const std::string &_res = obj.asString();
    if (_res == "VK_WHOLE_SIZE")
        o = (~0ULL);
    else
        sscanf(_res.c_str(), "%" SCNu64, &o);
}

static void parse_VkFlags(const Json::Value &obj, VkFlags &o)
{
    const std::string &_res = obj.asString();
    sscanf(_res.c_str(), "%u", &o);
}

static void parse_VkSampleMask(const Json::Value &obj, VkSampleMask &o)
{
    const std::string &_res = obj.asString();
    sscanf(_res.c_str(), "%u", &o);
}

static void parse_VkBuffer(const Json::Value &obj, VkBuffer &o)
{
    //    const std::string& _res = obj.asString();
}

static void parse_VkImage(const Json::Value &obj, VkImage &o)
{
    //    const std::string& _res = obj.asString();
}

static void parse_VkInstance(const Json::Value &obj, VkInstance &o)
{
    //    const std::string& _res = obj.asString();
}

static void parse_VkPhysicalDevice(const Json::Value &obj, VkPhysicalDevice &o)
{
    //    const std::string& _res = obj.asString();
}

static void parse_VkDevice(const Json::Value &obj, VkDevice &o)
{
    //    const std::string& _res = obj.asString();
}

static void parse_VkQueue(const Json::Value &obj, VkQueue &o)
{
    //    const std::string& _res = obj.asString();
}

static void parse_VkSemaphore(const Json::Value &obj, VkSemaphore &o)
{
    //    const std::string& _res = obj.asString();
}

static void parse_VkCommandBuffer(const Json::Value &obj, VkCommandBuffer &o)
{
    //    const std::string& _res = obj.asString();
}

static void parse_VkFence(const Json::Value &obj, VkFence &o)
{
    //    const std::string& _res = obj.asString();
}

static void parse_VkDeviceMemory(const Json::Value &obj, VkDeviceMemory &o)
{
    //    const std::string& _res = obj.asString();
}

static void parse_VkEvent(const Json::Value &obj, VkEvent &o)
{
    //    const std::string& _res = obj.asString();
}

static void parse_VkQueryPool(const Json::Value &obj, VkQueryPool &o)
{
    //    const std::string& _res = obj.asString();
}

static void parse_VkBufferView(const Json::Value &obj, VkBufferView &o)
{
    //    const std::string& _res = obj.asString();
}

static void parse_VkImageView(const Json::Value &obj, VkImageView &o)
{
    //    const std::string& _res = obj.asString();
}

static void parse_VkShaderModule(const Json::Value &obj, VkShaderModule &o)
{
    //    const std::string& _res = obj.asString();
}

static void parse_VkPipelineCache(const Json::Value &obj, VkPipelineCache &o)
{
    //    const std::string& _res = obj.asString();
}

static void parse_VkPipelineLayout(const Json::Value &obj, VkPipelineLayout &o)
{
    //    const std::string& _res = obj.asString();
}

static void parse_VkPipeline(const Json::Value &obj, VkPipeline &o)
{
    //    const std::string& _res = obj.asString();
}

static void parse_VkRenderPass(const Json::Value &obj, VkRenderPass &o)
{
    //    const std::string& _res = obj.asString();
}

static void parse_VkDescriptorSetLayout(const Json::Value &obj, VkDescriptorSetLayout &o)
{
    //    const std::string& _res = obj.asString();
}

static void parse_VkSampler(const Json::Value &obj, VkSampler &o)
{
    //    const std::string& _res = obj.asString();
}

static void parse_VkDescriptorSet(const Json::Value &obj, VkDescriptorSet &o)
{
    //    const std::string& _res = obj.asString();
}

static void parse_VkDescriptorPool(const Json::Value &obj, VkDescriptorPool &o)
{
    //    const std::string& _res = obj.asString();
}

static void parse_VkFramebuffer(const Json::Value &obj, VkFramebuffer &o)
{
    //    const std::string& _res = obj.asString();
}

static void parse_VkCommandPool(const Json::Value &obj, VkCommandPool &o)
{
    //    const std::string& _res = obj.asString();
}

static std::map<std::string, int> VkResult_map = {
    std::make_pair("VK_SUCCESS", 0),
    std::make_pair("VK_NOT_READY", 1),
    std::make_pair("VK_TIMEOUT", 2),
    std::make_pair("VK_EVENT_SET", 3),
    std::make_pair("VK_EVENT_RESET", 4),
    std::make_pair("VK_INCOMPLETE", 5),
    std::make_pair("VK_ERROR_OUT_OF_HOST_MEMORY", -1),
    std::make_pair("VK_ERROR_OUT_OF_DEVICE_MEMORY", -2),
    std::make_pair("VK_ERROR_INITIALIZATION_FAILED", -3),
    std::make_pair("VK_ERROR_DEVICE_LOST", -4),
    std::make_pair("VK_ERROR_MEMORY_MAP_FAILED", -5),
    std::make_pair("VK_ERROR_LAYER_NOT_PRESENT", -6),
    std::make_pair("VK_ERROR_EXTENSION_NOT_PRESENT", -7),
    std::make_pair("VK_ERROR_FEATURE_NOT_PRESENT", -8),
    std::make_pair("VK_ERROR_INCOMPATIBLE_DRIVER", -9),
    std::make_pair("VK_ERROR_TOO_MANY_OBJECTS", -10),
    std::make_pair("VK_ERROR_FORMAT_NOT_SUPPORTED", -11),
    std::make_pair("VK_ERROR_FRAGMENTED_POOL", -12),
    std::make_pair("VK_ERROR_UNKNOWN", -13),
    std::make_pair("VK_ERROR_OUT_OF_POOL_MEMORY", 1000069000),
    std::make_pair("VK_ERROR_INVALID_EXTERNAL_HANDLE", 1000072003),
    std::make_pair("VK_ERROR_FRAGMENTATION", 1000161000),
    std::make_pair("VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS", 1000257000),
    std::make_pair("VK_PIPELINE_COMPILE_REQUIRED", 1000297000),
    std::make_pair("VK_ERROR_VALIDATION_FAILED", 1000011001),
    std::make_pair("VK_ERROR_INVALID_PIPELINE_CACHE_DATA", 1000298000),
    std::make_pair("VK_ERROR_NO_PIPELINE_MATCH", 1000298001),
    std::make_pair("VK_ERROR_SURFACE_LOST_KHR", 1000000000),
    std::make_pair("VK_ERROR_NATIVE_WINDOW_IN_USE_KHR", 1000000001),
    std::make_pair("VK_SUBOPTIMAL_KHR", 1000001003),
    std::make_pair("VK_ERROR_OUT_OF_DATE_KHR", 1000001004),
    std::make_pair("VK_ERROR_INCOMPATIBLE_DISPLAY_KHR", 1000003001),
    std::make_pair("VK_ERROR_VALIDATION_FAILED_EXT", 1000011001),
    std::make_pair("VK_ERROR_INVALID_SHADER_NV", 1000012000),
    std::make_pair("VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR", 1000023000),
    std::make_pair("VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR", 1000023001),
    std::make_pair("VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR", 1000023002),
    std::make_pair("VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR", 1000023003),
    std::make_pair("VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR", 1000023004),
    std::make_pair("VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR", 1000023005),
    std::make_pair("VK_ERROR_OUT_OF_POOL_MEMORY_KHR", 1000069000),
    std::make_pair("VK_ERROR_INVALID_EXTERNAL_HANDLE_KHR", 1000072003),
    std::make_pair("VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT", 1000158000),
    std::make_pair("VK_ERROR_FRAGMENTATION_EXT", 1000161000),
    std::make_pair("VK_ERROR_NOT_PERMITTED_EXT", 1000174001),
    std::make_pair("VK_ERROR_NOT_PERMITTED_KHR", 1000174001),
    std::make_pair("VK_ERROR_INVALID_DEVICE_ADDRESS_EXT", 1000257000),
    std::make_pair("VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT", 1000255000),
    std::make_pair("VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS_KHR", 1000257000),
    std::make_pair("VK_THREAD_IDLE_KHR", 1000268000),
    std::make_pair("VK_THREAD_DONE_KHR", 1000268001),
    std::make_pair("VK_OPERATION_DEFERRED_KHR", 1000268002),
    std::make_pair("VK_OPERATION_NOT_DEFERRED_KHR", 1000268003),
    std::make_pair("VK_PIPELINE_COMPILE_REQUIRED_EXT", 1000297000),
    std::make_pair("VK_ERROR_PIPELINE_COMPILE_REQUIRED_EXT", 1000297000),
    std::make_pair("VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR", 1000299000),
    std::make_pair("VK_ERROR_COMPRESSION_EXHAUSTED_EXT", 1000338000),
    std::make_pair("VK_INCOMPATIBLE_SHADER_BINARY_EXT", 1000482000),
    std::make_pair("VK_ERROR_INCOMPATIBLE_SHADER_BINARY_EXT", 1000482000),
};
static void parse_VkResult(const Json::Value &obj, VkResult &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkResult)VkResult_map[std::string(_res)];
}

static std::map<std::string, int> VkStructureType_map = {
    std::make_pair("VK_STRUCTURE_TYPE_APPLICATION_INFO", 0),
    std::make_pair("VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO", 1),
    std::make_pair("VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO", 2),
    std::make_pair("VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO", 3),
    std::make_pair("VK_STRUCTURE_TYPE_SUBMIT_INFO", 4),
    std::make_pair("VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO", 5),
    std::make_pair("VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE", 6),
    std::make_pair("VK_STRUCTURE_TYPE_FENCE_CREATE_INFO", 8),
    std::make_pair("VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO", 9),
    std::make_pair("VK_STRUCTURE_TYPE_EVENT_CREATE_INFO", 10),
    std::make_pair("VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO", 11),
    std::make_pair("VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO", 12),
    std::make_pair("VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO", 13),
    std::make_pair("VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO", 14),
    std::make_pair("VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO", 15),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO", 17),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO", 18),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO", 19),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO", 20),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO", 21),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO", 22),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO", 23),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO", 24),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO", 25),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO", 26),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO", 27),
    std::make_pair("VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO", 28),
    std::make_pair("VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO", 29),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO", 30),
    std::make_pair("VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO", 31),
    std::make_pair("VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO", 32),
    std::make_pair("VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO", 33),
    std::make_pair("VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO", 34),
    std::make_pair("VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET", 35),
    std::make_pair("VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET", 36),
    std::make_pair("VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO", 37),
    std::make_pair("VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO", 38),
    std::make_pair("VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO", 39),
    std::make_pair("VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO", 40),
    std::make_pair("VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO", 41),
    std::make_pair("VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO", 42),
    std::make_pair("VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO", 43),
    std::make_pair("VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER", 44),
    std::make_pair("VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER", 45),
    std::make_pair("VK_STRUCTURE_TYPE_MEMORY_BARRIER", 46),
    std::make_pair("VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO", 47),
    std::make_pair("VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO", 48),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES", 1000094000),
    std::make_pair("VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO", 1000157000),
    std::make_pair("VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO", 1000157001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES", 1000083000),
    std::make_pair("VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS", 1000127000),
    std::make_pair("VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO", 1000127001),
    std::make_pair("VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO", 1000060000),
    std::make_pair("VK_STRUCTURE_TYPE_DEVICE_GROUP_RENDER_PASS_BEGIN_INFO", 1000060003),
    std::make_pair("VK_STRUCTURE_TYPE_DEVICE_GROUP_COMMAND_BUFFER_BEGIN_INFO", 1000060004),
    std::make_pair("VK_STRUCTURE_TYPE_DEVICE_GROUP_SUBMIT_INFO", 1000060005),
    std::make_pair("VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_DEVICE_GROUP_INFO", 1000060013),
    std::make_pair("VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_DEVICE_GROUP_INFO", 1000060014),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GROUP_PROPERTIES", 1000070000),
    std::make_pair("VK_STRUCTURE_TYPE_DEVICE_GROUP_DEVICE_CREATE_INFO", 1000070001),
    std::make_pair("VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2", 1000146000),
    std::make_pair("VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2", 1000146001),
    std::make_pair("VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2", 1000146003),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2", 1000059000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2", 1000059001),
    std::make_pair("VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2", 1000059002),
    std::make_pair("VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2", 1000059003),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2", 1000059004),
    std::make_pair("VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2", 1000059005),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2", 1000059006),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_POINT_CLIPPING_PROPERTIES", 1000117000),
    std::make_pair("VK_STRUCTURE_TYPE_RENDER_PASS_INPUT_ATTACHMENT_ASPECT_CREATE_INFO", 1000117001),
    std::make_pair("VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO", 1000117002),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_DOMAIN_ORIGIN_STATE_CREATE_INFO", 1000117003),
    std::make_pair("VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO", 1000053000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES", 1000053001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES", 1000053002),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VARIABLE_POINTERS_FEATURES", 1000120000),
    std::make_pair("VK_STRUCTURE_TYPE_PROTECTED_SUBMIT_INFO", 1000145000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES", 1000145001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_PROPERTIES", 1000145002),
    std::make_pair("VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2", 1000145003),
    std::make_pair("VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO", 1000156000),
    std::make_pair("VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO", 1000156001),
    std::make_pair("VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO", 1000156002),
    std::make_pair("VK_STRUCTURE_TYPE_IMAGE_PLANE_MEMORY_REQUIREMENTS_INFO", 1000156003),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES", 1000156004),
    std::make_pair("VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_IMAGE_FORMAT_PROPERTIES", 1000156005),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO", 1000071000),
    std::make_pair("VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES", 1000071001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_BUFFER_INFO", 1000071002),
    std::make_pair("VK_STRUCTURE_TYPE_EXTERNAL_BUFFER_PROPERTIES", 1000071003),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES", 1000071004),
    std::make_pair("VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO", 1000072000),
    std::make_pair("VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO", 1000072001),
    std::make_pair("VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO", 1000072002),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_FENCE_INFO", 1000112000),
    std::make_pair("VK_STRUCTURE_TYPE_EXTERNAL_FENCE_PROPERTIES", 1000112001),
    std::make_pair("VK_STRUCTURE_TYPE_EXPORT_FENCE_CREATE_INFO", 1000113000),
    std::make_pair("VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO", 1000077000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO", 1000076000),
    std::make_pair("VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES", 1000076001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES", 1000168000),
    std::make_pair("VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_SUPPORT", 1000168001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES", 1000063000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES", 49),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES", 50),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES", 51),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES", 52),
    std::make_pair("VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO", 1000147000),
    std::make_pair("VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2", 1000109000),
    std::make_pair("VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2", 1000109001),
    std::make_pair("VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2", 1000109002),
    std::make_pair("VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2", 1000109003),
    std::make_pair("VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2", 1000109004),
    std::make_pair("VK_STRUCTURE_TYPE_SUBPASS_BEGIN_INFO", 1000109005),
    std::make_pair("VK_STRUCTURE_TYPE_SUBPASS_END_INFO", 1000109006),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES", 1000177000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES", 1000196000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES", 1000180000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES", 1000082000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES", 1000197000),
    std::make_pair("VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO", 1000161000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES", 1000161001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES", 1000161002),
    std::make_pair("VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO", 1000161003),
    std::make_pair("VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_LAYOUT_SUPPORT", 1000161004),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_STENCIL_RESOLVE_PROPERTIES", 1000199000),
    std::make_pair("VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE", 1000199001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES", 1000221000),
    std::make_pair("VK_STRUCTURE_TYPE_IMAGE_STENCIL_USAGE_CREATE_INFO", 1000246000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_FILTER_MINMAX_PROPERTIES", 1000130000),
    std::make_pair("VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO", 1000130001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES", 1000211000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGELESS_FRAMEBUFFER_FEATURES", 1000108000),
    std::make_pair("VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO", 1000108001),
    std::make_pair("VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO", 1000108002),
    std::make_pair("VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO", 1000108003),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFORM_BUFFER_STANDARD_LAYOUT_FEATURES", 1000253000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SUBGROUP_EXTENDED_TYPES_FEATURES", 1000175000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES", 1000241000),
    std::make_pair("VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_STENCIL_LAYOUT", 1000241001),
    std::make_pair("VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_STENCIL_LAYOUT", 1000241002),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES", 1000261000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES", 1000207000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_PROPERTIES", 1000207001),
    std::make_pair("VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO", 1000207002),
    std::make_pair("VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO", 1000207003),
    std::make_pair("VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO", 1000207004),
    std::make_pair("VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO", 1000207005),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES", 1000257000),
    std::make_pair("VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO", 1000244001),
    std::make_pair("VK_STRUCTURE_TYPE_BUFFER_OPAQUE_CAPTURE_ADDRESS_CREATE_INFO", 1000257002),
    std::make_pair("VK_STRUCTURE_TYPE_MEMORY_OPAQUE_CAPTURE_ADDRESS_ALLOCATE_INFO", 1000257003),
    std::make_pair("VK_STRUCTURE_TYPE_DEVICE_MEMORY_OPAQUE_CAPTURE_ADDRESS_INFO", 1000257004),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES", 53),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES", 54),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_CREATION_FEEDBACK_CREATE_INFO", 1000192000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_TERMINATE_INVOCATION_FEATURES", 1000215000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TOOL_PROPERTIES", 1000245000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DEMOTE_TO_HELPER_INVOCATION_FEATURES", 1000276000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIVATE_DATA_FEATURES", 1000295000),
    std::make_pair("VK_STRUCTURE_TYPE_DEVICE_PRIVATE_DATA_CREATE_INFO", 1000295001),
    std::make_pair("VK_STRUCTURE_TYPE_PRIVATE_DATA_SLOT_CREATE_INFO", 1000295002),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_CREATION_CACHE_CONTROL_FEATURES", 1000297000),
    std::make_pair("VK_STRUCTURE_TYPE_MEMORY_BARRIER_2", 1000314000),
    std::make_pair("VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2", 1000314001),
    std::make_pair("VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2", 1000314002),
    std::make_pair("VK_STRUCTURE_TYPE_DEPENDENCY_INFO", 1000314003),
    std::make_pair("VK_STRUCTURE_TYPE_SUBMIT_INFO_2", 1000314004),
    std::make_pair("VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO", 1000314005),
    std::make_pair("VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO", 1000314006),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES", 1000314007),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ZERO_INITIALIZE_WORKGROUP_MEMORY_FEATURES", 1000325000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_ROBUSTNESS_FEATURES", 1000335000),
    std::make_pair("VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2", 1000337000),
    std::make_pair("VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2", 1000337001),
    std::make_pair("VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2", 1000337002),
    std::make_pair("VK_STRUCTURE_TYPE_COPY_IMAGE_TO_BUFFER_INFO_2", 1000337003),
    std::make_pair("VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2", 1000337004),
    std::make_pair("VK_STRUCTURE_TYPE_RESOLVE_IMAGE_INFO_2", 1000337005),
    std::make_pair("VK_STRUCTURE_TYPE_BUFFER_COPY_2", 1000337006),
    std::make_pair("VK_STRUCTURE_TYPE_IMAGE_COPY_2", 1000337007),
    std::make_pair("VK_STRUCTURE_TYPE_IMAGE_BLIT_2", 1000337008),
    std::make_pair("VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2", 1000337009),
    std::make_pair("VK_STRUCTURE_TYPE_IMAGE_RESOLVE_2", 1000337010),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES", 1000225000),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO", 1000225001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES", 1000225002),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_FEATURES", 1000138000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_PROPERTIES", 1000138001),
    std::make_pair("VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_INLINE_UNIFORM_BLOCK", 1000138002),
    std::make_pair("VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_INLINE_UNIFORM_BLOCK_CREATE_INFO", 1000138003),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TEXTURE_COMPRESSION_ASTC_HDR_FEATURES", 1000066000),
    std::make_pair("VK_STRUCTURE_TYPE_RENDERING_INFO", 1000044000),
    std::make_pair("VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO", 1000044001),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO", 1000044002),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES", 1000044003),
    std::make_pair("VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO", 1000044004),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_INTEGER_DOT_PRODUCT_FEATURES", 1000280000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_INTEGER_DOT_PRODUCT_PROPERTIES", 1000280001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TEXEL_BUFFER_ALIGNMENT_PROPERTIES", 1000281001),
    std::make_pair("VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_3", 1000360000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_FEATURES", 1000413000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_PROPERTIES", 1000413001),
    std::make_pair("VK_STRUCTURE_TYPE_DEVICE_BUFFER_MEMORY_REQUIREMENTS", 1000413002),
    std::make_pair("VK_STRUCTURE_TYPE_DEVICE_IMAGE_MEMORY_REQUIREMENTS", 1000413003),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_SC_1_0_FEATURES", 1000298000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_SC_1_0_PROPERTIES", 1000298001),
    std::make_pair("VK_STRUCTURE_TYPE_DEVICE_OBJECT_RESERVATION_CREATE_INFO", 1000298002),
    std::make_pair("VK_STRUCTURE_TYPE_COMMAND_POOL_MEMORY_RESERVATION_CREATE_INFO", 1000298003),
    std::make_pair("VK_STRUCTURE_TYPE_COMMAND_POOL_MEMORY_CONSUMPTION", 1000298004),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_POOL_SIZE", 1000298005),
    std::make_pair("VK_STRUCTURE_TYPE_FAULT_DATA", 1000298007),
    std::make_pair("VK_STRUCTURE_TYPE_FAULT_CALLBACK_INFO", 1000298008),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_OFFLINE_CREATE_INFO", 1000298010),
    std::make_pair("VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR", 1000001000),
    std::make_pair("VK_STRUCTURE_TYPE_PRESENT_INFO_KHR", 1000001001),
    std::make_pair("VK_STRUCTURE_TYPE_DEVICE_GROUP_PRESENT_CAPABILITIES_KHR", 1000060007),
    std::make_pair("VK_STRUCTURE_TYPE_IMAGE_SWAPCHAIN_CREATE_INFO_KHR", 1000060008),
    std::make_pair("VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_SWAPCHAIN_INFO_KHR", 1000060009),
    std::make_pair("VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR", 1000060010),
    std::make_pair("VK_STRUCTURE_TYPE_DEVICE_GROUP_PRESENT_INFO_KHR", 1000060011),
    std::make_pair("VK_STRUCTURE_TYPE_DEVICE_GROUP_SWAPCHAIN_CREATE_INFO_KHR", 1000060012),
    std::make_pair("VK_STRUCTURE_TYPE_DISPLAY_MODE_CREATE_INFO_KHR", 1000002000),
    std::make_pair("VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR", 1000002001),
    std::make_pair("VK_STRUCTURE_TYPE_DISPLAY_PRESENT_INFO_KHR", 1000003000),
    std::make_pair("VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR", 1000004000),
    std::make_pair("VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR", 1000005000),
    std::make_pair("VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR", 1000006000),
    std::make_pair("VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR", 1000008000),
    std::make_pair("VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR", 1000009000),
    std::make_pair("VK_STRUCTURE_TYPE_NATIVE_BUFFER_ANDROID", 1000010000),
    std::make_pair("VK_STRUCTURE_TYPE_SWAPCHAIN_IMAGE_CREATE_INFO_ANDROID", 1000010001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENTATION_PROPERTIES_ANDROID", 1000010002),
    std::make_pair("VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT", 1000011000),
    std::make_pair("VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT", 1000011000),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_RASTERIZATION_ORDER_AMD", 1000018000),
    std::make_pair("VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT", 1000022000),
    std::make_pair("VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_TAG_INFO_EXT", 1000022001),
    std::make_pair("VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT", 1000022002),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR", 1000023000),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_CAPABILITIES_KHR", 1000023001),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR", 1000023002),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_SESSION_MEMORY_REQUIREMENTS_KHR", 1000023003),
    std::make_pair("VK_STRUCTURE_TYPE_BIND_VIDEO_SESSION_MEMORY_INFO_KHR", 1000023004),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_SESSION_CREATE_INFO_KHR", 1000023005),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_SESSION_PARAMETERS_CREATE_INFO_KHR", 1000023006),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_SESSION_PARAMETERS_UPDATE_INFO_KHR", 1000023007),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_BEGIN_CODING_INFO_KHR", 1000023008),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_END_CODING_INFO_KHR", 1000023009),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_CODING_CONTROL_INFO_KHR", 1000023010),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR", 1000023011),
    std::make_pair("VK_STRUCTURE_TYPE_QUEUE_FAMILY_VIDEO_PROPERTIES_KHR", 1000023012),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_PROFILE_LIST_INFO_KHR", 1000023013),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VIDEO_FORMAT_INFO_KHR", 1000023014),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_FORMAT_PROPERTIES_KHR", 1000023015),
    std::make_pair("VK_STRUCTURE_TYPE_QUEUE_FAMILY_QUERY_RESULT_STATUS_PROPERTIES_KHR", 1000023016),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_DECODE_INFO_KHR", 1000024000),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_DECODE_CAPABILITIES_KHR", 1000024001),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_DECODE_USAGE_INFO_KHR", 1000024002),
    std::make_pair("VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_IMAGE_CREATE_INFO_NV", 1000026000),
    std::make_pair("VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_BUFFER_CREATE_INFO_NV", 1000026001),
    std::make_pair("VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_MEMORY_ALLOCATE_INFO_NV", 1000026002),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT", 1000028000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_PROPERTIES_EXT", 1000028001),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_STREAM_CREATE_INFO_EXT", 1000028002),
    std::make_pair("VK_STRUCTURE_TYPE_CU_MODULE_CREATE_INFO_NVX", 1000029000),
    std::make_pair("VK_STRUCTURE_TYPE_CU_FUNCTION_CREATE_INFO_NVX", 1000029001),
    std::make_pair("VK_STRUCTURE_TYPE_CU_LAUNCH_INFO_NVX", 1000029002),
    std::make_pair("VK_STRUCTURE_TYPE_IMAGE_VIEW_HANDLE_INFO_NVX", 1000030000),
    std::make_pair("VK_STRUCTURE_TYPE_IMAGE_VIEW_ADDRESS_PROPERTIES_NVX", 1000030001),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_CAPABILITIES_KHR", 1000038000),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_SESSION_PARAMETERS_CREATE_INFO_KHR", 1000038001),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_SESSION_PARAMETERS_ADD_INFO_KHR", 1000038002),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_PICTURE_INFO_KHR", 1000038003),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_DPB_SLOT_INFO_KHR", 1000038004),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_NALU_SLICE_INFO_KHR", 1000038005),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_GOP_REMAINING_FRAME_INFO_KHR", 1000038006),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_PROFILE_INFO_KHR", 1000038007),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_RATE_CONTROL_INFO_KHR", 1000038008),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_RATE_CONTROL_LAYER_INFO_KHR", 1000038009),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_SESSION_CREATE_INFO_KHR", 1000038010),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_QUALITY_LEVEL_PROPERTIES_KHR", 1000038011),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_SESSION_PARAMETERS_GET_INFO_KHR", 1000038012),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_SESSION_PARAMETERS_FEEDBACK_INFO_KHR", 1000038013),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_CAPABILITIES_KHR", 1000039000),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_SESSION_PARAMETERS_CREATE_INFO_KHR", 1000039001),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_SESSION_PARAMETERS_ADD_INFO_KHR", 1000039002),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_PICTURE_INFO_KHR", 1000039003),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_DPB_SLOT_INFO_KHR", 1000039004),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_NALU_SLICE_SEGMENT_INFO_KHR", 1000039005),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_GOP_REMAINING_FRAME_INFO_KHR", 1000039006),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_PROFILE_INFO_KHR", 1000039007),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_RATE_CONTROL_INFO_KHR", 1000039009),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_RATE_CONTROL_LAYER_INFO_KHR", 1000039010),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_SESSION_CREATE_INFO_KHR", 1000039011),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_QUALITY_LEVEL_PROPERTIES_KHR", 1000039012),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_SESSION_PARAMETERS_GET_INFO_KHR", 1000039013),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_SESSION_PARAMETERS_FEEDBACK_INFO_KHR", 1000039014),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_CAPABILITIES_KHR", 1000040000),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_PICTURE_INFO_KHR", 1000040001),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_PROFILE_INFO_KHR", 1000040003),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_SESSION_PARAMETERS_CREATE_INFO_KHR", 1000040004),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_SESSION_PARAMETERS_ADD_INFO_KHR", 1000040005),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_DPB_SLOT_INFO_KHR", 1000040006),
    std::make_pair("VK_STRUCTURE_TYPE_TEXTURE_LOD_GATHER_FORMAT_PROPERTIES_AMD", 1000041000),
    std::make_pair("VK_STRUCTURE_TYPE_RENDERING_INFO_KHR", 1000044000),
    std::make_pair("VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR", 1000044001),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR", 1000044002),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR", 1000044003),
    std::make_pair("VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO_KHR", 1000044004),
    std::make_pair("VK_STRUCTURE_TYPE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_INFO_KHR", 1000044006),
    std::make_pair("VK_STRUCTURE_TYPE_RENDERING_FRAGMENT_DENSITY_MAP_ATTACHMENT_INFO_EXT", 1000044007),
    std::make_pair("VK_STRUCTURE_TYPE_ATTACHMENT_SAMPLE_COUNT_INFO_AMD", 1000044008),
    std::make_pair("VK_STRUCTURE_TYPE_ATTACHMENT_SAMPLE_COUNT_INFO_NV", 1000044008),
    std::make_pair("VK_STRUCTURE_TYPE_MULTIVIEW_PER_VIEW_ATTRIBUTES_INFO_NVX", 1000044009),
    std::make_pair("VK_STRUCTURE_TYPE_STREAM_DESCRIPTOR_SURFACE_CREATE_INFO_GGP", 1000049000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CORNER_SAMPLED_IMAGE_FEATURES_NV", 1000050000),
    std::make_pair("VK_STRUCTURE_TYPE_PRIVATE_VENDOR_INFO_PLACEHOLDER_OFFSET_0_NV", 1000051000),
    std::make_pair("VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO_KHR", 1000053000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES_KHR", 1000053001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES_KHR", 1000053002),
    std::make_pair("VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_NV", 1000056000),
    std::make_pair("VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_NV", 1000056001),
    std::make_pair("VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_NV", 1000057000),
    std::make_pair("VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_NV", 1000057001),
    std::make_pair("VK_STRUCTURE_TYPE_WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_NV", 1000058000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR", 1000059000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR", 1000059001),
    std::make_pair("VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2_KHR", 1000059002),
    std::make_pair("VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2_KHR", 1000059003),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2_KHR", 1000059004),
    std::make_pair("VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2_KHR", 1000059005),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2_KHR", 1000059006),
    std::make_pair("VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO_KHR", 1000060000),
    std::make_pair("VK_STRUCTURE_TYPE_DEVICE_GROUP_RENDER_PASS_BEGIN_INFO_KHR", 1000060003),
    std::make_pair("VK_STRUCTURE_TYPE_DEVICE_GROUP_COMMAND_BUFFER_BEGIN_INFO_KHR", 1000060004),
    std::make_pair("VK_STRUCTURE_TYPE_DEVICE_GROUP_SUBMIT_INFO_KHR", 1000060005),
    std::make_pair("VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_DEVICE_GROUP_INFO_KHR", 1000060013),
    std::make_pair("VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_DEVICE_GROUP_INFO_KHR", 1000060014),
    std::make_pair("VK_STRUCTURE_TYPE_DEVICE_GROUP_PRESENT_CAPABILITIES_KHR", 1000060007),
    std::make_pair("VK_STRUCTURE_TYPE_IMAGE_SWAPCHAIN_CREATE_INFO_KHR", 1000060008),
    std::make_pair("VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_SWAPCHAIN_INFO_KHR", 1000060009),
    std::make_pair("VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR", 1000060010),
    std::make_pair("VK_STRUCTURE_TYPE_DEVICE_GROUP_PRESENT_INFO_KHR", 1000060011),
    std::make_pair("VK_STRUCTURE_TYPE_DEVICE_GROUP_SWAPCHAIN_CREATE_INFO_KHR", 1000060012),
    std::make_pair("VK_STRUCTURE_TYPE_VALIDATION_FLAGS_EXT", 1000061000),
    std::make_pair("VK_STRUCTURE_TYPE_VI_SURFACE_CREATE_INFO_NN", 1000062000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TEXTURE_COMPRESSION_ASTC_HDR_FEATURES_EXT", 1000066000),
    std::make_pair("VK_STRUCTURE_TYPE_IMAGE_VIEW_ASTC_DECODE_MODE_EXT", 1000067000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ASTC_DECODE_FEATURES_EXT", 1000067001),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_ROBUSTNESS_CREATE_INFO_EXT", 1000068000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_ROBUSTNESS_FEATURES_EXT", 1000068001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_ROBUSTNESS_PROPERTIES_EXT", 1000068002),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GROUP_PROPERTIES_KHR", 1000070000),
    std::make_pair("VK_STRUCTURE_TYPE_DEVICE_GROUP_DEVICE_CREATE_INFO_KHR", 1000070001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO_KHR", 1000071000),
    std::make_pair("VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES_KHR", 1000071001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_BUFFER_INFO_KHR", 1000071002),
    std::make_pair("VK_STRUCTURE_TYPE_EXTERNAL_BUFFER_PROPERTIES_KHR", 1000071003),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES_KHR", 1000071004),
    std::make_pair("VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO_KHR", 1000072000),
    std::make_pair("VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_KHR", 1000072001),
    std::make_pair("VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_KHR", 1000072002),
    std::make_pair("VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR", 1000073000),
    std::make_pair("VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_KHR", 1000073001),
    std::make_pair("VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR", 1000073002),
    std::make_pair("VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR", 1000073003),
    std::make_pair("VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR", 1000074000),
    std::make_pair("VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR", 1000074001),
    std::make_pair("VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR", 1000074002),
    std::make_pair("VK_STRUCTURE_TYPE_WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_KHR", 1000075000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO_KHR", 1000076000),
    std::make_pair("VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES_KHR", 1000076001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES_KHR", 1000071004),
    std::make_pair("VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO_KHR", 1000077000),
    std::make_pair("VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR", 1000078000),
    std::make_pair("VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR", 1000078001),
    std::make_pair("VK_STRUCTURE_TYPE_D3D12_FENCE_SUBMIT_INFO_KHR", 1000078002),
    std::make_pair("VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR", 1000078003),
    std::make_pair("VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR", 1000079000),
    std::make_pair("VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR", 1000079001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PUSH_DESCRIPTOR_PROPERTIES_KHR", 1000080000),
    std::make_pair("VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_CONDITIONAL_RENDERING_INFO_EXT", 1000081000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONDITIONAL_RENDERING_FEATURES_EXT", 1000081001),
    std::make_pair("VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_EXT", 1000081002),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES_KHR", 1000082000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT16_INT8_FEATURES_KHR", 1000082000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES_KHR", 1000083000),
    std::make_pair("VK_STRUCTURE_TYPE_PRESENT_REGIONS_KHR", 1000084000),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_W_SCALING_STATE_CREATE_INFO_NV", 1000087000),
    std::make_pair("VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_EXT", 1000090000),
    std::make_pair("VK_STRUCTURE_TYPE_DISPLAY_POWER_INFO_EXT", 1000091000),
    std::make_pair("VK_STRUCTURE_TYPE_DEVICE_EVENT_INFO_EXT", 1000091001),
    std::make_pair("VK_STRUCTURE_TYPE_DISPLAY_EVENT_INFO_EXT", 1000091002),
    std::make_pair("VK_STRUCTURE_TYPE_SWAPCHAIN_COUNTER_CREATE_INFO_EXT", 1000091003),
    std::make_pair("VK_STRUCTURE_TYPE_PRESENT_TIMES_INFO_GOOGLE", 1000092000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PER_VIEW_ATTRIBUTES_PROPERTIES_NVX", 1000097000),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_SWIZZLE_STATE_CREATE_INFO_NV", 1000098000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DISCARD_RECTANGLE_PROPERTIES_EXT", 1000099000),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_DISCARD_RECTANGLE_STATE_CREATE_INFO_EXT", 1000099001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONSERVATIVE_RASTERIZATION_PROPERTIES_EXT", 1000101000),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT", 1000101001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_ENABLE_FEATURES_EXT", 1000102000),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_DEPTH_CLIP_STATE_CREATE_INFO_EXT", 1000102001),
    std::make_pair("VK_STRUCTURE_TYPE_HDR_METADATA_EXT", 1000105000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGELESS_FRAMEBUFFER_FEATURES_KHR", 1000108000),
    std::make_pair("VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO_KHR", 1000108001),
    std::make_pair("VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO_KHR", 1000108002),
    std::make_pair("VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO_KHR", 1000108003),
    std::make_pair("VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2_KHR", 1000109000),
    std::make_pair("VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2_KHR", 1000109001),
    std::make_pair("VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2_KHR", 1000109002),
    std::make_pair("VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2_KHR", 1000109003),
    std::make_pair("VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2_KHR", 1000109004),
    std::make_pair("VK_STRUCTURE_TYPE_SUBPASS_BEGIN_INFO_KHR", 1000109005),
    std::make_pair("VK_STRUCTURE_TYPE_SUBPASS_END_INFO_KHR", 1000109006),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RELAXED_LINE_RASTERIZATION_FEATURES_IMG", 1000110000),
    std::make_pair("VK_STRUCTURE_TYPE_SHARED_PRESENT_SURFACE_CAPABILITIES_KHR", 1000111000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_FENCE_INFO_KHR", 1000112000),
    std::make_pair("VK_STRUCTURE_TYPE_EXTERNAL_FENCE_PROPERTIES_KHR", 1000112001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES_KHR", 1000071004),
    std::make_pair("VK_STRUCTURE_TYPE_EXPORT_FENCE_CREATE_INFO_KHR", 1000113000),
    std::make_pair("VK_STRUCTURE_TYPE_IMPORT_FENCE_WIN32_HANDLE_INFO_KHR", 1000114000),
    std::make_pair("VK_STRUCTURE_TYPE_EXPORT_FENCE_WIN32_HANDLE_INFO_KHR", 1000114001),
    std::make_pair("VK_STRUCTURE_TYPE_FENCE_GET_WIN32_HANDLE_INFO_KHR", 1000114002),
    std::make_pair("VK_STRUCTURE_TYPE_IMPORT_FENCE_FD_INFO_KHR", 1000115000),
    std::make_pair("VK_STRUCTURE_TYPE_FENCE_GET_FD_INFO_KHR", 1000115001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PERFORMANCE_QUERY_FEATURES_KHR", 1000116000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PERFORMANCE_QUERY_PROPERTIES_KHR", 1000116001),
    std::make_pair("VK_STRUCTURE_TYPE_QUERY_POOL_PERFORMANCE_CREATE_INFO_KHR", 1000116002),
    std::make_pair("VK_STRUCTURE_TYPE_PERFORMANCE_QUERY_SUBMIT_INFO_KHR", 1000116003),
    std::make_pair("VK_STRUCTURE_TYPE_ACQUIRE_PROFILING_LOCK_INFO_KHR", 1000116004),
    std::make_pair("VK_STRUCTURE_TYPE_PERFORMANCE_COUNTER_KHR", 1000116005),
    std::make_pair("VK_STRUCTURE_TYPE_PERFORMANCE_COUNTER_DESCRIPTION_KHR", 1000116006),
    std::make_pair("VK_STRUCTURE_TYPE_PERFORMANCE_QUERY_RESERVATION_INFO_KHR", 1000116007),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_POINT_CLIPPING_PROPERTIES_KHR", 1000117000),
    std::make_pair("VK_STRUCTURE_TYPE_RENDER_PASS_INPUT_ATTACHMENT_ASPECT_CREATE_INFO_KHR", 1000117001),
    std::make_pair("VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO_KHR", 1000117002),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_DOMAIN_ORIGIN_STATE_CREATE_INFO_KHR", 1000117003),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR", 1000119000),
    std::make_pair("VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR", 1000119001),
    std::make_pair("VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR", 1000119002),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VARIABLE_POINTERS_FEATURES_KHR", 1000120000),
    std::make_pair("VK_STRUCTURE_TYPE_DISPLAY_PROPERTIES_2_KHR", 1000121000),
    std::make_pair("VK_STRUCTURE_TYPE_DISPLAY_PLANE_PROPERTIES_2_KHR", 1000121001),
    std::make_pair("VK_STRUCTURE_TYPE_DISPLAY_MODE_PROPERTIES_2_KHR", 1000121002),
    std::make_pair("VK_STRUCTURE_TYPE_DISPLAY_PLANE_INFO_2_KHR", 1000121003),
    std::make_pair("VK_STRUCTURE_TYPE_DISPLAY_PLANE_CAPABILITIES_2_KHR", 1000121004),
    std::make_pair("VK_STRUCTURE_TYPE_IOS_SURFACE_CREATE_INFO_MVK", 1000122000),
    std::make_pair("VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK", 1000123000),
    std::make_pair("VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS_KHR", 1000127000),
    std::make_pair("VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR", 1000127001),
    std::make_pair("VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT", 1000128000),
    std::make_pair("VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_TAG_INFO_EXT", 1000128001),
    std::make_pair("VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT", 1000128002),
    std::make_pair("VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT", 1000128003),
    std::make_pair("VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT", 1000128004),
    std::make_pair("VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_USAGE_ANDROID", 1000129000),
    std::make_pair("VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID", 1000129001),
    std::make_pair("VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID", 1000129002),
    std::make_pair("VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID", 1000129003),
    std::make_pair("VK_STRUCTURE_TYPE_MEMORY_GET_ANDROID_HARDWARE_BUFFER_INFO_ANDROID", 1000129004),
    std::make_pair("VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID", 1000129005),
    std::make_pair("VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_2_ANDROID", 1000129006),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_FILTER_MINMAX_PROPERTIES_EXT", 1000130000),
    std::make_pair("VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO_EXT", 1000130001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ENQUEUE_FEATURES_AMDX", 1000134000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ENQUEUE_PROPERTIES_AMDX", 1000134001),
    std::make_pair("VK_STRUCTURE_TYPE_EXECUTION_GRAPH_PIPELINE_SCRATCH_SIZE_AMDX", 1000134002),
    std::make_pair("VK_STRUCTURE_TYPE_EXECUTION_GRAPH_PIPELINE_CREATE_INFO_AMDX", 1000134003),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_NODE_CREATE_INFO_AMDX", 1000134004),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_FEATURES_EXT", 1000138000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK_PROPERTIES_EXT", 1000138001),
    std::make_pair("VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_INLINE_UNIFORM_BLOCK_EXT", 1000138002),
    std::make_pair("VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_INLINE_UNIFORM_BLOCK_CREATE_INFO_EXT", 1000138003),
    std::make_pair("VK_STRUCTURE_TYPE_SAMPLE_LOCATIONS_INFO_EXT", 1000143000),
    std::make_pair("VK_STRUCTURE_TYPE_RENDER_PASS_SAMPLE_LOCATIONS_BEGIN_INFO_EXT", 1000143001),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_SAMPLE_LOCATIONS_STATE_CREATE_INFO_EXT", 1000143002),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLE_LOCATIONS_PROPERTIES_EXT", 1000143003),
    std::make_pair("VK_STRUCTURE_TYPE_MULTISAMPLE_PROPERTIES_EXT", 1000143004),
    std::make_pair("VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2_KHR", 1000146000),
    std::make_pair("VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2_KHR", 1000146001),
    std::make_pair("VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2_KHR", 1000146003),
    std::make_pair("VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO_KHR", 1000147000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BLEND_OPERATION_ADVANCED_FEATURES_EXT", 1000148000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BLEND_OPERATION_ADVANCED_PROPERTIES_EXT", 1000148001),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_ADVANCED_STATE_CREATE_INFO_EXT", 1000148002),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_COVERAGE_TO_COLOR_STATE_CREATE_INFO_NV", 1000149000),
    std::make_pair("VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR", 1000150007),
    std::make_pair("VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR", 1000150000),
    std::make_pair("VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR", 1000150002),
    std::make_pair("VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR", 1000150003),
    std::make_pair("VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR", 1000150004),
    std::make_pair("VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR", 1000150005),
    std::make_pair("VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR", 1000150006),
    std::make_pair("VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_VERSION_INFO_KHR", 1000150009),
    std::make_pair("VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR", 1000150010),
    std::make_pair("VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_TO_MEMORY_INFO_KHR", 1000150011),
    std::make_pair("VK_STRUCTURE_TYPE_COPY_MEMORY_TO_ACCELERATION_STRUCTURE_INFO_KHR", 1000150012),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR", 1000150013),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR", 1000150014),
    std::make_pair("VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR", 1000150017),
    std::make_pair("VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR", 1000150020),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR", 1000347000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR", 1000347001),
    std::make_pair("VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR", 1000150015),
    std::make_pair("VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR", 1000150016),
    std::make_pair("VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_INTERFACE_CREATE_INFO_KHR", 1000150018),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR", 1000348013),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_COVERAGE_MODULATION_STATE_CREATE_INFO_NV", 1000152000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SM_BUILTINS_FEATURES_NV", 1000154000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SM_BUILTINS_PROPERTIES_NV", 1000154001),
    std::make_pair("VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO_KHR", 1000156000),
    std::make_pair("VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO_KHR", 1000156001),
    std::make_pair("VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO_KHR", 1000156002),
    std::make_pair("VK_STRUCTURE_TYPE_IMAGE_PLANE_MEMORY_REQUIREMENTS_INFO_KHR", 1000156003),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES_KHR", 1000156004),
    std::make_pair("VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_IMAGE_FORMAT_PROPERTIES_KHR", 1000156005),
    std::make_pair("VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO_KHR", 1000157000),
    std::make_pair("VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO_KHR", 1000157001),
    std::make_pair("VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT", 1000158000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT", 1000158002),
    std::make_pair("VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT", 1000158003),
    std::make_pair("VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT", 1000158004),
    std::make_pair("VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_PROPERTIES_EXT", 1000158005),
    std::make_pair("VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_2_EXT", 1000158006),
    std::make_pair("VK_STRUCTURE_TYPE_VALIDATION_CACHE_CREATE_INFO_EXT", 1000160000),
    std::make_pair("VK_STRUCTURE_TYPE_SHADER_MODULE_VALIDATION_CACHE_CREATE_INFO_EXT", 1000160001),
    std::make_pair("VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT", 1000161000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT", 1000161001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES_EXT", 1000161002),
    std::make_pair("VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT", 1000161003),
    std::make_pair("VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_LAYOUT_SUPPORT_EXT", 1000161004),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PORTABILITY_SUBSET_FEATURES_KHR", 1000163000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PORTABILITY_SUBSET_PROPERTIES_KHR", 1000163001),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_SHADING_RATE_IMAGE_STATE_CREATE_INFO_NV", 1000164000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADING_RATE_IMAGE_FEATURES_NV", 1000164001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADING_RATE_IMAGE_PROPERTIES_NV", 1000164002),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_COARSE_SAMPLE_ORDER_STATE_CREATE_INFO_NV", 1000164005),
    std::make_pair("VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_NV", 1000165000),
    std::make_pair("VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NV", 1000165001),
    std::make_pair("VK_STRUCTURE_TYPE_GEOMETRY_NV", 1000165003),
    std::make_pair("VK_STRUCTURE_TYPE_GEOMETRY_TRIANGLES_NV", 1000165004),
    std::make_pair("VK_STRUCTURE_TYPE_GEOMETRY_AABB_NV", 1000165005),
    std::make_pair("VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NV", 1000165006),
    std::make_pair("VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_NV", 1000165007),
    std::make_pair("VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV", 1000165008),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PROPERTIES_NV", 1000165009),
    std::make_pair("VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV", 1000165011),
    std::make_pair("VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV", 1000165012),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_REPRESENTATIVE_FRAGMENT_TEST_FEATURES_NV", 1000166000),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_REPRESENTATIVE_FRAGMENT_TEST_STATE_CREATE_INFO_NV", 1000166001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES_KHR", 1000168000),
    std::make_pair("VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_SUPPORT_KHR", 1000168001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_VIEW_IMAGE_FORMAT_INFO_EXT", 1000170000),
    std::make_pair("VK_STRUCTURE_TYPE_FILTER_CUBIC_IMAGE_VIEW_IMAGE_FORMAT_PROPERTIES_EXT", 1000170001),
    std::make_pair("VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO_EXT", 1000174000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SUBGROUP_EXTENDED_TYPES_FEATURES_KHR", 1000175000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES_KHR", 1000177000),
    std::make_pair("VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT", 1000178000),
    std::make_pair("VK_STRUCTURE_TYPE_MEMORY_HOST_POINTER_PROPERTIES_EXT", 1000178001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT", 1000178002),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES_KHR", 1000180000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CLOCK_FEATURES_KHR", 1000181000),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_COMPILER_CONTROL_CREATE_INFO_AMD", 1000183000),
    std::make_pair("VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_EXT", 1000184000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CORE_PROPERTIES_AMD", 1000185000),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_CAPABILITIES_KHR", 1000187000),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_SESSION_PARAMETERS_CREATE_INFO_KHR", 1000187001),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_SESSION_PARAMETERS_ADD_INFO_KHR", 1000187002),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_PROFILE_INFO_KHR", 1000187003),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_PICTURE_INFO_KHR", 1000187004),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_DPB_SLOT_INFO_KHR", 1000187005),
    std::make_pair("VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO_KHR", 1000174000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GLOBAL_PRIORITY_QUERY_FEATURES_KHR", 1000388000),
    std::make_pair("VK_STRUCTURE_TYPE_QUEUE_FAMILY_GLOBAL_PRIORITY_PROPERTIES_KHR", 1000388001),
    std::make_pair("VK_STRUCTURE_TYPE_DEVICE_MEMORY_OVERALLOCATION_CREATE_INFO_AMD", 1000189000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_PROPERTIES_EXT", 1000190000),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_DIVISOR_STATE_CREATE_INFO_EXT", 1000190001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES_EXT", 1000190002),
    std::make_pair("VK_STRUCTURE_TYPE_PRESENT_FRAME_TOKEN_GGP", 1000191000),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_CREATION_FEEDBACK_CREATE_INFO_EXT", 1000192000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES_KHR", 1000196000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES_KHR", 1000197000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_STENCIL_RESOLVE_PROPERTIES_KHR", 1000199000),
    std::make_pair("VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE_KHR", 1000199001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COMPUTE_SHADER_DERIVATIVES_FEATURES_NV", 1000201000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_NV", 1000202000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_NV", 1000202001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_NV", 1000203000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_IMAGE_FOOTPRINT_FEATURES_NV", 1000204000),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_EXCLUSIVE_SCISSOR_STATE_CREATE_INFO_NV", 1000205000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXCLUSIVE_SCISSOR_FEATURES_NV", 1000205002),
    std::make_pair("VK_STRUCTURE_TYPE_CHECKPOINT_DATA_NV", 1000206000),
    std::make_pair("VK_STRUCTURE_TYPE_QUEUE_FAMILY_CHECKPOINT_PROPERTIES_NV", 1000206001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES_KHR", 1000207000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_PROPERTIES_KHR", 1000207001),
    std::make_pair("VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO_KHR", 1000207002),
    std::make_pair("VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO_KHR", 1000207003),
    std::make_pair("VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO_KHR", 1000207004),
    std::make_pair("VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO_KHR", 1000207005),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_INTEGER_FUNCTIONS_2_FEATURES_INTEL", 1000209000),
    std::make_pair("VK_STRUCTURE_TYPE_QUERY_POOL_PERFORMANCE_QUERY_CREATE_INFO_INTEL", 1000210000),
    std::make_pair("VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO_INTEL", 1000210000),
    std::make_pair("VK_STRUCTURE_TYPE_INITIALIZE_PERFORMANCE_API_INFO_INTEL", 1000210001),
    std::make_pair("VK_STRUCTURE_TYPE_PERFORMANCE_MARKER_INFO_INTEL", 1000210002),
    std::make_pair("VK_STRUCTURE_TYPE_PERFORMANCE_STREAM_MARKER_INFO_INTEL", 1000210003),
    std::make_pair("VK_STRUCTURE_TYPE_PERFORMANCE_OVERRIDE_INFO_INTEL", 1000210004),
    std::make_pair("VK_STRUCTURE_TYPE_PERFORMANCE_CONFIGURATION_ACQUIRE_INFO_INTEL", 1000210005),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES_KHR", 1000211000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PCI_BUS_INFO_PROPERTIES_EXT", 1000212000),
    std::make_pair("VK_STRUCTURE_TYPE_DISPLAY_NATIVE_HDR_SURFACE_CAPABILITIES_AMD", 1000213000),
    std::make_pair("VK_STRUCTURE_TYPE_SWAPCHAIN_DISPLAY_NATIVE_HDR_CREATE_INFO_AMD", 1000213001),
    std::make_pair("VK_STRUCTURE_TYPE_IMAGEPIPE_SURFACE_CREATE_INFO_FUCHSIA", 1000214000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_TERMINATE_INVOCATION_FEATURES_KHR", 1000215000),
    std::make_pair("VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT", 1000217000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT", 1000218000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_PROPERTIES_EXT", 1000218001),
    std::make_pair("VK_STRUCTURE_TYPE_RENDER_PASS_FRAGMENT_DENSITY_MAP_CREATE_INFO_EXT", 1000218002),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES_EXT", 1000221000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES_EXT", 1000225000),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO_EXT", 1000225001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES_EXT", 1000225002),
    std::make_pair("VK_STRUCTURE_TYPE_FRAGMENT_SHADING_RATE_ATTACHMENT_INFO_KHR", 1000226000),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_FRAGMENT_SHADING_RATE_STATE_CREATE_INFO_KHR", 1000226001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_PROPERTIES_KHR", 1000226002),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR", 1000226003),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_KHR", 1000226004),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CORE_PROPERTIES_2_AMD", 1000227000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COHERENT_MEMORY_FEATURES_AMD", 1000229000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_LOCAL_READ_FEATURES_KHR", 1000232000),
    std::make_pair("VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_LOCATION_INFO_KHR", 1000232001),
    std::make_pair("VK_STRUCTURE_TYPE_RENDERING_INPUT_ATTACHMENT_INDEX_INFO_KHR", 1000232002),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_IMAGE_ATOMIC_INT64_FEATURES_EXT", 1000234000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_QUAD_CONTROL_FEATURES_KHR", 1000235000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT", 1000237000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PRIORITY_FEATURES_EXT", 1000238000),
    std::make_pair("VK_STRUCTURE_TYPE_MEMORY_PRIORITY_ALLOCATE_INFO_EXT", 1000238001),
    std::make_pair("VK_STRUCTURE_TYPE_SURFACE_PROTECTED_CAPABILITIES_KHR", 1000239000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEDICATED_ALLOCATION_IMAGE_ALIASING_FEATURES_NV", 1000240000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES_KHR", 1000241000),
    std::make_pair("VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_STENCIL_LAYOUT_KHR", 1000241001),
    std::make_pair("VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_STENCIL_LAYOUT_KHR", 1000241002),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_EXT", 1000244000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_ADDRESS_FEATURES_EXT", 1000244000),
    std::make_pair("VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_EXT", 1000244001),
    std::make_pair("VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_CREATE_INFO_EXT", 1000244002),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TOOL_PROPERTIES_EXT", 1000245000),
    std::make_pair("VK_STRUCTURE_TYPE_IMAGE_STENCIL_USAGE_CREATE_INFO_EXT", 1000246000),
    std::make_pair("VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT", 1000247000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_FEATURES_KHR", 1000248000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_FEATURES_NV", 1000249000),
    std::make_pair("VK_STRUCTURE_TYPE_COOPERATIVE_MATRIX_PROPERTIES_NV", 1000249001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_PROPERTIES_NV", 1000249002),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COVERAGE_REDUCTION_MODE_FEATURES_NV", 1000250000),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_COVERAGE_REDUCTION_STATE_CREATE_INFO_NV", 1000250001),
    std::make_pair("VK_STRUCTURE_TYPE_FRAMEBUFFER_MIXED_SAMPLES_COMBINATION_NV", 1000250002),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT", 1000251000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_YCBCR_IMAGE_ARRAYS_FEATURES_EXT", 1000252000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFORM_BUFFER_STANDARD_LAYOUT_FEATURES_KHR", 1000253000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROVOKING_VERTEX_FEATURES_EXT", 1000254000),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_PROVOKING_VERTEX_STATE_CREATE_INFO_EXT", 1000254001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROVOKING_VERTEX_PROPERTIES_EXT", 1000254002),
    std::make_pair("VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT", 1000255000),
    std::make_pair("VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_FULL_SCREEN_EXCLUSIVE_EXT", 1000255002),
    std::make_pair("VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_WIN32_INFO_EXT", 1000255001),
    std::make_pair("VK_STRUCTURE_TYPE_HEADLESS_SURFACE_CREATE_INFO_EXT", 1000256000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR", 1000257000),
    std::make_pair("VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_KHR", 1000244001),
    std::make_pair("VK_STRUCTURE_TYPE_BUFFER_OPAQUE_CAPTURE_ADDRESS_CREATE_INFO_KHR", 1000257002),
    std::make_pair("VK_STRUCTURE_TYPE_MEMORY_OPAQUE_CAPTURE_ADDRESS_ALLOCATE_INFO_KHR", 1000257003),
    std::make_pair("VK_STRUCTURE_TYPE_DEVICE_MEMORY_OPAQUE_CAPTURE_ADDRESS_INFO_KHR", 1000257004),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_FEATURES_EXT", 1000259000),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_LINE_STATE_CREATE_INFO_EXT", 1000259001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_PROPERTIES_EXT", 1000259002),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT", 1000260000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES_EXT", 1000261000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INDEX_TYPE_UINT8_FEATURES_EXT", 1000265000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT", 1000267000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_EXECUTABLE_PROPERTIES_FEATURES_KHR", 1000269000),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_INFO_KHR", 1000269001),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_PROPERTIES_KHR", 1000269002),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_INFO_KHR", 1000269003),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_STATISTIC_KHR", 1000269004),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_INTERNAL_REPRESENTATION_KHR", 1000269005),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_IMAGE_COPY_FEATURES_EXT", 1000270000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_IMAGE_COPY_PROPERTIES_EXT", 1000270001),
    std::make_pair("VK_STRUCTURE_TYPE_MEMORY_TO_IMAGE_COPY_EXT", 1000270002),
    std::make_pair("VK_STRUCTURE_TYPE_IMAGE_TO_MEMORY_COPY_EXT", 1000270003),
    std::make_pair("VK_STRUCTURE_TYPE_COPY_IMAGE_TO_MEMORY_INFO_EXT", 1000270004),
    std::make_pair("VK_STRUCTURE_TYPE_COPY_MEMORY_TO_IMAGE_INFO_EXT", 1000270005),
    std::make_pair("VK_STRUCTURE_TYPE_HOST_IMAGE_LAYOUT_TRANSITION_INFO_EXT", 1000270006),
    std::make_pair("VK_STRUCTURE_TYPE_COPY_IMAGE_TO_IMAGE_INFO_EXT", 1000270007),
    std::make_pair("VK_STRUCTURE_TYPE_SUBRESOURCE_HOST_MEMCPY_SIZE_EXT", 1000270008),
    std::make_pair("VK_STRUCTURE_TYPE_HOST_IMAGE_COPY_DEVICE_PERFORMANCE_QUERY_EXT", 1000270009),
    std::make_pair("VK_STRUCTURE_TYPE_MEMORY_MAP_INFO_KHR", 1000271000),
    std::make_pair("VK_STRUCTURE_TYPE_MEMORY_UNMAP_INFO_KHR", 1000271001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAP_MEMORY_PLACED_FEATURES_EXT", 1000272000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAP_MEMORY_PLACED_PROPERTIES_EXT", 1000272001),
    std::make_pair("VK_STRUCTURE_TYPE_MEMORY_MAP_PLACED_INFO_EXT", 1000272002),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_2_FEATURES_EXT", 1000273000),
    std::make_pair("VK_STRUCTURE_TYPE_SURFACE_PRESENT_MODE_EXT", 1000274000),
    std::make_pair("VK_STRUCTURE_TYPE_SURFACE_PRESENT_SCALING_CAPABILITIES_EXT", 1000274001),
    std::make_pair("VK_STRUCTURE_TYPE_SURFACE_PRESENT_MODE_COMPATIBILITY_EXT", 1000274002),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT", 1000275000),
    std::make_pair("VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT", 1000275001),
    std::make_pair("VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_MODES_CREATE_INFO_EXT", 1000275002),
    std::make_pair("VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_MODE_INFO_EXT", 1000275003),
    std::make_pair("VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_SCALING_CREATE_INFO_EXT", 1000275004),
    std::make_pair("VK_STRUCTURE_TYPE_RELEASE_SWAPCHAIN_IMAGES_INFO_EXT", 1000275005),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DEMOTE_TO_HELPER_INVOCATION_FEATURES_EXT", 1000276000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_PROPERTIES_NV", 1000277000),
    std::make_pair("VK_STRUCTURE_TYPE_GRAPHICS_SHADER_GROUP_CREATE_INFO_NV", 1000277001),
    std::make_pair("VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_SHADER_GROUPS_CREATE_INFO_NV", 1000277002),
    std::make_pair("VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_TOKEN_NV", 1000277003),
    std::make_pair("VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_CREATE_INFO_NV", 1000277004),
    std::make_pair("VK_STRUCTURE_TYPE_GENERATED_COMMANDS_INFO_NV", 1000277005),
    std::make_pair("VK_STRUCTURE_TYPE_GENERATED_COMMANDS_MEMORY_REQUIREMENTS_INFO_NV", 1000277006),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_FEATURES_NV", 1000277007),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INHERITED_VIEWPORT_SCISSOR_FEATURES_NV", 1000278000),
    std::make_pair("VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_VIEWPORT_SCISSOR_INFO_NV", 1000278001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_INTEGER_DOT_PRODUCT_FEATURES_KHR", 1000280000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_INTEGER_DOT_PRODUCT_PROPERTIES_KHR", 1000280001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TEXEL_BUFFER_ALIGNMENT_FEATURES_EXT", 1000281000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TEXEL_BUFFER_ALIGNMENT_PROPERTIES_EXT", 1000281001),
    std::make_pair("VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDER_PASS_TRANSFORM_INFO_QCOM", 1000282000),
    std::make_pair("VK_STRUCTURE_TYPE_RENDER_PASS_TRANSFORM_BEGIN_INFO_QCOM", 1000282001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_BIAS_CONTROL_FEATURES_EXT", 1000283000),
    std::make_pair("VK_STRUCTURE_TYPE_DEPTH_BIAS_INFO_EXT", 1000283001),
    std::make_pair("VK_STRUCTURE_TYPE_DEPTH_BIAS_REPRESENTATION_INFO_EXT", 1000283002),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_MEMORY_REPORT_FEATURES_EXT", 1000284000),
    std::make_pair("VK_STRUCTURE_TYPE_DEVICE_DEVICE_MEMORY_REPORT_CREATE_INFO_EXT", 1000284001),
    std::make_pair("VK_STRUCTURE_TYPE_DEVICE_MEMORY_REPORT_CALLBACK_DATA_EXT", 1000284002),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT", 1000286000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_PROPERTIES_EXT", 1000286001),
    std::make_pair("VK_STRUCTURE_TYPE_SAMPLER_CUSTOM_BORDER_COLOR_CREATE_INFO_EXT", 1000287000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_PROPERTIES_EXT", 1000287001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT", 1000287002),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR", 1000290000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_BARRIER_FEATURES_NV", 1000292000),
    std::make_pair("VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_PRESENT_BARRIER_NV", 1000292001),
    std::make_pair("VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_BARRIER_CREATE_INFO_NV", 1000292002),
    std::make_pair("VK_STRUCTURE_TYPE_PRESENT_ID_KHR", 1000294000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR", 1000294001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIVATE_DATA_FEATURES_EXT", 1000295000),
    std::make_pair("VK_STRUCTURE_TYPE_DEVICE_PRIVATE_DATA_CREATE_INFO_EXT", 1000295001),
    std::make_pair("VK_STRUCTURE_TYPE_PRIVATE_DATA_SLOT_CREATE_INFO_EXT", 1000295002),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_CREATION_CACHE_CONTROL_FEATURES_EXT", 1000297000),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_ENCODE_INFO_KHR", 1000299000),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_ENCODE_RATE_CONTROL_INFO_KHR", 1000299001),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_ENCODE_RATE_CONTROL_LAYER_INFO_KHR", 1000299002),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_ENCODE_CAPABILITIES_KHR", 1000299003),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_ENCODE_USAGE_INFO_KHR", 1000299004),
    std::make_pair("VK_STRUCTURE_TYPE_QUERY_POOL_VIDEO_ENCODE_FEEDBACK_CREATE_INFO_KHR", 1000299005),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VIDEO_ENCODE_QUALITY_LEVEL_INFO_KHR", 1000299006),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_ENCODE_QUALITY_LEVEL_PROPERTIES_KHR", 1000299007),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_ENCODE_QUALITY_LEVEL_INFO_KHR", 1000299008),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_ENCODE_SESSION_PARAMETERS_GET_INFO_KHR", 1000299009),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_ENCODE_SESSION_PARAMETERS_FEEDBACK_INFO_KHR", 1000299010),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DIAGNOSTICS_CONFIG_FEATURES_NV", 1000300000),
    std::make_pair("VK_STRUCTURE_TYPE_DEVICE_DIAGNOSTICS_CONFIG_CREATE_INFO_NV", 1000300001),
    std::make_pair("VK_STRUCTURE_TYPE_CUDA_MODULE_CREATE_INFO_NV", 1000307000),
    std::make_pair("VK_STRUCTURE_TYPE_CUDA_FUNCTION_CREATE_INFO_NV", 1000307001),
    std::make_pair("VK_STRUCTURE_TYPE_CUDA_LAUNCH_INFO_NV", 1000307002),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUDA_KERNEL_LAUNCH_FEATURES_NV", 1000307003),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUDA_KERNEL_LAUNCH_PROPERTIES_NV", 1000307004),
    std::make_pair("VK_STRUCTURE_TYPE_REFRESH_OBJECT_LIST_KHR", 1000308000),
    std::make_pair("VK_STRUCTURE_TYPE_QUERY_LOW_LATENCY_SUPPORT_NV", 1000310000),
    std::make_pair("VK_STRUCTURE_TYPE_EXPORT_METAL_OBJECT_CREATE_INFO_EXT", 1000311000),
    std::make_pair("VK_STRUCTURE_TYPE_EXPORT_METAL_OBJECTS_INFO_EXT", 1000311001),
    std::make_pair("VK_STRUCTURE_TYPE_EXPORT_METAL_DEVICE_INFO_EXT", 1000311002),
    std::make_pair("VK_STRUCTURE_TYPE_EXPORT_METAL_COMMAND_QUEUE_INFO_EXT", 1000311003),
    std::make_pair("VK_STRUCTURE_TYPE_EXPORT_METAL_BUFFER_INFO_EXT", 1000311004),
    std::make_pair("VK_STRUCTURE_TYPE_IMPORT_METAL_BUFFER_INFO_EXT", 1000311005),
    std::make_pair("VK_STRUCTURE_TYPE_EXPORT_METAL_TEXTURE_INFO_EXT", 1000311006),
    std::make_pair("VK_STRUCTURE_TYPE_IMPORT_METAL_TEXTURE_INFO_EXT", 1000311007),
    std::make_pair("VK_STRUCTURE_TYPE_EXPORT_METAL_IO_SURFACE_INFO_EXT", 1000311008),
    std::make_pair("VK_STRUCTURE_TYPE_IMPORT_METAL_IO_SURFACE_INFO_EXT", 1000311009),
    std::make_pair("VK_STRUCTURE_TYPE_EXPORT_METAL_SHARED_EVENT_INFO_EXT", 1000311010),
    std::make_pair("VK_STRUCTURE_TYPE_IMPORT_METAL_SHARED_EVENT_INFO_EXT", 1000311011),
    std::make_pair("VK_STRUCTURE_TYPE_MEMORY_BARRIER_2_KHR", 1000314000),
    std::make_pair("VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2_KHR", 1000314001),
    std::make_pair("VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR", 1000314002),
    std::make_pair("VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR", 1000314003),
    std::make_pair("VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR", 1000314004),
    std::make_pair("VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR", 1000314005),
    std::make_pair("VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO_KHR", 1000314006),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR", 1000314007),
    std::make_pair("VK_STRUCTURE_TYPE_QUEUE_FAMILY_CHECKPOINT_PROPERTIES_2_NV", 1000314008),
    std::make_pair("VK_STRUCTURE_TYPE_CHECKPOINT_DATA_2_NV", 1000314009),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT", 1000316000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_DENSITY_MAP_PROPERTIES_EXT", 1000316001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT", 1000316002),
    std::make_pair("VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT", 1000316003),
    std::make_pair("VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT", 1000316004),
    std::make_pair("VK_STRUCTURE_TYPE_BUFFER_CAPTURE_DESCRIPTOR_DATA_INFO_EXT", 1000316005),
    std::make_pair("VK_STRUCTURE_TYPE_IMAGE_CAPTURE_DESCRIPTOR_DATA_INFO_EXT", 1000316006),
    std::make_pair("VK_STRUCTURE_TYPE_IMAGE_VIEW_CAPTURE_DESCRIPTOR_DATA_INFO_EXT", 1000316007),
    std::make_pair("VK_STRUCTURE_TYPE_SAMPLER_CAPTURE_DESCRIPTOR_DATA_INFO_EXT", 1000316008),
    std::make_pair("VK_STRUCTURE_TYPE_OPAQUE_CAPTURE_DESCRIPTOR_DATA_CREATE_INFO_EXT", 1000316010),
    std::make_pair("VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT", 1000316011),
    std::make_pair("VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_PUSH_DESCRIPTOR_BUFFER_HANDLE_EXT", 1000316012),
    std::make_pair("VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CAPTURE_DESCRIPTOR_DATA_INFO_EXT", 1000316009),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT", 1000320000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_PROPERTIES_EXT", 1000320001),
    std::make_pair("VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT", 1000320002),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_EARLY_AND_LATE_FRAGMENT_TESTS_FEATURES_AMD", 1000321000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_KHR", 1000203000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_PROPERTIES_KHR", 1000322000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SUBGROUP_UNIFORM_CONTROL_FLOW_FEATURES_KHR", 1000323000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ZERO_INITIALIZE_WORKGROUP_MEMORY_FEATURES_KHR", 1000325000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_ENUMS_PROPERTIES_NV", 1000326000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_ENUMS_FEATURES_NV", 1000326001),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_FRAGMENT_SHADING_RATE_ENUM_STATE_CREATE_INFO_NV", 1000326002),
    std::make_pair("VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_MOTION_TRIANGLES_DATA_NV", 1000327000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_MOTION_BLUR_FEATURES_NV", 1000327001),
    std::make_pair("VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MOTION_INFO_NV", 1000327002),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT", 1000328000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT", 1000328001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_YCBCR_2_PLANE_444_FORMATS_FEATURES_EXT", 1000330000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_2_FEATURES_EXT", 1000332000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_2_PROPERTIES_EXT", 1000332001),
    std::make_pair("VK_STRUCTURE_TYPE_COPY_COMMAND_TRANSFORM_INFO_QCOM", 1000333000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_ROBUSTNESS_FEATURES_EXT", 1000335000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_WORKGROUP_MEMORY_EXPLICIT_LAYOUT_FEATURES_KHR", 1000336000),
    std::make_pair("VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2_KHR", 1000337000),
    std::make_pair("VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2_KHR", 1000337001),
    std::make_pair("VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2_KHR", 1000337002),
    std::make_pair("VK_STRUCTURE_TYPE_COPY_IMAGE_TO_BUFFER_INFO_2_KHR", 1000337003),
    std::make_pair("VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2_KHR", 1000337004),
    std::make_pair("VK_STRUCTURE_TYPE_RESOLVE_IMAGE_INFO_2_KHR", 1000337005),
    std::make_pair("VK_STRUCTURE_TYPE_BUFFER_COPY_2_KHR", 1000337006),
    std::make_pair("VK_STRUCTURE_TYPE_IMAGE_COPY_2_KHR", 1000337007),
    std::make_pair("VK_STRUCTURE_TYPE_IMAGE_BLIT_2_KHR", 1000337008),
    std::make_pair("VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2_KHR", 1000337009),
    std::make_pair("VK_STRUCTURE_TYPE_IMAGE_RESOLVE_2_KHR", 1000337010),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_COMPRESSION_CONTROL_FEATURES_EXT", 1000338000),
    std::make_pair("VK_STRUCTURE_TYPE_IMAGE_COMPRESSION_CONTROL_EXT", 1000338001),
    std::make_pair("VK_STRUCTURE_TYPE_SUBRESOURCE_LAYOUT_2_EXT", 1000338002),
    std::make_pair("VK_STRUCTURE_TYPE_IMAGE_SUBRESOURCE_2_EXT", 1000338003),
    std::make_pair("VK_STRUCTURE_TYPE_IMAGE_COMPRESSION_PROPERTIES_EXT", 1000338004),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ATTACHMENT_FEEDBACK_LOOP_LAYOUT_FEATURES_EXT", 1000339000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_4444_FORMATS_FEATURES_EXT", 1000340000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FAULT_FEATURES_EXT", 1000341000),
    std::make_pair("VK_STRUCTURE_TYPE_DEVICE_FAULT_COUNTS_EXT", 1000341001),
    std::make_pair("VK_STRUCTURE_TYPE_DEVICE_FAULT_INFO_EXT", 1000341002),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_FEATURES_ARM", 1000342000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RGBA10X6_FORMATS_FEATURES_EXT", 1000344000),
    std::make_pair("VK_STRUCTURE_TYPE_DIRECTFB_SURFACE_CREATE_INFO_EXT", 1000346000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MUTABLE_DESCRIPTOR_TYPE_FEATURES_VALVE", 1000351000),
    std::make_pair("VK_STRUCTURE_TYPE_MUTABLE_DESCRIPTOR_TYPE_CREATE_INFO_VALVE", 1000351002),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_INPUT_DYNAMIC_STATE_FEATURES_EXT", 1000352000),
    std::make_pair("VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT", 1000352001),
    std::make_pair("VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT", 1000352002),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRM_PROPERTIES_EXT", 1000353000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ADDRESS_BINDING_REPORT_FEATURES_EXT", 1000354000),
    std::make_pair("VK_STRUCTURE_TYPE_DEVICE_ADDRESS_BINDING_CALLBACK_DATA_EXT", 1000354001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_CONTROL_FEATURES_EXT", 1000355000),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_DEPTH_CLIP_CONTROL_CREATE_INFO_EXT", 1000355001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIMITIVE_TOPOLOGY_LIST_RESTART_FEATURES_EXT", 1000356000),
    std::make_pair("VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_3_KHR", 1000360000),
    std::make_pair("VK_STRUCTURE_TYPE_IMPORT_MEMORY_ZIRCON_HANDLE_INFO_FUCHSIA", 1000364000),
    std::make_pair("VK_STRUCTURE_TYPE_MEMORY_ZIRCON_HANDLE_PROPERTIES_FUCHSIA", 1000364001),
    std::make_pair("VK_STRUCTURE_TYPE_MEMORY_GET_ZIRCON_HANDLE_INFO_FUCHSIA", 1000364002),
    std::make_pair("VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_ZIRCON_HANDLE_INFO_FUCHSIA", 1000365000),
    std::make_pair("VK_STRUCTURE_TYPE_SEMAPHORE_GET_ZIRCON_HANDLE_INFO_FUCHSIA", 1000365001),
    std::make_pair("VK_STRUCTURE_TYPE_BUFFER_COLLECTION_CREATE_INFO_FUCHSIA", 1000366000),
    std::make_pair("VK_STRUCTURE_TYPE_IMPORT_MEMORY_BUFFER_COLLECTION_FUCHSIA", 1000366001),
    std::make_pair("VK_STRUCTURE_TYPE_BUFFER_COLLECTION_IMAGE_CREATE_INFO_FUCHSIA", 1000366002),
    std::make_pair("VK_STRUCTURE_TYPE_BUFFER_COLLECTION_PROPERTIES_FUCHSIA", 1000366003),
    std::make_pair("VK_STRUCTURE_TYPE_BUFFER_CONSTRAINTS_INFO_FUCHSIA", 1000366004),
    std::make_pair("VK_STRUCTURE_TYPE_BUFFER_COLLECTION_BUFFER_CREATE_INFO_FUCHSIA", 1000366005),
    std::make_pair("VK_STRUCTURE_TYPE_IMAGE_CONSTRAINTS_INFO_FUCHSIA", 1000366006),
    std::make_pair("VK_STRUCTURE_TYPE_IMAGE_FORMAT_CONSTRAINTS_INFO_FUCHSIA", 1000366007),
    std::make_pair("VK_STRUCTURE_TYPE_SYSMEM_COLOR_SPACE_FUCHSIA", 1000366008),
    std::make_pair("VK_STRUCTURE_TYPE_BUFFER_COLLECTION_CONSTRAINTS_INFO_FUCHSIA", 1000366009),
    std::make_pair("VK_STRUCTURE_TYPE_SUBPASS_SHADING_PIPELINE_CREATE_INFO_HUAWEI", 1000369000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBPASS_SHADING_FEATURES_HUAWEI", 1000369001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBPASS_SHADING_PROPERTIES_HUAWEI", 1000369002),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INVOCATION_MASK_FEATURES_HUAWEI", 1000370000),
    std::make_pair("VK_STRUCTURE_TYPE_MEMORY_GET_REMOTE_ADDRESS_INFO_NV", 1000371000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_RDMA_FEATURES_NV", 1000371001),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_PROPERTIES_IDENTIFIER_EXT", 1000372000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_PROPERTIES_FEATURES_EXT", 1000372001),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_INFO_EXT", 1000269001),
    std::make_pair("VK_STRUCTURE_TYPE_IMPORT_FENCE_SCI_SYNC_INFO_NV", 1000373000),
    std::make_pair("VK_STRUCTURE_TYPE_EXPORT_FENCE_SCI_SYNC_INFO_NV", 1000373001),
    std::make_pair("VK_STRUCTURE_TYPE_FENCE_GET_SCI_SYNC_INFO_NV", 1000373002),
    std::make_pair("VK_STRUCTURE_TYPE_SCI_SYNC_ATTRIBUTES_INFO_NV", 1000373003),
    std::make_pair("VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_SCI_SYNC_INFO_NV", 1000373004),
    std::make_pair("VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_SCI_SYNC_INFO_NV", 1000373005),
    std::make_pair("VK_STRUCTURE_TYPE_SEMAPHORE_GET_SCI_SYNC_INFO_NV", 1000373006),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SCI_SYNC_FEATURES_NV", 1000373007),
    std::make_pair("VK_STRUCTURE_TYPE_IMPORT_MEMORY_SCI_BUF_INFO_NV", 1000374000),
    std::make_pair("VK_STRUCTURE_TYPE_EXPORT_MEMORY_SCI_BUF_INFO_NV", 1000374001),
    std::make_pair("VK_STRUCTURE_TYPE_MEMORY_GET_SCI_BUF_INFO_NV", 1000374002),
    std::make_pair("VK_STRUCTURE_TYPE_MEMORY_SCI_BUF_PROPERTIES_NV", 1000374003),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_SCI_BUF_FEATURES_NV", 1000374004),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SCI_BUF_FEATURES_NV", 1000374004),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAME_BOUNDARY_FEATURES_EXT", 1000375000),
    std::make_pair("VK_STRUCTURE_TYPE_FRAME_BOUNDARY_EXT", 1000375001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_FEATURES_EXT", 1000376000),
    std::make_pair("VK_STRUCTURE_TYPE_SUBPASS_RESOLVE_PERFORMANCE_QUERY_EXT", 1000376001),
    std::make_pair("VK_STRUCTURE_TYPE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_INFO_EXT", 1000376002),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_2_FEATURES_EXT", 1000377000),
    std::make_pair("VK_STRUCTURE_TYPE_SCREEN_SURFACE_CREATE_INFO_QNX", 1000378000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COLOR_WRITE_ENABLE_FEATURES_EXT", 1000381000),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_COLOR_WRITE_CREATE_INFO_EXT", 1000381001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRIMITIVES_GENERATED_QUERY_FEATURES_EXT", 1000382000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_MAINTENANCE_1_FEATURES_KHR", 1000386000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GLOBAL_PRIORITY_QUERY_FEATURES_EXT", 1000388000),
    std::make_pair("VK_STRUCTURE_TYPE_QUEUE_FAMILY_GLOBAL_PRIORITY_PROPERTIES_EXT", 1000388001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_VIEW_MIN_LOD_FEATURES_EXT", 1000391000),
    std::make_pair("VK_STRUCTURE_TYPE_IMAGE_VIEW_MIN_LOD_CREATE_INFO_EXT", 1000391001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTI_DRAW_FEATURES_EXT", 1000392000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTI_DRAW_PROPERTIES_EXT", 1000392001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_2D_VIEW_OF_3D_FEATURES_EXT", 1000393000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_TILE_IMAGE_FEATURES_EXT", 1000395000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_TILE_IMAGE_PROPERTIES_EXT", 1000395001),
    std::make_pair("VK_STRUCTURE_TYPE_MICROMAP_BUILD_INFO_EXT", 1000396000),
    std::make_pair("VK_STRUCTURE_TYPE_MICROMAP_VERSION_INFO_EXT", 1000396001),
    std::make_pair("VK_STRUCTURE_TYPE_COPY_MICROMAP_INFO_EXT", 1000396002),
    std::make_pair("VK_STRUCTURE_TYPE_COPY_MICROMAP_TO_MEMORY_INFO_EXT", 1000396003),
    std::make_pair("VK_STRUCTURE_TYPE_COPY_MEMORY_TO_MICROMAP_INFO_EXT", 1000396004),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_OPACITY_MICROMAP_FEATURES_EXT", 1000396005),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_OPACITY_MICROMAP_PROPERTIES_EXT", 1000396006),
    std::make_pair("VK_STRUCTURE_TYPE_MICROMAP_CREATE_INFO_EXT", 1000396007),
    std::make_pair("VK_STRUCTURE_TYPE_MICROMAP_BUILD_SIZES_INFO_EXT", 1000396008),
    std::make_pair("VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_TRIANGLES_OPACITY_MICROMAP_EXT", 1000396009),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DISPLACEMENT_MICROMAP_FEATURES_NV", 1000397000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DISPLACEMENT_MICROMAP_PROPERTIES_NV", 1000397001),
    std::make_pair("VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_TRIANGLES_DISPLACEMENT_MICROMAP_NV", 1000397002),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CLUSTER_CULLING_SHADER_FEATURES_HUAWEI", 1000404000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CLUSTER_CULLING_SHADER_PROPERTIES_HUAWEI", 1000404001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CLUSTER_CULLING_SHADER_VRS_FEATURES_HUAWEI", 1000404002),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BORDER_COLOR_SWIZZLE_FEATURES_EXT", 1000411000),
    std::make_pair("VK_STRUCTURE_TYPE_SAMPLER_BORDER_COLOR_COMPONENT_MAPPING_CREATE_INFO_EXT", 1000411001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PAGEABLE_DEVICE_LOCAL_MEMORY_FEATURES_EXT", 1000412000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_FEATURES_KHR", 1000413000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_PROPERTIES_KHR", 1000413001),
    std::make_pair("VK_STRUCTURE_TYPE_DEVICE_BUFFER_MEMORY_REQUIREMENTS_KHR", 1000413002),
    std::make_pair("VK_STRUCTURE_TYPE_DEVICE_IMAGE_MEMORY_REQUIREMENTS_KHR", 1000413003),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CORE_PROPERTIES_ARM", 1000415000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SUBGROUP_ROTATE_FEATURES_KHR", 1000416000),
    std::make_pair("VK_STRUCTURE_TYPE_DEVICE_QUEUE_SHADER_CORE_CONTROL_CREATE_INFO_ARM", 1000417000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCHEDULING_CONTROLS_FEATURES_ARM", 1000417001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCHEDULING_CONTROLS_PROPERTIES_ARM", 1000417002),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_SLICED_VIEW_OF_3D_FEATURES_EXT", 1000418000),
    std::make_pair("VK_STRUCTURE_TYPE_IMAGE_VIEW_SLICED_CREATE_INFO_EXT", 1000418001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_SET_HOST_MAPPING_FEATURES_VALVE", 1000420000),
    std::make_pair("VK_STRUCTURE_TYPE_DESCRIPTOR_SET_BINDING_REFERENCE_VALVE", 1000420001),
    std::make_pair("VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_HOST_MAPPING_INFO_VALVE", 1000420002),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLAMP_ZERO_ONE_FEATURES_EXT", 1000421000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_NON_SEAMLESS_CUBE_MAP_FEATURES_EXT", 1000422000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RENDER_PASS_STRIPED_FEATURES_ARM", 1000424000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RENDER_PASS_STRIPED_PROPERTIES_ARM", 1000424001),
    std::make_pair("VK_STRUCTURE_TYPE_RENDER_PASS_STRIPE_BEGIN_INFO_ARM", 1000424002),
    std::make_pair("VK_STRUCTURE_TYPE_RENDER_PASS_STRIPE_INFO_ARM", 1000424003),
    std::make_pair("VK_STRUCTURE_TYPE_RENDER_PASS_STRIPE_SUBMIT_INFO_ARM", 1000424004),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_OFFSET_FEATURES_QCOM", 1000425000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_OFFSET_PROPERTIES_QCOM", 1000425001),
    std::make_pair("VK_STRUCTURE_TYPE_SUBPASS_FRAGMENT_DENSITY_MAP_OFFSET_END_INFO_QCOM", 1000425002),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COPY_MEMORY_INDIRECT_FEATURES_NV", 1000426000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COPY_MEMORY_INDIRECT_PROPERTIES_NV", 1000426001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_DECOMPRESSION_FEATURES_NV", 1000427000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_DECOMPRESSION_PROPERTIES_NV", 1000427001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_COMPUTE_FEATURES_NV", 1000428000),
    std::make_pair("VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_INDIRECT_BUFFER_INFO_NV", 1000428001),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_INDIRECT_DEVICE_ADDRESS_INFO_NV", 1000428002),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINEAR_COLOR_ATTACHMENT_FEATURES_NV", 1000430000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_MAXIMAL_RECONVERGENCE_FEATURES_KHR", 1000434000),
    std::make_pair("VK_STRUCTURE_TYPE_APPLICATION_PARAMETERS_EXT", 1000435000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_COMPRESSION_CONTROL_SWAPCHAIN_FEATURES_EXT", 1000437000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_PROCESSING_FEATURES_QCOM", 1000440000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_PROCESSING_PROPERTIES_QCOM", 1000440001),
    std::make_pair("VK_STRUCTURE_TYPE_IMAGE_VIEW_SAMPLE_WEIGHT_CREATE_INFO_QCOM", 1000440002),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_NESTED_COMMAND_BUFFER_FEATURES_EXT", 1000451000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_NESTED_COMMAND_BUFFER_PROPERTIES_EXT", 1000451001),
    std::make_pair("VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_ACQUIRE_UNMODIFIED_EXT", 1000453000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT", 1000455000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_PROPERTIES_EXT", 1000455001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBPASS_MERGE_FEEDBACK_FEATURES_EXT", 1000458000),
    std::make_pair("VK_STRUCTURE_TYPE_RENDER_PASS_CREATION_CONTROL_EXT", 1000458001),
    std::make_pair("VK_STRUCTURE_TYPE_RENDER_PASS_CREATION_FEEDBACK_CREATE_INFO_EXT", 1000458002),
    std::make_pair("VK_STRUCTURE_TYPE_RENDER_PASS_SUBPASS_FEEDBACK_CREATE_INFO_EXT", 1000458003),
    std::make_pair("VK_STRUCTURE_TYPE_DIRECT_DRIVER_LOADING_INFO_LUNARG", 1000459000),
    std::make_pair("VK_STRUCTURE_TYPE_DIRECT_DRIVER_LOADING_LIST_LUNARG", 1000459001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_MODULE_IDENTIFIER_FEATURES_EXT", 1000462000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_MODULE_IDENTIFIER_PROPERTIES_EXT", 1000462001),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_MODULE_IDENTIFIER_CREATE_INFO_EXT", 1000462002),
    std::make_pair("VK_STRUCTURE_TYPE_SHADER_MODULE_IDENTIFIER_EXT", 1000462003),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_FEATURES_EXT", 1000342000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_OPTICAL_FLOW_FEATURES_NV", 1000464000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_OPTICAL_FLOW_PROPERTIES_NV", 1000464001),
    std::make_pair("VK_STRUCTURE_TYPE_OPTICAL_FLOW_IMAGE_FORMAT_INFO_NV", 1000464002),
    std::make_pair("VK_STRUCTURE_TYPE_OPTICAL_FLOW_IMAGE_FORMAT_PROPERTIES_NV", 1000464003),
    std::make_pair("VK_STRUCTURE_TYPE_OPTICAL_FLOW_SESSION_CREATE_INFO_NV", 1000464004),
    std::make_pair("VK_STRUCTURE_TYPE_OPTICAL_FLOW_EXECUTE_INFO_NV", 1000464005),
    std::make_pair("VK_STRUCTURE_TYPE_OPTICAL_FLOW_SESSION_CREATE_PRIVATE_DATA_INFO_NV", 1000464010),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LEGACY_DITHERING_FEATURES_EXT", 1000465000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_PROTECTED_ACCESS_FEATURES_EXT", 1000466000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_FORMAT_RESOLVE_FEATURES_ANDROID", 1000468000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_FORMAT_RESOLVE_PROPERTIES_ANDROID", 1000468001),
    std::make_pair("VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_RESOLVE_PROPERTIES_ANDROID", 1000468002),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_5_FEATURES_KHR", 1000470000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_5_PROPERTIES_KHR", 1000470001),
    std::make_pair("VK_STRUCTURE_TYPE_RENDERING_AREA_INFO_KHR", 1000470003),
    std::make_pair("VK_STRUCTURE_TYPE_DEVICE_IMAGE_SUBRESOURCE_INFO_KHR", 1000470004),
    std::make_pair("VK_STRUCTURE_TYPE_SUBRESOURCE_LAYOUT_2_KHR", 1000338002),
    std::make_pair("VK_STRUCTURE_TYPE_IMAGE_SUBRESOURCE_2_KHR", 1000338003),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO_KHR", 1000470005),
    std::make_pair("VK_STRUCTURE_TYPE_BUFFER_USAGE_FLAGS_2_CREATE_INFO_KHR", 1000470006),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_POSITION_FETCH_FEATURES_KHR", 1000481000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT", 1000482000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_PROPERTIES_EXT", 1000482001),
    std::make_pair("VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT", 1000482002),
    std::make_pair("VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT", 1000352001),
    std::make_pair("VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT", 1000352002),
    std::make_pair("VK_STRUCTURE_TYPE_SHADER_REQUIRED_SUBGROUP_SIZE_CREATE_INFO_EXT", 1000225001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TILE_PROPERTIES_FEATURES_QCOM", 1000484000),
    std::make_pair("VK_STRUCTURE_TYPE_TILE_PROPERTIES_QCOM", 1000484001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_AMIGO_PROFILING_FEATURES_SEC", 1000485000),
    std::make_pair("VK_STRUCTURE_TYPE_AMIGO_PROFILING_SUBMIT_INFO_SEC", 1000485001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PER_VIEW_VIEWPORTS_FEATURES_QCOM", 1000488000),
    std::make_pair("VK_STRUCTURE_TYPE_SEMAPHORE_SCI_SYNC_POOL_CREATE_INFO_NV", 1000489000),
    std::make_pair("VK_STRUCTURE_TYPE_SEMAPHORE_SCI_SYNC_CREATE_INFO_NV", 1000489001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SCI_SYNC_2_FEATURES_NV", 1000489002),
    std::make_pair("VK_STRUCTURE_TYPE_IMPORT_FENCE_SCI_SYNC_INFO_NV", 1000373000),
    std::make_pair("VK_STRUCTURE_TYPE_EXPORT_FENCE_SCI_SYNC_INFO_NV", 1000373001),
    std::make_pair("VK_STRUCTURE_TYPE_FENCE_GET_SCI_SYNC_INFO_NV", 1000373002),
    std::make_pair("VK_STRUCTURE_TYPE_SCI_SYNC_ATTRIBUTES_INFO_NV", 1000373003),
    std::make_pair("VK_STRUCTURE_TYPE_DEVICE_SEMAPHORE_SCI_SYNC_POOL_RESERVATION_CREATE_INFO_NV", 1000489003),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_INVOCATION_REORDER_FEATURES_NV", 1000490000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_INVOCATION_REORDER_PROPERTIES_NV", 1000490001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_SPARSE_ADDRESS_SPACE_FEATURES_NV", 1000492000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_SPARSE_ADDRESS_SPACE_PROPERTIES_NV", 1000492001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MUTABLE_DESCRIPTOR_TYPE_FEATURES_EXT", 1000351000),
    std::make_pair("VK_STRUCTURE_TYPE_MUTABLE_DESCRIPTOR_TYPE_CREATE_INFO_EXT", 1000351002),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LEGACY_VERTEX_ATTRIBUTES_FEATURES_EXT", 1000495000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LEGACY_VERTEX_ATTRIBUTES_PROPERTIES_EXT", 1000495001),
    std::make_pair("VK_STRUCTURE_TYPE_LAYER_SETTINGS_CREATE_INFO_EXT", 1000496000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CORE_BUILTINS_FEATURES_ARM", 1000497000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CORE_BUILTINS_PROPERTIES_ARM", 1000497001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_LIBRARY_GROUP_HANDLES_FEATURES_EXT", 1000498000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_UNUSED_ATTACHMENTS_FEATURES_EXT", 1000499000),
    std::make_pair("VK_STRUCTURE_TYPE_LATENCY_SLEEP_MODE_INFO_NV", 1000505000),
    std::make_pair("VK_STRUCTURE_TYPE_LATENCY_SLEEP_INFO_NV", 1000505001),
    std::make_pair("VK_STRUCTURE_TYPE_SET_LATENCY_MARKER_INFO_NV", 1000505002),
    std::make_pair("VK_STRUCTURE_TYPE_GET_LATENCY_MARKER_INFO_NV", 1000505003),
    std::make_pair("VK_STRUCTURE_TYPE_LATENCY_TIMINGS_FRAME_REPORT_NV", 1000505004),
    std::make_pair("VK_STRUCTURE_TYPE_LATENCY_SUBMISSION_PRESENT_ID_NV", 1000505005),
    std::make_pair("VK_STRUCTURE_TYPE_OUT_OF_BAND_QUEUE_TYPE_INFO_NV", 1000505006),
    std::make_pair("VK_STRUCTURE_TYPE_SWAPCHAIN_LATENCY_CREATE_INFO_NV", 1000505007),
    std::make_pair("VK_STRUCTURE_TYPE_LATENCY_SURFACE_CAPABILITIES_NV", 1000505008),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_FEATURES_KHR", 1000506000),
    std::make_pair("VK_STRUCTURE_TYPE_COOPERATIVE_MATRIX_PROPERTIES_KHR", 1000506001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_PROPERTIES_KHR", 1000506002),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PER_VIEW_RENDER_AREAS_FEATURES_QCOM", 1000510000),
    std::make_pair("VK_STRUCTURE_TYPE_MULTIVIEW_PER_VIEW_RENDER_AREAS_RENDER_PASS_BEGIN_INFO_QCOM", 1000510001),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_DECODE_AV1_CAPABILITIES_KHR", 1000512000),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_DECODE_AV1_PICTURE_INFO_KHR", 1000512001),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_DECODE_AV1_PROFILE_INFO_KHR", 1000512003),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_DECODE_AV1_SESSION_PARAMETERS_CREATE_INFO_KHR", 1000512004),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_DECODE_AV1_DPB_SLOT_INFO_KHR", 1000512005),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VIDEO_MAINTENANCE_1_FEATURES_KHR", 1000515000),
    std::make_pair("VK_STRUCTURE_TYPE_VIDEO_INLINE_QUERY_INFO_KHR", 1000515001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PER_STAGE_DESCRIPTOR_SET_FEATURES_NV", 1000516000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_PROCESSING_2_FEATURES_QCOM", 1000518000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_PROCESSING_2_PROPERTIES_QCOM", 1000518001),
    std::make_pair("VK_STRUCTURE_TYPE_SAMPLER_BLOCK_MATCH_WINDOW_CREATE_INFO_QCOM", 1000518002),
    std::make_pair("VK_STRUCTURE_TYPE_SAMPLER_CUBIC_WEIGHTS_CREATE_INFO_QCOM", 1000519000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUBIC_WEIGHTS_FEATURES_QCOM", 1000519001),
    std::make_pair("VK_STRUCTURE_TYPE_BLIT_IMAGE_CUBIC_WEIGHTS_INFO_QCOM", 1000519002),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_YCBCR_DEGAMMA_FEATURES_QCOM", 1000520000),
    std::make_pair("VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_YCBCR_DEGAMMA_CREATE_INFO_QCOM", 1000520001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUBIC_CLAMP_FEATURES_QCOM", 1000521000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ATTACHMENT_FEEDBACK_LOOP_DYNAMIC_STATE_FEATURES_EXT", 1000524000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_PROPERTIES_KHR", 1000525000),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_DIVISOR_STATE_CREATE_INFO_KHR", 1000190001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES_KHR", 1000190002),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT_CONTROLS_2_FEATURES_KHR", 1000528000),
    std::make_pair("VK_STRUCTURE_TYPE_SCREEN_BUFFER_PROPERTIES_QNX", 1000529000),
    std::make_pair("VK_STRUCTURE_TYPE_SCREEN_BUFFER_FORMAT_PROPERTIES_QNX", 1000529001),
    std::make_pair("VK_STRUCTURE_TYPE_IMPORT_SCREEN_BUFFER_INFO_QNX", 1000529002),
    std::make_pair("VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_QNX", 1000529003),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_SCREEN_BUFFER_FEATURES_QNX", 1000529004),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LAYERED_DRIVER_PROPERTIES_MSFT", 1000530000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INDEX_TYPE_UINT8_FEATURES_KHR", 1000265000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_FEATURES_KHR", 1000259000),
    std::make_pair("VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_LINE_STATE_CREATE_INFO_KHR", 1000259001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_PROPERTIES_KHR", 1000259002),
    std::make_pair("VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_KHR", 1000184000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_EXPECT_ASSUME_FEATURES_KHR", 1000544000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_6_FEATURES_KHR", 1000545000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_6_PROPERTIES_KHR", 1000545001),
    std::make_pair("VK_STRUCTURE_TYPE_BIND_MEMORY_STATUS_KHR", 1000545002),
    std::make_pair("VK_STRUCTURE_TYPE_BIND_DESCRIPTOR_SETS_INFO_KHR", 1000545003),
    std::make_pair("VK_STRUCTURE_TYPE_PUSH_CONSTANTS_INFO_KHR", 1000545004),
    std::make_pair("VK_STRUCTURE_TYPE_PUSH_DESCRIPTOR_SET_INFO_KHR", 1000545005),
    std::make_pair("VK_STRUCTURE_TYPE_PUSH_DESCRIPTOR_SET_WITH_TEMPLATE_INFO_KHR", 1000545006),
    std::make_pair("VK_STRUCTURE_TYPE_SET_DESCRIPTOR_BUFFER_OFFSETS_INFO_EXT", 1000545007),
    std::make_pair("VK_STRUCTURE_TYPE_BIND_DESCRIPTOR_BUFFER_EMBEDDED_SAMPLERS_INFO_EXT", 1000545008),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_POOL_OVERALLOCATION_FEATURES_NV", 1000546000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAW_ACCESS_CHAINS_FEATURES_NV", 1000555000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_RELAXED_EXTENDED_INSTRUCTION_FEATURES_KHR", 1000558000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_7_FEATURES_KHR", 1000562000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_7_PROPERTIES_KHR", 1000562001),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LAYERED_API_PROPERTIES_LIST_KHR", 1000562002),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LAYERED_API_PROPERTIES_KHR", 1000562003),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LAYERED_API_VULKAN_PROPERTIES_KHR", 1000562004),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT16_VECTOR_FEATURES_NV", 1000563000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_REPLICATED_COMPOSITES_FEATURES_EXT", 1000564000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_VALIDATION_FEATURES_NV", 1000568000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_ALIGNMENT_CONTROL_FEATURES_MESA", 1000575000),
    std::make_pair("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_ALIGNMENT_CONTROL_PROPERTIES_MESA", 1000575001),
    std::make_pair("VK_STRUCTURE_TYPE_IMAGE_ALIGNMENT_CONTROL_CREATE_INFO_MESA", 1000575002),
};
static void parse_VkStructureType(const Json::Value &obj, VkStructureType &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkStructureType)VkStructureType_map[std::string(_res)];
}

static std::map<std::string, int> VkPipelineCacheHeaderVersion_map = {
    std::make_pair("VK_PIPELINE_CACHE_HEADER_VERSION_ONE", 1),
    std::make_pair("VK_PIPELINE_CACHE_HEADER_VERSION_SAFETY_CRITICAL_ONE", 1000298001),
};
static void parse_VkPipelineCacheHeaderVersion(const Json::Value &obj, VkPipelineCacheHeaderVersion &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkPipelineCacheHeaderVersion)VkPipelineCacheHeaderVersion_map[std::string(_res)];
}

static std::map<std::string, int> VkAccessFlagBits_map = {
    std::make_pair("VK_ACCESS_INDIRECT_COMMAND_READ_BIT", 1UL << 0),
    std::make_pair("VK_ACCESS_INDEX_READ_BIT", 1UL << 1),
    std::make_pair("VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT", 1UL << 2),
    std::make_pair("VK_ACCESS_UNIFORM_READ_BIT", 1UL << 3),
    std::make_pair("VK_ACCESS_INPUT_ATTACHMENT_READ_BIT", 1UL << 4),
    std::make_pair("VK_ACCESS_SHADER_READ_BIT", 1UL << 5),
    std::make_pair("VK_ACCESS_SHADER_WRITE_BIT", 1UL << 6),
    std::make_pair("VK_ACCESS_COLOR_ATTACHMENT_READ_BIT", 1UL << 7),
    std::make_pair("VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT", 1UL << 8),
    std::make_pair("VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT", 1UL << 9),
    std::make_pair("VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT", 1UL << 10),
    std::make_pair("VK_ACCESS_TRANSFER_READ_BIT", 1UL << 11),
    std::make_pair("VK_ACCESS_TRANSFER_WRITE_BIT", 1UL << 12),
    std::make_pair("VK_ACCESS_HOST_READ_BIT", 1UL << 13),
    std::make_pair("VK_ACCESS_HOST_WRITE_BIT", 1UL << 14),
    std::make_pair("VK_ACCESS_MEMORY_READ_BIT", 1UL << 15),
    std::make_pair("VK_ACCESS_MEMORY_WRITE_BIT", 1UL << 16),
    std::make_pair("VK_ACCESS_NONE", 0),
    std::make_pair("VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT", 1UL << 25),
    std::make_pair("VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT", 1UL << 26),
    std::make_pair("VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT", 1UL << 27),
    std::make_pair("VK_ACCESS_CONDITIONAL_RENDERING_READ_BIT_EXT", 1UL << 20),
    std::make_pair("VK_ACCESS_COLOR_ATTACHMENT_READ_NONCOHERENT_BIT_EXT", 1UL << 19),
    std::make_pair("VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR", 1UL << 21),
    std::make_pair("VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR", 1UL << 22),
    std::make_pair("VK_ACCESS_SHADING_RATE_IMAGE_READ_BIT_NV", 1UL << 23),
    std::make_pair("VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV", 1UL << 21),
    std::make_pair("VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV", 1UL << 22),
    std::make_pair("VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT", 1UL << 24),
    std::make_pair("VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR", 1UL << 23),
    std::make_pair("VK_ACCESS_COMMAND_PREPROCESS_READ_BIT_NV", 1UL << 17),
    std::make_pair("VK_ACCESS_COMMAND_PREPROCESS_WRITE_BIT_NV", 1UL << 18),
    std::make_pair("VK_ACCESS_NONE_KHR", 0),
};
static void parse_VkAccessFlagBits(const Json::Value &obj, VkAccessFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkAccessFlagBits)VkAccessFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkImageLayout_map = {
    std::make_pair("VK_IMAGE_LAYOUT_UNDEFINED", 0),
    std::make_pair("VK_IMAGE_LAYOUT_GENERAL", 1),
    std::make_pair("VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL", 2),
    std::make_pair("VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL", 3),
    std::make_pair("VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL", 4),
    std::make_pair("VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL", 5),
    std::make_pair("VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL", 6),
    std::make_pair("VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL", 7),
    std::make_pair("VK_IMAGE_LAYOUT_PREINITIALIZED", 8),
    std::make_pair("VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL", 1000117000),
    std::make_pair("VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL", 1000117001),
    std::make_pair("VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL", 1000241000),
    std::make_pair("VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL", 1000241001),
    std::make_pair("VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL", 1000241002),
    std::make_pair("VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL", 1000241003),
    std::make_pair("VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL", 1000314000),
    std::make_pair("VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL", 1000314001),
    std::make_pair("VK_IMAGE_LAYOUT_PRESENT_SRC_KHR", 1000001002),
    std::make_pair("VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR", 1000024000),
    std::make_pair("VK_IMAGE_LAYOUT_VIDEO_DECODE_SRC_KHR", 1000024001),
    std::make_pair("VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR", 1000024002),
    std::make_pair("VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR", 1000111000),
    std::make_pair("VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL_KHR", 1000117000),
    std::make_pair("VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL_KHR", 1000117001),
    std::make_pair("VK_IMAGE_LAYOUT_SHADING_RATE_OPTIMAL_NV", 1000164003),
    std::make_pair("VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT", 1000218000),
    std::make_pair("VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR", 1000164003),
    std::make_pair("VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR", 1000232000),
    std::make_pair("VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL_KHR", 1000241000),
    std::make_pair("VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL_KHR", 1000241001),
    std::make_pair("VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL_KHR", 1000241002),
    std::make_pair("VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL_KHR", 1000241003),
    std::make_pair("VK_IMAGE_LAYOUT_VIDEO_ENCODE_DST_KHR", 1000299000),
    std::make_pair("VK_IMAGE_LAYOUT_VIDEO_ENCODE_SRC_KHR", 1000299001),
    std::make_pair("VK_IMAGE_LAYOUT_VIDEO_ENCODE_DPB_KHR", 1000299002),
    std::make_pair("VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR", 1000314000),
    std::make_pair("VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR", 1000314001),
    std::make_pair("VK_IMAGE_LAYOUT_ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT", 1000339000),
};
static void parse_VkImageLayout(const Json::Value &obj, VkImageLayout &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkImageLayout)VkImageLayout_map[std::string(_res)];
}

static std::map<std::string, int> VkImageAspectFlagBits_map = {
    std::make_pair("VK_IMAGE_ASPECT_COLOR_BIT", 1UL << 0),
    std::make_pair("VK_IMAGE_ASPECT_DEPTH_BIT", 1UL << 1),
    std::make_pair("VK_IMAGE_ASPECT_STENCIL_BIT", 1UL << 2),
    std::make_pair("VK_IMAGE_ASPECT_METADATA_BIT", 1UL << 3),
    std::make_pair("VK_IMAGE_ASPECT_PLANE_0_BIT", 1UL << 4),
    std::make_pair("VK_IMAGE_ASPECT_PLANE_1_BIT", 1UL << 5),
    std::make_pair("VK_IMAGE_ASPECT_PLANE_2_BIT", 1UL << 6),
    std::make_pair("VK_IMAGE_ASPECT_NONE", 0),
    std::make_pair("VK_IMAGE_ASPECT_PLANE_0_BIT_KHR", 1UL << 4),
    std::make_pair("VK_IMAGE_ASPECT_PLANE_1_BIT_KHR", 1UL << 5),
    std::make_pair("VK_IMAGE_ASPECT_PLANE_2_BIT_KHR", 1UL << 6),
    std::make_pair("VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT", 1UL << 7),
    std::make_pair("VK_IMAGE_ASPECT_MEMORY_PLANE_1_BIT_EXT", 1UL << 8),
    std::make_pair("VK_IMAGE_ASPECT_MEMORY_PLANE_2_BIT_EXT", 1UL << 9),
    std::make_pair("VK_IMAGE_ASPECT_MEMORY_PLANE_3_BIT_EXT", 1UL << 10),
    std::make_pair("VK_IMAGE_ASPECT_NONE_KHR", 0),
};
static void parse_VkImageAspectFlagBits(const Json::Value &obj, VkImageAspectFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkImageAspectFlagBits)VkImageAspectFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkObjectType_map = {
    std::make_pair("VK_OBJECT_TYPE_UNKNOWN", 0),
    std::make_pair("VK_OBJECT_TYPE_INSTANCE", 1),
    std::make_pair("VK_OBJECT_TYPE_PHYSICAL_DEVICE", 2),
    std::make_pair("VK_OBJECT_TYPE_DEVICE", 3),
    std::make_pair("VK_OBJECT_TYPE_QUEUE", 4),
    std::make_pair("VK_OBJECT_TYPE_SEMAPHORE", 5),
    std::make_pair("VK_OBJECT_TYPE_COMMAND_BUFFER", 6),
    std::make_pair("VK_OBJECT_TYPE_FENCE", 7),
    std::make_pair("VK_OBJECT_TYPE_DEVICE_MEMORY", 8),
    std::make_pair("VK_OBJECT_TYPE_BUFFER", 9),
    std::make_pair("VK_OBJECT_TYPE_IMAGE", 10),
    std::make_pair("VK_OBJECT_TYPE_EVENT", 11),
    std::make_pair("VK_OBJECT_TYPE_QUERY_POOL", 12),
    std::make_pair("VK_OBJECT_TYPE_BUFFER_VIEW", 13),
    std::make_pair("VK_OBJECT_TYPE_IMAGE_VIEW", 14),
    std::make_pair("VK_OBJECT_TYPE_SHADER_MODULE", 15),
    std::make_pair("VK_OBJECT_TYPE_PIPELINE_CACHE", 16),
    std::make_pair("VK_OBJECT_TYPE_PIPELINE_LAYOUT", 17),
    std::make_pair("VK_OBJECT_TYPE_RENDER_PASS", 18),
    std::make_pair("VK_OBJECT_TYPE_PIPELINE", 19),
    std::make_pair("VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT", 20),
    std::make_pair("VK_OBJECT_TYPE_SAMPLER", 21),
    std::make_pair("VK_OBJECT_TYPE_DESCRIPTOR_POOL", 22),
    std::make_pair("VK_OBJECT_TYPE_DESCRIPTOR_SET", 23),
    std::make_pair("VK_OBJECT_TYPE_FRAMEBUFFER", 24),
    std::make_pair("VK_OBJECT_TYPE_COMMAND_POOL", 25),
    std::make_pair("VK_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION", 1000156000),
    std::make_pair("VK_OBJECT_TYPE_PRIVATE_DATA_SLOT", 1000295000),
    std::make_pair("VK_OBJECT_TYPE_SURFACE_KHR", 1000000000),
    std::make_pair("VK_OBJECT_TYPE_SWAPCHAIN_KHR", 1000001000),
    std::make_pair("VK_OBJECT_TYPE_DISPLAY_KHR", 1000002000),
    std::make_pair("VK_OBJECT_TYPE_DISPLAY_MODE_KHR", 1000002001),
    std::make_pair("VK_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT", 1000011000),
    std::make_pair("VK_OBJECT_TYPE_VIDEO_SESSION_KHR", 1000023000),
    std::make_pair("VK_OBJECT_TYPE_VIDEO_SESSION_PARAMETERS_KHR", 1000023001),
    std::make_pair("VK_OBJECT_TYPE_CU_MODULE_NVX", 1000029000),
    std::make_pair("VK_OBJECT_TYPE_CU_FUNCTION_NVX", 1000029001),
    std::make_pair("VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT", 1000128000),
    std::make_pair("VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR", 1000150000),
    std::make_pair("VK_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION_KHR", 1000156000),
    std::make_pair("VK_OBJECT_TYPE_VALIDATION_CACHE_EXT", 1000160000),
    std::make_pair("VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_NV", 1000165000),
    std::make_pair("VK_OBJECT_TYPE_PERFORMANCE_CONFIGURATION_INTEL", 1000210000),
    std::make_pair("VK_OBJECT_TYPE_DEFERRED_OPERATION_KHR", 1000268000),
    std::make_pair("VK_OBJECT_TYPE_INDIRECT_COMMANDS_LAYOUT_NV", 1000277000),
    std::make_pair("VK_OBJECT_TYPE_PRIVATE_DATA_SLOT_EXT", 1000295000),
    std::make_pair("VK_OBJECT_TYPE_CUDA_MODULE_NV", 1000307000),
    std::make_pair("VK_OBJECT_TYPE_CUDA_FUNCTION_NV", 1000307001),
    std::make_pair("VK_OBJECT_TYPE_BUFFER_COLLECTION_FUCHSIA", 1000366000),
    std::make_pair("VK_OBJECT_TYPE_MICROMAP_EXT", 1000396000),
    std::make_pair("VK_OBJECT_TYPE_OPTICAL_FLOW_SESSION_NV", 1000464000),
    std::make_pair("VK_OBJECT_TYPE_SHADER_EXT", 1000482000),
    std::make_pair("VK_OBJECT_TYPE_SEMAPHORE_SCI_SYNC_POOL_NV", 1000489000),
};
static void parse_VkObjectType(const Json::Value &obj, VkObjectType &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkObjectType)VkObjectType_map[std::string(_res)];
}

static std::map<std::string, int> VkVendorId_map = {
    std::make_pair("VK_VENDOR_ID_KHRONOS", 0x10000),  std::make_pair("VK_VENDOR_ID_VIV", 0x10001),
    std::make_pair("VK_VENDOR_ID_VSI", 0x10002),      std::make_pair("VK_VENDOR_ID_KAZAN", 0x10003),
    std::make_pair("VK_VENDOR_ID_CODEPLAY", 0x10004), std::make_pair("VK_VENDOR_ID_MESA", 0x10005),
    std::make_pair("VK_VENDOR_ID_POCL", 0x10006),     std::make_pair("VK_VENDOR_ID_MOBILEYE", 0x10007),
};
static void parse_VkVendorId(const Json::Value &obj, VkVendorId &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkVendorId)VkVendorId_map[std::string(_res)];
}

static std::map<std::string, int> VkSystemAllocationScope_map = {
    std::make_pair("VK_SYSTEM_ALLOCATION_SCOPE_COMMAND", 0),  std::make_pair("VK_SYSTEM_ALLOCATION_SCOPE_OBJECT", 1),
    std::make_pair("VK_SYSTEM_ALLOCATION_SCOPE_CACHE", 2),    std::make_pair("VK_SYSTEM_ALLOCATION_SCOPE_DEVICE", 3),
    std::make_pair("VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE", 4),
};
static void parse_VkSystemAllocationScope(const Json::Value &obj, VkSystemAllocationScope &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkSystemAllocationScope)VkSystemAllocationScope_map[std::string(_res)];
}

static std::map<std::string, int> VkInternalAllocationType_map = {
    std::make_pair("VK_INTERNAL_ALLOCATION_TYPE_EXECUTABLE", 0),
};
static void parse_VkInternalAllocationType(const Json::Value &obj, VkInternalAllocationType &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkInternalAllocationType)VkInternalAllocationType_map[std::string(_res)];
}

static std::map<std::string, int> VkFormat_map = {
    std::make_pair("VK_FORMAT_UNDEFINED", 0),
    std::make_pair("VK_FORMAT_R4G4_UNORM_PACK8", 1),
    std::make_pair("VK_FORMAT_R4G4B4A4_UNORM_PACK16", 2),
    std::make_pair("VK_FORMAT_B4G4R4A4_UNORM_PACK16", 3),
    std::make_pair("VK_FORMAT_R5G6B5_UNORM_PACK16", 4),
    std::make_pair("VK_FORMAT_B5G6R5_UNORM_PACK16", 5),
    std::make_pair("VK_FORMAT_R5G5B5A1_UNORM_PACK16", 6),
    std::make_pair("VK_FORMAT_B5G5R5A1_UNORM_PACK16", 7),
    std::make_pair("VK_FORMAT_A1R5G5B5_UNORM_PACK16", 8),
    std::make_pair("VK_FORMAT_R8_UNORM", 9),
    std::make_pair("VK_FORMAT_R8_SNORM", 10),
    std::make_pair("VK_FORMAT_R8_USCALED", 11),
    std::make_pair("VK_FORMAT_R8_SSCALED", 12),
    std::make_pair("VK_FORMAT_R8_UINT", 13),
    std::make_pair("VK_FORMAT_R8_SINT", 14),
    std::make_pair("VK_FORMAT_R8_SRGB", 15),
    std::make_pair("VK_FORMAT_R8G8_UNORM", 16),
    std::make_pair("VK_FORMAT_R8G8_SNORM", 17),
    std::make_pair("VK_FORMAT_R8G8_USCALED", 18),
    std::make_pair("VK_FORMAT_R8G8_SSCALED", 19),
    std::make_pair("VK_FORMAT_R8G8_UINT", 20),
    std::make_pair("VK_FORMAT_R8G8_SINT", 21),
    std::make_pair("VK_FORMAT_R8G8_SRGB", 22),
    std::make_pair("VK_FORMAT_R8G8B8_UNORM", 23),
    std::make_pair("VK_FORMAT_R8G8B8_SNORM", 24),
    std::make_pair("VK_FORMAT_R8G8B8_USCALED", 25),
    std::make_pair("VK_FORMAT_R8G8B8_SSCALED", 26),
    std::make_pair("VK_FORMAT_R8G8B8_UINT", 27),
    std::make_pair("VK_FORMAT_R8G8B8_SINT", 28),
    std::make_pair("VK_FORMAT_R8G8B8_SRGB", 29),
    std::make_pair("VK_FORMAT_B8G8R8_UNORM", 30),
    std::make_pair("VK_FORMAT_B8G8R8_SNORM", 31),
    std::make_pair("VK_FORMAT_B8G8R8_USCALED", 32),
    std::make_pair("VK_FORMAT_B8G8R8_SSCALED", 33),
    std::make_pair("VK_FORMAT_B8G8R8_UINT", 34),
    std::make_pair("VK_FORMAT_B8G8R8_SINT", 35),
    std::make_pair("VK_FORMAT_B8G8R8_SRGB", 36),
    std::make_pair("VK_FORMAT_R8G8B8A8_UNORM", 37),
    std::make_pair("VK_FORMAT_R8G8B8A8_SNORM", 38),
    std::make_pair("VK_FORMAT_R8G8B8A8_USCALED", 39),
    std::make_pair("VK_FORMAT_R8G8B8A8_SSCALED", 40),
    std::make_pair("VK_FORMAT_R8G8B8A8_UINT", 41),
    std::make_pair("VK_FORMAT_R8G8B8A8_SINT", 42),
    std::make_pair("VK_FORMAT_R8G8B8A8_SRGB", 43),
    std::make_pair("VK_FORMAT_B8G8R8A8_UNORM", 44),
    std::make_pair("VK_FORMAT_B8G8R8A8_SNORM", 45),
    std::make_pair("VK_FORMAT_B8G8R8A8_USCALED", 46),
    std::make_pair("VK_FORMAT_B8G8R8A8_SSCALED", 47),
    std::make_pair("VK_FORMAT_B8G8R8A8_UINT", 48),
    std::make_pair("VK_FORMAT_B8G8R8A8_SINT", 49),
    std::make_pair("VK_FORMAT_B8G8R8A8_SRGB", 50),
    std::make_pair("VK_FORMAT_A8B8G8R8_UNORM_PACK32", 51),
    std::make_pair("VK_FORMAT_A8B8G8R8_SNORM_PACK32", 52),
    std::make_pair("VK_FORMAT_A8B8G8R8_USCALED_PACK32", 53),
    std::make_pair("VK_FORMAT_A8B8G8R8_SSCALED_PACK32", 54),
    std::make_pair("VK_FORMAT_A8B8G8R8_UINT_PACK32", 55),
    std::make_pair("VK_FORMAT_A8B8G8R8_SINT_PACK32", 56),
    std::make_pair("VK_FORMAT_A8B8G8R8_SRGB_PACK32", 57),
    std::make_pair("VK_FORMAT_A2R10G10B10_UNORM_PACK32", 58),
    std::make_pair("VK_FORMAT_A2R10G10B10_SNORM_PACK32", 59),
    std::make_pair("VK_FORMAT_A2R10G10B10_USCALED_PACK32", 60),
    std::make_pair("VK_FORMAT_A2R10G10B10_SSCALED_PACK32", 61),
    std::make_pair("VK_FORMAT_A2R10G10B10_UINT_PACK32", 62),
    std::make_pair("VK_FORMAT_A2R10G10B10_SINT_PACK32", 63),
    std::make_pair("VK_FORMAT_A2B10G10R10_UNORM_PACK32", 64),
    std::make_pair("VK_FORMAT_A2B10G10R10_SNORM_PACK32", 65),
    std::make_pair("VK_FORMAT_A2B10G10R10_USCALED_PACK32", 66),
    std::make_pair("VK_FORMAT_A2B10G10R10_SSCALED_PACK32", 67),
    std::make_pair("VK_FORMAT_A2B10G10R10_UINT_PACK32", 68),
    std::make_pair("VK_FORMAT_A2B10G10R10_SINT_PACK32", 69),
    std::make_pair("VK_FORMAT_R16_UNORM", 70),
    std::make_pair("VK_FORMAT_R16_SNORM", 71),
    std::make_pair("VK_FORMAT_R16_USCALED", 72),
    std::make_pair("VK_FORMAT_R16_SSCALED", 73),
    std::make_pair("VK_FORMAT_R16_UINT", 74),
    std::make_pair("VK_FORMAT_R16_SINT", 75),
    std::make_pair("VK_FORMAT_R16_SFLOAT", 76),
    std::make_pair("VK_FORMAT_R16G16_UNORM", 77),
    std::make_pair("VK_FORMAT_R16G16_SNORM", 78),
    std::make_pair("VK_FORMAT_R16G16_USCALED", 79),
    std::make_pair("VK_FORMAT_R16G16_SSCALED", 80),
    std::make_pair("VK_FORMAT_R16G16_UINT", 81),
    std::make_pair("VK_FORMAT_R16G16_SINT", 82),
    std::make_pair("VK_FORMAT_R16G16_SFLOAT", 83),
    std::make_pair("VK_FORMAT_R16G16B16_UNORM", 84),
    std::make_pair("VK_FORMAT_R16G16B16_SNORM", 85),
    std::make_pair("VK_FORMAT_R16G16B16_USCALED", 86),
    std::make_pair("VK_FORMAT_R16G16B16_SSCALED", 87),
    std::make_pair("VK_FORMAT_R16G16B16_UINT", 88),
    std::make_pair("VK_FORMAT_R16G16B16_SINT", 89),
    std::make_pair("VK_FORMAT_R16G16B16_SFLOAT", 90),
    std::make_pair("VK_FORMAT_R16G16B16A16_UNORM", 91),
    std::make_pair("VK_FORMAT_R16G16B16A16_SNORM", 92),
    std::make_pair("VK_FORMAT_R16G16B16A16_USCALED", 93),
    std::make_pair("VK_FORMAT_R16G16B16A16_SSCALED", 94),
    std::make_pair("VK_FORMAT_R16G16B16A16_UINT", 95),
    std::make_pair("VK_FORMAT_R16G16B16A16_SINT", 96),
    std::make_pair("VK_FORMAT_R16G16B16A16_SFLOAT", 97),
    std::make_pair("VK_FORMAT_R32_UINT", 98),
    std::make_pair("VK_FORMAT_R32_SINT", 99),
    std::make_pair("VK_FORMAT_R32_SFLOAT", 100),
    std::make_pair("VK_FORMAT_R32G32_UINT", 101),
    std::make_pair("VK_FORMAT_R32G32_SINT", 102),
    std::make_pair("VK_FORMAT_R32G32_SFLOAT", 103),
    std::make_pair("VK_FORMAT_R32G32B32_UINT", 104),
    std::make_pair("VK_FORMAT_R32G32B32_SINT", 105),
    std::make_pair("VK_FORMAT_R32G32B32_SFLOAT", 106),
    std::make_pair("VK_FORMAT_R32G32B32A32_UINT", 107),
    std::make_pair("VK_FORMAT_R32G32B32A32_SINT", 108),
    std::make_pair("VK_FORMAT_R32G32B32A32_SFLOAT", 109),
    std::make_pair("VK_FORMAT_R64_UINT", 110),
    std::make_pair("VK_FORMAT_R64_SINT", 111),
    std::make_pair("VK_FORMAT_R64_SFLOAT", 112),
    std::make_pair("VK_FORMAT_R64G64_UINT", 113),
    std::make_pair("VK_FORMAT_R64G64_SINT", 114),
    std::make_pair("VK_FORMAT_R64G64_SFLOAT", 115),
    std::make_pair("VK_FORMAT_R64G64B64_UINT", 116),
    std::make_pair("VK_FORMAT_R64G64B64_SINT", 117),
    std::make_pair("VK_FORMAT_R64G64B64_SFLOAT", 118),
    std::make_pair("VK_FORMAT_R64G64B64A64_UINT", 119),
    std::make_pair("VK_FORMAT_R64G64B64A64_SINT", 120),
    std::make_pair("VK_FORMAT_R64G64B64A64_SFLOAT", 121),
    std::make_pair("VK_FORMAT_B10G11R11_UFLOAT_PACK32", 122),
    std::make_pair("VK_FORMAT_E5B9G9R9_UFLOAT_PACK32", 123),
    std::make_pair("VK_FORMAT_D16_UNORM", 124),
    std::make_pair("VK_FORMAT_X8_D24_UNORM_PACK32", 125),
    std::make_pair("VK_FORMAT_D32_SFLOAT", 126),
    std::make_pair("VK_FORMAT_S8_UINT", 127),
    std::make_pair("VK_FORMAT_D16_UNORM_S8_UINT", 128),
    std::make_pair("VK_FORMAT_D24_UNORM_S8_UINT", 129),
    std::make_pair("VK_FORMAT_D32_SFLOAT_S8_UINT", 130),
    std::make_pair("VK_FORMAT_BC1_RGB_UNORM_BLOCK", 131),
    std::make_pair("VK_FORMAT_BC1_RGB_SRGB_BLOCK", 132),
    std::make_pair("VK_FORMAT_BC1_RGBA_UNORM_BLOCK", 133),
    std::make_pair("VK_FORMAT_BC1_RGBA_SRGB_BLOCK", 134),
    std::make_pair("VK_FORMAT_BC2_UNORM_BLOCK", 135),
    std::make_pair("VK_FORMAT_BC2_SRGB_BLOCK", 136),
    std::make_pair("VK_FORMAT_BC3_UNORM_BLOCK", 137),
    std::make_pair("VK_FORMAT_BC3_SRGB_BLOCK", 138),
    std::make_pair("VK_FORMAT_BC4_UNORM_BLOCK", 139),
    std::make_pair("VK_FORMAT_BC4_SNORM_BLOCK", 140),
    std::make_pair("VK_FORMAT_BC5_UNORM_BLOCK", 141),
    std::make_pair("VK_FORMAT_BC5_SNORM_BLOCK", 142),
    std::make_pair("VK_FORMAT_BC6H_UFLOAT_BLOCK", 143),
    std::make_pair("VK_FORMAT_BC6H_SFLOAT_BLOCK", 144),
    std::make_pair("VK_FORMAT_BC7_UNORM_BLOCK", 145),
    std::make_pair("VK_FORMAT_BC7_SRGB_BLOCK", 146),
    std::make_pair("VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK", 147),
    std::make_pair("VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK", 148),
    std::make_pair("VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK", 149),
    std::make_pair("VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK", 150),
    std::make_pair("VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK", 151),
    std::make_pair("VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK", 152),
    std::make_pair("VK_FORMAT_EAC_R11_UNORM_BLOCK", 153),
    std::make_pair("VK_FORMAT_EAC_R11_SNORM_BLOCK", 154),
    std::make_pair("VK_FORMAT_EAC_R11G11_UNORM_BLOCK", 155),
    std::make_pair("VK_FORMAT_EAC_R11G11_SNORM_BLOCK", 156),
    std::make_pair("VK_FORMAT_ASTC_4x4_UNORM_BLOCK", 157),
    std::make_pair("VK_FORMAT_ASTC_4x4_SRGB_BLOCK", 158),
    std::make_pair("VK_FORMAT_ASTC_5x4_UNORM_BLOCK", 159),
    std::make_pair("VK_FORMAT_ASTC_5x4_SRGB_BLOCK", 160),
    std::make_pair("VK_FORMAT_ASTC_5x5_UNORM_BLOCK", 161),
    std::make_pair("VK_FORMAT_ASTC_5x5_SRGB_BLOCK", 162),
    std::make_pair("VK_FORMAT_ASTC_6x5_UNORM_BLOCK", 163),
    std::make_pair("VK_FORMAT_ASTC_6x5_SRGB_BLOCK", 164),
    std::make_pair("VK_FORMAT_ASTC_6x6_UNORM_BLOCK", 165),
    std::make_pair("VK_FORMAT_ASTC_6x6_SRGB_BLOCK", 166),
    std::make_pair("VK_FORMAT_ASTC_8x5_UNORM_BLOCK", 167),
    std::make_pair("VK_FORMAT_ASTC_8x5_SRGB_BLOCK", 168),
    std::make_pair("VK_FORMAT_ASTC_8x6_UNORM_BLOCK", 169),
    std::make_pair("VK_FORMAT_ASTC_8x6_SRGB_BLOCK", 170),
    std::make_pair("VK_FORMAT_ASTC_8x8_UNORM_BLOCK", 171),
    std::make_pair("VK_FORMAT_ASTC_8x8_SRGB_BLOCK", 172),
    std::make_pair("VK_FORMAT_ASTC_10x5_UNORM_BLOCK", 173),
    std::make_pair("VK_FORMAT_ASTC_10x5_SRGB_BLOCK", 174),
    std::make_pair("VK_FORMAT_ASTC_10x6_UNORM_BLOCK", 175),
    std::make_pair("VK_FORMAT_ASTC_10x6_SRGB_BLOCK", 176),
    std::make_pair("VK_FORMAT_ASTC_10x8_UNORM_BLOCK", 177),
    std::make_pair("VK_FORMAT_ASTC_10x8_SRGB_BLOCK", 178),
    std::make_pair("VK_FORMAT_ASTC_10x10_UNORM_BLOCK", 179),
    std::make_pair("VK_FORMAT_ASTC_10x10_SRGB_BLOCK", 180),
    std::make_pair("VK_FORMAT_ASTC_12x10_UNORM_BLOCK", 181),
    std::make_pair("VK_FORMAT_ASTC_12x10_SRGB_BLOCK", 182),
    std::make_pair("VK_FORMAT_ASTC_12x12_UNORM_BLOCK", 183),
    std::make_pair("VK_FORMAT_ASTC_12x12_SRGB_BLOCK", 184),
    std::make_pair("VK_FORMAT_G8B8G8R8_422_UNORM", 1000156000),
    std::make_pair("VK_FORMAT_B8G8R8G8_422_UNORM", 1000156001),
    std::make_pair("VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM", 1000156002),
    std::make_pair("VK_FORMAT_G8_B8R8_2PLANE_420_UNORM", 1000156003),
    std::make_pair("VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM", 1000156004),
    std::make_pair("VK_FORMAT_G8_B8R8_2PLANE_422_UNORM", 1000156005),
    std::make_pair("VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM", 1000156006),
    std::make_pair("VK_FORMAT_R10X6_UNORM_PACK16", 1000156007),
    std::make_pair("VK_FORMAT_R10X6G10X6_UNORM_2PACK16", 1000156008),
    std::make_pair("VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16", 1000156009),
    std::make_pair("VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16", 1000156010),
    std::make_pair("VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16", 1000156011),
    std::make_pair("VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16", 1000156012),
    std::make_pair("VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16", 1000156013),
    std::make_pair("VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16", 1000156014),
    std::make_pair("VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16", 1000156015),
    std::make_pair("VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16", 1000156016),
    std::make_pair("VK_FORMAT_R12X4_UNORM_PACK16", 1000156017),
    std::make_pair("VK_FORMAT_R12X4G12X4_UNORM_2PACK16", 1000156018),
    std::make_pair("VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16", 1000156019),
    std::make_pair("VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16", 1000156020),
    std::make_pair("VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16", 1000156021),
    std::make_pair("VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16", 1000156022),
    std::make_pair("VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16", 1000156023),
    std::make_pair("VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16", 1000156024),
    std::make_pair("VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16", 1000156025),
    std::make_pair("VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16", 1000156026),
    std::make_pair("VK_FORMAT_G16B16G16R16_422_UNORM", 1000156027),
    std::make_pair("VK_FORMAT_B16G16R16G16_422_UNORM", 1000156028),
    std::make_pair("VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM", 1000156029),
    std::make_pair("VK_FORMAT_G16_B16R16_2PLANE_420_UNORM", 1000156030),
    std::make_pair("VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM", 1000156031),
    std::make_pair("VK_FORMAT_G16_B16R16_2PLANE_422_UNORM", 1000156032),
    std::make_pair("VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM", 1000156033),
    std::make_pair("VK_FORMAT_G8_B8R8_2PLANE_444_UNORM", 1000330000),
    std::make_pair("VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16", 1000330001),
    std::make_pair("VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16", 1000330002),
    std::make_pair("VK_FORMAT_G16_B16R16_2PLANE_444_UNORM", 1000330003),
    std::make_pair("VK_FORMAT_A4R4G4B4_UNORM_PACK16", 1000340000),
    std::make_pair("VK_FORMAT_A4B4G4R4_UNORM_PACK16", 1000340001),
    std::make_pair("VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK", 1000066000),
    std::make_pair("VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK", 1000066001),
    std::make_pair("VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK", 1000066002),
    std::make_pair("VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK", 1000066003),
    std::make_pair("VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK", 1000066004),
    std::make_pair("VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK", 1000066005),
    std::make_pair("VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK", 1000066006),
    std::make_pair("VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK", 1000066007),
    std::make_pair("VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK", 1000066008),
    std::make_pair("VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK", 1000066009),
    std::make_pair("VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK", 1000066010),
    std::make_pair("VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK", 1000066011),
    std::make_pair("VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK", 1000066012),
    std::make_pair("VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK", 1000066013),
    std::make_pair("VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG", 1000054000),
    std::make_pair("VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG", 1000054001),
    std::make_pair("VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG", 1000054002),
    std::make_pair("VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG", 1000054003),
    std::make_pair("VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG", 1000054004),
    std::make_pair("VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG", 1000054005),
    std::make_pair("VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG", 1000054006),
    std::make_pair("VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG", 1000054007),
    std::make_pair("VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK_EXT", 1000066000),
    std::make_pair("VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK_EXT", 1000066001),
    std::make_pair("VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK_EXT", 1000066002),
    std::make_pair("VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK_EXT", 1000066003),
    std::make_pair("VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK_EXT", 1000066004),
    std::make_pair("VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK_EXT", 1000066005),
    std::make_pair("VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK_EXT", 1000066006),
    std::make_pair("VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK_EXT", 1000066007),
    std::make_pair("VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK_EXT", 1000066008),
    std::make_pair("VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK_EXT", 1000066009),
    std::make_pair("VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK_EXT", 1000066010),
    std::make_pair("VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK_EXT", 1000066011),
    std::make_pair("VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK_EXT", 1000066012),
    std::make_pair("VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK_EXT", 1000066013),
    std::make_pair("VK_FORMAT_G8B8G8R8_422_UNORM_KHR", 1000156000),
    std::make_pair("VK_FORMAT_B8G8R8G8_422_UNORM_KHR", 1000156001),
    std::make_pair("VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM_KHR", 1000156002),
    std::make_pair("VK_FORMAT_G8_B8R8_2PLANE_420_UNORM_KHR", 1000156003),
    std::make_pair("VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM_KHR", 1000156004),
    std::make_pair("VK_FORMAT_G8_B8R8_2PLANE_422_UNORM_KHR", 1000156005),
    std::make_pair("VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM_KHR", 1000156006),
    std::make_pair("VK_FORMAT_R10X6_UNORM_PACK16_KHR", 1000156007),
    std::make_pair("VK_FORMAT_R10X6G10X6_UNORM_2PACK16_KHR", 1000156008),
    std::make_pair("VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16_KHR", 1000156009),
    std::make_pair("VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16_KHR", 1000156010),
    std::make_pair("VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16_KHR", 1000156011),
    std::make_pair("VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16_KHR", 1000156012),
    std::make_pair("VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16_KHR", 1000156013),
    std::make_pair("VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16_KHR", 1000156014),
    std::make_pair("VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16_KHR", 1000156015),
    std::make_pair("VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16_KHR", 1000156016),
    std::make_pair("VK_FORMAT_R12X4_UNORM_PACK16_KHR", 1000156017),
    std::make_pair("VK_FORMAT_R12X4G12X4_UNORM_2PACK16_KHR", 1000156018),
    std::make_pair("VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16_KHR", 1000156019),
    std::make_pair("VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16_KHR", 1000156020),
    std::make_pair("VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16_KHR", 1000156021),
    std::make_pair("VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16_KHR", 1000156022),
    std::make_pair("VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16_KHR", 1000156023),
    std::make_pair("VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16_KHR", 1000156024),
    std::make_pair("VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16_KHR", 1000156025),
    std::make_pair("VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16_KHR", 1000156026),
    std::make_pair("VK_FORMAT_G16B16G16R16_422_UNORM_KHR", 1000156027),
    std::make_pair("VK_FORMAT_B16G16R16G16_422_UNORM_KHR", 1000156028),
    std::make_pair("VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM_KHR", 1000156029),
    std::make_pair("VK_FORMAT_G16_B16R16_2PLANE_420_UNORM_KHR", 1000156030),
    std::make_pair("VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM_KHR", 1000156031),
    std::make_pair("VK_FORMAT_G16_B16R16_2PLANE_422_UNORM_KHR", 1000156032),
    std::make_pair("VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM_KHR", 1000156033),
    std::make_pair("VK_FORMAT_ASTC_3x3x3_UNORM_BLOCK_EXT", 1000288000),
    std::make_pair("VK_FORMAT_ASTC_3x3x3_SRGB_BLOCK_EXT", 1000288001),
    std::make_pair("VK_FORMAT_ASTC_3x3x3_SFLOAT_BLOCK_EXT", 1000288002),
    std::make_pair("VK_FORMAT_ASTC_4x3x3_UNORM_BLOCK_EXT", 1000288003),
    std::make_pair("VK_FORMAT_ASTC_4x3x3_SRGB_BLOCK_EXT", 1000288004),
    std::make_pair("VK_FORMAT_ASTC_4x3x3_SFLOAT_BLOCK_EXT", 1000288005),
    std::make_pair("VK_FORMAT_ASTC_4x4x3_UNORM_BLOCK_EXT", 1000288006),
    std::make_pair("VK_FORMAT_ASTC_4x4x3_SRGB_BLOCK_EXT", 1000288007),
    std::make_pair("VK_FORMAT_ASTC_4x4x3_SFLOAT_BLOCK_EXT", 1000288008),
    std::make_pair("VK_FORMAT_ASTC_4x4x4_UNORM_BLOCK_EXT", 1000288009),
    std::make_pair("VK_FORMAT_ASTC_4x4x4_SRGB_BLOCK_EXT", 1000288010),
    std::make_pair("VK_FORMAT_ASTC_4x4x4_SFLOAT_BLOCK_EXT", 1000288011),
    std::make_pair("VK_FORMAT_ASTC_5x4x4_UNORM_BLOCK_EXT", 1000288012),
    std::make_pair("VK_FORMAT_ASTC_5x4x4_SRGB_BLOCK_EXT", 1000288013),
    std::make_pair("VK_FORMAT_ASTC_5x4x4_SFLOAT_BLOCK_EXT", 1000288014),
    std::make_pair("VK_FORMAT_ASTC_5x5x4_UNORM_BLOCK_EXT", 1000288015),
    std::make_pair("VK_FORMAT_ASTC_5x5x4_SRGB_BLOCK_EXT", 1000288016),
    std::make_pair("VK_FORMAT_ASTC_5x5x4_SFLOAT_BLOCK_EXT", 1000288017),
    std::make_pair("VK_FORMAT_ASTC_5x5x5_UNORM_BLOCK_EXT", 1000288018),
    std::make_pair("VK_FORMAT_ASTC_5x5x5_SRGB_BLOCK_EXT", 1000288019),
    std::make_pair("VK_FORMAT_ASTC_5x5x5_SFLOAT_BLOCK_EXT", 1000288020),
    std::make_pair("VK_FORMAT_ASTC_6x5x5_UNORM_BLOCK_EXT", 1000288021),
    std::make_pair("VK_FORMAT_ASTC_6x5x5_SRGB_BLOCK_EXT", 1000288022),
    std::make_pair("VK_FORMAT_ASTC_6x5x5_SFLOAT_BLOCK_EXT", 1000288023),
    std::make_pair("VK_FORMAT_ASTC_6x6x5_UNORM_BLOCK_EXT", 1000288024),
    std::make_pair("VK_FORMAT_ASTC_6x6x5_SRGB_BLOCK_EXT", 1000288025),
    std::make_pair("VK_FORMAT_ASTC_6x6x5_SFLOAT_BLOCK_EXT", 1000288026),
    std::make_pair("VK_FORMAT_ASTC_6x6x6_UNORM_BLOCK_EXT", 1000288027),
    std::make_pair("VK_FORMAT_ASTC_6x6x6_SRGB_BLOCK_EXT", 1000288028),
    std::make_pair("VK_FORMAT_ASTC_6x6x6_SFLOAT_BLOCK_EXT", 1000288029),
    std::make_pair("VK_FORMAT_G8_B8R8_2PLANE_444_UNORM_EXT", 1000330000),
    std::make_pair("VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16_EXT", 1000330001),
    std::make_pair("VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16_EXT", 1000330002),
    std::make_pair("VK_FORMAT_G16_B16R16_2PLANE_444_UNORM_EXT", 1000330003),
    std::make_pair("VK_FORMAT_A4R4G4B4_UNORM_PACK16_EXT", 1000340000),
    std::make_pair("VK_FORMAT_A4B4G4R4_UNORM_PACK16_EXT", 1000340001),
    std::make_pair("VK_FORMAT_R16G16_SFIXED5_NV", 1000464000),
    std::make_pair("VK_FORMAT_R16G16_S10_5_NV", 1000464000),
    std::make_pair("VK_FORMAT_A1B5G5R5_UNORM_PACK16_KHR", 1000470000),
    std::make_pair("VK_FORMAT_A8_UNORM_KHR", 1000470001),
};
static void parse_VkFormat(const Json::Value &obj, VkFormat &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkFormat)VkFormat_map[std::string(_res)];
}

static std::map<std::string, int> VkFormatFeatureFlagBits_map = {
    std::make_pair("VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT", 1UL << 0),
    std::make_pair("VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT", 1UL << 1),
    std::make_pair("VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT", 1UL << 2),
    std::make_pair("VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT", 1UL << 3),
    std::make_pair("VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT", 1UL << 4),
    std::make_pair("VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_ATOMIC_BIT", 1UL << 5),
    std::make_pair("VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT", 1UL << 6),
    std::make_pair("VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT", 1UL << 7),
    std::make_pair("VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT", 1UL << 8),
    std::make_pair("VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT", 1UL << 9),
    std::make_pair("VK_FORMAT_FEATURE_BLIT_SRC_BIT", 1UL << 10),
    std::make_pair("VK_FORMAT_FEATURE_BLIT_DST_BIT", 1UL << 11),
    std::make_pair("VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT", 1UL << 12),
    std::make_pair("VK_FORMAT_FEATURE_TRANSFER_SRC_BIT", 1UL << 14),
    std::make_pair("VK_FORMAT_FEATURE_TRANSFER_DST_BIT", 1UL << 15),
    std::make_pair("VK_FORMAT_FEATURE_MIDPOINT_CHROMA_SAMPLES_BIT", 1UL << 17),
    std::make_pair("VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER_BIT", 1UL << 18),
    std::make_pair("VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_SEPARATE_RECONSTRUCTION_FILTER_BIT", 1UL << 19),
    std::make_pair("VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_BIT", 1UL << 20),
    std::make_pair("VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_FORCEABLE_BIT",
                   1UL << 21),
    std::make_pair("VK_FORMAT_FEATURE_DISJOINT_BIT", 1UL << 22),
    std::make_pair("VK_FORMAT_FEATURE_COSITED_CHROMA_SAMPLES_BIT", 1UL << 23),
    std::make_pair("VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_MINMAX_BIT", 1UL << 16),
    std::make_pair("VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_CUBIC_BIT_IMG", 1UL << 13),
    std::make_pair("VK_FORMAT_FEATURE_VIDEO_DECODE_OUTPUT_BIT_KHR", 1UL << 25),
    std::make_pair("VK_FORMAT_FEATURE_VIDEO_DECODE_DPB_BIT_KHR", 1UL << 26),
    std::make_pair("VK_FORMAT_FEATURE_TRANSFER_SRC_BIT_KHR", 1UL << 14),
    std::make_pair("VK_FORMAT_FEATURE_TRANSFER_DST_BIT_KHR", 1UL << 15),
    std::make_pair("VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_MINMAX_BIT_EXT", 1UL << 16),
    std::make_pair("VK_FORMAT_FEATURE_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR", 1UL << 29),
    std::make_pair("VK_FORMAT_FEATURE_MIDPOINT_CHROMA_SAMPLES_BIT_KHR", 1UL << 17),
    std::make_pair("VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER_BIT_KHR", 1UL << 18),
    std::make_pair("VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_SEPARATE_RECONSTRUCTION_FILTER_BIT_KHR",
                   1UL << 19),
    std::make_pair("VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_BIT_KHR",
                   1UL << 20),
    std::make_pair("VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_FORCEABLE_BIT_KHR",
                   1UL << 21),
    std::make_pair("VK_FORMAT_FEATURE_DISJOINT_BIT_KHR", 1UL << 22),
    std::make_pair("VK_FORMAT_FEATURE_COSITED_CHROMA_SAMPLES_BIT_KHR", 1UL << 23),
    std::make_pair("VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_CUBIC_BIT_EXT", 1UL << 13),
    std::make_pair("VK_FORMAT_FEATURE_FRAGMENT_DENSITY_MAP_BIT_EXT", 1UL << 24),
    std::make_pair("VK_FORMAT_FEATURE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR", 1UL << 30),
    std::make_pair("VK_FORMAT_FEATURE_VIDEO_ENCODE_INPUT_BIT_KHR", 1UL << 27),
    std::make_pair("VK_FORMAT_FEATURE_VIDEO_ENCODE_DPB_BIT_KHR", 1UL << 28),
};
static void parse_VkFormatFeatureFlagBits(const Json::Value &obj, VkFormatFeatureFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkFormatFeatureFlagBits)VkFormatFeatureFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkImageCreateFlagBits_map = {
    std::make_pair("VK_IMAGE_CREATE_SPARSE_BINDING_BIT", 1UL << 0),
    std::make_pair("VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT", 1UL << 1),
    std::make_pair("VK_IMAGE_CREATE_SPARSE_ALIASED_BIT", 1UL << 2),
    std::make_pair("VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT", 1UL << 3),
    std::make_pair("VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT", 1UL << 4),
    std::make_pair("VK_IMAGE_CREATE_ALIAS_BIT", 1UL << 10),
    std::make_pair("VK_IMAGE_CREATE_SPLIT_INSTANCE_BIND_REGIONS_BIT", 1UL << 6),
    std::make_pair("VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT", 1UL << 5),
    std::make_pair("VK_IMAGE_CREATE_BLOCK_TEXEL_VIEW_COMPATIBLE_BIT", 1UL << 7),
    std::make_pair("VK_IMAGE_CREATE_EXTENDED_USAGE_BIT", 1UL << 8),
    std::make_pair("VK_IMAGE_CREATE_PROTECTED_BIT", 1UL << 11),
    std::make_pair("VK_IMAGE_CREATE_DISJOINT_BIT", 1UL << 9),
    std::make_pair("VK_IMAGE_CREATE_CORNER_SAMPLED_BIT_NV", 1UL << 13),
    std::make_pair("VK_IMAGE_CREATE_SPLIT_INSTANCE_BIND_REGIONS_BIT_KHR", 1UL << 6),
    std::make_pair("VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT_KHR", 1UL << 5),
    std::make_pair("VK_IMAGE_CREATE_BLOCK_TEXEL_VIEW_COMPATIBLE_BIT_KHR", 1UL << 7),
    std::make_pair("VK_IMAGE_CREATE_EXTENDED_USAGE_BIT_KHR", 1UL << 8),
    std::make_pair("VK_IMAGE_CREATE_SAMPLE_LOCATIONS_COMPATIBLE_DEPTH_BIT_EXT", 1UL << 12),
    std::make_pair("VK_IMAGE_CREATE_DISJOINT_BIT_KHR", 1UL << 9),
    std::make_pair("VK_IMAGE_CREATE_ALIAS_BIT_KHR", 1UL << 10),
    std::make_pair("VK_IMAGE_CREATE_SUBSAMPLED_BIT_EXT", 1UL << 14),
    std::make_pair("VK_IMAGE_CREATE_RESERVED_19_BIT_EXT", 1UL << 19),
    std::make_pair("VK_IMAGE_CREATE_DESCRIPTOR_BUFFER_CAPTURE_REPLAY_BIT_EXT", 1UL << 16),
    std::make_pair("VK_IMAGE_CREATE_MULTISAMPLED_RENDER_TO_SINGLE_SAMPLED_BIT_EXT", 1UL << 18),
    std::make_pair("VK_IMAGE_CREATE_2D_VIEW_COMPATIBLE_BIT_EXT", 1UL << 17),
    std::make_pair("VK_IMAGE_CREATE_FRAGMENT_DENSITY_MAP_OFFSET_BIT_QCOM", 1UL << 15),
    std::make_pair("VK_IMAGE_CREATE_VIDEO_PROFILE_INDEPENDENT_BIT_KHR", 1UL << 20),
};
static void parse_VkImageCreateFlagBits(const Json::Value &obj, VkImageCreateFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkImageCreateFlagBits)VkImageCreateFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkSampleCountFlagBits_map = {
    std::make_pair("VK_SAMPLE_COUNT_1_BIT", 1UL << 0),  std::make_pair("VK_SAMPLE_COUNT_2_BIT", 1UL << 1),
    std::make_pair("VK_SAMPLE_COUNT_4_BIT", 1UL << 2),  std::make_pair("VK_SAMPLE_COUNT_8_BIT", 1UL << 3),
    std::make_pair("VK_SAMPLE_COUNT_16_BIT", 1UL << 4), std::make_pair("VK_SAMPLE_COUNT_32_BIT", 1UL << 5),
    std::make_pair("VK_SAMPLE_COUNT_64_BIT", 1UL << 6),
};
static void parse_VkSampleCountFlagBits(const Json::Value &obj, VkSampleCountFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkSampleCountFlagBits)VkSampleCountFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkImageTiling_map = {
    std::make_pair("VK_IMAGE_TILING_OPTIMAL", 0),
    std::make_pair("VK_IMAGE_TILING_LINEAR", 1),
    std::make_pair("VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT", 1000158000),
};
static void parse_VkImageTiling(const Json::Value &obj, VkImageTiling &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkImageTiling)VkImageTiling_map[std::string(_res)];
}

static std::map<std::string, int> VkImageType_map = {
    std::make_pair("VK_IMAGE_TYPE_1D", 0),
    std::make_pair("VK_IMAGE_TYPE_2D", 1),
    std::make_pair("VK_IMAGE_TYPE_3D", 2),
};
static void parse_VkImageType(const Json::Value &obj, VkImageType &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkImageType)VkImageType_map[std::string(_res)];
}

static std::map<std::string, int> VkImageUsageFlagBits_map = {
    std::make_pair("VK_IMAGE_USAGE_TRANSFER_SRC_BIT", 1UL << 0),
    std::make_pair("VK_IMAGE_USAGE_TRANSFER_DST_BIT", 1UL << 1),
    std::make_pair("VK_IMAGE_USAGE_SAMPLED_BIT", 1UL << 2),
    std::make_pair("VK_IMAGE_USAGE_STORAGE_BIT", 1UL << 3),
    std::make_pair("VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT", 1UL << 4),
    std::make_pair("VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT", 1UL << 5),
    std::make_pair("VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT", 1UL << 6),
    std::make_pair("VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT", 1UL << 7),
    std::make_pair("VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR", 1UL << 10),
    std::make_pair("VK_IMAGE_USAGE_VIDEO_DECODE_SRC_BIT_KHR", 1UL << 11),
    std::make_pair("VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR", 1UL << 12),
    std::make_pair("VK_IMAGE_USAGE_SHADING_RATE_IMAGE_BIT_NV", 1UL << 8),
    std::make_pair("VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT", 1UL << 9),
    std::make_pair("VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR", 1UL << 8),
    std::make_pair("VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT", 1UL << 22),
    std::make_pair("VK_IMAGE_USAGE_VIDEO_ENCODE_DST_BIT_KHR", 1UL << 13),
    std::make_pair("VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR", 1UL << 14),
    std::make_pair("VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR", 1UL << 15),
    std::make_pair("VK_IMAGE_USAGE_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT", 1UL << 19),
    std::make_pair("VK_IMAGE_USAGE_INVOCATION_MASK_BIT_HUAWEI", 1UL << 18),
    std::make_pair("VK_IMAGE_USAGE_SAMPLE_WEIGHT_BIT_QCOM", 1UL << 20),
    std::make_pair("VK_IMAGE_USAGE_SAMPLE_BLOCK_MATCH_BIT_QCOM", 1UL << 21),
    std::make_pair("VK_IMAGE_USAGE_RESERVED_24_BIT_COREAVI", 1UL << 24),
    std::make_pair("VK_IMAGE_USAGE_RESERVED_23_BIT_EXT", 1UL << 23),
    std::make_pair("VK_IMAGE_USAGE_RESERVED_25_BIT_KHR", 1UL << 25),
    std::make_pair("VK_IMAGE_USAGE_RESERVED_26_BIT_KHR", 1UL << 26),
};
static void parse_VkImageUsageFlagBits(const Json::Value &obj, VkImageUsageFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkImageUsageFlagBits)VkImageUsageFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkInstanceCreateFlagBits_map = {
    std::make_pair("VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR", 1UL << 0),
};
static void parse_VkInstanceCreateFlagBits(const Json::Value &obj, VkInstanceCreateFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkInstanceCreateFlagBits)VkInstanceCreateFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkMemoryHeapFlagBits_map = {
    std::make_pair("VK_MEMORY_HEAP_DEVICE_LOCAL_BIT", 1UL << 0),
    std::make_pair("VK_MEMORY_HEAP_MULTI_INSTANCE_BIT", 1UL << 1),
    std::make_pair("VK_MEMORY_HEAP_SEU_SAFE_BIT", 1UL << 2),
    std::make_pair("VK_MEMORY_HEAP_MULTI_INSTANCE_BIT_KHR", 1UL << 1),
};
static void parse_VkMemoryHeapFlagBits(const Json::Value &obj, VkMemoryHeapFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkMemoryHeapFlagBits)VkMemoryHeapFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkMemoryPropertyFlagBits_map = {
    std::make_pair("VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT", 1UL << 0),
    std::make_pair("VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT", 1UL << 1),
    std::make_pair("VK_MEMORY_PROPERTY_HOST_COHERENT_BIT", 1UL << 2),
    std::make_pair("VK_MEMORY_PROPERTY_HOST_CACHED_BIT", 1UL << 3),
    std::make_pair("VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT", 1UL << 4),
    std::make_pair("VK_MEMORY_PROPERTY_PROTECTED_BIT", 1UL << 5),
    std::make_pair("VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD", 1UL << 6),
    std::make_pair("VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD", 1UL << 7),
    std::make_pair("VK_MEMORY_PROPERTY_RDMA_CAPABLE_BIT_NV", 1UL << 8),
};
static void parse_VkMemoryPropertyFlagBits(const Json::Value &obj, VkMemoryPropertyFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkMemoryPropertyFlagBits)VkMemoryPropertyFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkPhysicalDeviceType_map = {
    std::make_pair("VK_PHYSICAL_DEVICE_TYPE_OTHER", 0),
    std::make_pair("VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU", 1),
    std::make_pair("VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU", 2),
    std::make_pair("VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU", 3),
    std::make_pair("VK_PHYSICAL_DEVICE_TYPE_CPU", 4),
};
static void parse_VkPhysicalDeviceType(const Json::Value &obj, VkPhysicalDeviceType &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkPhysicalDeviceType)VkPhysicalDeviceType_map[std::string(_res)];
}

static std::map<std::string, int> VkQueueFlagBits_map = {
    std::make_pair("VK_QUEUE_GRAPHICS_BIT", 1UL << 0),         std::make_pair("VK_QUEUE_COMPUTE_BIT", 1UL << 1),
    std::make_pair("VK_QUEUE_TRANSFER_BIT", 1UL << 2),         std::make_pair("VK_QUEUE_PROTECTED_BIT", 1UL << 4),
    std::make_pair("VK_QUEUE_VIDEO_DECODE_BIT_KHR", 1UL << 5), std::make_pair("VK_QUEUE_RESERVED_9_BIT_EXT", 1UL << 9),
    std::make_pair("VK_QUEUE_VIDEO_ENCODE_BIT_KHR", 1UL << 6), std::make_pair("VK_QUEUE_RESERVED_7_BIT_QCOM", 1UL << 7),
    std::make_pair("VK_QUEUE_RESERVED_11_BIT_ARM", 1UL << 11), std::make_pair("VK_QUEUE_OPTICAL_FLOW_BIT_NV", 1UL << 8),
    std::make_pair("VK_QUEUE_RESERVED_10_BIT_EXT", 1UL << 10),
};
static void parse_VkQueueFlagBits(const Json::Value &obj, VkQueueFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkQueueFlagBits)VkQueueFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkDeviceQueueCreateFlagBits_map = {
    std::make_pair("VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT", 1UL << 0),
    std::make_pair("VK_DEVICE_QUEUE_CREATE_RESERVED_1_BIT_QCOM", 1UL << 1),
};
static void parse_VkDeviceQueueCreateFlagBits(const Json::Value &obj, VkDeviceQueueCreateFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkDeviceQueueCreateFlagBits)VkDeviceQueueCreateFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkPipelineStageFlagBits_map = {
    std::make_pair("VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT", 1UL << 0),
    std::make_pair("VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT", 1UL << 1),
    std::make_pair("VK_PIPELINE_STAGE_VERTEX_INPUT_BIT", 1UL << 2),
    std::make_pair("VK_PIPELINE_STAGE_VERTEX_SHADER_BIT", 1UL << 3),
    std::make_pair("VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT", 1UL << 4),
    std::make_pair("VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT", 1UL << 5),
    std::make_pair("VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT", 1UL << 6),
    std::make_pair("VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT", 1UL << 7),
    std::make_pair("VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT", 1UL << 8),
    std::make_pair("VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT", 1UL << 9),
    std::make_pair("VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT", 1UL << 10),
    std::make_pair("VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT", 1UL << 11),
    std::make_pair("VK_PIPELINE_STAGE_TRANSFER_BIT", 1UL << 12),
    std::make_pair("VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT", 1UL << 13),
    std::make_pair("VK_PIPELINE_STAGE_HOST_BIT", 1UL << 14),
    std::make_pair("VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT", 1UL << 15),
    std::make_pair("VK_PIPELINE_STAGE_ALL_COMMANDS_BIT", 1UL << 16),
    std::make_pair("VK_PIPELINE_STAGE_NONE", 0),
    std::make_pair("VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT", 1UL << 24),
    std::make_pair("VK_PIPELINE_STAGE_CONDITIONAL_RENDERING_BIT_EXT", 1UL << 18),
    std::make_pair("VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR", 1UL << 25),
    std::make_pair("VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR", 1UL << 21),
    std::make_pair("VK_PIPELINE_STAGE_SHADING_RATE_IMAGE_BIT_NV", 1UL << 22),
    std::make_pair("VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV", 1UL << 21),
    std::make_pair("VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV", 1UL << 25),
    std::make_pair("VK_PIPELINE_STAGE_TASK_SHADER_BIT_NV", 1UL << 19),
    std::make_pair("VK_PIPELINE_STAGE_MESH_SHADER_BIT_NV", 1UL << 20),
    std::make_pair("VK_PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT_EXT", 1UL << 23),
    std::make_pair("VK_PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR", 1UL << 22),
    std::make_pair("VK_PIPELINE_STAGE_COMMAND_PREPROCESS_BIT_NV", 1UL << 17),
    std::make_pair("VK_PIPELINE_STAGE_NONE_KHR", 0),
    std::make_pair("VK_PIPELINE_STAGE_TASK_SHADER_BIT_EXT", 1UL << 19),
    std::make_pair("VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT", 1UL << 20),
};
static void parse_VkPipelineStageFlagBits(const Json::Value &obj, VkPipelineStageFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkPipelineStageFlagBits)VkPipelineStageFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkMemoryMapFlagBits_map = {
    std::make_pair("VK_MEMORY_MAP_PLACED_BIT_EXT", 1UL << 0),
};
static void parse_VkMemoryMapFlagBits(const Json::Value &obj, VkMemoryMapFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkMemoryMapFlagBits)VkMemoryMapFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkFenceCreateFlagBits_map = {
    std::make_pair("VK_FENCE_CREATE_SIGNALED_BIT", 1UL << 0),
};
static void parse_VkFenceCreateFlagBits(const Json::Value &obj, VkFenceCreateFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkFenceCreateFlagBits)VkFenceCreateFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkEventCreateFlagBits_map = {
    std::make_pair("VK_EVENT_CREATE_DEVICE_ONLY_BIT", 1UL << 0),
    std::make_pair("VK_EVENT_CREATE_DEVICE_ONLY_BIT_KHR", 1UL << 0),
};
static void parse_VkEventCreateFlagBits(const Json::Value &obj, VkEventCreateFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkEventCreateFlagBits)VkEventCreateFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkQueryPipelineStatisticFlagBits_map = {
    std::make_pair("VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT", 1UL << 0),
    std::make_pair("VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT", 1UL << 1),
    std::make_pair("VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT", 1UL << 2),
    std::make_pair("VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT", 1UL << 3),
    std::make_pair("VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT", 1UL << 4),
    std::make_pair("VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT", 1UL << 5),
    std::make_pair("VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT", 1UL << 6),
    std::make_pair("VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT", 1UL << 7),
    std::make_pair("VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT", 1UL << 8),
    std::make_pair("VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT", 1UL << 9),
    std::make_pair("VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT", 1UL << 10),
    std::make_pair("VK_QUERY_PIPELINE_STATISTIC_TASK_SHADER_INVOCATIONS_BIT_EXT", 1UL << 11),
    std::make_pair("VK_QUERY_PIPELINE_STATISTIC_MESH_SHADER_INVOCATIONS_BIT_EXT", 1UL << 12),
    std::make_pair("VK_QUERY_PIPELINE_STATISTIC_CLUSTER_CULLING_SHADER_INVOCATIONS_BIT_HUAWEI", 1UL << 13),
};
static void parse_VkQueryPipelineStatisticFlagBits(const Json::Value &obj, VkQueryPipelineStatisticFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkQueryPipelineStatisticFlagBits)VkQueryPipelineStatisticFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkQueryType_map = {
    std::make_pair("VK_QUERY_TYPE_OCCLUSION", 0),
    std::make_pair("VK_QUERY_TYPE_PIPELINE_STATISTICS", 1),
    std::make_pair("VK_QUERY_TYPE_TIMESTAMP", 2),
    std::make_pair("VK_QUERY_TYPE_RESULT_STATUS_ONLY_KHR", 1000023000),
    std::make_pair("VK_QUERY_TYPE_TRANSFORM_FEEDBACK_STREAM_EXT", 1000028004),
    std::make_pair("VK_QUERY_TYPE_PERFORMANCE_QUERY_KHR", 1000116000),
    std::make_pair("VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR", 1000150000),
    std::make_pair("VK_QUERY_TYPE_ACCELERATION_STRUCTURE_SERIALIZATION_SIZE_KHR", 1000150001),
    std::make_pair("VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_NV", 1000165000),
    std::make_pair("VK_QUERY_TYPE_PERFORMANCE_QUERY_INTEL", 1000210000),
    std::make_pair("VK_QUERY_TYPE_VIDEO_ENCODE_FEEDBACK_KHR", 1000299000),
    std::make_pair("VK_QUERY_TYPE_MESH_PRIMITIVES_GENERATED_EXT", 1000328000),
    std::make_pair("VK_QUERY_TYPE_PRIMITIVES_GENERATED_EXT", 1000382000),
    std::make_pair("VK_QUERY_TYPE_ACCELERATION_STRUCTURE_SERIALIZATION_BOTTOM_LEVEL_POINTERS_KHR", 1000386000),
    std::make_pair("VK_QUERY_TYPE_ACCELERATION_STRUCTURE_SIZE_KHR", 1000386001),
    std::make_pair("VK_QUERY_TYPE_MICROMAP_SERIALIZATION_SIZE_EXT", 1000396000),
    std::make_pair("VK_QUERY_TYPE_MICROMAP_COMPACTED_SIZE_EXT", 1000396001),
};
static void parse_VkQueryType(const Json::Value &obj, VkQueryType &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkQueryType)VkQueryType_map[std::string(_res)];
}

static std::map<std::string, int> VkQueryResultFlagBits_map = {
    std::make_pair("VK_QUERY_RESULT_64_BIT", 1UL << 0),
    std::make_pair("VK_QUERY_RESULT_WAIT_BIT", 1UL << 1),
    std::make_pair("VK_QUERY_RESULT_WITH_AVAILABILITY_BIT", 1UL << 2),
    std::make_pair("VK_QUERY_RESULT_PARTIAL_BIT", 1UL << 3),
    std::make_pair("VK_QUERY_RESULT_WITH_STATUS_BIT_KHR", 1UL << 4),
};
static void parse_VkQueryResultFlagBits(const Json::Value &obj, VkQueryResultFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkQueryResultFlagBits)VkQueryResultFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkBufferCreateFlagBits_map = {
    std::make_pair("VK_BUFFER_CREATE_SPARSE_BINDING_BIT", 1UL << 0),
    std::make_pair("VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT", 1UL << 1),
    std::make_pair("VK_BUFFER_CREATE_SPARSE_ALIASED_BIT", 1UL << 2),
    std::make_pair("VK_BUFFER_CREATE_PROTECTED_BIT", 1UL << 3),
    std::make_pair("VK_BUFFER_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT", 1UL << 4),
    std::make_pair("VK_BUFFER_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT_EXT", 1UL << 4),
    std::make_pair("VK_BUFFER_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT_KHR", 1UL << 4),
    std::make_pair("VK_BUFFER_CREATE_DESCRIPTOR_BUFFER_CAPTURE_REPLAY_BIT_EXT", 1UL << 5),
    std::make_pair("VK_BUFFER_CREATE_VIDEO_PROFILE_INDEPENDENT_BIT_KHR", 1UL << 6),
};
static void parse_VkBufferCreateFlagBits(const Json::Value &obj, VkBufferCreateFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkBufferCreateFlagBits)VkBufferCreateFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkBufferUsageFlagBits_map = {
    std::make_pair("VK_BUFFER_USAGE_TRANSFER_SRC_BIT", 1UL << 0),
    std::make_pair("VK_BUFFER_USAGE_TRANSFER_DST_BIT", 1UL << 1),
    std::make_pair("VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT", 1UL << 2),
    std::make_pair("VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT", 1UL << 3),
    std::make_pair("VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT", 1UL << 4),
    std::make_pair("VK_BUFFER_USAGE_STORAGE_BUFFER_BIT", 1UL << 5),
    std::make_pair("VK_BUFFER_USAGE_INDEX_BUFFER_BIT", 1UL << 6),
    std::make_pair("VK_BUFFER_USAGE_VERTEX_BUFFER_BIT", 1UL << 7),
    std::make_pair("VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT", 1UL << 8),
    std::make_pair("VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT", 1UL << 17),
    std::make_pair("VK_BUFFER_USAGE_VIDEO_DECODE_SRC_BIT_KHR", 1UL << 13),
    std::make_pair("VK_BUFFER_USAGE_VIDEO_DECODE_DST_BIT_KHR", 1UL << 14),
    std::make_pair("VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT", 1UL << 11),
    std::make_pair("VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_COUNTER_BUFFER_BIT_EXT", 1UL << 12),
    std::make_pair("VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT", 1UL << 9),
    std::make_pair("VK_BUFFER_USAGE_EXECUTION_GRAPH_SCRATCH_BIT_AMDX", 1UL << 25),
    std::make_pair("VK_BUFFER_USAGE_RESERVED_28_BIT_KHR", 1UL << 28),
    std::make_pair("VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR", 1UL << 19),
    std::make_pair("VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR", 1UL << 20),
    std::make_pair("VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR", 1UL << 10),
    std::make_pair("VK_BUFFER_USAGE_RAY_TRACING_BIT_NV", 1UL << 10),
    std::make_pair("VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_EXT", 1UL << 17),
    std::make_pair("VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR", 1UL << 17),
    std::make_pair("VK_BUFFER_USAGE_VIDEO_ENCODE_DST_BIT_KHR", 1UL << 15),
    std::make_pair("VK_BUFFER_USAGE_VIDEO_ENCODE_SRC_BIT_KHR", 1UL << 16),
    std::make_pair("VK_BUFFER_USAGE_RESERVED_27_BIT_QCOM", 1UL << 27),
    std::make_pair("VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT", 1UL << 21),
    std::make_pair("VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT", 1UL << 22),
    std::make_pair("VK_BUFFER_USAGE_PUSH_DESCRIPTORS_DESCRIPTOR_BUFFER_BIT_EXT", 1UL << 26),
    std::make_pair("VK_BUFFER_USAGE_MICROMAP_BUILD_INPUT_READ_ONLY_BIT_EXT", 1UL << 23),
    std::make_pair("VK_BUFFER_USAGE_MICROMAP_STORAGE_BIT_EXT", 1UL << 24),
};
static void parse_VkBufferUsageFlagBits(const Json::Value &obj, VkBufferUsageFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkBufferUsageFlagBits)VkBufferUsageFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkSharingMode_map = {
    std::make_pair("VK_SHARING_MODE_EXCLUSIVE", 0),
    std::make_pair("VK_SHARING_MODE_CONCURRENT", 1),
};
static void parse_VkSharingMode(const Json::Value &obj, VkSharingMode &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkSharingMode)VkSharingMode_map[std::string(_res)];
}

static std::map<std::string, int> VkComponentSwizzle_map = {
    std::make_pair("VK_COMPONENT_SWIZZLE_IDENTITY", 0), std::make_pair("VK_COMPONENT_SWIZZLE_ZERO", 1),
    std::make_pair("VK_COMPONENT_SWIZZLE_ONE", 2),      std::make_pair("VK_COMPONENT_SWIZZLE_R", 3),
    std::make_pair("VK_COMPONENT_SWIZZLE_G", 4),        std::make_pair("VK_COMPONENT_SWIZZLE_B", 5),
    std::make_pair("VK_COMPONENT_SWIZZLE_A", 6),
};
static void parse_VkComponentSwizzle(const Json::Value &obj, VkComponentSwizzle &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkComponentSwizzle)VkComponentSwizzle_map[std::string(_res)];
}

static std::map<std::string, int> VkImageViewCreateFlagBits_map = {
    std::make_pair("VK_IMAGE_VIEW_CREATE_FRAGMENT_DENSITY_MAP_DYNAMIC_BIT_EXT", 1UL << 0),
    std::make_pair("VK_IMAGE_VIEW_CREATE_DESCRIPTOR_BUFFER_CAPTURE_REPLAY_BIT_EXT", 1UL << 2),
    std::make_pair("VK_IMAGE_VIEW_CREATE_FRAGMENT_DENSITY_MAP_DEFERRED_BIT_EXT", 1UL << 1),
};
static void parse_VkImageViewCreateFlagBits(const Json::Value &obj, VkImageViewCreateFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkImageViewCreateFlagBits)VkImageViewCreateFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkImageViewType_map = {
    std::make_pair("VK_IMAGE_VIEW_TYPE_1D", 0),         std::make_pair("VK_IMAGE_VIEW_TYPE_2D", 1),
    std::make_pair("VK_IMAGE_VIEW_TYPE_3D", 2),         std::make_pair("VK_IMAGE_VIEW_TYPE_CUBE", 3),
    std::make_pair("VK_IMAGE_VIEW_TYPE_1D_ARRAY", 4),   std::make_pair("VK_IMAGE_VIEW_TYPE_2D_ARRAY", 5),
    std::make_pair("VK_IMAGE_VIEW_TYPE_CUBE_ARRAY", 6),
};
static void parse_VkImageViewType(const Json::Value &obj, VkImageViewType &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkImageViewType)VkImageViewType_map[std::string(_res)];
}

static std::map<std::string, int> VkPipelineCacheCreateFlagBits_map = {
    std::make_pair("VK_PIPELINE_CACHE_CREATE_EXTERNALLY_SYNCHRONIZED_BIT", 1UL << 0),
    std::make_pair("VK_PIPELINE_CACHE_CREATE_READ_ONLY_BIT", 1UL << 1),
    std::make_pair("VK_PIPELINE_CACHE_CREATE_USE_APPLICATION_STORAGE_BIT", 1UL << 2),
    std::make_pair("VK_PIPELINE_CACHE_CREATE_EXTERNALLY_SYNCHRONIZED_BIT_EXT", 1UL << 0),
};
static void parse_VkPipelineCacheCreateFlagBits(const Json::Value &obj, VkPipelineCacheCreateFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkPipelineCacheCreateFlagBits)VkPipelineCacheCreateFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkBlendFactor_map = {
    std::make_pair("VK_BLEND_FACTOR_ZERO", 0),
    std::make_pair("VK_BLEND_FACTOR_ONE", 1),
    std::make_pair("VK_BLEND_FACTOR_SRC_COLOR", 2),
    std::make_pair("VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR", 3),
    std::make_pair("VK_BLEND_FACTOR_DST_COLOR", 4),
    std::make_pair("VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR", 5),
    std::make_pair("VK_BLEND_FACTOR_SRC_ALPHA", 6),
    std::make_pair("VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA", 7),
    std::make_pair("VK_BLEND_FACTOR_DST_ALPHA", 8),
    std::make_pair("VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA", 9),
    std::make_pair("VK_BLEND_FACTOR_CONSTANT_COLOR", 10),
    std::make_pair("VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR", 11),
    std::make_pair("VK_BLEND_FACTOR_CONSTANT_ALPHA", 12),
    std::make_pair("VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA", 13),
    std::make_pair("VK_BLEND_FACTOR_SRC_ALPHA_SATURATE", 14),
    std::make_pair("VK_BLEND_FACTOR_SRC1_COLOR", 15),
    std::make_pair("VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR", 16),
    std::make_pair("VK_BLEND_FACTOR_SRC1_ALPHA", 17),
    std::make_pair("VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA", 18),
};
static void parse_VkBlendFactor(const Json::Value &obj, VkBlendFactor &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkBlendFactor)VkBlendFactor_map[std::string(_res)];
}

static std::map<std::string, int> VkBlendOp_map = {
    std::make_pair("VK_BLEND_OP_ADD", 0),
    std::make_pair("VK_BLEND_OP_SUBTRACT", 1),
    std::make_pair("VK_BLEND_OP_REVERSE_SUBTRACT", 2),
    std::make_pair("VK_BLEND_OP_MIN", 3),
    std::make_pair("VK_BLEND_OP_MAX", 4),
    std::make_pair("VK_BLEND_OP_ZERO_EXT", 1000148000),
    std::make_pair("VK_BLEND_OP_SRC_EXT", 1000148001),
    std::make_pair("VK_BLEND_OP_DST_EXT", 1000148002),
    std::make_pair("VK_BLEND_OP_SRC_OVER_EXT", 1000148003),
    std::make_pair("VK_BLEND_OP_DST_OVER_EXT", 1000148004),
    std::make_pair("VK_BLEND_OP_SRC_IN_EXT", 1000148005),
    std::make_pair("VK_BLEND_OP_DST_IN_EXT", 1000148006),
    std::make_pair("VK_BLEND_OP_SRC_OUT_EXT", 1000148007),
    std::make_pair("VK_BLEND_OP_DST_OUT_EXT", 1000148008),
    std::make_pair("VK_BLEND_OP_SRC_ATOP_EXT", 1000148009),
    std::make_pair("VK_BLEND_OP_DST_ATOP_EXT", 1000148010),
    std::make_pair("VK_BLEND_OP_XOR_EXT", 1000148011),
    std::make_pair("VK_BLEND_OP_MULTIPLY_EXT", 1000148012),
    std::make_pair("VK_BLEND_OP_SCREEN_EXT", 1000148013),
    std::make_pair("VK_BLEND_OP_OVERLAY_EXT", 1000148014),
    std::make_pair("VK_BLEND_OP_DARKEN_EXT", 1000148015),
    std::make_pair("VK_BLEND_OP_LIGHTEN_EXT", 1000148016),
    std::make_pair("VK_BLEND_OP_COLORDODGE_EXT", 1000148017),
    std::make_pair("VK_BLEND_OP_COLORBURN_EXT", 1000148018),
    std::make_pair("VK_BLEND_OP_HARDLIGHT_EXT", 1000148019),
    std::make_pair("VK_BLEND_OP_SOFTLIGHT_EXT", 1000148020),
    std::make_pair("VK_BLEND_OP_DIFFERENCE_EXT", 1000148021),
    std::make_pair("VK_BLEND_OP_EXCLUSION_EXT", 1000148022),
    std::make_pair("VK_BLEND_OP_INVERT_EXT", 1000148023),
    std::make_pair("VK_BLEND_OP_INVERT_RGB_EXT", 1000148024),
    std::make_pair("VK_BLEND_OP_LINEARDODGE_EXT", 1000148025),
    std::make_pair("VK_BLEND_OP_LINEARBURN_EXT", 1000148026),
    std::make_pair("VK_BLEND_OP_VIVIDLIGHT_EXT", 1000148027),
    std::make_pair("VK_BLEND_OP_LINEARLIGHT_EXT", 1000148028),
    std::make_pair("VK_BLEND_OP_PINLIGHT_EXT", 1000148029),
    std::make_pair("VK_BLEND_OP_HARDMIX_EXT", 1000148030),
    std::make_pair("VK_BLEND_OP_HSL_HUE_EXT", 1000148031),
    std::make_pair("VK_BLEND_OP_HSL_SATURATION_EXT", 1000148032),
    std::make_pair("VK_BLEND_OP_HSL_COLOR_EXT", 1000148033),
    std::make_pair("VK_BLEND_OP_HSL_LUMINOSITY_EXT", 1000148034),
    std::make_pair("VK_BLEND_OP_PLUS_EXT", 1000148035),
    std::make_pair("VK_BLEND_OP_PLUS_CLAMPED_EXT", 1000148036),
    std::make_pair("VK_BLEND_OP_PLUS_CLAMPED_ALPHA_EXT", 1000148037),
    std::make_pair("VK_BLEND_OP_PLUS_DARKER_EXT", 1000148038),
    std::make_pair("VK_BLEND_OP_MINUS_EXT", 1000148039),
    std::make_pair("VK_BLEND_OP_MINUS_CLAMPED_EXT", 1000148040),
    std::make_pair("VK_BLEND_OP_CONTRAST_EXT", 1000148041),
    std::make_pair("VK_BLEND_OP_INVERT_OVG_EXT", 1000148042),
    std::make_pair("VK_BLEND_OP_RED_EXT", 1000148043),
    std::make_pair("VK_BLEND_OP_GREEN_EXT", 1000148044),
    std::make_pair("VK_BLEND_OP_BLUE_EXT", 1000148045),
};
static void parse_VkBlendOp(const Json::Value &obj, VkBlendOp &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkBlendOp)VkBlendOp_map[std::string(_res)];
}

static std::map<std::string, int> VkColorComponentFlagBits_map = {
    std::make_pair("VK_COLOR_COMPONENT_R_BIT", 1UL << 0),
    std::make_pair("VK_COLOR_COMPONENT_G_BIT", 1UL << 1),
    std::make_pair("VK_COLOR_COMPONENT_B_BIT", 1UL << 2),
    std::make_pair("VK_COLOR_COMPONENT_A_BIT", 1UL << 3),
};
static void parse_VkColorComponentFlagBits(const Json::Value &obj, VkColorComponentFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkColorComponentFlagBits)VkColorComponentFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkCompareOp_map = {
    std::make_pair("VK_COMPARE_OP_NEVER", 0),
    std::make_pair("VK_COMPARE_OP_LESS", 1),
    std::make_pair("VK_COMPARE_OP_EQUAL", 2),
    std::make_pair("VK_COMPARE_OP_LESS_OR_EQUAL", 3),
    std::make_pair("VK_COMPARE_OP_GREATER", 4),
    std::make_pair("VK_COMPARE_OP_NOT_EQUAL", 5),
    std::make_pair("VK_COMPARE_OP_GREATER_OR_EQUAL", 6),
    std::make_pair("VK_COMPARE_OP_ALWAYS", 7),
};
static void parse_VkCompareOp(const Json::Value &obj, VkCompareOp &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkCompareOp)VkCompareOp_map[std::string(_res)];
}

static std::map<std::string, int> VkPipelineCreateFlagBits_map = {
    std::make_pair("VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT", 1UL << 0),
    std::make_pair("VK_PIPELINE_CREATE_VIEW_INDEX_FROM_DEVICE_INDEX_BIT", 1UL << 3),
    std::make_pair("VK_PIPELINE_CREATE_DISPATCH_BASE_BIT", 1UL << 4),
    std::make_pair("VK_PIPELINE_CREATE_DISPATCH_BASE", 1UL << 4),
    std::make_pair("VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT", 1UL << 8),
    std::make_pair("VK_PIPELINE_CREATE_EARLY_RETURN_ON_FAILURE_BIT", 1UL << 9),
    std::make_pair("VK_PIPELINE_CREATE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR", 1UL << 21),
    std::make_pair("VK_PIPELINE_RASTERIZATION_STATE_CREATE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR", 1UL << 21),
    std::make_pair("VK_PIPELINE_CREATE_RENDERING_FRAGMENT_DENSITY_MAP_ATTACHMENT_BIT_EXT", 1UL << 22),
    std::make_pair("VK_PIPELINE_RASTERIZATION_STATE_CREATE_FRAGMENT_DENSITY_MAP_ATTACHMENT_BIT_EXT", 1UL << 22),
    std::make_pair("VK_PIPELINE_CREATE_VIEW_INDEX_FROM_DEVICE_INDEX_BIT_KHR", 1UL << 3),
    std::make_pair("VK_PIPELINE_CREATE_RAY_TRACING_NO_NULL_ANY_HIT_SHADERS_BIT_KHR", 1UL << 14),
    std::make_pair("VK_PIPELINE_CREATE_RAY_TRACING_NO_NULL_CLOSEST_HIT_SHADERS_BIT_KHR", 1UL << 15),
    std::make_pair("VK_PIPELINE_CREATE_RAY_TRACING_NO_NULL_MISS_SHADERS_BIT_KHR", 1UL << 16),
    std::make_pair("VK_PIPELINE_CREATE_RAY_TRACING_NO_NULL_INTERSECTION_SHADERS_BIT_KHR", 1UL << 17),
    std::make_pair("VK_PIPELINE_CREATE_RAY_TRACING_SKIP_TRIANGLES_BIT_KHR", 1UL << 12),
    std::make_pair("VK_PIPELINE_CREATE_RAY_TRACING_SKIP_AABBS_BIT_KHR", 1UL << 13),
    std::make_pair("VK_PIPELINE_CREATE_RAY_TRACING_SHADER_GROUP_HANDLE_CAPTURE_REPLAY_BIT_KHR", 1UL << 19),
    std::make_pair("VK_PIPELINE_CREATE_DEFER_COMPILE_BIT_NV", 1UL << 5),
    std::make_pair("VK_PIPELINE_CREATE_CAPTURE_STATISTICS_BIT_KHR", 1UL << 6),
    std::make_pair("VK_PIPELINE_CREATE_CAPTURE_INTERNAL_REPRESENTATIONS_BIT_KHR", 1UL << 7),
    std::make_pair("VK_PIPELINE_CREATE_INDIRECT_BINDABLE_BIT_NV", 1UL << 18),
    std::make_pair("VK_PIPELINE_CREATE_LIBRARY_BIT_KHR", 1UL << 11),
    std::make_pair("VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT_EXT", 1UL << 8),
    std::make_pair("VK_PIPELINE_CREATE_EARLY_RETURN_ON_FAILURE_BIT_EXT", 1UL << 9),
    std::make_pair("VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT", 1UL << 29),
    std::make_pair("VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT", 1UL << 23),
    std::make_pair("VK_PIPELINE_CREATE_LINK_TIME_OPTIMIZATION_BIT_EXT", 1UL << 10),
    std::make_pair("VK_PIPELINE_CREATE_RAY_TRACING_ALLOW_MOTION_BIT_NV", 1UL << 20),
    std::make_pair("VK_PIPELINE_CREATE_COLOR_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT", 1UL << 25),
    std::make_pair("VK_PIPELINE_CREATE_DEPTH_STENCIL_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT", 1UL << 26),
    std::make_pair("VK_PIPELINE_CREATE_RAY_TRACING_OPACITY_MICROMAP_BIT_EXT", 1UL << 24),
    std::make_pair("VK_PIPELINE_CREATE_RAY_TRACING_DISPLACEMENT_MICROMAP_BIT_NV", 1UL << 28),
    std::make_pair("VK_PIPELINE_CREATE_NO_PROTECTED_ACCESS_BIT_EXT", 1UL << 27),
    std::make_pair("VK_PIPELINE_CREATE_PROTECTED_ACCESS_ONLY_BIT_EXT", 1UL << 30),
};
static void parse_VkPipelineCreateFlagBits(const Json::Value &obj, VkPipelineCreateFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkPipelineCreateFlagBits)VkPipelineCreateFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkPipelineShaderStageCreateFlagBits_map = {
    std::make_pair("VK_PIPELINE_SHADER_STAGE_CREATE_ALLOW_VARYING_SUBGROUP_SIZE_BIT", 1UL << 0),
    std::make_pair("VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT", 1UL << 1),
    std::make_pair("VK_PIPELINE_SHADER_STAGE_CREATE_ALLOW_VARYING_SUBGROUP_SIZE_BIT_EXT", 1UL << 0),
    std::make_pair("VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT_EXT", 1UL << 1),
    std::make_pair("VK_PIPELINE_SHADER_STAGE_CREATE_RESERVED_3_BIT_KHR", 1UL << 3),
};
static void parse_VkPipelineShaderStageCreateFlagBits(const Json::Value &obj, VkPipelineShaderStageCreateFlagBits &o)
{
    const std::string &_res = obj.asString();
    o = (VkPipelineShaderStageCreateFlagBits)VkPipelineShaderStageCreateFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkShaderStageFlagBits_map = {
    std::make_pair("VK_SHADER_STAGE_VERTEX_BIT", 1UL << 0),
    std::make_pair("VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT", 1UL << 1),
    std::make_pair("VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT", 1UL << 2),
    std::make_pair("VK_SHADER_STAGE_GEOMETRY_BIT", 1UL << 3),
    std::make_pair("VK_SHADER_STAGE_FRAGMENT_BIT", 1UL << 4),
    std::make_pair("VK_SHADER_STAGE_COMPUTE_BIT", 1UL << 5),
    std::make_pair("VK_SHADER_STAGE_ALL_GRAPHICS", 0x0000001F),
    std::make_pair("VK_SHADER_STAGE_ALL", 0x7FFFFFFF),
    std::make_pair("VK_SHADER_STAGE_RAYGEN_BIT_KHR", 1UL << 8),
    std::make_pair("VK_SHADER_STAGE_ANY_HIT_BIT_KHR", 1UL << 9),
    std::make_pair("VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR", 1UL << 10),
    std::make_pair("VK_SHADER_STAGE_MISS_BIT_KHR", 1UL << 11),
    std::make_pair("VK_SHADER_STAGE_INTERSECTION_BIT_KHR", 1UL << 12),
    std::make_pair("VK_SHADER_STAGE_CALLABLE_BIT_KHR", 1UL << 13),
    std::make_pair("VK_SHADER_STAGE_RAYGEN_BIT_NV", 1UL << 8),
    std::make_pair("VK_SHADER_STAGE_ANY_HIT_BIT_NV", 1UL << 9),
    std::make_pair("VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV", 1UL << 10),
    std::make_pair("VK_SHADER_STAGE_MISS_BIT_NV", 1UL << 11),
    std::make_pair("VK_SHADER_STAGE_INTERSECTION_BIT_NV", 1UL << 12),
    std::make_pair("VK_SHADER_STAGE_CALLABLE_BIT_NV", 1UL << 13),
    std::make_pair("VK_SHADER_STAGE_TASK_BIT_NV", 1UL << 6),
    std::make_pair("VK_SHADER_STAGE_MESH_BIT_NV", 1UL << 7),
    std::make_pair("VK_SHADER_STAGE_TASK_BIT_EXT", 1UL << 6),
    std::make_pair("VK_SHADER_STAGE_MESH_BIT_EXT", 1UL << 7),
    std::make_pair("VK_SHADER_STAGE_SUBPASS_SHADING_BIT_HUAWEI", 1UL << 14),
    std::make_pair("VK_SHADER_STAGE_CLUSTER_CULLING_BIT_HUAWEI", 1UL << 19),
};
static void parse_VkShaderStageFlagBits(const Json::Value &obj, VkShaderStageFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkShaderStageFlagBits)VkShaderStageFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkCullModeFlagBits_map = {
    std::make_pair("VK_CULL_MODE_NONE", 0),
    std::make_pair("VK_CULL_MODE_FRONT_BIT", 1UL << 0),
    std::make_pair("VK_CULL_MODE_BACK_BIT", 1UL << 1),
    std::make_pair("VK_CULL_MODE_FRONT_AND_BACK", 0x00000003),
};
static void parse_VkCullModeFlagBits(const Json::Value &obj, VkCullModeFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkCullModeFlagBits)VkCullModeFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkDynamicState_map = {
    std::make_pair("VK_DYNAMIC_STATE_VIEWPORT", 0),
    std::make_pair("VK_DYNAMIC_STATE_SCISSOR", 1),
    std::make_pair("VK_DYNAMIC_STATE_LINE_WIDTH", 2),
    std::make_pair("VK_DYNAMIC_STATE_DEPTH_BIAS", 3),
    std::make_pair("VK_DYNAMIC_STATE_BLEND_CONSTANTS", 4),
    std::make_pair("VK_DYNAMIC_STATE_DEPTH_BOUNDS", 5),
    std::make_pair("VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK", 6),
    std::make_pair("VK_DYNAMIC_STATE_STENCIL_WRITE_MASK", 7),
    std::make_pair("VK_DYNAMIC_STATE_STENCIL_REFERENCE", 8),
    std::make_pair("VK_DYNAMIC_STATE_CULL_MODE", 1000267000),
    std::make_pair("VK_DYNAMIC_STATE_FRONT_FACE", 1000267001),
    std::make_pair("VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY", 1000267002),
    std::make_pair("VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT", 1000267003),
    std::make_pair("VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT", 1000267004),
    std::make_pair("VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE", 1000267005),
    std::make_pair("VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE", 1000267006),
    std::make_pair("VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE", 1000267007),
    std::make_pair("VK_DYNAMIC_STATE_DEPTH_COMPARE_OP", 1000267008),
    std::make_pair("VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE", 1000267009),
    std::make_pair("VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE", 1000267010),
    std::make_pair("VK_DYNAMIC_STATE_STENCIL_OP", 1000267011),
    std::make_pair("VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE", 1000377001),
    std::make_pair("VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE", 1000377002),
    std::make_pair("VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE", 1000377004),
    std::make_pair("VK_DYNAMIC_STATE_VIEWPORT_W_SCALING_NV", 1000087000),
    std::make_pair("VK_DYNAMIC_STATE_DISCARD_RECTANGLE_EXT", 1000099000),
    std::make_pair("VK_DYNAMIC_STATE_DISCARD_RECTANGLE_ENABLE_EXT", 1000099001),
    std::make_pair("VK_DYNAMIC_STATE_DISCARD_RECTANGLE_MODE_EXT", 1000099002),
    std::make_pair("VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_EXT", 1000143000),
    std::make_pair("VK_DYNAMIC_STATE_RAY_TRACING_PIPELINE_STACK_SIZE_KHR", 1000347000),
    std::make_pair("VK_DYNAMIC_STATE_VIEWPORT_SHADING_RATE_PALETTE_NV", 1000164004),
    std::make_pair("VK_DYNAMIC_STATE_VIEWPORT_COARSE_SAMPLE_ORDER_NV", 1000164006),
    std::make_pair("VK_DYNAMIC_STATE_EXCLUSIVE_SCISSOR_ENABLE_NV", 1000205000),
    std::make_pair("VK_DYNAMIC_STATE_EXCLUSIVE_SCISSOR_NV", 1000205001),
    std::make_pair("VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR", 1000226000),
    std::make_pair("VK_DYNAMIC_STATE_LINE_STIPPLE_EXT", 1000259000),
    std::make_pair("VK_DYNAMIC_STATE_CULL_MODE_EXT", 1000267000),
    std::make_pair("VK_DYNAMIC_STATE_FRONT_FACE_EXT", 1000267001),
    std::make_pair("VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY_EXT", 1000267002),
    std::make_pair("VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT_EXT", 1000267003),
    std::make_pair("VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT_EXT", 1000267004),
    std::make_pair("VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE_EXT", 1000267005),
    std::make_pair("VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE_EXT", 1000267006),
    std::make_pair("VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE_EXT", 1000267007),
    std::make_pair("VK_DYNAMIC_STATE_DEPTH_COMPARE_OP_EXT", 1000267008),
    std::make_pair("VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE_EXT", 1000267009),
    std::make_pair("VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE_EXT", 1000267010),
    std::make_pair("VK_DYNAMIC_STATE_STENCIL_OP_EXT", 1000267011),
    std::make_pair("VK_DYNAMIC_STATE_VERTEX_INPUT_EXT", 1000352000),
    std::make_pair("VK_DYNAMIC_STATE_PATCH_CONTROL_POINTS_EXT", 1000377000),
    std::make_pair("VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE_EXT", 1000377001),
    std::make_pair("VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE_EXT", 1000377002),
    std::make_pair("VK_DYNAMIC_STATE_LOGIC_OP_EXT", 1000377003),
    std::make_pair("VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE_EXT", 1000377004),
    std::make_pair("VK_DYNAMIC_STATE_COLOR_WRITE_ENABLE_EXT", 1000381000),
    std::make_pair("VK_DYNAMIC_STATE_DEPTH_CLAMP_ENABLE_EXT", 1000455003),
    std::make_pair("VK_DYNAMIC_STATE_POLYGON_MODE_EXT", 1000455004),
    std::make_pair("VK_DYNAMIC_STATE_RASTERIZATION_SAMPLES_EXT", 1000455005),
    std::make_pair("VK_DYNAMIC_STATE_SAMPLE_MASK_EXT", 1000455006),
    std::make_pair("VK_DYNAMIC_STATE_ALPHA_TO_COVERAGE_ENABLE_EXT", 1000455007),
    std::make_pair("VK_DYNAMIC_STATE_ALPHA_TO_ONE_ENABLE_EXT", 1000455008),
    std::make_pair("VK_DYNAMIC_STATE_LOGIC_OP_ENABLE_EXT", 1000455009),
    std::make_pair("VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT", 1000455010),
    std::make_pair("VK_DYNAMIC_STATE_COLOR_BLEND_EQUATION_EXT", 1000455011),
    std::make_pair("VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT", 1000455012),
    std::make_pair("VK_DYNAMIC_STATE_TESSELLATION_DOMAIN_ORIGIN_EXT", 1000455002),
    std::make_pair("VK_DYNAMIC_STATE_RASTERIZATION_STREAM_EXT", 1000455013),
    std::make_pair("VK_DYNAMIC_STATE_CONSERVATIVE_RASTERIZATION_MODE_EXT", 1000455014),
    std::make_pair("VK_DYNAMIC_STATE_EXTRA_PRIMITIVE_OVERESTIMATION_SIZE_EXT", 1000455015),
    std::make_pair("VK_DYNAMIC_STATE_DEPTH_CLIP_ENABLE_EXT", 1000455016),
    std::make_pair("VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_ENABLE_EXT", 1000455017),
    std::make_pair("VK_DYNAMIC_STATE_COLOR_BLEND_ADVANCED_EXT", 1000455018),
    std::make_pair("VK_DYNAMIC_STATE_PROVOKING_VERTEX_MODE_EXT", 1000455019),
    std::make_pair("VK_DYNAMIC_STATE_LINE_RASTERIZATION_MODE_EXT", 1000455020),
    std::make_pair("VK_DYNAMIC_STATE_LINE_STIPPLE_ENABLE_EXT", 1000455021),
    std::make_pair("VK_DYNAMIC_STATE_DEPTH_CLIP_NEGATIVE_ONE_TO_ONE_EXT", 1000455022),
    std::make_pair("VK_DYNAMIC_STATE_VIEWPORT_W_SCALING_ENABLE_NV", 1000455023),
    std::make_pair("VK_DYNAMIC_STATE_VIEWPORT_SWIZZLE_NV", 1000455024),
    std::make_pair("VK_DYNAMIC_STATE_COVERAGE_TO_COLOR_ENABLE_NV", 1000455025),
    std::make_pair("VK_DYNAMIC_STATE_COVERAGE_TO_COLOR_LOCATION_NV", 1000455026),
    std::make_pair("VK_DYNAMIC_STATE_COVERAGE_MODULATION_MODE_NV", 1000455027),
    std::make_pair("VK_DYNAMIC_STATE_COVERAGE_MODULATION_TABLE_ENABLE_NV", 1000455028),
    std::make_pair("VK_DYNAMIC_STATE_COVERAGE_MODULATION_TABLE_NV", 1000455029),
    std::make_pair("VK_DYNAMIC_STATE_SHADING_RATE_IMAGE_ENABLE_NV", 1000455030),
    std::make_pair("VK_DYNAMIC_STATE_REPRESENTATIVE_FRAGMENT_TEST_ENABLE_NV", 1000455031),
    std::make_pair("VK_DYNAMIC_STATE_COVERAGE_REDUCTION_MODE_NV", 1000455032),
    std::make_pair("VK_DYNAMIC_STATE_ATTACHMENT_FEEDBACK_LOOP_ENABLE_EXT", 1000524000),
    std::make_pair("VK_DYNAMIC_STATE_LINE_STIPPLE_KHR", 1000259000),
};
static void parse_VkDynamicState(const Json::Value &obj, VkDynamicState &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkDynamicState)VkDynamicState_map[std::string(_res)];
}

static std::map<std::string, int> VkFrontFace_map = {
    std::make_pair("VK_FRONT_FACE_COUNTER_CLOCKWISE", 0),
    std::make_pair("VK_FRONT_FACE_CLOCKWISE", 1),
};
static void parse_VkFrontFace(const Json::Value &obj, VkFrontFace &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkFrontFace)VkFrontFace_map[std::string(_res)];
}

static std::map<std::string, int> VkVertexInputRate_map = {
    std::make_pair("VK_VERTEX_INPUT_RATE_VERTEX", 0),
    std::make_pair("VK_VERTEX_INPUT_RATE_INSTANCE", 1),
};
static void parse_VkVertexInputRate(const Json::Value &obj, VkVertexInputRate &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkVertexInputRate)VkVertexInputRate_map[std::string(_res)];
}

static std::map<std::string, int> VkPrimitiveTopology_map = {
    std::make_pair("VK_PRIMITIVE_TOPOLOGY_POINT_LIST", 0),
    std::make_pair("VK_PRIMITIVE_TOPOLOGY_LINE_LIST", 1),
    std::make_pair("VK_PRIMITIVE_TOPOLOGY_LINE_STRIP", 2),
    std::make_pair("VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST", 3),
    std::make_pair("VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP", 4),
    std::make_pair("VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN", 5),
    std::make_pair("VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY", 6),
    std::make_pair("VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY", 7),
    std::make_pair("VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY", 8),
    std::make_pair("VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY", 9),
    std::make_pair("VK_PRIMITIVE_TOPOLOGY_PATCH_LIST", 10),
};
static void parse_VkPrimitiveTopology(const Json::Value &obj, VkPrimitiveTopology &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkPrimitiveTopology)VkPrimitiveTopology_map[std::string(_res)];
}

static std::map<std::string, int> VkPolygonMode_map = {
    std::make_pair("VK_POLYGON_MODE_FILL", 0),
    std::make_pair("VK_POLYGON_MODE_LINE", 1),
    std::make_pair("VK_POLYGON_MODE_POINT", 2),
    std::make_pair("VK_POLYGON_MODE_FILL_RECTANGLE_NV", 1000153000),
};
static void parse_VkPolygonMode(const Json::Value &obj, VkPolygonMode &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkPolygonMode)VkPolygonMode_map[std::string(_res)];
}

static std::map<std::string, int> VkStencilOp_map = {
    std::make_pair("VK_STENCIL_OP_KEEP", 0),
    std::make_pair("VK_STENCIL_OP_ZERO", 1),
    std::make_pair("VK_STENCIL_OP_REPLACE", 2),
    std::make_pair("VK_STENCIL_OP_INCREMENT_AND_CLAMP", 3),
    std::make_pair("VK_STENCIL_OP_DECREMENT_AND_CLAMP", 4),
    std::make_pair("VK_STENCIL_OP_INVERT", 5),
    std::make_pair("VK_STENCIL_OP_INCREMENT_AND_WRAP", 6),
    std::make_pair("VK_STENCIL_OP_DECREMENT_AND_WRAP", 7),
};
static void parse_VkStencilOp(const Json::Value &obj, VkStencilOp &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkStencilOp)VkStencilOp_map[std::string(_res)];
}

static std::map<std::string, int> VkLogicOp_map = {
    std::make_pair("VK_LOGIC_OP_CLEAR", 0),
    std::make_pair("VK_LOGIC_OP_AND", 1),
    std::make_pair("VK_LOGIC_OP_AND_REVERSE", 2),
    std::make_pair("VK_LOGIC_OP_COPY", 3),
    std::make_pair("VK_LOGIC_OP_AND_INVERTED", 4),
    std::make_pair("VK_LOGIC_OP_NO_OP", 5),
    std::make_pair("VK_LOGIC_OP_XOR", 6),
    std::make_pair("VK_LOGIC_OP_OR", 7),
    std::make_pair("VK_LOGIC_OP_NOR", 8),
    std::make_pair("VK_LOGIC_OP_EQUIVALENT", 9),
    std::make_pair("VK_LOGIC_OP_INVERT", 10),
    std::make_pair("VK_LOGIC_OP_OR_REVERSE", 11),
    std::make_pair("VK_LOGIC_OP_COPY_INVERTED", 12),
    std::make_pair("VK_LOGIC_OP_OR_INVERTED", 13),
    std::make_pair("VK_LOGIC_OP_NAND", 14),
    std::make_pair("VK_LOGIC_OP_SET", 15),
};
static void parse_VkLogicOp(const Json::Value &obj, VkLogicOp &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkLogicOp)VkLogicOp_map[std::string(_res)];
}

static std::map<std::string, int> VkPipelineLayoutCreateFlagBits_map = {
    std::make_pair("VK_PIPELINE_LAYOUT_CREATE_RESERVED_0_BIT_AMD", 1UL << 0),
    std::make_pair("VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT", 1UL << 1),
};
static void parse_VkPipelineLayoutCreateFlagBits(const Json::Value &obj, VkPipelineLayoutCreateFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkPipelineLayoutCreateFlagBits)VkPipelineLayoutCreateFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkBorderColor_map = {
    std::make_pair("VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK", 0),
    std::make_pair("VK_BORDER_COLOR_INT_TRANSPARENT_BLACK", 1),
    std::make_pair("VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK", 2),
    std::make_pair("VK_BORDER_COLOR_INT_OPAQUE_BLACK", 3),
    std::make_pair("VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE", 4),
    std::make_pair("VK_BORDER_COLOR_INT_OPAQUE_WHITE", 5),
    std::make_pair("VK_BORDER_COLOR_FLOAT_CUSTOM_EXT", 1000287003),
    std::make_pair("VK_BORDER_COLOR_INT_CUSTOM_EXT", 1000287004),
};
static void parse_VkBorderColor(const Json::Value &obj, VkBorderColor &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkBorderColor)VkBorderColor_map[std::string(_res)];
}

static std::map<std::string, int> VkFilter_map = {
    std::make_pair("VK_FILTER_NEAREST", 0),
    std::make_pair("VK_FILTER_LINEAR", 1),
    std::make_pair("VK_FILTER_CUBIC_IMG", 1000015000),
    std::make_pair("VK_FILTER_CUBIC_EXT", 1000015000),
};
static void parse_VkFilter(const Json::Value &obj, VkFilter &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkFilter)VkFilter_map[std::string(_res)];
}

static std::map<std::string, int> VkSamplerAddressMode_map = {
    std::make_pair("VK_SAMPLER_ADDRESS_MODE_REPEAT", 0),
    std::make_pair("VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT", 1),
    std::make_pair("VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE", 2),
    std::make_pair("VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER", 3),
    std::make_pair("VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE", 4),
    std::make_pair("VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE", 4),
    std::make_pair("VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE_KHR", 4),
};
static void parse_VkSamplerAddressMode(const Json::Value &obj, VkSamplerAddressMode &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkSamplerAddressMode)VkSamplerAddressMode_map[std::string(_res)];
}

static std::map<std::string, int> VkSamplerCreateFlagBits_map = {
    std::make_pair("VK_SAMPLER_CREATE_SUBSAMPLED_BIT_EXT", 1UL << 0),
    std::make_pair("VK_SAMPLER_CREATE_SUBSAMPLED_COARSE_RECONSTRUCTION_BIT_EXT", 1UL << 1),
    std::make_pair("VK_SAMPLER_CREATE_DESCRIPTOR_BUFFER_CAPTURE_REPLAY_BIT_EXT", 1UL << 3),
    std::make_pair("VK_SAMPLER_CREATE_NON_SEAMLESS_CUBE_MAP_BIT_EXT", 1UL << 2),
    std::make_pair("VK_SAMPLER_CREATE_IMAGE_PROCESSING_BIT_QCOM", 1UL << 4),
};
static void parse_VkSamplerCreateFlagBits(const Json::Value &obj, VkSamplerCreateFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkSamplerCreateFlagBits)VkSamplerCreateFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkSamplerMipmapMode_map = {
    std::make_pair("VK_SAMPLER_MIPMAP_MODE_NEAREST", 0),
    std::make_pair("VK_SAMPLER_MIPMAP_MODE_LINEAR", 1),
};
static void parse_VkSamplerMipmapMode(const Json::Value &obj, VkSamplerMipmapMode &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkSamplerMipmapMode)VkSamplerMipmapMode_map[std::string(_res)];
}

static std::map<std::string, int> VkDescriptorPoolCreateFlagBits_map = {
    std::make_pair("VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT", 1UL << 0),
    std::make_pair("VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT", 1UL << 1),
    std::make_pair("VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT", 1UL << 1),
    std::make_pair("VK_DESCRIPTOR_POOL_CREATE_HOST_ONLY_BIT_VALVE", 1UL << 2),
    std::make_pair("VK_DESCRIPTOR_POOL_CREATE_HOST_ONLY_BIT_EXT", 1UL << 2),
    std::make_pair("VK_DESCRIPTOR_POOL_CREATE_ALLOW_OVERALLOCATION_SETS_BIT_NV", 1UL << 3),
    std::make_pair("VK_DESCRIPTOR_POOL_CREATE_ALLOW_OVERALLOCATION_POOLS_BIT_NV", 1UL << 4),
};
static void parse_VkDescriptorPoolCreateFlagBits(const Json::Value &obj, VkDescriptorPoolCreateFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkDescriptorPoolCreateFlagBits)VkDescriptorPoolCreateFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkDescriptorType_map = {
    std::make_pair("VK_DESCRIPTOR_TYPE_SAMPLER", 0),
    std::make_pair("VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER", 1),
    std::make_pair("VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE", 2),
    std::make_pair("VK_DESCRIPTOR_TYPE_STORAGE_IMAGE", 3),
    std::make_pair("VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER", 4),
    std::make_pair("VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER", 5),
    std::make_pair("VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER", 6),
    std::make_pair("VK_DESCRIPTOR_TYPE_STORAGE_BUFFER", 7),
    std::make_pair("VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC", 8),
    std::make_pair("VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC", 9),
    std::make_pair("VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT", 10),
    std::make_pair("VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK", 1000138000),
    std::make_pair("VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT", 1000138000),
    std::make_pair("VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR", 1000150000),
    std::make_pair("VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV", 1000165000),
    std::make_pair("VK_DESCRIPTOR_TYPE_MUTABLE_VALVE", 1000351000),
    std::make_pair("VK_DESCRIPTOR_TYPE_SAMPLE_WEIGHT_IMAGE_QCOM", 1000440000),
    std::make_pair("VK_DESCRIPTOR_TYPE_BLOCK_MATCH_IMAGE_QCOM", 1000440001),
    std::make_pair("VK_DESCRIPTOR_TYPE_MUTABLE_EXT", 1000351000),
};
static void parse_VkDescriptorType(const Json::Value &obj, VkDescriptorType &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkDescriptorType)VkDescriptorType_map[std::string(_res)];
}

static std::map<std::string, int> VkDescriptorSetLayoutCreateFlagBits_map = {
    std::make_pair("VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT", 1UL << 1),
    std::make_pair("VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR", 1UL << 0),
    std::make_pair("VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT", 1UL << 1),
    std::make_pair("VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT", 1UL << 4),
    std::make_pair("VK_DESCRIPTOR_SET_LAYOUT_CREATE_EMBEDDED_IMMUTABLE_SAMPLERS_BIT_EXT", 1UL << 5),
    std::make_pair("VK_DESCRIPTOR_SET_LAYOUT_CREATE_RESERVED_3_BIT_AMD", 1UL << 3),
    std::make_pair("VK_DESCRIPTOR_SET_LAYOUT_CREATE_HOST_ONLY_POOL_BIT_VALVE", 1UL << 2),
    std::make_pair("VK_DESCRIPTOR_SET_LAYOUT_CREATE_INDIRECT_BINDABLE_BIT_NV", 1UL << 7),
    std::make_pair("VK_DESCRIPTOR_SET_LAYOUT_CREATE_HOST_ONLY_POOL_BIT_EXT", 1UL << 2),
    std::make_pair("VK_DESCRIPTOR_SET_LAYOUT_CREATE_PER_STAGE_BIT_NV", 1UL << 6),
};
static void parse_VkDescriptorSetLayoutCreateFlagBits(const Json::Value &obj, VkDescriptorSetLayoutCreateFlagBits &o)
{
    const std::string &_res = obj.asString();
    o = (VkDescriptorSetLayoutCreateFlagBits)VkDescriptorSetLayoutCreateFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkAttachmentDescriptionFlagBits_map = {
    std::make_pair("VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT", 1UL << 0),
};
static void parse_VkAttachmentDescriptionFlagBits(const Json::Value &obj, VkAttachmentDescriptionFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkAttachmentDescriptionFlagBits)VkAttachmentDescriptionFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkAttachmentLoadOp_map = {
    std::make_pair("VK_ATTACHMENT_LOAD_OP_LOAD", 0),
    std::make_pair("VK_ATTACHMENT_LOAD_OP_CLEAR", 1),
    std::make_pair("VK_ATTACHMENT_LOAD_OP_DONT_CARE", 2),
    std::make_pair("VK_ATTACHMENT_LOAD_OP_NONE_EXT", 1000400000),
    std::make_pair("VK_ATTACHMENT_LOAD_OP_NONE_KHR", 1000400000),
};
static void parse_VkAttachmentLoadOp(const Json::Value &obj, VkAttachmentLoadOp &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkAttachmentLoadOp)VkAttachmentLoadOp_map[std::string(_res)];
}

static std::map<std::string, int> VkAttachmentStoreOp_map = {
    std::make_pair("VK_ATTACHMENT_STORE_OP_STORE", 0),
    std::make_pair("VK_ATTACHMENT_STORE_OP_DONT_CARE", 1),
    std::make_pair("VK_ATTACHMENT_STORE_OP_NONE", 1000301000),
    std::make_pair("VK_ATTACHMENT_STORE_OP_NONE_KHR", 1000301000),
    std::make_pair("VK_ATTACHMENT_STORE_OP_NONE_QCOM", 1000301000),
    std::make_pair("VK_ATTACHMENT_STORE_OP_NONE_EXT", 1000301000),
    std::make_pair("VK_ATTACHMENT_STORE_OP_NONE_KHR", 1000301000),
};
static void parse_VkAttachmentStoreOp(const Json::Value &obj, VkAttachmentStoreOp &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkAttachmentStoreOp)VkAttachmentStoreOp_map[std::string(_res)];
}

static std::map<std::string, int> VkDependencyFlagBits_map = {
    std::make_pair("VK_DEPENDENCY_BY_REGION_BIT", 1UL << 0),
    std::make_pair("VK_DEPENDENCY_DEVICE_GROUP_BIT", 1UL << 2),
    std::make_pair("VK_DEPENDENCY_VIEW_LOCAL_BIT", 1UL << 1),
    std::make_pair("VK_DEPENDENCY_VIEW_LOCAL_BIT_KHR", 1UL << 1),
    std::make_pair("VK_DEPENDENCY_DEVICE_GROUP_BIT_KHR", 1UL << 2),
    std::make_pair("VK_DEPENDENCY_FEEDBACK_LOOP_BIT_EXT", 1UL << 3),
    std::make_pair("VK_DEPENDENCY_EXTENSION_586_BIT_IMG", 1UL << 4),
};
static void parse_VkDependencyFlagBits(const Json::Value &obj, VkDependencyFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkDependencyFlagBits)VkDependencyFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkFramebufferCreateFlagBits_map = {
    std::make_pair("VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT", 1UL << 0),
    std::make_pair("VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT_KHR", 1UL << 0),
};
static void parse_VkFramebufferCreateFlagBits(const Json::Value &obj, VkFramebufferCreateFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkFramebufferCreateFlagBits)VkFramebufferCreateFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkPipelineBindPoint_map = {
    std::make_pair("VK_PIPELINE_BIND_POINT_GRAPHICS", 0),
    std::make_pair("VK_PIPELINE_BIND_POINT_COMPUTE", 1),
    std::make_pair("VK_PIPELINE_BIND_POINT_EXECUTION_GRAPH_AMDX", 1000134000),
    std::make_pair("VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR", 1000165000),
    std::make_pair("VK_PIPELINE_BIND_POINT_RAY_TRACING_NV", 1000165000),
    std::make_pair("VK_PIPELINE_BIND_POINT_SUBPASS_SHADING_HUAWEI", 1000369003),
};
static void parse_VkPipelineBindPoint(const Json::Value &obj, VkPipelineBindPoint &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkPipelineBindPoint)VkPipelineBindPoint_map[std::string(_res)];
}

static std::map<std::string, int> VkRenderPassCreateFlagBits_map = {
    std::make_pair("VK_RENDER_PASS_CREATE_RESERVED_0_BIT_KHR", 1UL << 0),
    std::make_pair("VK_RENDER_PASS_CREATE_TRANSFORM_BIT_QCOM", 1UL << 1),
};
static void parse_VkRenderPassCreateFlagBits(const Json::Value &obj, VkRenderPassCreateFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkRenderPassCreateFlagBits)VkRenderPassCreateFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkSubpassDescriptionFlagBits_map = {
    std::make_pair("VK_SUBPASS_DESCRIPTION_PER_VIEW_ATTRIBUTES_BIT_NVX", 1UL << 0),
    std::make_pair("VK_SUBPASS_DESCRIPTION_PER_VIEW_POSITION_X_ONLY_BIT_NVX", 1UL << 1),
    std::make_pair("VK_SUBPASS_DESCRIPTION_FRAGMENT_REGION_BIT_QCOM", 1UL << 2),
    std::make_pair("VK_SUBPASS_DESCRIPTION_SHADER_RESOLVE_BIT_QCOM", 1UL << 3),
    std::make_pair("VK_SUBPASS_DESCRIPTION_RASTERIZATION_ORDER_ATTACHMENT_COLOR_ACCESS_BIT_ARM", 1UL << 4),
    std::make_pair("VK_SUBPASS_DESCRIPTION_RASTERIZATION_ORDER_ATTACHMENT_DEPTH_ACCESS_BIT_ARM", 1UL << 5),
    std::make_pair("VK_SUBPASS_DESCRIPTION_RASTERIZATION_ORDER_ATTACHMENT_STENCIL_ACCESS_BIT_ARM", 1UL << 6),
    std::make_pair("VK_SUBPASS_DESCRIPTION_RASTERIZATION_ORDER_ATTACHMENT_COLOR_ACCESS_BIT_EXT", 1UL << 4),
    std::make_pair("VK_SUBPASS_DESCRIPTION_RASTERIZATION_ORDER_ATTACHMENT_DEPTH_ACCESS_BIT_EXT", 1UL << 5),
    std::make_pair("VK_SUBPASS_DESCRIPTION_RASTERIZATION_ORDER_ATTACHMENT_STENCIL_ACCESS_BIT_EXT", 1UL << 6),
    std::make_pair("VK_SUBPASS_DESCRIPTION_ENABLE_LEGACY_DITHERING_BIT_EXT", 1UL << 7),
};
static void parse_VkSubpassDescriptionFlagBits(const Json::Value &obj, VkSubpassDescriptionFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkSubpassDescriptionFlagBits)VkSubpassDescriptionFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkCommandPoolCreateFlagBits_map = {
    std::make_pair("VK_COMMAND_POOL_CREATE_TRANSIENT_BIT", 1UL << 0),
    std::make_pair("VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT", 1UL << 1),
    std::make_pair("VK_COMMAND_POOL_CREATE_PROTECTED_BIT", 1UL << 2),
};
static void parse_VkCommandPoolCreateFlagBits(const Json::Value &obj, VkCommandPoolCreateFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkCommandPoolCreateFlagBits)VkCommandPoolCreateFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkCommandPoolResetFlagBits_map = {
    std::make_pair("VK_COMMAND_POOL_RESET_RESERVED_1_BIT_COREAVI", 1UL << 1),
};
static void parse_VkCommandPoolResetFlagBits(const Json::Value &obj, VkCommandPoolResetFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkCommandPoolResetFlagBits)VkCommandPoolResetFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkCommandBufferLevel_map = {
    std::make_pair("VK_COMMAND_BUFFER_LEVEL_PRIMARY", 0),
    std::make_pair("VK_COMMAND_BUFFER_LEVEL_SECONDARY", 1),
};
static void parse_VkCommandBufferLevel(const Json::Value &obj, VkCommandBufferLevel &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkCommandBufferLevel)VkCommandBufferLevel_map[std::string(_res)];
}

static std::map<std::string, int> VkCommandBufferUsageFlagBits_map = {
    std::make_pair("VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT", 1UL << 0),
    std::make_pair("VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT", 1UL << 1),
    std::make_pair("VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT", 1UL << 2),
};
static void parse_VkCommandBufferUsageFlagBits(const Json::Value &obj, VkCommandBufferUsageFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkCommandBufferUsageFlagBits)VkCommandBufferUsageFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkQueryControlFlagBits_map = {
    std::make_pair("VK_QUERY_CONTROL_PRECISE_BIT", 1UL << 0),
};
static void parse_VkQueryControlFlagBits(const Json::Value &obj, VkQueryControlFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkQueryControlFlagBits)VkQueryControlFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkCommandBufferResetFlagBits_map = {
    std::make_pair("VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT", 1UL << 0),
};
static void parse_VkCommandBufferResetFlagBits(const Json::Value &obj, VkCommandBufferResetFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkCommandBufferResetFlagBits)VkCommandBufferResetFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkIndexType_map = {
    std::make_pair("VK_INDEX_TYPE_UINT16", 0),
    std::make_pair("VK_INDEX_TYPE_UINT32", 1),
    std::make_pair("VK_INDEX_TYPE_NONE_KHR", 1000165000),
    std::make_pair("VK_INDEX_TYPE_NONE_NV", 1000165000),
    std::make_pair("VK_INDEX_TYPE_UINT8_EXT", 1000265000),
    std::make_pair("VK_INDEX_TYPE_UINT8_KHR", 1000265000),
};
static void parse_VkIndexType(const Json::Value &obj, VkIndexType &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkIndexType)VkIndexType_map[std::string(_res)];
}

static std::map<std::string, int> VkStencilFaceFlagBits_map = {
    std::make_pair("VK_STENCIL_FACE_FRONT_BIT", 1UL << 0),
    std::make_pair("VK_STENCIL_FACE_BACK_BIT", 1UL << 1),
    std::make_pair("VK_STENCIL_FACE_FRONT_AND_BACK", 0x00000003),
};
static void parse_VkStencilFaceFlagBits(const Json::Value &obj, VkStencilFaceFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkStencilFaceFlagBits)VkStencilFaceFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkSubpassContents_map = {
    std::make_pair("VK_SUBPASS_CONTENTS_INLINE", 0),
    std::make_pair("VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS", 1),
    std::make_pair("VK_SUBPASS_CONTENTS_INLINE_AND_SECONDARY_COMMAND_BUFFERS_EXT", 1000451000),
    std::make_pair("VK_SUBPASS_CONTENTS_INLINE_AND_SECONDARY_COMMAND_BUFFERS_KHR", 1000451000),
};
static void parse_VkSubpassContents(const Json::Value &obj, VkSubpassContents &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkSubpassContents)VkSubpassContents_map[std::string(_res)];
}

static void parse_VkAccessFlags(const Json::Value &obj, VkAccessFlags &o)
{
    o                       = (VkAccessFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkAccessFlagBits)VkAccessFlagBits_map[it];
    }
}

static void parse_VkImageAspectFlags(const Json::Value &obj, VkImageAspectFlags &o)
{
    o                       = (VkImageAspectFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkImageAspectFlagBits)VkImageAspectFlagBits_map[it];
    }
}

static void parse_VkFormatFeatureFlags(const Json::Value &obj, VkFormatFeatureFlags &o)
{
    o                       = (VkFormatFeatureFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkFormatFeatureFlagBits)VkFormatFeatureFlagBits_map[it];
    }
}

static void parse_VkImageCreateFlags(const Json::Value &obj, VkImageCreateFlags &o)
{
    o                       = (VkImageCreateFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkImageCreateFlagBits)VkImageCreateFlagBits_map[it];
    }
}

static void parse_VkSampleCountFlags(const Json::Value &obj, VkSampleCountFlags &o)
{
    o                       = (VkSampleCountFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkSampleCountFlagBits)VkSampleCountFlagBits_map[it];
    }
}

static void parse_VkImageUsageFlags(const Json::Value &obj, VkImageUsageFlags &o)
{
    o                       = (VkImageUsageFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkImageUsageFlagBits)VkImageUsageFlagBits_map[it];
    }
}

static void parse_VkInstanceCreateFlags(const Json::Value &obj, VkInstanceCreateFlags &o)
{
    o                       = (VkInstanceCreateFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkInstanceCreateFlagBits)VkInstanceCreateFlagBits_map[it];
    }
}

static void parse_VkMemoryHeapFlags(const Json::Value &obj, VkMemoryHeapFlags &o)
{
    o                       = (VkMemoryHeapFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkMemoryHeapFlagBits)VkMemoryHeapFlagBits_map[it];
    }
}

static void parse_VkMemoryPropertyFlags(const Json::Value &obj, VkMemoryPropertyFlags &o)
{
    o                       = (VkMemoryPropertyFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkMemoryPropertyFlagBits)VkMemoryPropertyFlagBits_map[it];
    }
}

static void parse_VkQueueFlags(const Json::Value &obj, VkQueueFlags &o)
{
    o                       = (VkQueueFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkQueueFlagBits)VkQueueFlagBits_map[it];
    }
}

static void parse_VkDeviceCreateFlags(const Json::Value &obj, VkDeviceCreateFlags &o)
{
    if (obj.isString())
    {
        const std::string &_res = obj.asString();
        sscanf(_res.c_str(), "%u", &o);
    }
    else
    {
        o = obj.asUInt();
    }
}

static void parse_VkDeviceQueueCreateFlags(const Json::Value &obj, VkDeviceQueueCreateFlags &o)
{
    o                       = (VkDeviceQueueCreateFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkDeviceQueueCreateFlagBits)VkDeviceQueueCreateFlagBits_map[it];
    }
}

static void parse_VkPipelineStageFlags(const Json::Value &obj, VkPipelineStageFlags &o)
{
    o                       = (VkPipelineStageFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkPipelineStageFlagBits)VkPipelineStageFlagBits_map[it];
    }
}

static void parse_VkMemoryMapFlags(const Json::Value &obj, VkMemoryMapFlags &o)
{
    o                       = (VkMemoryMapFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkMemoryMapFlagBits)VkMemoryMapFlagBits_map[it];
    }
}

static void parse_VkFenceCreateFlags(const Json::Value &obj, VkFenceCreateFlags &o)
{
    o                       = (VkFenceCreateFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkFenceCreateFlagBits)VkFenceCreateFlagBits_map[it];
    }
}

static void parse_VkSemaphoreCreateFlags(const Json::Value &obj, VkSemaphoreCreateFlags &o)
{
    if (obj.isString())
    {
        const std::string &_res = obj.asString();
        sscanf(_res.c_str(), "%u", &o);
    }
    else
    {
        o = obj.asUInt();
    }
}

static void parse_VkEventCreateFlags(const Json::Value &obj, VkEventCreateFlags &o)
{
    o                       = (VkEventCreateFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkEventCreateFlagBits)VkEventCreateFlagBits_map[it];
    }
}

static void parse_VkQueryPipelineStatisticFlags(const Json::Value &obj, VkQueryPipelineStatisticFlags &o)
{
    o                       = (VkQueryPipelineStatisticFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkQueryPipelineStatisticFlagBits)VkQueryPipelineStatisticFlagBits_map[it];
    }
}

static void parse_VkQueryPoolCreateFlags(const Json::Value &obj, VkQueryPoolCreateFlags &o)
{
    if (obj.isString())
    {
        const std::string &_res = obj.asString();
        sscanf(_res.c_str(), "%u", &o);
    }
    else
    {
        o = obj.asUInt();
    }
}

static void parse_VkQueryResultFlags(const Json::Value &obj, VkQueryResultFlags &o)
{
    o                       = (VkQueryResultFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkQueryResultFlagBits)VkQueryResultFlagBits_map[it];
    }
}

static void parse_VkBufferCreateFlags(const Json::Value &obj, VkBufferCreateFlags &o)
{
    o                       = (VkBufferCreateFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkBufferCreateFlagBits)VkBufferCreateFlagBits_map[it];
    }
}

static void parse_VkBufferUsageFlags(const Json::Value &obj, VkBufferUsageFlags &o)
{
    o                       = (VkBufferUsageFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkBufferUsageFlagBits)VkBufferUsageFlagBits_map[it];
    }
}

static void parse_VkBufferViewCreateFlags(const Json::Value &obj, VkBufferViewCreateFlags &o)
{
    if (obj.isString())
    {
        const std::string &_res = obj.asString();
        sscanf(_res.c_str(), "%u", &o);
    }
    else
    {
        o = obj.asUInt();
    }
}

static void parse_VkImageViewCreateFlags(const Json::Value &obj, VkImageViewCreateFlags &o)
{
    o                       = (VkImageViewCreateFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkImageViewCreateFlagBits)VkImageViewCreateFlagBits_map[it];
    }
}

static void parse_VkPipelineCacheCreateFlags(const Json::Value &obj, VkPipelineCacheCreateFlags &o)
{
    o                       = (VkPipelineCacheCreateFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkPipelineCacheCreateFlagBits)VkPipelineCacheCreateFlagBits_map[it];
    }
}

static void parse_VkColorComponentFlags(const Json::Value &obj, VkColorComponentFlags &o)
{
    o                       = (VkColorComponentFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkColorComponentFlagBits)VkColorComponentFlagBits_map[it];
    }
}

static void parse_VkPipelineCreateFlags(const Json::Value &obj, VkPipelineCreateFlags &o)
{
    o                       = (VkPipelineCreateFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkPipelineCreateFlagBits)VkPipelineCreateFlagBits_map[it];
    }
}

static void parse_VkPipelineShaderStageCreateFlags(const Json::Value &obj, VkPipelineShaderStageCreateFlags &o)
{
    o                       = (VkPipelineShaderStageCreateFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkPipelineShaderStageCreateFlagBits)VkPipelineShaderStageCreateFlagBits_map[it];
    }
}

static void parse_VkCullModeFlags(const Json::Value &obj, VkCullModeFlags &o)
{
    o                       = (VkCullModeFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkCullModeFlagBits)VkCullModeFlagBits_map[it];
    }
}

static void parse_VkPipelineVertexInputStateCreateFlags(const Json::Value &obj,
                                                        VkPipelineVertexInputStateCreateFlags &o)
{
    if (obj.isString())
    {
        const std::string &_res = obj.asString();
        sscanf(_res.c_str(), "%u", &o);
    }
    else
    {
        o = obj.asUInt();
    }
}

static void parse_VkPipelineInputAssemblyStateCreateFlags(const Json::Value &obj,
                                                          VkPipelineInputAssemblyStateCreateFlags &o)
{
    if (obj.isString())
    {
        const std::string &_res = obj.asString();
        sscanf(_res.c_str(), "%u", &o);
    }
    else
    {
        o = obj.asUInt();
    }
}

static void parse_VkPipelineTessellationStateCreateFlags(const Json::Value &obj,
                                                         VkPipelineTessellationStateCreateFlags &o)
{
    if (obj.isString())
    {
        const std::string &_res = obj.asString();
        sscanf(_res.c_str(), "%u", &o);
    }
    else
    {
        o = obj.asUInt();
    }
}

static void parse_VkPipelineViewportStateCreateFlags(const Json::Value &obj, VkPipelineViewportStateCreateFlags &o)
{
    if (obj.isString())
    {
        const std::string &_res = obj.asString();
        sscanf(_res.c_str(), "%u", &o);
    }
    else
    {
        o = obj.asUInt();
    }
}

static void parse_VkPipelineRasterizationStateCreateFlags(const Json::Value &obj,
                                                          VkPipelineRasterizationStateCreateFlags &o)
{
    if (obj.isString())
    {
        const std::string &_res = obj.asString();
        sscanf(_res.c_str(), "%u", &o);
    }
    else
    {
        o = obj.asUInt();
    }
}

static void parse_VkPipelineMultisampleStateCreateFlags(const Json::Value &obj,
                                                        VkPipelineMultisampleStateCreateFlags &o)
{
    if (obj.isString())
    {
        const std::string &_res = obj.asString();
        sscanf(_res.c_str(), "%u", &o);
    }
    else
    {
        o = obj.asUInt();
    }
}

static void parse_VkPipelineDepthStencilStateCreateFlags(const Json::Value &obj,
                                                         VkPipelineDepthStencilStateCreateFlags &o)
{
    if (obj.isString())
    {
        const std::string &_res = obj.asString();
        sscanf(_res.c_str(), "%u", &o);
    }
    else
    {
        o = obj.asUInt();
    }
}

static void parse_VkPipelineColorBlendStateCreateFlags(const Json::Value &obj, VkPipelineColorBlendStateCreateFlags &o)
{
    if (obj.isString())
    {
        const std::string &_res = obj.asString();
        sscanf(_res.c_str(), "%u", &o);
    }
    else
    {
        o = obj.asUInt();
    }
}

static void parse_VkPipelineDynamicStateCreateFlags(const Json::Value &obj, VkPipelineDynamicStateCreateFlags &o)
{
    if (obj.isString())
    {
        const std::string &_res = obj.asString();
        sscanf(_res.c_str(), "%u", &o);
    }
    else
    {
        o = obj.asUInt();
    }
}

static void parse_VkPipelineLayoutCreateFlags(const Json::Value &obj, VkPipelineLayoutCreateFlags &o)
{
    o                       = (VkPipelineLayoutCreateFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkPipelineLayoutCreateFlagBits)VkPipelineLayoutCreateFlagBits_map[it];
    }
}

static void parse_VkShaderStageFlags(const Json::Value &obj, VkShaderStageFlags &o)
{
    o                       = (VkShaderStageFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkShaderStageFlagBits)VkShaderStageFlagBits_map[it];
    }
}

static void parse_VkSamplerCreateFlags(const Json::Value &obj, VkSamplerCreateFlags &o)
{
    o                       = (VkSamplerCreateFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkSamplerCreateFlagBits)VkSamplerCreateFlagBits_map[it];
    }
}

static void parse_VkDescriptorPoolCreateFlags(const Json::Value &obj, VkDescriptorPoolCreateFlags &o)
{
    o                       = (VkDescriptorPoolCreateFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkDescriptorPoolCreateFlagBits)VkDescriptorPoolCreateFlagBits_map[it];
    }
}

static void parse_VkDescriptorPoolResetFlags(const Json::Value &obj, VkDescriptorPoolResetFlags &o)
{
    if (obj.isString())
    {
        const std::string &_res = obj.asString();
        sscanf(_res.c_str(), "%u", &o);
    }
    else
    {
        o = obj.asUInt();
    }
}

static void parse_VkDescriptorSetLayoutCreateFlags(const Json::Value &obj, VkDescriptorSetLayoutCreateFlags &o)
{
    o                       = (VkDescriptorSetLayoutCreateFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkDescriptorSetLayoutCreateFlagBits)VkDescriptorSetLayoutCreateFlagBits_map[it];
    }
}

static void parse_VkAttachmentDescriptionFlags(const Json::Value &obj, VkAttachmentDescriptionFlags &o)
{
    o                       = (VkAttachmentDescriptionFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkAttachmentDescriptionFlagBits)VkAttachmentDescriptionFlagBits_map[it];
    }
}

static void parse_VkDependencyFlags(const Json::Value &obj, VkDependencyFlags &o)
{
    o                       = (VkDependencyFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkDependencyFlagBits)VkDependencyFlagBits_map[it];
    }
}

static void parse_VkFramebufferCreateFlags(const Json::Value &obj, VkFramebufferCreateFlags &o)
{
    o                       = (VkFramebufferCreateFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkFramebufferCreateFlagBits)VkFramebufferCreateFlagBits_map[it];
    }
}

static void parse_VkRenderPassCreateFlags(const Json::Value &obj, VkRenderPassCreateFlags &o)
{
    o                       = (VkRenderPassCreateFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkRenderPassCreateFlagBits)VkRenderPassCreateFlagBits_map[it];
    }
}

static void parse_VkSubpassDescriptionFlags(const Json::Value &obj, VkSubpassDescriptionFlags &o)
{
    o                       = (VkSubpassDescriptionFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkSubpassDescriptionFlagBits)VkSubpassDescriptionFlagBits_map[it];
    }
}

static void parse_VkCommandPoolCreateFlags(const Json::Value &obj, VkCommandPoolCreateFlags &o)
{
    o                       = (VkCommandPoolCreateFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkCommandPoolCreateFlagBits)VkCommandPoolCreateFlagBits_map[it];
    }
}

static void parse_VkCommandPoolResetFlags(const Json::Value &obj, VkCommandPoolResetFlags &o)
{
    o                       = (VkCommandPoolResetFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkCommandPoolResetFlagBits)VkCommandPoolResetFlagBits_map[it];
    }
}

static void parse_VkCommandBufferUsageFlags(const Json::Value &obj, VkCommandBufferUsageFlags &o)
{
    o                       = (VkCommandBufferUsageFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkCommandBufferUsageFlagBits)VkCommandBufferUsageFlagBits_map[it];
    }
}

static void parse_VkQueryControlFlags(const Json::Value &obj, VkQueryControlFlags &o)
{
    o                       = (VkQueryControlFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkQueryControlFlagBits)VkQueryControlFlagBits_map[it];
    }
}

static void parse_VkCommandBufferResetFlags(const Json::Value &obj, VkCommandBufferResetFlags &o)
{
    o                       = (VkCommandBufferResetFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkCommandBufferResetFlagBits)VkCommandBufferResetFlagBits_map[it];
    }
}

static void parse_VkStencilFaceFlags(const Json::Value &obj, VkStencilFaceFlags &o)
{
    o                       = (VkStencilFaceFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkStencilFaceFlagBits)VkStencilFaceFlagBits_map[it];
    }
}

static void parse_VkExtent2D(const Json::Value &obj, VkExtent2D &o)
{

    parse_uint32_t(obj["width"], (o.width));

    parse_uint32_t(obj["height"], (o.height));
}

static void parse_VkExtent3D(const Json::Value &obj, VkExtent3D &o)
{

    parse_uint32_t(obj["width"], (o.width));

    parse_uint32_t(obj["height"], (o.height));

    parse_uint32_t(obj["depth"], (o.depth));
}

static void parse_VkOffset2D(const Json::Value &obj, VkOffset2D &o)
{

    parse_int32_t(obj["x"], (o.x));

    parse_int32_t(obj["y"], (o.y));
}

static void parse_VkOffset3D(const Json::Value &obj, VkOffset3D &o)
{

    parse_int32_t(obj["x"], (o.x));

    parse_int32_t(obj["y"], (o.y));

    parse_int32_t(obj["z"], (o.z));
}

static void parse_VkRect2D(const Json::Value &obj, VkRect2D &o)
{

    parse_VkOffset2D(obj["offset"], (o.offset));

    parse_VkExtent2D(obj["extent"], (o.extent));
}

static void parse_VkBaseInStructure(const Json::Value &obj, VkBaseInStructure &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkBaseInStructure *)parsePNextChain(obj);
}

static void parse_VkBaseOutStructure(const Json::Value &obj, VkBaseOutStructure &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkBaseOutStructure *)parsePNextChain(obj);
}

static void parse_VkBufferMemoryBarrier(const Json::Value &obj, VkBufferMemoryBarrier &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkBufferMemoryBarrier *)parsePNextChain(obj);

    parse_VkAccessFlags(obj["srcAccessMask"], (o.srcAccessMask));

    parse_VkAccessFlags(obj["dstAccessMask"], (o.dstAccessMask));

    parse_uint32_t(obj["srcQueueFamilyIndex"], (o.srcQueueFamilyIndex));

    parse_uint32_t(obj["dstQueueFamilyIndex"], (o.dstQueueFamilyIndex));

    parse_VkDeviceSize(obj["offset"], (o.offset));

    parse_VkDeviceSize(obj["size"], (o.size));
}

static void parse_VkDispatchIndirectCommand(const Json::Value &obj, VkDispatchIndirectCommand &o)
{

    parse_uint32_t(obj["x"], (o.x));

    parse_uint32_t(obj["y"], (o.y));

    parse_uint32_t(obj["z"], (o.z));
}

static void parse_VkDrawIndexedIndirectCommand(const Json::Value &obj, VkDrawIndexedIndirectCommand &o)
{

    parse_uint32_t(obj["indexCount"], (o.indexCount));

    parse_uint32_t(obj["instanceCount"], (o.instanceCount));

    parse_uint32_t(obj["firstIndex"], (o.firstIndex));

    parse_int32_t(obj["vertexOffset"], (o.vertexOffset));

    parse_uint32_t(obj["firstInstance"], (o.firstInstance));
}

static void parse_VkDrawIndirectCommand(const Json::Value &obj, VkDrawIndirectCommand &o)
{

    parse_uint32_t(obj["vertexCount"], (o.vertexCount));

    parse_uint32_t(obj["instanceCount"], (o.instanceCount));

    parse_uint32_t(obj["firstVertex"], (o.firstVertex));

    parse_uint32_t(obj["firstInstance"], (o.firstInstance));
}

static void parse_VkImageSubresourceRange(const Json::Value &obj, VkImageSubresourceRange &o)
{

    parse_VkImageAspectFlags(obj["aspectMask"], (o.aspectMask));

    parse_uint32_t(obj["baseMipLevel"], (o.baseMipLevel));

    parse_uint32_t(obj["levelCount"], (o.levelCount));

    parse_uint32_t(obj["baseArrayLayer"], (o.baseArrayLayer));

    parse_uint32_t(obj["layerCount"], (o.layerCount));
}

static void parse_VkImageMemoryBarrier(const Json::Value &obj, VkImageMemoryBarrier &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkImageMemoryBarrier *)parsePNextChain(obj);

    parse_VkAccessFlags(obj["srcAccessMask"], (o.srcAccessMask));

    parse_VkAccessFlags(obj["dstAccessMask"], (o.dstAccessMask));

    parse_VkImageLayout(obj["oldLayout"], (o.oldLayout));

    parse_VkImageLayout(obj["newLayout"], (o.newLayout));

    parse_uint32_t(obj["srcQueueFamilyIndex"], (o.srcQueueFamilyIndex));

    parse_uint32_t(obj["dstQueueFamilyIndex"], (o.dstQueueFamilyIndex));

    parse_VkImageSubresourceRange(obj["subresourceRange"], (o.subresourceRange));
}

static void parse_VkMemoryBarrier(const Json::Value &obj, VkMemoryBarrier &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkMemoryBarrier *)parsePNextChain(obj);

    parse_VkAccessFlags(obj["srcAccessMask"], (o.srcAccessMask));

    parse_VkAccessFlags(obj["dstAccessMask"], (o.dstAccessMask));
}

static void parse_VkPipelineCacheHeaderVersionOne(const Json::Value &obj, VkPipelineCacheHeaderVersionOne &o)
{

    parse_uint32_t(obj["headerSize"], (o.headerSize));

    parse_VkPipelineCacheHeaderVersion(obj["headerVersion"], (o.headerVersion));

    parse_uint32_t(obj["vendorID"], (o.vendorID));

    parse_uint32_t(obj["deviceID"], (o.deviceID));

    const Json::Value &obj_pipelineCacheUUID_arr = obj["pipelineCacheUUID"];
    for (unsigned int i = 0; i < obj_pipelineCacheUUID_arr.size(); i++)
    {
        parse_uint8_t(obj_pipelineCacheUUID_arr[i], const_cast<uint8_t &>((o.pipelineCacheUUID[i])));
    }
}

static void parse_VkAllocationCallbacks(const Json::Value &obj, VkAllocationCallbacks &o)
{

    /** Note: Ignoring function pointer (PFN_vkAllocationFunction). **/

    /** Note: Ignoring function pointer (PFN_vkReallocationFunction). **/

    /** Note: Ignoring function pointer (PFN_vkFreeFunction). **/

    /** Note: Ignoring function pointer (PFN_vkInternalAllocationNotification). **/

    /** Note: Ignoring function pointer (PFN_vkInternalFreeNotification). **/
}

static void parse_VkApplicationInfo(const Json::Value &obj, VkApplicationInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkApplicationInfo *)parsePNextChain(obj);

    /** TODO: Handle this - pApplicationName **/

    parse_uint32_t(obj["applicationVersion"], (o.applicationVersion));

    /** TODO: Handle this - pEngineName **/

    parse_uint32_t(obj["engineVersion"], (o.engineVersion));

    parse_uint32_t(obj["apiVersion"], (o.apiVersion));
}

static void parse_VkFormatProperties(const Json::Value &obj, VkFormatProperties &o)
{

    parse_VkFormatFeatureFlags(obj["linearTilingFeatures"], (o.linearTilingFeatures));

    parse_VkFormatFeatureFlags(obj["optimalTilingFeatures"], (o.optimalTilingFeatures));

    parse_VkFormatFeatureFlags(obj["bufferFeatures"], (o.bufferFeatures));
}

static void parse_VkImageFormatProperties(const Json::Value &obj, VkImageFormatProperties &o)
{

    parse_VkExtent3D(obj["maxExtent"], (o.maxExtent));

    parse_uint32_t(obj["maxMipLevels"], (o.maxMipLevels));

    parse_uint32_t(obj["maxArrayLayers"], (o.maxArrayLayers));

    parse_VkSampleCountFlags(obj["sampleCounts"], (o.sampleCounts));

    parse_VkDeviceSize(obj["maxResourceSize"], (o.maxResourceSize));
}

static void parse_VkInstanceCreateInfo(const Json::Value &obj, VkInstanceCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkInstanceCreateInfo *)parsePNextChain(obj);

    parse_VkInstanceCreateFlags(obj["flags"], (o.flags));

    {
        const Json::Value &obj_pApplicationInfo = obj["pApplicationInfo"];
        if (obj_pApplicationInfo.size() == 0)
        {
            (o.pApplicationInfo) = nullptr;
        }
        else
        {
            (o.pApplicationInfo) = (VkApplicationInfo *)s_globalMem.allocate(1, sizeof(VkApplicationInfo));
            parse_VkApplicationInfo(obj_pApplicationInfo, const_cast<VkApplicationInfo &>(*(o.pApplicationInfo)));
        }
    }

    parse_uint32_t(obj["enabledLayerCount"], (o.enabledLayerCount));

    /** TODO: Handle this - ppEnabledLayerNames **/

    parse_uint32_t(obj["enabledExtensionCount"], (o.enabledExtensionCount));

    /** TODO: Handle this - ppEnabledExtensionNames **/
}

static void parse_VkMemoryHeap(const Json::Value &obj, VkMemoryHeap &o)
{

    parse_VkDeviceSize(obj["size"], (o.size));

    parse_VkMemoryHeapFlags(obj["flags"], (o.flags));
}

static void parse_VkMemoryType(const Json::Value &obj, VkMemoryType &o)
{

    parse_VkMemoryPropertyFlags(obj["propertyFlags"], (o.propertyFlags));

    parse_uint32_t(obj["heapIndex"], (o.heapIndex));
}

static void parse_VkPhysicalDeviceFeatures(const Json::Value &obj, VkPhysicalDeviceFeatures &o)
{

    parse_VkBool32(obj["robustBufferAccess"], (o.robustBufferAccess));

    parse_VkBool32(obj["fullDrawIndexUint32"], (o.fullDrawIndexUint32));

    parse_VkBool32(obj["imageCubeArray"], (o.imageCubeArray));

    parse_VkBool32(obj["independentBlend"], (o.independentBlend));

    parse_VkBool32(obj["geometryShader"], (o.geometryShader));

    parse_VkBool32(obj["tessellationShader"], (o.tessellationShader));

    parse_VkBool32(obj["sampleRateShading"], (o.sampleRateShading));

    parse_VkBool32(obj["dualSrcBlend"], (o.dualSrcBlend));

    parse_VkBool32(obj["logicOp"], (o.logicOp));

    parse_VkBool32(obj["multiDrawIndirect"], (o.multiDrawIndirect));

    parse_VkBool32(obj["drawIndirectFirstInstance"], (o.drawIndirectFirstInstance));

    parse_VkBool32(obj["depthClamp"], (o.depthClamp));

    parse_VkBool32(obj["depthBiasClamp"], (o.depthBiasClamp));

    parse_VkBool32(obj["fillModeNonSolid"], (o.fillModeNonSolid));

    parse_VkBool32(obj["depthBounds"], (o.depthBounds));

    parse_VkBool32(obj["wideLines"], (o.wideLines));

    parse_VkBool32(obj["largePoints"], (o.largePoints));

    parse_VkBool32(obj["alphaToOne"], (o.alphaToOne));

    parse_VkBool32(obj["multiViewport"], (o.multiViewport));

    parse_VkBool32(obj["samplerAnisotropy"], (o.samplerAnisotropy));

    parse_VkBool32(obj["textureCompressionETC2"], (o.textureCompressionETC2));

    parse_VkBool32(obj["textureCompressionASTC_LDR"], (o.textureCompressionASTC_LDR));

    parse_VkBool32(obj["textureCompressionBC"], (o.textureCompressionBC));

    parse_VkBool32(obj["occlusionQueryPrecise"], (o.occlusionQueryPrecise));

    parse_VkBool32(obj["pipelineStatisticsQuery"], (o.pipelineStatisticsQuery));

    parse_VkBool32(obj["vertexPipelineStoresAndAtomics"], (o.vertexPipelineStoresAndAtomics));

    parse_VkBool32(obj["fragmentStoresAndAtomics"], (o.fragmentStoresAndAtomics));

    parse_VkBool32(obj["shaderTessellationAndGeometryPointSize"], (o.shaderTessellationAndGeometryPointSize));

    parse_VkBool32(obj["shaderImageGatherExtended"], (o.shaderImageGatherExtended));

    parse_VkBool32(obj["shaderStorageImageExtendedFormats"], (o.shaderStorageImageExtendedFormats));

    parse_VkBool32(obj["shaderStorageImageMultisample"], (o.shaderStorageImageMultisample));

    parse_VkBool32(obj["shaderStorageImageReadWithoutFormat"], (o.shaderStorageImageReadWithoutFormat));

    parse_VkBool32(obj["shaderStorageImageWriteWithoutFormat"], (o.shaderStorageImageWriteWithoutFormat));

    parse_VkBool32(obj["shaderUniformBufferArrayDynamicIndexing"], (o.shaderUniformBufferArrayDynamicIndexing));

    parse_VkBool32(obj["shaderSampledImageArrayDynamicIndexing"], (o.shaderSampledImageArrayDynamicIndexing));

    parse_VkBool32(obj["shaderStorageBufferArrayDynamicIndexing"], (o.shaderStorageBufferArrayDynamicIndexing));

    parse_VkBool32(obj["shaderStorageImageArrayDynamicIndexing"], (o.shaderStorageImageArrayDynamicIndexing));

    parse_VkBool32(obj["shaderClipDistance"], (o.shaderClipDistance));

    parse_VkBool32(obj["shaderCullDistance"], (o.shaderCullDistance));

    parse_VkBool32(obj["shaderFloat64"], (o.shaderFloat64));

    parse_VkBool32(obj["shaderInt64"], (o.shaderInt64));

    parse_VkBool32(obj["shaderInt16"], (o.shaderInt16));

    parse_VkBool32(obj["shaderResourceResidency"], (o.shaderResourceResidency));

    parse_VkBool32(obj["shaderResourceMinLod"], (o.shaderResourceMinLod));

    parse_VkBool32(obj["sparseBinding"], (o.sparseBinding));

    parse_VkBool32(obj["sparseResidencyBuffer"], (o.sparseResidencyBuffer));

    parse_VkBool32(obj["sparseResidencyImage2D"], (o.sparseResidencyImage2D));

    parse_VkBool32(obj["sparseResidencyImage3D"], (o.sparseResidencyImage3D));

    parse_VkBool32(obj["sparseResidency2Samples"], (o.sparseResidency2Samples));

    parse_VkBool32(obj["sparseResidency4Samples"], (o.sparseResidency4Samples));

    parse_VkBool32(obj["sparseResidency8Samples"], (o.sparseResidency8Samples));

    parse_VkBool32(obj["sparseResidency16Samples"], (o.sparseResidency16Samples));

    parse_VkBool32(obj["sparseResidencyAliased"], (o.sparseResidencyAliased));

    parse_VkBool32(obj["variableMultisampleRate"], (o.variableMultisampleRate));

    parse_VkBool32(obj["inheritedQueries"], (o.inheritedQueries));
}

static void parse_VkPhysicalDeviceLimits(const Json::Value &obj, VkPhysicalDeviceLimits &o)
{

    parse_uint32_t(obj["maxImageDimension1D"], (o.maxImageDimension1D));

    parse_uint32_t(obj["maxImageDimension2D"], (o.maxImageDimension2D));

    parse_uint32_t(obj["maxImageDimension3D"], (o.maxImageDimension3D));

    parse_uint32_t(obj["maxImageDimensionCube"], (o.maxImageDimensionCube));

    parse_uint32_t(obj["maxImageArrayLayers"], (o.maxImageArrayLayers));

    parse_uint32_t(obj["maxTexelBufferElements"], (o.maxTexelBufferElements));

    parse_uint32_t(obj["maxUniformBufferRange"], (o.maxUniformBufferRange));

    parse_uint32_t(obj["maxStorageBufferRange"], (o.maxStorageBufferRange));

    parse_uint32_t(obj["maxPushConstantsSize"], (o.maxPushConstantsSize));

    parse_uint32_t(obj["maxMemoryAllocationCount"], (o.maxMemoryAllocationCount));

    parse_uint32_t(obj["maxSamplerAllocationCount"], (o.maxSamplerAllocationCount));

    parse_VkDeviceSize(obj["bufferImageGranularity"], (o.bufferImageGranularity));

    parse_VkDeviceSize(obj["sparseAddressSpaceSize"], (o.sparseAddressSpaceSize));

    parse_uint32_t(obj["maxBoundDescriptorSets"], (o.maxBoundDescriptorSets));

    parse_uint32_t(obj["maxPerStageDescriptorSamplers"], (o.maxPerStageDescriptorSamplers));

    parse_uint32_t(obj["maxPerStageDescriptorUniformBuffers"], (o.maxPerStageDescriptorUniformBuffers));

    parse_uint32_t(obj["maxPerStageDescriptorStorageBuffers"], (o.maxPerStageDescriptorStorageBuffers));

    parse_uint32_t(obj["maxPerStageDescriptorSampledImages"], (o.maxPerStageDescriptorSampledImages));

    parse_uint32_t(obj["maxPerStageDescriptorStorageImages"], (o.maxPerStageDescriptorStorageImages));

    parse_uint32_t(obj["maxPerStageDescriptorInputAttachments"], (o.maxPerStageDescriptorInputAttachments));

    parse_uint32_t(obj["maxPerStageResources"], (o.maxPerStageResources));

    parse_uint32_t(obj["maxDescriptorSetSamplers"], (o.maxDescriptorSetSamplers));

    parse_uint32_t(obj["maxDescriptorSetUniformBuffers"], (o.maxDescriptorSetUniformBuffers));

    parse_uint32_t(obj["maxDescriptorSetUniformBuffersDynamic"], (o.maxDescriptorSetUniformBuffersDynamic));

    parse_uint32_t(obj["maxDescriptorSetStorageBuffers"], (o.maxDescriptorSetStorageBuffers));

    parse_uint32_t(obj["maxDescriptorSetStorageBuffersDynamic"], (o.maxDescriptorSetStorageBuffersDynamic));

    parse_uint32_t(obj["maxDescriptorSetSampledImages"], (o.maxDescriptorSetSampledImages));

    parse_uint32_t(obj["maxDescriptorSetStorageImages"], (o.maxDescriptorSetStorageImages));

    parse_uint32_t(obj["maxDescriptorSetInputAttachments"], (o.maxDescriptorSetInputAttachments));

    parse_uint32_t(obj["maxVertexInputAttributes"], (o.maxVertexInputAttributes));

    parse_uint32_t(obj["maxVertexInputBindings"], (o.maxVertexInputBindings));

    parse_uint32_t(obj["maxVertexInputAttributeOffset"], (o.maxVertexInputAttributeOffset));

    parse_uint32_t(obj["maxVertexInputBindingStride"], (o.maxVertexInputBindingStride));

    parse_uint32_t(obj["maxVertexOutputComponents"], (o.maxVertexOutputComponents));

    parse_uint32_t(obj["maxTessellationGenerationLevel"], (o.maxTessellationGenerationLevel));

    parse_uint32_t(obj["maxTessellationPatchSize"], (o.maxTessellationPatchSize));

    parse_uint32_t(obj["maxTessellationControlPerVertexInputComponents"],
                   (o.maxTessellationControlPerVertexInputComponents));

    parse_uint32_t(obj["maxTessellationControlPerVertexOutputComponents"],
                   (o.maxTessellationControlPerVertexOutputComponents));

    parse_uint32_t(obj["maxTessellationControlPerPatchOutputComponents"],
                   (o.maxTessellationControlPerPatchOutputComponents));

    parse_uint32_t(obj["maxTessellationControlTotalOutputComponents"], (o.maxTessellationControlTotalOutputComponents));

    parse_uint32_t(obj["maxTessellationEvaluationInputComponents"], (o.maxTessellationEvaluationInputComponents));

    parse_uint32_t(obj["maxTessellationEvaluationOutputComponents"], (o.maxTessellationEvaluationOutputComponents));

    parse_uint32_t(obj["maxGeometryShaderInvocations"], (o.maxGeometryShaderInvocations));

    parse_uint32_t(obj["maxGeometryInputComponents"], (o.maxGeometryInputComponents));

    parse_uint32_t(obj["maxGeometryOutputComponents"], (o.maxGeometryOutputComponents));

    parse_uint32_t(obj["maxGeometryOutputVertices"], (o.maxGeometryOutputVertices));

    parse_uint32_t(obj["maxGeometryTotalOutputComponents"], (o.maxGeometryTotalOutputComponents));

    parse_uint32_t(obj["maxFragmentInputComponents"], (o.maxFragmentInputComponents));

    parse_uint32_t(obj["maxFragmentOutputAttachments"], (o.maxFragmentOutputAttachments));

    parse_uint32_t(obj["maxFragmentDualSrcAttachments"], (o.maxFragmentDualSrcAttachments));

    parse_uint32_t(obj["maxFragmentCombinedOutputResources"], (o.maxFragmentCombinedOutputResources));

    parse_uint32_t(obj["maxComputeSharedMemorySize"], (o.maxComputeSharedMemorySize));

    const Json::Value &obj_maxComputeWorkGroupCount_arr = obj["maxComputeWorkGroupCount"];
    for (unsigned int i = 0; i < obj_maxComputeWorkGroupCount_arr.size(); i++)
    {
        parse_uint32_t(obj_maxComputeWorkGroupCount_arr[i], const_cast<uint32_t &>((o.maxComputeWorkGroupCount[i])));
    }

    parse_uint32_t(obj["maxComputeWorkGroupInvocations"], (o.maxComputeWorkGroupInvocations));

    const Json::Value &obj_maxComputeWorkGroupSize_arr = obj["maxComputeWorkGroupSize"];
    for (unsigned int i = 0; i < obj_maxComputeWorkGroupSize_arr.size(); i++)
    {
        parse_uint32_t(obj_maxComputeWorkGroupSize_arr[i], const_cast<uint32_t &>((o.maxComputeWorkGroupSize[i])));
    }

    parse_uint32_t(obj["subPixelPrecisionBits"], (o.subPixelPrecisionBits));

    parse_uint32_t(obj["subTexelPrecisionBits"], (o.subTexelPrecisionBits));

    parse_uint32_t(obj["mipmapPrecisionBits"], (o.mipmapPrecisionBits));

    parse_uint32_t(obj["maxDrawIndexedIndexValue"], (o.maxDrawIndexedIndexValue));

    parse_uint32_t(obj["maxDrawIndirectCount"], (o.maxDrawIndirectCount));

    parse_float(obj["maxSamplerLodBias"], (o.maxSamplerLodBias));

    parse_float(obj["maxSamplerAnisotropy"], (o.maxSamplerAnisotropy));

    parse_uint32_t(obj["maxViewports"], (o.maxViewports));

    const Json::Value &obj_maxViewportDimensions_arr = obj["maxViewportDimensions"];
    for (unsigned int i = 0; i < obj_maxViewportDimensions_arr.size(); i++)
    {
        parse_uint32_t(obj_maxViewportDimensions_arr[i], const_cast<uint32_t &>((o.maxViewportDimensions[i])));
    }

    const Json::Value &obj_viewportBoundsRange_arr = obj["viewportBoundsRange"];
    for (unsigned int i = 0; i < obj_viewportBoundsRange_arr.size(); i++)
    {
        parse_float(obj_viewportBoundsRange_arr[i], const_cast<float &>((o.viewportBoundsRange[i])));
    }

    parse_uint32_t(obj["viewportSubPixelBits"], (o.viewportSubPixelBits));

    parse_size_t(obj["minMemoryMapAlignment"], (o.minMemoryMapAlignment));

    parse_VkDeviceSize(obj["minTexelBufferOffsetAlignment"], (o.minTexelBufferOffsetAlignment));

    parse_VkDeviceSize(obj["minUniformBufferOffsetAlignment"], (o.minUniformBufferOffsetAlignment));

    parse_VkDeviceSize(obj["minStorageBufferOffsetAlignment"], (o.minStorageBufferOffsetAlignment));

    parse_int32_t(obj["minTexelOffset"], (o.minTexelOffset));

    parse_uint32_t(obj["maxTexelOffset"], (o.maxTexelOffset));

    parse_int32_t(obj["minTexelGatherOffset"], (o.minTexelGatherOffset));

    parse_uint32_t(obj["maxTexelGatherOffset"], (o.maxTexelGatherOffset));

    parse_float(obj["minInterpolationOffset"], (o.minInterpolationOffset));

    parse_float(obj["maxInterpolationOffset"], (o.maxInterpolationOffset));

    parse_uint32_t(obj["subPixelInterpolationOffsetBits"], (o.subPixelInterpolationOffsetBits));

    parse_uint32_t(obj["maxFramebufferWidth"], (o.maxFramebufferWidth));

    parse_uint32_t(obj["maxFramebufferHeight"], (o.maxFramebufferHeight));

    parse_uint32_t(obj["maxFramebufferLayers"], (o.maxFramebufferLayers));

    parse_VkSampleCountFlags(obj["framebufferColorSampleCounts"], (o.framebufferColorSampleCounts));

    parse_VkSampleCountFlags(obj["framebufferDepthSampleCounts"], (o.framebufferDepthSampleCounts));

    parse_VkSampleCountFlags(obj["framebufferStencilSampleCounts"], (o.framebufferStencilSampleCounts));

    parse_VkSampleCountFlags(obj["framebufferNoAttachmentsSampleCounts"], (o.framebufferNoAttachmentsSampleCounts));

    parse_uint32_t(obj["maxColorAttachments"], (o.maxColorAttachments));

    parse_VkSampleCountFlags(obj["sampledImageColorSampleCounts"], (o.sampledImageColorSampleCounts));

    parse_VkSampleCountFlags(obj["sampledImageIntegerSampleCounts"], (o.sampledImageIntegerSampleCounts));

    parse_VkSampleCountFlags(obj["sampledImageDepthSampleCounts"], (o.sampledImageDepthSampleCounts));

    parse_VkSampleCountFlags(obj["sampledImageStencilSampleCounts"], (o.sampledImageStencilSampleCounts));

    parse_VkSampleCountFlags(obj["storageImageSampleCounts"], (o.storageImageSampleCounts));

    parse_uint32_t(obj["maxSampleMaskWords"], (o.maxSampleMaskWords));

    parse_VkBool32(obj["timestampComputeAndGraphics"], (o.timestampComputeAndGraphics));

    parse_float(obj["timestampPeriod"], (o.timestampPeriod));

    parse_uint32_t(obj["maxClipDistances"], (o.maxClipDistances));

    parse_uint32_t(obj["maxCullDistances"], (o.maxCullDistances));

    parse_uint32_t(obj["maxCombinedClipAndCullDistances"], (o.maxCombinedClipAndCullDistances));

    parse_uint32_t(obj["discreteQueuePriorities"], (o.discreteQueuePriorities));

    const Json::Value &obj_pointSizeRange_arr = obj["pointSizeRange"];
    for (unsigned int i = 0; i < obj_pointSizeRange_arr.size(); i++)
    {
        parse_float(obj_pointSizeRange_arr[i], const_cast<float &>((o.pointSizeRange[i])));
    }

    const Json::Value &obj_lineWidthRange_arr = obj["lineWidthRange"];
    for (unsigned int i = 0; i < obj_lineWidthRange_arr.size(); i++)
    {
        parse_float(obj_lineWidthRange_arr[i], const_cast<float &>((o.lineWidthRange[i])));
    }

    parse_float(obj["pointSizeGranularity"], (o.pointSizeGranularity));

    parse_float(obj["lineWidthGranularity"], (o.lineWidthGranularity));

    parse_VkBool32(obj["strictLines"], (o.strictLines));

    parse_VkBool32(obj["standardSampleLocations"], (o.standardSampleLocations));

    parse_VkDeviceSize(obj["optimalBufferCopyOffsetAlignment"], (o.optimalBufferCopyOffsetAlignment));

    parse_VkDeviceSize(obj["optimalBufferCopyRowPitchAlignment"], (o.optimalBufferCopyRowPitchAlignment));

    parse_VkDeviceSize(obj["nonCoherentAtomSize"], (o.nonCoherentAtomSize));
}

static void parse_VkPhysicalDeviceMemoryProperties(const Json::Value &obj, VkPhysicalDeviceMemoryProperties &o)
{

    parse_uint32_t(obj["memoryTypeCount"], (o.memoryTypeCount));

    const Json::Value &obj_memoryTypes_arr = obj["memoryTypes"];
    for (unsigned int i = 0; i < obj_memoryTypes_arr.size(); i++)
    {
        parse_VkMemoryType(obj_memoryTypes_arr[i], const_cast<VkMemoryType &>((o.memoryTypes[i])));
    }

    parse_uint32_t(obj["memoryHeapCount"], (o.memoryHeapCount));

    const Json::Value &obj_memoryHeaps_arr = obj["memoryHeaps"];
    for (unsigned int i = 0; i < obj_memoryHeaps_arr.size(); i++)
    {
        parse_VkMemoryHeap(obj_memoryHeaps_arr[i], const_cast<VkMemoryHeap &>((o.memoryHeaps[i])));
    }
}

static void parse_VkPhysicalDeviceSparseProperties(const Json::Value &obj, VkPhysicalDeviceSparseProperties &o)
{

    parse_VkBool32(obj["residencyStandard2DBlockShape"], (o.residencyStandard2DBlockShape));

    parse_VkBool32(obj["residencyStandard2DMultisampleBlockShape"], (o.residencyStandard2DMultisampleBlockShape));

    parse_VkBool32(obj["residencyStandard3DBlockShape"], (o.residencyStandard3DBlockShape));

    parse_VkBool32(obj["residencyAlignedMipSize"], (o.residencyAlignedMipSize));

    parse_VkBool32(obj["residencyNonResidentStrict"], (o.residencyNonResidentStrict));
}

static void parse_VkPhysicalDeviceProperties(const Json::Value &obj, VkPhysicalDeviceProperties &o)
{

    parse_uint32_t(obj["apiVersion"], (o.apiVersion));

    parse_uint32_t(obj["driverVersion"], (o.driverVersion));

    parse_uint32_t(obj["vendorID"], (o.vendorID));

    parse_uint32_t(obj["deviceID"], (o.deviceID));

    parse_VkPhysicalDeviceType(obj["deviceType"], (o.deviceType));

    /** TODO: Handle this - deviceName **/

    const Json::Value &obj_pipelineCacheUUID_arr = obj["pipelineCacheUUID"];
    for (unsigned int i = 0; i < obj_pipelineCacheUUID_arr.size(); i++)
    {
        parse_uint8_t(obj_pipelineCacheUUID_arr[i], const_cast<uint8_t &>((o.pipelineCacheUUID[i])));
    }

    parse_VkPhysicalDeviceLimits(obj["limits"], (o.limits));

    parse_VkPhysicalDeviceSparseProperties(obj["sparseProperties"], (o.sparseProperties));
}

static void parse_VkQueueFamilyProperties(const Json::Value &obj, VkQueueFamilyProperties &o)
{

    parse_VkQueueFlags(obj["queueFlags"], (o.queueFlags));

    parse_uint32_t(obj["queueCount"], (o.queueCount));

    parse_uint32_t(obj["timestampValidBits"], (o.timestampValidBits));

    parse_VkExtent3D(obj["minImageTransferGranularity"], (o.minImageTransferGranularity));
}

static void parse_VkDeviceQueueCreateInfo(const Json::Value &obj, VkDeviceQueueCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkDeviceQueueCreateInfo *)parsePNextChain(obj);

    parse_VkDeviceQueueCreateFlags(obj["flags"], (o.flags));

    parse_uint32_t(obj["queueFamilyIndex"], (o.queueFamilyIndex));

    parse_uint32_t(obj["queueCount"], (o.queueCount));

    (o.pQueuePriorities)                        = (float *)s_globalMem.allocate((o.queueCount), sizeof(float));
    const Json::Value &obj_pQueuePriorities_arr = obj["pQueuePriorities"];
    for (unsigned int i = 0; i < obj_pQueuePriorities_arr.size(); i++)
    {
        parse_float(obj_pQueuePriorities_arr[i], const_cast<float &>((o.pQueuePriorities[i])));
    }
}

static void parse_VkDeviceCreateInfo(const Json::Value &obj, VkDeviceCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkDeviceCreateInfo *)parsePNextChain(obj);

    parse_VkDeviceCreateFlags(obj["flags"], (o.flags));

    parse_uint32_t(obj["queueCreateInfoCount"], (o.queueCreateInfoCount));

    (o.pQueueCreateInfos) =
        (VkDeviceQueueCreateInfo *)s_globalMem.allocate((o.queueCreateInfoCount), sizeof(VkDeviceQueueCreateInfo));
    const Json::Value &obj_pQueueCreateInfos = obj["pQueueCreateInfos"];
    if (obj_pQueueCreateInfos.size() == 0)
        (o.pQueueCreateInfos) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.queueCreateInfoCount); i++)
        {
            parse_VkDeviceQueueCreateInfo(obj_pQueueCreateInfos[i],
                                          const_cast<VkDeviceQueueCreateInfo &>((o.pQueueCreateInfos[i])));
        }
    }

    parse_uint32_t(obj["enabledLayerCount"], (o.enabledLayerCount));

    /** TODO: Handle this - ppEnabledLayerNames **/

    parse_uint32_t(obj["enabledExtensionCount"], (o.enabledExtensionCount));

    /** TODO: Handle this - ppEnabledExtensionNames **/

    {
        const Json::Value &obj_pEnabledFeatures = obj["pEnabledFeatures"];
        if (obj_pEnabledFeatures.size() == 0)
        {
            (o.pEnabledFeatures) = nullptr;
        }
        else
        {
            (o.pEnabledFeatures) =
                (VkPhysicalDeviceFeatures *)s_globalMem.allocate(1, sizeof(VkPhysicalDeviceFeatures));
            parse_VkPhysicalDeviceFeatures(obj_pEnabledFeatures,
                                           const_cast<VkPhysicalDeviceFeatures &>(*(o.pEnabledFeatures)));
        }
    }
}

static void parse_VkExtensionProperties(const Json::Value &obj, VkExtensionProperties &o)
{

    /** TODO: Handle this - extensionName **/

    parse_uint32_t(obj["specVersion"], (o.specVersion));
}

static void parse_VkLayerProperties(const Json::Value &obj, VkLayerProperties &o)
{

    /** TODO: Handle this - layerName **/

    parse_uint32_t(obj["specVersion"], (o.specVersion));

    parse_uint32_t(obj["implementationVersion"], (o.implementationVersion));

    /** TODO: Handle this - description **/
}

static void parse_VkSubmitInfo(const Json::Value &obj, VkSubmitInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkSubmitInfo *)parsePNextChain(obj);

    parse_uint32_t(obj["waitSemaphoreCount"], (o.waitSemaphoreCount));

    (o.pWaitSemaphores) = (VkSemaphore *)s_globalMem.allocate((o.waitSemaphoreCount), sizeof(VkSemaphore));
    const Json::Value &obj_pWaitSemaphores_arr = obj["pWaitSemaphores"];
    for (unsigned int i = 0; i < obj_pWaitSemaphores_arr.size(); i++)
    {
        parse_VkSemaphore(obj_pWaitSemaphores_arr[i], const_cast<VkSemaphore &>((o.pWaitSemaphores[i])));
    }

    (o.pWaitDstStageMask) =
        (VkPipelineStageFlags *)s_globalMem.allocate((o.waitSemaphoreCount), sizeof(VkPipelineStageFlags));
    const Json::Value &obj_pWaitDstStageMask_arr = obj["pWaitDstStageMask"];
    for (unsigned int i = 0; i < obj_pWaitDstStageMask_arr.size(); i++)
    {
        parse_VkPipelineStageFlags(obj_pWaitDstStageMask_arr[i],
                                   const_cast<VkPipelineStageFlags &>((o.pWaitDstStageMask[i])));
    }

    parse_uint32_t(obj["commandBufferCount"], (o.commandBufferCount));

    (o.pCommandBuffers) = (VkCommandBuffer *)s_globalMem.allocate((o.commandBufferCount), sizeof(VkCommandBuffer));
    const Json::Value &obj_pCommandBuffers_arr = obj["pCommandBuffers"];
    for (unsigned int i = 0; i < obj_pCommandBuffers_arr.size(); i++)
    {
        parse_VkCommandBuffer(obj_pCommandBuffers_arr[i], const_cast<VkCommandBuffer &>((o.pCommandBuffers[i])));
    }

    parse_uint32_t(obj["signalSemaphoreCount"], (o.signalSemaphoreCount));

    (o.pSignalSemaphores) = (VkSemaphore *)s_globalMem.allocate((o.signalSemaphoreCount), sizeof(VkSemaphore));
    const Json::Value &obj_pSignalSemaphores_arr = obj["pSignalSemaphores"];
    for (unsigned int i = 0; i < obj_pSignalSemaphores_arr.size(); i++)
    {
        parse_VkSemaphore(obj_pSignalSemaphores_arr[i], const_cast<VkSemaphore &>((o.pSignalSemaphores[i])));
    }
}

static void parse_VkMappedMemoryRange(const Json::Value &obj, VkMappedMemoryRange &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkMappedMemoryRange *)parsePNextChain(obj);

    parse_VkDeviceSize(obj["offset"], (o.offset));

    parse_VkDeviceSize(obj["size"], (o.size));
}

static void parse_VkMemoryAllocateInfo(const Json::Value &obj, VkMemoryAllocateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkMemoryAllocateInfo *)parsePNextChain(obj);

    parse_VkDeviceSize(obj["allocationSize"], (o.allocationSize));

    parse_uint32_t(obj["memoryTypeIndex"], (o.memoryTypeIndex));
}

static void parse_VkMemoryRequirements(const Json::Value &obj, VkMemoryRequirements &o)
{

    parse_VkDeviceSize(obj["size"], (o.size));

    parse_VkDeviceSize(obj["alignment"], (o.alignment));

    parse_uint32_t(obj["memoryTypeBits"], (o.memoryTypeBits));
}

static void parse_VkImageSubresource(const Json::Value &obj, VkImageSubresource &o)
{

    parse_VkImageAspectFlags(obj["aspectMask"], (o.aspectMask));

    parse_uint32_t(obj["mipLevel"], (o.mipLevel));

    parse_uint32_t(obj["arrayLayer"], (o.arrayLayer));
}

static void parse_VkFenceCreateInfo(const Json::Value &obj, VkFenceCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkFenceCreateInfo *)parsePNextChain(obj);

    parse_VkFenceCreateFlags(obj["flags"], (o.flags));
}

static void parse_VkSemaphoreCreateInfo(const Json::Value &obj, VkSemaphoreCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkSemaphoreCreateInfo *)parsePNextChain(obj);

    parse_VkSemaphoreCreateFlags(obj["flags"], (o.flags));
}

static void parse_VkEventCreateInfo(const Json::Value &obj, VkEventCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkEventCreateInfo *)parsePNextChain(obj);

    parse_VkEventCreateFlags(obj["flags"], (o.flags));
}

static void parse_VkQueryPoolCreateInfo(const Json::Value &obj, VkQueryPoolCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkQueryPoolCreateInfo *)parsePNextChain(obj);

    parse_VkQueryPoolCreateFlags(obj["flags"], (o.flags));

    parse_VkQueryType(obj["queryType"], (o.queryType));

    parse_uint32_t(obj["queryCount"], (o.queryCount));

    parse_VkQueryPipelineStatisticFlags(obj["pipelineStatistics"], (o.pipelineStatistics));
}

static void parse_VkBufferCreateInfo(const Json::Value &obj, VkBufferCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkBufferCreateInfo *)parsePNextChain(obj);

    parse_VkBufferCreateFlags(obj["flags"], (o.flags));

    parse_VkDeviceSize(obj["size"], (o.size));

    parse_VkBufferUsageFlags(obj["usage"], (o.usage));

    parse_VkSharingMode(obj["sharingMode"], (o.sharingMode));

    parse_uint32_t(obj["queueFamilyIndexCount"], (o.queueFamilyIndexCount));

    (o.pQueueFamilyIndices) = (uint32_t *)s_globalMem.allocate((o.queueFamilyIndexCount), sizeof(uint32_t));
    const Json::Value &obj_pQueueFamilyIndices_arr = obj["pQueueFamilyIndices"];
    for (unsigned int i = 0; i < obj_pQueueFamilyIndices_arr.size(); i++)
    {
        parse_uint32_t(obj_pQueueFamilyIndices_arr[i], const_cast<uint32_t &>((o.pQueueFamilyIndices[i])));
    }
}

static void parse_VkBufferViewCreateInfo(const Json::Value &obj, VkBufferViewCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkBufferViewCreateInfo *)parsePNextChain(obj);

    parse_VkBufferViewCreateFlags(obj["flags"], (o.flags));

    parse_VkFormat(obj["format"], (o.format));

    parse_VkDeviceSize(obj["offset"], (o.offset));

    parse_VkDeviceSize(obj["range"], (o.range));
}

static void parse_VkImageCreateInfo(const Json::Value &obj, VkImageCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkImageCreateInfo *)parsePNextChain(obj);

    parse_VkImageCreateFlags(obj["flags"], (o.flags));

    parse_VkImageType(obj["imageType"], (o.imageType));

    parse_VkFormat(obj["format"], (o.format));

    parse_VkExtent3D(obj["extent"], (o.extent));

    parse_uint32_t(obj["mipLevels"], (o.mipLevels));

    parse_uint32_t(obj["arrayLayers"], (o.arrayLayers));

    parse_VkSampleCountFlagBits(obj["samples"], (o.samples));

    parse_VkImageTiling(obj["tiling"], (o.tiling));

    parse_VkImageUsageFlags(obj["usage"], (o.usage));

    parse_VkSharingMode(obj["sharingMode"], (o.sharingMode));

    parse_uint32_t(obj["queueFamilyIndexCount"], (o.queueFamilyIndexCount));

    (o.pQueueFamilyIndices) = (uint32_t *)s_globalMem.allocate((o.queueFamilyIndexCount), sizeof(uint32_t));
    const Json::Value &obj_pQueueFamilyIndices_arr = obj["pQueueFamilyIndices"];
    for (unsigned int i = 0; i < obj_pQueueFamilyIndices_arr.size(); i++)
    {
        parse_uint32_t(obj_pQueueFamilyIndices_arr[i], const_cast<uint32_t &>((o.pQueueFamilyIndices[i])));
    }

    parse_VkImageLayout(obj["initialLayout"], (o.initialLayout));
}

static void parse_VkSubresourceLayout(const Json::Value &obj, VkSubresourceLayout &o)
{

    parse_VkDeviceSize(obj["offset"], (o.offset));

    parse_VkDeviceSize(obj["size"], (o.size));

    parse_VkDeviceSize(obj["rowPitch"], (o.rowPitch));

    parse_VkDeviceSize(obj["arrayPitch"], (o.arrayPitch));

    parse_VkDeviceSize(obj["depthPitch"], (o.depthPitch));
}

static void parse_VkComponentMapping(const Json::Value &obj, VkComponentMapping &o)
{

    parse_VkComponentSwizzle(obj["r"], (o.r));

    parse_VkComponentSwizzle(obj["g"], (o.g));

    parse_VkComponentSwizzle(obj["b"], (o.b));

    parse_VkComponentSwizzle(obj["a"], (o.a));
}

static void parse_VkImageViewCreateInfo(const Json::Value &obj, VkImageViewCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkImageViewCreateInfo *)parsePNextChain(obj);

    parse_VkImageViewCreateFlags(obj["flags"], (o.flags));

    parse_VkImageViewType(obj["viewType"], (o.viewType));

    parse_VkFormat(obj["format"], (o.format));

    parse_VkComponentMapping(obj["components"], (o.components));

    parse_VkImageSubresourceRange(obj["subresourceRange"], (o.subresourceRange));
}

static void parse_VkPipelineCacheCreateInfo(const Json::Value &obj, VkPipelineCacheCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPipelineCacheCreateInfo *)parsePNextChain(obj);

    parse_VkPipelineCacheCreateFlags(obj["flags"], (o.flags));

    parse_size_t(obj["initialDataSize"], (o.initialDataSize));

    if (o.initialDataSize > 0U)
    {
        void *data = s_globalMem.allocate(uint32_t(o.initialDataSize));
        parse_void_data(obj["pInitialData"], data, int(o.initialDataSize));
        o.pInitialData = data;
    }
    else
        o.pInitialData = NULL;
}

static void parse_VkSpecializationMapEntry(const Json::Value &obj, VkSpecializationMapEntry &o)
{

    parse_uint32_t(obj["constantID"], (o.constantID));

    parse_uint32_t(obj["offset"], (o.offset));

    parse_size_t(obj["size"], (o.size));
}

static void parse_VkSpecializationInfo(const Json::Value &obj, VkSpecializationInfo &o)
{

    parse_uint32_t(obj["mapEntryCount"], (o.mapEntryCount));

    (o.pMapEntries) =
        (VkSpecializationMapEntry *)s_globalMem.allocate((o.mapEntryCount), sizeof(VkSpecializationMapEntry));
    const Json::Value &obj_pMapEntries = obj["pMapEntries"];
    if (obj_pMapEntries.size() == 0)
        (o.pMapEntries) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.mapEntryCount); i++)
        {
            parse_VkSpecializationMapEntry(obj_pMapEntries[i],
                                           const_cast<VkSpecializationMapEntry &>((o.pMapEntries[i])));
        }
    }

    parse_size_t(obj["dataSize"], (o.dataSize));

    if (o.dataSize > 0U)
    {
        void *data = s_globalMem.allocate(uint32_t(o.dataSize));
        parse_void_data(obj["pData"], data, int(o.dataSize));
        o.pData = data;
    }
    else
        o.pData = NULL;
}

static void parse_VkPipelineShaderStageCreateInfo(const Json::Value &obj, VkPipelineShaderStageCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPipelineShaderStageCreateInfo *)parsePNextChain(obj);

    parse_VkPipelineShaderStageCreateFlags(obj["flags"], (o.flags));

    parse_VkShaderStageFlagBits(obj["stage"], (o.stage));

    uint64_t moduleInternal = 0;
    parse_uint64_t(obj["module"], moduleInternal);
    o.module = VkShaderModule(reinterpret_cast<void *>(moduleInternal));

    (o.pName) = (const char *)s_globalMem.allocate(255);
    parse_char(obj["pName"], &(o.pName));

    {
        const Json::Value &obj_pSpecializationInfo = obj["pSpecializationInfo"];
        if (obj_pSpecializationInfo.size() == 0)
        {
            (o.pSpecializationInfo) = nullptr;
        }
        else
        {
            (o.pSpecializationInfo) = (VkSpecializationInfo *)s_globalMem.allocate(1, sizeof(VkSpecializationInfo));
            parse_VkSpecializationInfo(obj_pSpecializationInfo,
                                       const_cast<VkSpecializationInfo &>(*(o.pSpecializationInfo)));
        }
    }
}

static void parse_VkComputePipelineCreateInfo(const Json::Value &obj, VkComputePipelineCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkComputePipelineCreateInfo *)parsePNextChain(obj);

    parse_VkPipelineCreateFlags(obj["flags"], (o.flags));

    parse_VkPipelineShaderStageCreateInfo(obj["stage"], (o.stage));

    uint64_t layoutInternal = 0;
    parse_uint64_t(obj["layout"], layoutInternal);
    o.layout = VkPipelineLayout(reinterpret_cast<void *>(layoutInternal));

    parse_int32_t(obj["basePipelineIndex"], (o.basePipelineIndex));
}

static void parse_VkVertexInputBindingDescription(const Json::Value &obj, VkVertexInputBindingDescription &o)
{

    parse_uint32_t(obj["binding"], (o.binding));

    parse_uint32_t(obj["stride"], (o.stride));

    parse_VkVertexInputRate(obj["inputRate"], (o.inputRate));
}

static void parse_VkVertexInputAttributeDescription(const Json::Value &obj, VkVertexInputAttributeDescription &o)
{

    parse_uint32_t(obj["location"], (o.location));

    parse_uint32_t(obj["binding"], (o.binding));

    parse_VkFormat(obj["format"], (o.format));

    parse_uint32_t(obj["offset"], (o.offset));
}

static void parse_VkPipelineVertexInputStateCreateInfo(const Json::Value &obj, VkPipelineVertexInputStateCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPipelineVertexInputStateCreateInfo *)parsePNextChain(obj);

    parse_VkPipelineVertexInputStateCreateFlags(obj["flags"], (o.flags));

    parse_uint32_t(obj["vertexBindingDescriptionCount"], (o.vertexBindingDescriptionCount));

    (o.pVertexBindingDescriptions) = (VkVertexInputBindingDescription *)s_globalMem.allocate(
        (o.vertexBindingDescriptionCount), sizeof(VkVertexInputBindingDescription));
    const Json::Value &obj_pVertexBindingDescriptions = obj["pVertexBindingDescriptions"];
    if (obj_pVertexBindingDescriptions.size() == 0)
        (o.pVertexBindingDescriptions) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.vertexBindingDescriptionCount); i++)
        {
            parse_VkVertexInputBindingDescription(
                obj_pVertexBindingDescriptions[i],
                const_cast<VkVertexInputBindingDescription &>((o.pVertexBindingDescriptions[i])));
        }
    }

    parse_uint32_t(obj["vertexAttributeDescriptionCount"], (o.vertexAttributeDescriptionCount));

    (o.pVertexAttributeDescriptions) = (VkVertexInputAttributeDescription *)s_globalMem.allocate(
        (o.vertexAttributeDescriptionCount), sizeof(VkVertexInputAttributeDescription));
    const Json::Value &obj_pVertexAttributeDescriptions = obj["pVertexAttributeDescriptions"];
    if (obj_pVertexAttributeDescriptions.size() == 0)
        (o.pVertexAttributeDescriptions) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.vertexAttributeDescriptionCount); i++)
        {
            parse_VkVertexInputAttributeDescription(
                obj_pVertexAttributeDescriptions[i],
                const_cast<VkVertexInputAttributeDescription &>((o.pVertexAttributeDescriptions[i])));
        }
    }
}

static void parse_VkPipelineInputAssemblyStateCreateInfo(const Json::Value &obj,
                                                         VkPipelineInputAssemblyStateCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPipelineInputAssemblyStateCreateInfo *)parsePNextChain(obj);

    parse_VkPipelineInputAssemblyStateCreateFlags(obj["flags"], (o.flags));

    parse_VkPrimitiveTopology(obj["topology"], (o.topology));

    parse_VkBool32(obj["primitiveRestartEnable"], (o.primitiveRestartEnable));
}

static void parse_VkPipelineTessellationStateCreateInfo(const Json::Value &obj,
                                                        VkPipelineTessellationStateCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPipelineTessellationStateCreateInfo *)parsePNextChain(obj);

    parse_VkPipelineTessellationStateCreateFlags(obj["flags"], (o.flags));

    parse_uint32_t(obj["patchControlPoints"], (o.patchControlPoints));
}

static void parse_VkViewport(const Json::Value &obj, VkViewport &o)
{

    parse_float(obj["x"], (o.x));

    parse_float(obj["y"], (o.y));

    parse_float(obj["width"], (o.width));

    parse_float(obj["height"], (o.height));

    parse_float(obj["minDepth"], (o.minDepth));

    parse_float(obj["maxDepth"], (o.maxDepth));
}

static void parse_VkPipelineViewportStateCreateInfo(const Json::Value &obj, VkPipelineViewportStateCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPipelineViewportStateCreateInfo *)parsePNextChain(obj);

    parse_VkPipelineViewportStateCreateFlags(obj["flags"], (o.flags));

    parse_uint32_t(obj["viewportCount"], (o.viewportCount));

    (o.pViewports)                    = (VkViewport *)s_globalMem.allocate((o.viewportCount), sizeof(VkViewport));
    const Json::Value &obj_pViewports = obj["pViewports"];
    if (obj_pViewports.size() == 0)
        (o.pViewports) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.viewportCount); i++)
        {
            parse_VkViewport(obj_pViewports[i], const_cast<VkViewport &>((o.pViewports[i])));
        }
    }

    parse_uint32_t(obj["scissorCount"], (o.scissorCount));

    (o.pScissors)                    = (VkRect2D *)s_globalMem.allocate((o.scissorCount), sizeof(VkRect2D));
    const Json::Value &obj_pScissors = obj["pScissors"];
    if (obj_pScissors.size() == 0)
        (o.pScissors) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.scissorCount); i++)
        {
            parse_VkRect2D(obj_pScissors[i], const_cast<VkRect2D &>((o.pScissors[i])));
        }
    }
}

static void parse_VkPipelineRasterizationStateCreateInfo(const Json::Value &obj,
                                                         VkPipelineRasterizationStateCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPipelineRasterizationStateCreateInfo *)parsePNextChain(obj);

    parse_VkPipelineRasterizationStateCreateFlags(obj["flags"], (o.flags));

    parse_VkBool32(obj["depthClampEnable"], (o.depthClampEnable));

    parse_VkBool32(obj["rasterizerDiscardEnable"], (o.rasterizerDiscardEnable));

    parse_VkPolygonMode(obj["polygonMode"], (o.polygonMode));

    parse_VkCullModeFlags(obj["cullMode"], (o.cullMode));

    parse_VkFrontFace(obj["frontFace"], (o.frontFace));

    parse_VkBool32(obj["depthBiasEnable"], (o.depthBiasEnable));

    parse_float(obj["depthBiasConstantFactor"], (o.depthBiasConstantFactor));

    parse_float(obj["depthBiasClamp"], (o.depthBiasClamp));

    parse_float(obj["depthBiasSlopeFactor"], (o.depthBiasSlopeFactor));

    parse_float(obj["lineWidth"], (o.lineWidth));
}

static void parse_VkPipelineMultisampleStateCreateInfo(const Json::Value &obj, VkPipelineMultisampleStateCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPipelineMultisampleStateCreateInfo *)parsePNextChain(obj);

    parse_VkPipelineMultisampleStateCreateFlags(obj["flags"], (o.flags));

    parse_VkSampleCountFlagBits(obj["rasterizationSamples"], (o.rasterizationSamples));

    parse_VkBool32(obj["sampleShadingEnable"], (o.sampleShadingEnable));

    parse_float(obj["minSampleShading"], (o.minSampleShading));

    (o.pSampleMask) =
        (VkSampleMask *)s_globalMem.allocate((uint32_t(o.rasterizationSamples + 31) / 32), sizeof(VkSampleMask));
    const Json::Value &obj_pSampleMask = obj["pSampleMask"];
    if (o.rasterizationSamples == 0 || obj_pSampleMask.size() == 0)
    {
        (o.pSampleMask) = nullptr;
    }
    else
    {
        for (uint32_t i = 0; i < (uint32_t(o.rasterizationSamples + 31) / 32); i++)
        {
            parse_uint32_t(obj_pSampleMask[i], const_cast<VkSampleMask &>((o.pSampleMask[i])));
        }
    }

    parse_VkBool32(obj["alphaToCoverageEnable"], (o.alphaToCoverageEnable));

    parse_VkBool32(obj["alphaToOneEnable"], (o.alphaToOneEnable));
}

static void parse_VkStencilOpState(const Json::Value &obj, VkStencilOpState &o)
{

    parse_VkStencilOp(obj["failOp"], (o.failOp));

    parse_VkStencilOp(obj["passOp"], (o.passOp));

    parse_VkStencilOp(obj["depthFailOp"], (o.depthFailOp));

    parse_VkCompareOp(obj["compareOp"], (o.compareOp));

    parse_uint32_t(obj["compareMask"], (o.compareMask));

    parse_uint32_t(obj["writeMask"], (o.writeMask));

    parse_uint32_t(obj["reference"], (o.reference));
}

static void parse_VkPipelineDepthStencilStateCreateInfo(const Json::Value &obj,
                                                        VkPipelineDepthStencilStateCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPipelineDepthStencilStateCreateInfo *)parsePNextChain(obj);

    parse_VkPipelineDepthStencilStateCreateFlags(obj["flags"], (o.flags));

    parse_VkBool32(obj["depthTestEnable"], (o.depthTestEnable));

    parse_VkBool32(obj["depthWriteEnable"], (o.depthWriteEnable));

    parse_VkCompareOp(obj["depthCompareOp"], (o.depthCompareOp));

    parse_VkBool32(obj["depthBoundsTestEnable"], (o.depthBoundsTestEnable));

    parse_VkBool32(obj["stencilTestEnable"], (o.stencilTestEnable));

    parse_VkStencilOpState(obj["front"], (o.front));

    parse_VkStencilOpState(obj["back"], (o.back));

    parse_float(obj["minDepthBounds"], (o.minDepthBounds));

    parse_float(obj["maxDepthBounds"], (o.maxDepthBounds));
}

static void parse_VkPipelineColorBlendAttachmentState(const Json::Value &obj, VkPipelineColorBlendAttachmentState &o)
{

    parse_VkBool32(obj["blendEnable"], (o.blendEnable));

    parse_VkBlendFactor(obj["srcColorBlendFactor"], (o.srcColorBlendFactor));

    parse_VkBlendFactor(obj["dstColorBlendFactor"], (o.dstColorBlendFactor));

    parse_VkBlendOp(obj["colorBlendOp"], (o.colorBlendOp));

    parse_VkBlendFactor(obj["srcAlphaBlendFactor"], (o.srcAlphaBlendFactor));

    parse_VkBlendFactor(obj["dstAlphaBlendFactor"], (o.dstAlphaBlendFactor));

    parse_VkBlendOp(obj["alphaBlendOp"], (o.alphaBlendOp));

    parse_VkColorComponentFlags(obj["colorWriteMask"], (o.colorWriteMask));
}

static void parse_VkPipelineColorBlendStateCreateInfo(const Json::Value &obj, VkPipelineColorBlendStateCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPipelineColorBlendStateCreateInfo *)parsePNextChain(obj);

    parse_VkPipelineColorBlendStateCreateFlags(obj["flags"], (o.flags));

    parse_VkBool32(obj["logicOpEnable"], (o.logicOpEnable));

    parse_VkLogicOp(obj["logicOp"], (o.logicOp));

    parse_uint32_t(obj["attachmentCount"], (o.attachmentCount));

    (o.pAttachments) = (VkPipelineColorBlendAttachmentState *)s_globalMem.allocate(
        (o.attachmentCount), sizeof(VkPipelineColorBlendAttachmentState));
    const Json::Value &obj_pAttachments = obj["pAttachments"];
    if (obj_pAttachments.size() == 0)
        (o.pAttachments) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.attachmentCount); i++)
        {
            parse_VkPipelineColorBlendAttachmentState(
                obj_pAttachments[i], const_cast<VkPipelineColorBlendAttachmentState &>((o.pAttachments[i])));
        }
    }

    const Json::Value &obj_blendConstants_arr = obj["blendConstants"];
    for (unsigned int i = 0; i < obj_blendConstants_arr.size(); i++)
    {
        parse_float(obj_blendConstants_arr[i], const_cast<float &>((o.blendConstants[i])));
    }
}

static void parse_VkPipelineDynamicStateCreateInfo(const Json::Value &obj, VkPipelineDynamicStateCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPipelineDynamicStateCreateInfo *)parsePNextChain(obj);

    parse_VkPipelineDynamicStateCreateFlags(obj["flags"], (o.flags));

    parse_uint32_t(obj["dynamicStateCount"], (o.dynamicStateCount));

    (o.pDynamicStates) = (VkDynamicState *)s_globalMem.allocate((o.dynamicStateCount), sizeof(VkDynamicState));
    const Json::Value &obj_pDynamicStates_arr = obj["pDynamicStates"];
    for (unsigned int i = 0; i < obj_pDynamicStates_arr.size(); i++)
    {
        parse_VkDynamicState(obj_pDynamicStates_arr[i], const_cast<VkDynamicState &>((o.pDynamicStates[i])));
    }
}

static void parse_VkGraphicsPipelineCreateInfo(const Json::Value &obj, VkGraphicsPipelineCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkGraphicsPipelineCreateInfo *)parsePNextChain(obj);

    parse_VkPipelineCreateFlags(obj["flags"], (o.flags));

    parse_uint32_t(obj["stageCount"], (o.stageCount));

    (o.pStages)                    = (VkPipelineShaderStageCreateInfo *)s_globalMem.allocate((o.stageCount),
                                                                                             sizeof(VkPipelineShaderStageCreateInfo));
    const Json::Value &obj_pStages = obj["pStages"];
    if (obj_pStages.size() == 0)
        (o.pStages) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.stageCount); i++)
        {
            parse_VkPipelineShaderStageCreateInfo(obj_pStages[i],
                                                  const_cast<VkPipelineShaderStageCreateInfo &>((o.pStages[i])));
        }
    }

    {
        const Json::Value &obj_pVertexInputState = obj["pVertexInputState"];
        if (obj_pVertexInputState.size() == 0)
        {
            (o.pVertexInputState) = nullptr;
        }
        else
        {
            (o.pVertexInputState) = (VkPipelineVertexInputStateCreateInfo *)s_globalMem.allocate(
                1, sizeof(VkPipelineVertexInputStateCreateInfo));
            parse_VkPipelineVertexInputStateCreateInfo(
                obj_pVertexInputState, const_cast<VkPipelineVertexInputStateCreateInfo &>(*(o.pVertexInputState)));
        }
    }

    {
        const Json::Value &obj_pInputAssemblyState = obj["pInputAssemblyState"];
        if (obj_pInputAssemblyState.size() == 0)
        {
            (o.pInputAssemblyState) = nullptr;
        }
        else
        {
            (o.pInputAssemblyState) = (VkPipelineInputAssemblyStateCreateInfo *)s_globalMem.allocate(
                1, sizeof(VkPipelineInputAssemblyStateCreateInfo));
            parse_VkPipelineInputAssemblyStateCreateInfo(
                obj_pInputAssemblyState,
                const_cast<VkPipelineInputAssemblyStateCreateInfo &>(*(o.pInputAssemblyState)));
        }
    }

    {
        const Json::Value &obj_pTessellationState = obj["pTessellationState"];
        if (obj_pTessellationState.size() == 0)
        {
            (o.pTessellationState) = nullptr;
        }
        else
        {
            (o.pTessellationState) = (VkPipelineTessellationStateCreateInfo *)s_globalMem.allocate(
                1, sizeof(VkPipelineTessellationStateCreateInfo));
            parse_VkPipelineTessellationStateCreateInfo(
                obj_pTessellationState, const_cast<VkPipelineTessellationStateCreateInfo &>(*(o.pTessellationState)));
        }
    }

    {
        const Json::Value &obj_pViewportState = obj["pViewportState"];
        if (obj_pViewportState.size() == 0)
        {
            (o.pViewportState) = nullptr;
        }
        else
        {
            (o.pViewportState) =
                (VkPipelineViewportStateCreateInfo *)s_globalMem.allocate(1, sizeof(VkPipelineViewportStateCreateInfo));
            parse_VkPipelineViewportStateCreateInfo(
                obj_pViewportState, const_cast<VkPipelineViewportStateCreateInfo &>(*(o.pViewportState)));
        }
    }

    {
        const Json::Value &obj_pRasterizationState = obj["pRasterizationState"];
        if (obj_pRasterizationState.size() == 0)
        {
            (o.pRasterizationState) = nullptr;
        }
        else
        {
            (o.pRasterizationState) = (VkPipelineRasterizationStateCreateInfo *)s_globalMem.allocate(
                1, sizeof(VkPipelineRasterizationStateCreateInfo));
            parse_VkPipelineRasterizationStateCreateInfo(
                obj_pRasterizationState,
                const_cast<VkPipelineRasterizationStateCreateInfo &>(*(o.pRasterizationState)));
        }
    }

    {
        const Json::Value &obj_pMultisampleState = obj["pMultisampleState"];
        if (obj_pMultisampleState.size() == 0)
        {
            (o.pMultisampleState) = nullptr;
        }
        else
        {
            (o.pMultisampleState) = (VkPipelineMultisampleStateCreateInfo *)s_globalMem.allocate(
                1, sizeof(VkPipelineMultisampleStateCreateInfo));
            parse_VkPipelineMultisampleStateCreateInfo(
                obj_pMultisampleState, const_cast<VkPipelineMultisampleStateCreateInfo &>(*(o.pMultisampleState)));
        }
    }

    {
        const Json::Value &obj_pDepthStencilState = obj["pDepthStencilState"];
        if (obj_pDepthStencilState.size() == 0)
        {
            (o.pDepthStencilState) = nullptr;
        }
        else
        {
            (o.pDepthStencilState) = (VkPipelineDepthStencilStateCreateInfo *)s_globalMem.allocate(
                1, sizeof(VkPipelineDepthStencilStateCreateInfo));
            parse_VkPipelineDepthStencilStateCreateInfo(
                obj_pDepthStencilState, const_cast<VkPipelineDepthStencilStateCreateInfo &>(*(o.pDepthStencilState)));
        }
    }

    {
        const Json::Value &obj_pColorBlendState = obj["pColorBlendState"];
        if (obj_pColorBlendState.size() == 0)
        {
            (o.pColorBlendState) = nullptr;
        }
        else
        {
            (o.pColorBlendState) = (VkPipelineColorBlendStateCreateInfo *)s_globalMem.allocate(
                1, sizeof(VkPipelineColorBlendStateCreateInfo));
            parse_VkPipelineColorBlendStateCreateInfo(
                obj_pColorBlendState, const_cast<VkPipelineColorBlendStateCreateInfo &>(*(o.pColorBlendState)));
        }
    }

    {
        const Json::Value &obj_pDynamicState = obj["pDynamicState"];
        if (obj_pDynamicState.size() == 0)
        {
            (o.pDynamicState) = nullptr;
        }
        else
        {
            (o.pDynamicState) =
                (VkPipelineDynamicStateCreateInfo *)s_globalMem.allocate(1, sizeof(VkPipelineDynamicStateCreateInfo));
            parse_VkPipelineDynamicStateCreateInfo(obj_pDynamicState,
                                                   const_cast<VkPipelineDynamicStateCreateInfo &>(*(o.pDynamicState)));
        }
    }

    uint64_t layoutInternal = 0;
    parse_uint64_t(obj["layout"], layoutInternal);
    o.layout = VkPipelineLayout(reinterpret_cast<void *>(layoutInternal));

    uint64_t renderPassInternal = 0;
    parse_uint64_t(obj["renderPass"], renderPassInternal);
    o.renderPass = VkRenderPass(reinterpret_cast<void *>(renderPassInternal));

    parse_uint32_t(obj["subpass"], (o.subpass));

    parse_int32_t(obj["basePipelineIndex"], (o.basePipelineIndex));
}

static void parse_VkPushConstantRange(const Json::Value &obj, VkPushConstantRange &o)
{

    parse_VkShaderStageFlags(obj["stageFlags"], (o.stageFlags));

    parse_uint32_t(obj["offset"], (o.offset));

    parse_uint32_t(obj["size"], (o.size));
}

static void parse_VkPipelineLayoutCreateInfo(const Json::Value &obj, VkPipelineLayoutCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPipelineLayoutCreateInfo *)parsePNextChain(obj);

    parse_VkPipelineLayoutCreateFlags(obj["flags"], (o.flags));

    parse_uint32_t(obj["setLayoutCount"], (o.setLayoutCount));

    VkDescriptorSetLayout *pSetLayoutsTab =
        (VkDescriptorSetLayout *)s_globalMem.allocate((o.setLayoutCount), sizeof(VkDescriptorSetLayout));
    const Json::Value &obj_pSetLayouts_arr = obj["pSetLayouts"];
    for (unsigned int i = 0; i < obj_pSetLayouts_arr.size(); i++)
    {
        uint64_t pSetLayoutsInternal = 0;
        parse_uint64_t(obj_pSetLayouts_arr[i], pSetLayoutsInternal);
        pSetLayoutsTab[i] = VkDescriptorSetLayout(reinterpret_cast<void *>(pSetLayoutsInternal));
    }
    o.pSetLayouts = pSetLayoutsTab;

    parse_uint32_t(obj["pushConstantRangeCount"], (o.pushConstantRangeCount));

    (o.pPushConstantRanges) =
        (VkPushConstantRange *)s_globalMem.allocate((o.pushConstantRangeCount), sizeof(VkPushConstantRange));
    const Json::Value &obj_pPushConstantRanges = obj["pPushConstantRanges"];
    if (obj_pPushConstantRanges.size() == 0)
        (o.pPushConstantRanges) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.pushConstantRangeCount); i++)
        {
            parse_VkPushConstantRange(obj_pPushConstantRanges[i],
                                      const_cast<VkPushConstantRange &>((o.pPushConstantRanges[i])));
        }
    }
}

static void parse_VkSamplerCreateInfo(const Json::Value &obj, VkSamplerCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkSamplerCreateInfo *)parsePNextChain(obj);

    parse_VkSamplerCreateFlags(obj["flags"], (o.flags));

    parse_VkFilter(obj["magFilter"], (o.magFilter));

    parse_VkFilter(obj["minFilter"], (o.minFilter));

    parse_VkSamplerMipmapMode(obj["mipmapMode"], (o.mipmapMode));

    parse_VkSamplerAddressMode(obj["addressModeU"], (o.addressModeU));

    parse_VkSamplerAddressMode(obj["addressModeV"], (o.addressModeV));

    parse_VkSamplerAddressMode(obj["addressModeW"], (o.addressModeW));

    parse_float(obj["mipLodBias"], (o.mipLodBias));

    parse_VkBool32(obj["anisotropyEnable"], (o.anisotropyEnable));

    parse_float(obj["maxAnisotropy"], (o.maxAnisotropy));

    parse_VkBool32(obj["compareEnable"], (o.compareEnable));

    parse_VkCompareOp(obj["compareOp"], (o.compareOp));

    parse_float(obj["minLod"], (o.minLod));

    parse_float(obj["maxLod"], (o.maxLod));

    parse_VkBorderColor(obj["borderColor"], (o.borderColor));

    parse_VkBool32(obj["unnormalizedCoordinates"], (o.unnormalizedCoordinates));
}

static void parse_VkCopyDescriptorSet(const Json::Value &obj, VkCopyDescriptorSet &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkCopyDescriptorSet *)parsePNextChain(obj);

    parse_uint32_t(obj["srcBinding"], (o.srcBinding));

    parse_uint32_t(obj["srcArrayElement"], (o.srcArrayElement));

    parse_uint32_t(obj["dstBinding"], (o.dstBinding));

    parse_uint32_t(obj["dstArrayElement"], (o.dstArrayElement));

    parse_uint32_t(obj["descriptorCount"], (o.descriptorCount));
}

static void parse_VkDescriptorBufferInfo(const Json::Value &obj, VkDescriptorBufferInfo &o)
{

    parse_VkDeviceSize(obj["offset"], (o.offset));

    parse_VkDeviceSize(obj["range"], (o.range));
}

static void parse_VkDescriptorImageInfo(const Json::Value &obj, VkDescriptorImageInfo &o)
{

    parse_VkImageLayout(obj["imageLayout"], (o.imageLayout));
}

static void parse_VkDescriptorPoolSize(const Json::Value &obj, VkDescriptorPoolSize &o)
{

    parse_VkDescriptorType(obj["type"], (o.type));

    parse_uint32_t(obj["descriptorCount"], (o.descriptorCount));
}

static void parse_VkDescriptorPoolCreateInfo(const Json::Value &obj, VkDescriptorPoolCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkDescriptorPoolCreateInfo *)parsePNextChain(obj);

    parse_VkDescriptorPoolCreateFlags(obj["flags"], (o.flags));

    parse_uint32_t(obj["maxSets"], (o.maxSets));

    parse_uint32_t(obj["poolSizeCount"], (o.poolSizeCount));

    (o.pPoolSizes) = (VkDescriptorPoolSize *)s_globalMem.allocate((o.poolSizeCount), sizeof(VkDescriptorPoolSize));
    const Json::Value &obj_pPoolSizes = obj["pPoolSizes"];
    if (obj_pPoolSizes.size() == 0)
        (o.pPoolSizes) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.poolSizeCount); i++)
        {
            parse_VkDescriptorPoolSize(obj_pPoolSizes[i], const_cast<VkDescriptorPoolSize &>((o.pPoolSizes[i])));
        }
    }
}

static void parse_VkDescriptorSetAllocateInfo(const Json::Value &obj, VkDescriptorSetAllocateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkDescriptorSetAllocateInfo *)parsePNextChain(obj);

    parse_uint32_t(obj["descriptorSetCount"], (o.descriptorSetCount));

    (o.pSetLayouts) =
        (VkDescriptorSetLayout *)s_globalMem.allocate((o.descriptorSetCount), sizeof(VkDescriptorSetLayout));
    const Json::Value &obj_pSetLayouts_arr = obj["pSetLayouts"];
    for (unsigned int i = 0; i < obj_pSetLayouts_arr.size(); i++)
    {
        parse_VkDescriptorSetLayout(obj_pSetLayouts_arr[i], const_cast<VkDescriptorSetLayout &>((o.pSetLayouts[i])));
    }
}

static void parse_VkDescriptorSetLayoutBinding(const Json::Value &obj, VkDescriptorSetLayoutBinding &o)
{

    parse_uint32_t(obj["binding"], (o.binding));

    parse_VkDescriptorType(obj["descriptorType"], (o.descriptorType));

    parse_uint32_t(obj["descriptorCount"], (o.descriptorCount));

    parse_VkShaderStageFlags(obj["stageFlags"], (o.stageFlags));

    const Json::Value &obj_pImmutableSamplers = obj["pImmutableSamplers"];
    if (obj_pImmutableSamplers.empty() ||
        (obj_pImmutableSamplers.isString() && obj_pImmutableSamplers.asString() == "NULL"))
        o.pImmutableSamplers = nullptr;
    else
    {
        VkSampler *samplers = (VkSampler *)s_globalMem.allocate((o.descriptorCount), sizeof(VkSampler));
        for (unsigned int i = 0; i < obj_pImmutableSamplers.size(); i++)
        {
            uint64_t sInternal = 0;
            parse_uint64_t(obj_pImmutableSamplers[i], sInternal);
            samplers[i] = VkSampler(reinterpret_cast<void *>(sInternal));
        }
        o.pImmutableSamplers = samplers;
    }
}

static void parse_VkDescriptorSetLayoutCreateInfo(const Json::Value &obj, VkDescriptorSetLayoutCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkDescriptorSetLayoutCreateInfo *)parsePNextChain(obj);

    parse_VkDescriptorSetLayoutCreateFlags(obj["flags"], (o.flags));

    parse_uint32_t(obj["bindingCount"], (o.bindingCount));

    (o.pBindings) =
        (VkDescriptorSetLayoutBinding *)s_globalMem.allocate((o.bindingCount), sizeof(VkDescriptorSetLayoutBinding));
    const Json::Value &obj_pBindings = obj["pBindings"];
    if (obj_pBindings.size() == 0)
        (o.pBindings) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.bindingCount); i++)
        {
            parse_VkDescriptorSetLayoutBinding(obj_pBindings[i],
                                               const_cast<VkDescriptorSetLayoutBinding &>((o.pBindings[i])));
        }
    }
}

static void parse_VkWriteDescriptorSet(const Json::Value &obj, VkWriteDescriptorSet &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkWriteDescriptorSet *)parsePNextChain(obj);

    parse_uint32_t(obj["dstBinding"], (o.dstBinding));

    parse_uint32_t(obj["dstArrayElement"], (o.dstArrayElement));

    parse_uint32_t(obj["descriptorCount"], (o.descriptorCount));

    parse_VkDescriptorType(obj["descriptorType"], (o.descriptorType));

    (o.pImageInfo) = (VkDescriptorImageInfo *)s_globalMem.allocate((o.descriptorCount), sizeof(VkDescriptorImageInfo));
    const Json::Value &obj_pImageInfo = obj["pImageInfo"];
    if (obj_pImageInfo.size() == 0)
        (o.pImageInfo) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.descriptorCount); i++)
        {
            parse_VkDescriptorImageInfo(obj_pImageInfo[i], const_cast<VkDescriptorImageInfo &>((o.pImageInfo[i])));
        }
    }

    (o.pBufferInfo) =
        (VkDescriptorBufferInfo *)s_globalMem.allocate((o.descriptorCount), sizeof(VkDescriptorBufferInfo));
    const Json::Value &obj_pBufferInfo = obj["pBufferInfo"];
    if (obj_pBufferInfo.size() == 0)
        (o.pBufferInfo) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.descriptorCount); i++)
        {
            parse_VkDescriptorBufferInfo(obj_pBufferInfo[i], const_cast<VkDescriptorBufferInfo &>((o.pBufferInfo[i])));
        }
    }

    (o.pTexelBufferView) = (VkBufferView *)s_globalMem.allocate((o.descriptorCount), sizeof(VkBufferView));
    const Json::Value &obj_pTexelBufferView_arr = obj["pTexelBufferView"];
    for (unsigned int i = 0; i < obj_pTexelBufferView_arr.size(); i++)
    {
        parse_VkBufferView(obj_pTexelBufferView_arr[i], const_cast<VkBufferView &>((o.pTexelBufferView[i])));
    }
}

static void parse_VkAttachmentDescription(const Json::Value &obj, VkAttachmentDescription &o)
{

    parse_VkAttachmentDescriptionFlags(obj["flags"], (o.flags));

    parse_VkFormat(obj["format"], (o.format));

    parse_VkSampleCountFlagBits(obj["samples"], (o.samples));

    parse_VkAttachmentLoadOp(obj["loadOp"], (o.loadOp));

    parse_VkAttachmentStoreOp(obj["storeOp"], (o.storeOp));

    parse_VkAttachmentLoadOp(obj["stencilLoadOp"], (o.stencilLoadOp));

    parse_VkAttachmentStoreOp(obj["stencilStoreOp"], (o.stencilStoreOp));

    parse_VkImageLayout(obj["initialLayout"], (o.initialLayout));

    parse_VkImageLayout(obj["finalLayout"], (o.finalLayout));
}

static void parse_VkAttachmentReference(const Json::Value &obj, VkAttachmentReference &o)
{

    parse_uint32_t(obj["attachment"], (o.attachment));

    parse_VkImageLayout(obj["layout"], (o.layout));
}

static void parse_VkFramebufferCreateInfo(const Json::Value &obj, VkFramebufferCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkFramebufferCreateInfo *)parsePNextChain(obj);

    parse_VkFramebufferCreateFlags(obj["flags"], (o.flags));

    uint64_t renderPassInternal = 0;
    parse_uint64_t(obj["renderPass"], renderPassInternal);
    o.renderPass = VkRenderPass(reinterpret_cast<void *>(renderPassInternal));

    parse_uint32_t(obj["attachmentCount"], (o.attachmentCount));

    (o.pAttachments) = (VkImageView *)s_globalMem.allocate((o.attachmentCount), sizeof(VkImageView));
    const Json::Value &obj_pAttachments_arr = obj["pAttachments"];
    for (unsigned int i = 0; i < obj_pAttachments_arr.size(); i++)
    {
        parse_VkImageView(obj_pAttachments_arr[i], const_cast<VkImageView &>((o.pAttachments[i])));
    }

    parse_uint32_t(obj["width"], (o.width));

    parse_uint32_t(obj["height"], (o.height));

    parse_uint32_t(obj["layers"], (o.layers));
}

static void parse_VkSubpassDescription(const Json::Value &obj, VkSubpassDescription &o)
{

    parse_VkSubpassDescriptionFlags(obj["flags"], (o.flags));

    parse_VkPipelineBindPoint(obj["pipelineBindPoint"], (o.pipelineBindPoint));

    parse_uint32_t(obj["inputAttachmentCount"], (o.inputAttachmentCount));

    (o.pInputAttachments) =
        (VkAttachmentReference *)s_globalMem.allocate((o.inputAttachmentCount), sizeof(VkAttachmentReference));
    const Json::Value &obj_pInputAttachments = obj["pInputAttachments"];
    if (obj_pInputAttachments.size() == 0)
        (o.pInputAttachments) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.inputAttachmentCount); i++)
        {
            parse_VkAttachmentReference(obj_pInputAttachments[i],
                                        const_cast<VkAttachmentReference &>((o.pInputAttachments[i])));
        }
    }

    parse_uint32_t(obj["colorAttachmentCount"], (o.colorAttachmentCount));

    (o.pColorAttachments) =
        (VkAttachmentReference *)s_globalMem.allocate((o.colorAttachmentCount), sizeof(VkAttachmentReference));
    const Json::Value &obj_pColorAttachments = obj["pColorAttachments"];
    if (obj_pColorAttachments.size() == 0)
        (o.pColorAttachments) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.colorAttachmentCount); i++)
        {
            parse_VkAttachmentReference(obj_pColorAttachments[i],
                                        const_cast<VkAttachmentReference &>((o.pColorAttachments[i])));
        }
    }

    (o.pResolveAttachments) =
        (VkAttachmentReference *)s_globalMem.allocate((o.colorAttachmentCount), sizeof(VkAttachmentReference));
    const Json::Value &obj_pResolveAttachments = obj["pResolveAttachments"];
    if (obj_pResolveAttachments.size() == 0)
        (o.pResolveAttachments) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.colorAttachmentCount); i++)
        {
            parse_VkAttachmentReference(obj_pResolveAttachments[i],
                                        const_cast<VkAttachmentReference &>((o.pResolveAttachments[i])));
        }
    }

    {
        const Json::Value &obj_pDepthStencilAttachment = obj["pDepthStencilAttachment"];
        if (obj_pDepthStencilAttachment.size() == 0)
        {
            (o.pDepthStencilAttachment) = nullptr;
        }
        else
        {
            (o.pDepthStencilAttachment) =
                (VkAttachmentReference *)s_globalMem.allocate(1, sizeof(VkAttachmentReference));
            parse_VkAttachmentReference(obj_pDepthStencilAttachment,
                                        const_cast<VkAttachmentReference &>(*(o.pDepthStencilAttachment)));
        }
    }

    parse_uint32_t(obj["preserveAttachmentCount"], (o.preserveAttachmentCount));

    (o.pPreserveAttachments) = (uint32_t *)s_globalMem.allocate((o.preserveAttachmentCount), sizeof(uint32_t));
    const Json::Value &obj_pPreserveAttachments_arr = obj["pPreserveAttachments"];
    for (unsigned int i = 0; i < obj_pPreserveAttachments_arr.size(); i++)
    {
        parse_uint32_t(obj_pPreserveAttachments_arr[i], const_cast<uint32_t &>((o.pPreserveAttachments[i])));
    }
}

static void parse_VkSubpassDependency(const Json::Value &obj, VkSubpassDependency &o)
{

    parse_uint32_t(obj["srcSubpass"], (o.srcSubpass));

    parse_uint32_t(obj["dstSubpass"], (o.dstSubpass));

    parse_VkPipelineStageFlags(obj["srcStageMask"], (o.srcStageMask));

    parse_VkPipelineStageFlags(obj["dstStageMask"], (o.dstStageMask));

    parse_VkAccessFlags(obj["srcAccessMask"], (o.srcAccessMask));

    parse_VkAccessFlags(obj["dstAccessMask"], (o.dstAccessMask));

    parse_VkDependencyFlags(obj["dependencyFlags"], (o.dependencyFlags));
}

static void parse_VkRenderPassCreateInfo(const Json::Value &obj, VkRenderPassCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkRenderPassCreateInfo *)parsePNextChain(obj);

    parse_VkRenderPassCreateFlags(obj["flags"], (o.flags));

    parse_uint32_t(obj["attachmentCount"], (o.attachmentCount));

    (o.pAttachments) =
        (VkAttachmentDescription *)s_globalMem.allocate((o.attachmentCount), sizeof(VkAttachmentDescription));
    const Json::Value &obj_pAttachments = obj["pAttachments"];
    if (obj_pAttachments.size() == 0)
        (o.pAttachments) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.attachmentCount); i++)
        {
            parse_VkAttachmentDescription(obj_pAttachments[i],
                                          const_cast<VkAttachmentDescription &>((o.pAttachments[i])));
        }
    }

    parse_uint32_t(obj["subpassCount"], (o.subpassCount));

    (o.pSubpasses) = (VkSubpassDescription *)s_globalMem.allocate((o.subpassCount), sizeof(VkSubpassDescription));
    const Json::Value &obj_pSubpasses = obj["pSubpasses"];
    if (obj_pSubpasses.size() == 0)
        (o.pSubpasses) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.subpassCount); i++)
        {
            parse_VkSubpassDescription(obj_pSubpasses[i], const_cast<VkSubpassDescription &>((o.pSubpasses[i])));
        }
    }

    parse_uint32_t(obj["dependencyCount"], (o.dependencyCount));

    (o.pDependencies) = (VkSubpassDependency *)s_globalMem.allocate((o.dependencyCount), sizeof(VkSubpassDependency));
    const Json::Value &obj_pDependencies = obj["pDependencies"];
    if (obj_pDependencies.size() == 0)
        (o.pDependencies) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.dependencyCount); i++)
        {
            parse_VkSubpassDependency(obj_pDependencies[i], const_cast<VkSubpassDependency &>((o.pDependencies[i])));
        }
    }
}

static void parse_VkCommandPoolCreateInfo(const Json::Value &obj, VkCommandPoolCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkCommandPoolCreateInfo *)parsePNextChain(obj);

    parse_VkCommandPoolCreateFlags(obj["flags"], (o.flags));

    parse_uint32_t(obj["queueFamilyIndex"], (o.queueFamilyIndex));
}

static void parse_VkCommandBufferAllocateInfo(const Json::Value &obj, VkCommandBufferAllocateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkCommandBufferAllocateInfo *)parsePNextChain(obj);

    parse_VkCommandBufferLevel(obj["level"], (o.level));

    parse_uint32_t(obj["commandBufferCount"], (o.commandBufferCount));
}

static void parse_VkCommandBufferInheritanceInfo(const Json::Value &obj, VkCommandBufferInheritanceInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkCommandBufferInheritanceInfo *)parsePNextChain(obj);

    uint64_t renderPassInternal = 0;
    parse_uint64_t(obj["renderPass"], renderPassInternal);
    o.renderPass = VkRenderPass(reinterpret_cast<void *>(renderPassInternal));

    parse_uint32_t(obj["subpass"], (o.subpass));

    parse_VkBool32(obj["occlusionQueryEnable"], (o.occlusionQueryEnable));

    parse_VkQueryControlFlags(obj["queryFlags"], (o.queryFlags));

    parse_VkQueryPipelineStatisticFlags(obj["pipelineStatistics"], (o.pipelineStatistics));
}

static void parse_VkCommandBufferBeginInfo(const Json::Value &obj, VkCommandBufferBeginInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkCommandBufferBeginInfo *)parsePNextChain(obj);

    parse_VkCommandBufferUsageFlags(obj["flags"], (o.flags));

    {
        const Json::Value &obj_pInheritanceInfo = obj["pInheritanceInfo"];
        if (obj_pInheritanceInfo.size() == 0)
        {
            (o.pInheritanceInfo) = nullptr;
        }
        else
        {
            (o.pInheritanceInfo) =
                (VkCommandBufferInheritanceInfo *)s_globalMem.allocate(1, sizeof(VkCommandBufferInheritanceInfo));
            parse_VkCommandBufferInheritanceInfo(obj_pInheritanceInfo,
                                                 const_cast<VkCommandBufferInheritanceInfo &>(*(o.pInheritanceInfo)));
        }
    }
}

static void parse_VkBufferCopy(const Json::Value &obj, VkBufferCopy &o)
{

    parse_VkDeviceSize(obj["srcOffset"], (o.srcOffset));

    parse_VkDeviceSize(obj["dstOffset"], (o.dstOffset));

    parse_VkDeviceSize(obj["size"], (o.size));
}

static void parse_VkImageSubresourceLayers(const Json::Value &obj, VkImageSubresourceLayers &o)
{

    parse_VkImageAspectFlags(obj["aspectMask"], (o.aspectMask));

    parse_uint32_t(obj["mipLevel"], (o.mipLevel));

    parse_uint32_t(obj["baseArrayLayer"], (o.baseArrayLayer));

    parse_uint32_t(obj["layerCount"], (o.layerCount));
}

static void parse_VkBufferImageCopy(const Json::Value &obj, VkBufferImageCopy &o)
{

    parse_VkDeviceSize(obj["bufferOffset"], (o.bufferOffset));

    parse_uint32_t(obj["bufferRowLength"], (o.bufferRowLength));

    parse_uint32_t(obj["bufferImageHeight"], (o.bufferImageHeight));

    parse_VkImageSubresourceLayers(obj["imageSubresource"], (o.imageSubresource));

    parse_VkOffset3D(obj["imageOffset"], (o.imageOffset));

    parse_VkExtent3D(obj["imageExtent"], (o.imageExtent));
}

static void parse_VkClearColorValue(const Json::Value &obj, VkClearColorValue &o)
{

    const Json::Value &obj_float32_arr = obj["float32"];
    for (unsigned int i = 0; i < obj_float32_arr.size(); i++)
    {
        parse_float(obj_float32_arr[i], const_cast<float &>((o.float32[i])));
    }

    const Json::Value &obj_int32_arr = obj["int32"];
    for (unsigned int i = 0; i < obj_int32_arr.size(); i++)
    {
        parse_int32_t(obj_int32_arr[i], const_cast<int32_t &>((o.int32[i])));
    }

    const Json::Value &obj_uint32_arr = obj["uint32"];
    for (unsigned int i = 0; i < obj_uint32_arr.size(); i++)
    {
        parse_uint32_t(obj_uint32_arr[i], const_cast<uint32_t &>((o.uint32[i])));
    }
}

static void parse_VkClearDepthStencilValue(const Json::Value &obj, VkClearDepthStencilValue &o)
{

    parse_float(obj["depth"], (o.depth));

    parse_uint32_t(obj["stencil"], (o.stencil));
}

static void parse_VkClearValue(const Json::Value &obj, VkClearValue &o)
{

    parse_VkClearColorValue(obj["color"], (o.color));

    parse_VkClearDepthStencilValue(obj["depthStencil"], (o.depthStencil));
}

static void parse_VkClearAttachment(const Json::Value &obj, VkClearAttachment &o)
{

    parse_VkImageAspectFlags(obj["aspectMask"], (o.aspectMask));

    parse_uint32_t(obj["colorAttachment"], (o.colorAttachment));

    parse_VkClearValue(obj["clearValue"], (o.clearValue));
}

static void parse_VkClearRect(const Json::Value &obj, VkClearRect &o)
{

    parse_VkRect2D(obj["rect"], (o.rect));

    parse_uint32_t(obj["baseArrayLayer"], (o.baseArrayLayer));

    parse_uint32_t(obj["layerCount"], (o.layerCount));
}

static void parse_VkImageBlit(const Json::Value &obj, VkImageBlit &o)
{

    parse_VkImageSubresourceLayers(obj["srcSubresource"], (o.srcSubresource));

    const Json::Value &obj_srcOffsets_arr = obj["srcOffsets"];
    for (unsigned int i = 0; i < obj_srcOffsets_arr.size(); i++)
    {
        parse_VkOffset3D(obj_srcOffsets_arr[i], const_cast<VkOffset3D &>((o.srcOffsets[i])));
    }

    parse_VkImageSubresourceLayers(obj["dstSubresource"], (o.dstSubresource));

    const Json::Value &obj_dstOffsets_arr = obj["dstOffsets"];
    for (unsigned int i = 0; i < obj_dstOffsets_arr.size(); i++)
    {
        parse_VkOffset3D(obj_dstOffsets_arr[i], const_cast<VkOffset3D &>((o.dstOffsets[i])));
    }
}

static void parse_VkImageCopy(const Json::Value &obj, VkImageCopy &o)
{

    parse_VkImageSubresourceLayers(obj["srcSubresource"], (o.srcSubresource));

    parse_VkOffset3D(obj["srcOffset"], (o.srcOffset));

    parse_VkImageSubresourceLayers(obj["dstSubresource"], (o.dstSubresource));

    parse_VkOffset3D(obj["dstOffset"], (o.dstOffset));

    parse_VkExtent3D(obj["extent"], (o.extent));
}

static void parse_VkImageResolve(const Json::Value &obj, VkImageResolve &o)
{

    parse_VkImageSubresourceLayers(obj["srcSubresource"], (o.srcSubresource));

    parse_VkOffset3D(obj["srcOffset"], (o.srcOffset));

    parse_VkImageSubresourceLayers(obj["dstSubresource"], (o.dstSubresource));

    parse_VkOffset3D(obj["dstOffset"], (o.dstOffset));

    parse_VkExtent3D(obj["extent"], (o.extent));
}

static void parse_VkRenderPassBeginInfo(const Json::Value &obj, VkRenderPassBeginInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkRenderPassBeginInfo *)parsePNextChain(obj);

    uint64_t renderPassInternal = 0;
    parse_uint64_t(obj["renderPass"], renderPassInternal);
    o.renderPass = VkRenderPass(reinterpret_cast<void *>(renderPassInternal));

    parse_VkRect2D(obj["renderArea"], (o.renderArea));

    parse_uint32_t(obj["clearValueCount"], (o.clearValueCount));

    (o.pClearValues) = (VkClearValue *)s_globalMem.allocate((o.clearValueCount), sizeof(VkClearValue));
    const Json::Value &obj_pClearValues_arr = obj["pClearValues"];
    for (unsigned int i = 0; i < obj_pClearValues_arr.size(); i++)
    {
        parse_VkClearValue(obj_pClearValues_arr[i], const_cast<VkClearValue &>((o.pClearValues[i])));
    }
}

static void parse_VkSamplerYcbcrConversion(const Json::Value &obj, VkSamplerYcbcrConversion &o)
{
    //    const std::string& _res = obj.asString();
}

static std::map<std::string, int> VkSubgroupFeatureFlagBits_map = {
    std::make_pair("VK_SUBGROUP_FEATURE_BASIC_BIT", 1UL << 0),
    std::make_pair("VK_SUBGROUP_FEATURE_VOTE_BIT", 1UL << 1),
    std::make_pair("VK_SUBGROUP_FEATURE_ARITHMETIC_BIT", 1UL << 2),
    std::make_pair("VK_SUBGROUP_FEATURE_BALLOT_BIT", 1UL << 3),
    std::make_pair("VK_SUBGROUP_FEATURE_SHUFFLE_BIT", 1UL << 4),
    std::make_pair("VK_SUBGROUP_FEATURE_SHUFFLE_RELATIVE_BIT", 1UL << 5),
    std::make_pair("VK_SUBGROUP_FEATURE_CLUSTERED_BIT", 1UL << 6),
    std::make_pair("VK_SUBGROUP_FEATURE_QUAD_BIT", 1UL << 7),
    std::make_pair("VK_SUBGROUP_FEATURE_PARTITIONED_BIT_NV", 1UL << 8),
    std::make_pair("VK_SUBGROUP_FEATURE_ROTATE_BIT_KHR", 1UL << 9),
    std::make_pair("VK_SUBGROUP_FEATURE_ROTATE_CLUSTERED_BIT_KHR", 1UL << 10),
};
static void parse_VkSubgroupFeatureFlagBits(const Json::Value &obj, VkSubgroupFeatureFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkSubgroupFeatureFlagBits)VkSubgroupFeatureFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkPeerMemoryFeatureFlagBits_map = {
    std::make_pair("VK_PEER_MEMORY_FEATURE_COPY_SRC_BIT", 1UL << 0),
    std::make_pair("VK_PEER_MEMORY_FEATURE_COPY_DST_BIT", 1UL << 1),
    std::make_pair("VK_PEER_MEMORY_FEATURE_GENERIC_SRC_BIT", 1UL << 2),
    std::make_pair("VK_PEER_MEMORY_FEATURE_GENERIC_DST_BIT", 1UL << 3),
    std::make_pair("VK_PEER_MEMORY_FEATURE_COPY_SRC_BIT_KHR", 1UL << 0),
    std::make_pair("VK_PEER_MEMORY_FEATURE_COPY_DST_BIT_KHR", 1UL << 1),
    std::make_pair("VK_PEER_MEMORY_FEATURE_GENERIC_SRC_BIT_KHR", 1UL << 2),
    std::make_pair("VK_PEER_MEMORY_FEATURE_GENERIC_DST_BIT_KHR", 1UL << 3),
};
static void parse_VkPeerMemoryFeatureFlagBits(const Json::Value &obj, VkPeerMemoryFeatureFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkPeerMemoryFeatureFlagBits)VkPeerMemoryFeatureFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkMemoryAllocateFlagBits_map = {
    std::make_pair("VK_MEMORY_ALLOCATE_DEVICE_MASK_BIT", 1UL << 0),
    std::make_pair("VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT", 1UL << 1),
    std::make_pair("VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT", 1UL << 2),
    std::make_pair("VK_MEMORY_ALLOCATE_DEVICE_MASK_BIT_KHR", 1UL << 0),
    std::make_pair("VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR", 1UL << 1),
    std::make_pair("VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT_KHR", 1UL << 2),
};
static void parse_VkMemoryAllocateFlagBits(const Json::Value &obj, VkMemoryAllocateFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkMemoryAllocateFlagBits)VkMemoryAllocateFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkPointClippingBehavior_map = {
    std::make_pair("VK_POINT_CLIPPING_BEHAVIOR_ALL_CLIP_PLANES", 0),
    std::make_pair("VK_POINT_CLIPPING_BEHAVIOR_USER_CLIP_PLANES_ONLY", 1),
    std::make_pair("VK_POINT_CLIPPING_BEHAVIOR_ALL_CLIP_PLANES_KHR", 0),
    std::make_pair("VK_POINT_CLIPPING_BEHAVIOR_USER_CLIP_PLANES_ONLY_KHR", 1),
};
static void parse_VkPointClippingBehavior(const Json::Value &obj, VkPointClippingBehavior &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkPointClippingBehavior)VkPointClippingBehavior_map[std::string(_res)];
}

static std::map<std::string, int> VkTessellationDomainOrigin_map = {
    std::make_pair("VK_TESSELLATION_DOMAIN_ORIGIN_UPPER_LEFT", 0),
    std::make_pair("VK_TESSELLATION_DOMAIN_ORIGIN_LOWER_LEFT", 1),
    std::make_pair("VK_TESSELLATION_DOMAIN_ORIGIN_UPPER_LEFT_KHR", 0),
    std::make_pair("VK_TESSELLATION_DOMAIN_ORIGIN_LOWER_LEFT_KHR", 1),
};
static void parse_VkTessellationDomainOrigin(const Json::Value &obj, VkTessellationDomainOrigin &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkTessellationDomainOrigin)VkTessellationDomainOrigin_map[std::string(_res)];
}

static std::map<std::string, int> VkSamplerYcbcrModelConversion_map = {
    std::make_pair("VK_SAMPLER_YCBCR_MODEL_CONVERSION_RGB_IDENTITY", 0),
    std::make_pair("VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_IDENTITY", 1),
    std::make_pair("VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_709", 2),
    std::make_pair("VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_601", 3),
    std::make_pair("VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_2020", 4),
    std::make_pair("VK_SAMPLER_YCBCR_MODEL_CONVERSION_RGB_IDENTITY_KHR", 0),
    std::make_pair("VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_IDENTITY_KHR", 1),
    std::make_pair("VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_709_KHR", 2),
    std::make_pair("VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_601_KHR", 3),
    std::make_pair("VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_2020_KHR", 4),
};
static void parse_VkSamplerYcbcrModelConversion(const Json::Value &obj, VkSamplerYcbcrModelConversion &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkSamplerYcbcrModelConversion)VkSamplerYcbcrModelConversion_map[std::string(_res)];
}

static std::map<std::string, int> VkSamplerYcbcrRange_map = {
    std::make_pair("VK_SAMPLER_YCBCR_RANGE_ITU_FULL", 0),
    std::make_pair("VK_SAMPLER_YCBCR_RANGE_ITU_NARROW", 1),
    std::make_pair("VK_SAMPLER_YCBCR_RANGE_ITU_FULL_KHR", 0),
    std::make_pair("VK_SAMPLER_YCBCR_RANGE_ITU_NARROW_KHR", 1),
};
static void parse_VkSamplerYcbcrRange(const Json::Value &obj, VkSamplerYcbcrRange &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkSamplerYcbcrRange)VkSamplerYcbcrRange_map[std::string(_res)];
}

static std::map<std::string, int> VkChromaLocation_map = {
    std::make_pair("VK_CHROMA_LOCATION_COSITED_EVEN", 0),
    std::make_pair("VK_CHROMA_LOCATION_MIDPOINT", 1),
    std::make_pair("VK_CHROMA_LOCATION_COSITED_EVEN_KHR", 0),
    std::make_pair("VK_CHROMA_LOCATION_MIDPOINT_KHR", 1),
};
static void parse_VkChromaLocation(const Json::Value &obj, VkChromaLocation &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkChromaLocation)VkChromaLocation_map[std::string(_res)];
}

static std::map<std::string, int> VkExternalMemoryHandleTypeFlagBits_map = {
    std::make_pair("VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT", 1UL << 0),
    std::make_pair("VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT", 1UL << 1),
    std::make_pair("VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT", 1UL << 2),
    std::make_pair("VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT", 1UL << 3),
    std::make_pair("VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_KMT_BIT", 1UL << 4),
    std::make_pair("VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_HEAP_BIT", 1UL << 5),
    std::make_pair("VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT", 1UL << 6),
    std::make_pair("VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR", 1UL << 0),
    std::make_pair("VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHR", 1UL << 1),
    std::make_pair("VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT_KHR", 1UL << 2),
    std::make_pair("VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT_KHR", 1UL << 3),
    std::make_pair("VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_KMT_BIT_KHR", 1UL << 4),
    std::make_pair("VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_HEAP_BIT_KHR", 1UL << 5),
    std::make_pair("VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT_KHR", 1UL << 6),
    std::make_pair("VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT", 1UL << 9),
    std::make_pair("VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID", 1UL << 10),
    std::make_pair("VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT", 1UL << 7),
    std::make_pair("VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_MAPPED_FOREIGN_MEMORY_BIT_EXT", 1UL << 8),
    std::make_pair("VK_EXTERNAL_MEMORY_HANDLE_TYPE_ZIRCON_VMO_BIT_FUCHSIA", 1UL << 11),
    std::make_pair("VK_EXTERNAL_MEMORY_HANDLE_TYPE_RDMA_ADDRESS_BIT_NV", 1UL << 12),
    std::make_pair("VK_EXTERNAL_MEMORY_HANDLE_TYPE_SCI_BUF_BIT_NV", 1UL << 13),
    std::make_pair("VK_EXTERNAL_MEMORY_HANDLE_TYPE_SCREEN_BUFFER_BIT_QNX", 1UL << 14),
    std::make_pair("VK_EXTERNAL_MEMORY_HANDLE_TYPE_590_BIT_HUAWEI", 1UL << 15),
};
static void parse_VkExternalMemoryHandleTypeFlagBits(const Json::Value &obj, VkExternalMemoryHandleTypeFlagBits &o)
{
    const std::string &_res = obj.asString();
    o = (VkExternalMemoryHandleTypeFlagBits)VkExternalMemoryHandleTypeFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkExternalMemoryFeatureFlagBits_map = {
    std::make_pair("VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT", 1UL << 0),
    std::make_pair("VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT", 1UL << 1),
    std::make_pair("VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT", 1UL << 2),
    std::make_pair("VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT_KHR", 1UL << 0),
    std::make_pair("VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT_KHR", 1UL << 1),
    std::make_pair("VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT_KHR", 1UL << 2),
};
static void parse_VkExternalMemoryFeatureFlagBits(const Json::Value &obj, VkExternalMemoryFeatureFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkExternalMemoryFeatureFlagBits)VkExternalMemoryFeatureFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkExternalFenceHandleTypeFlagBits_map = {
    std::make_pair("VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_FD_BIT", 1UL << 0),
    std::make_pair("VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_BIT", 1UL << 1),
    std::make_pair("VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT", 1UL << 2),
    std::make_pair("VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT", 1UL << 3),
    std::make_pair("VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_FD_BIT_KHR", 1UL << 0),
    std::make_pair("VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHR", 1UL << 1),
    std::make_pair("VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT_KHR", 1UL << 2),
    std::make_pair("VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT_KHR", 1UL << 3),
    std::make_pair("VK_EXTERNAL_FENCE_HANDLE_TYPE_SCI_SYNC_OBJ_BIT_NV", 1UL << 4),
    std::make_pair("VK_EXTERNAL_FENCE_HANDLE_TYPE_SCI_SYNC_FENCE_BIT_NV", 1UL << 5),
    std::make_pair("VK_EXTERNAL_FENCE_HANDLE_TYPE_SCI_SYNC_OBJ_BIT_NV", 1UL << 4),
    std::make_pair("VK_EXTERNAL_FENCE_HANDLE_TYPE_SCI_SYNC_FENCE_BIT_NV", 1UL << 5),
};
static void parse_VkExternalFenceHandleTypeFlagBits(const Json::Value &obj, VkExternalFenceHandleTypeFlagBits &o)
{
    const std::string &_res = obj.asString();
    o = (VkExternalFenceHandleTypeFlagBits)VkExternalFenceHandleTypeFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkExternalFenceFeatureFlagBits_map = {
    std::make_pair("VK_EXTERNAL_FENCE_FEATURE_EXPORTABLE_BIT", 1UL << 0),
    std::make_pair("VK_EXTERNAL_FENCE_FEATURE_IMPORTABLE_BIT", 1UL << 1),
    std::make_pair("VK_EXTERNAL_FENCE_FEATURE_EXPORTABLE_BIT_KHR", 1UL << 0),
    std::make_pair("VK_EXTERNAL_FENCE_FEATURE_IMPORTABLE_BIT_KHR", 1UL << 1),
};
static void parse_VkExternalFenceFeatureFlagBits(const Json::Value &obj, VkExternalFenceFeatureFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkExternalFenceFeatureFlagBits)VkExternalFenceFeatureFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkFenceImportFlagBits_map = {
    std::make_pair("VK_FENCE_IMPORT_TEMPORARY_BIT", 1UL << 0),
    std::make_pair("VK_FENCE_IMPORT_TEMPORARY_BIT_KHR", 1UL << 0),
};
static void parse_VkFenceImportFlagBits(const Json::Value &obj, VkFenceImportFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkFenceImportFlagBits)VkFenceImportFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkSemaphoreImportFlagBits_map = {
    std::make_pair("VK_SEMAPHORE_IMPORT_TEMPORARY_BIT", 1UL << 0),
    std::make_pair("VK_SEMAPHORE_IMPORT_TEMPORARY_BIT_KHR", 1UL << 0),
};
static void parse_VkSemaphoreImportFlagBits(const Json::Value &obj, VkSemaphoreImportFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkSemaphoreImportFlagBits)VkSemaphoreImportFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkExternalSemaphoreHandleTypeFlagBits_map = {
    std::make_pair("VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT", 1UL << 0),
    std::make_pair("VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT", 1UL << 1),
    std::make_pair("VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT", 1UL << 2),
    std::make_pair("VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT", 1UL << 3),
    std::make_pair("VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D11_FENCE_BIT", 1UL << 3),
    std::make_pair("VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT", 1UL << 4),
    std::make_pair("VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT_KHR", 1UL << 0),
    std::make_pair("VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHR", 1UL << 1),
    std::make_pair("VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT_KHR", 1UL << 2),
    std::make_pair("VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT_KHR", 1UL << 3),
    std::make_pair("VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT_KHR", 1UL << 4),
    std::make_pair("VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_ZIRCON_EVENT_BIT_FUCHSIA", 1UL << 7),
    std::make_pair("VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SCI_SYNC_OBJ_BIT_NV", 1UL << 5),
};
static void parse_VkExternalSemaphoreHandleTypeFlagBits(const Json::Value &obj,
                                                        VkExternalSemaphoreHandleTypeFlagBits &o)
{
    const std::string &_res = obj.asString();
    o = (VkExternalSemaphoreHandleTypeFlagBits)VkExternalSemaphoreHandleTypeFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkExternalSemaphoreFeatureFlagBits_map = {
    std::make_pair("VK_EXTERNAL_SEMAPHORE_FEATURE_EXPORTABLE_BIT", 1UL << 0),
    std::make_pair("VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT", 1UL << 1),
    std::make_pair("VK_EXTERNAL_SEMAPHORE_FEATURE_EXPORTABLE_BIT_KHR", 1UL << 0),
    std::make_pair("VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT_KHR", 1UL << 1),
};
static void parse_VkExternalSemaphoreFeatureFlagBits(const Json::Value &obj, VkExternalSemaphoreFeatureFlagBits &o)
{
    const std::string &_res = obj.asString();
    o = (VkExternalSemaphoreFeatureFlagBits)VkExternalSemaphoreFeatureFlagBits_map[std::string(_res)];
}

static void parse_VkSubgroupFeatureFlags(const Json::Value &obj, VkSubgroupFeatureFlags &o)
{
    o                       = (VkSubgroupFeatureFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkSubgroupFeatureFlagBits)VkSubgroupFeatureFlagBits_map[it];
    }
}

static void parse_VkPeerMemoryFeatureFlags(const Json::Value &obj, VkPeerMemoryFeatureFlags &o)
{
    o                       = (VkPeerMemoryFeatureFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkPeerMemoryFeatureFlagBits)VkPeerMemoryFeatureFlagBits_map[it];
    }
}

static void parse_VkMemoryAllocateFlags(const Json::Value &obj, VkMemoryAllocateFlags &o)
{
    o                       = (VkMemoryAllocateFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkMemoryAllocateFlagBits)VkMemoryAllocateFlagBits_map[it];
    }
}

static void parse_VkExternalMemoryHandleTypeFlags(const Json::Value &obj, VkExternalMemoryHandleTypeFlags &o)
{
    o                       = (VkExternalMemoryHandleTypeFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkExternalMemoryHandleTypeFlagBits)VkExternalMemoryHandleTypeFlagBits_map[it];
    }
}

static void parse_VkExternalMemoryFeatureFlags(const Json::Value &obj, VkExternalMemoryFeatureFlags &o)
{
    o                       = (VkExternalMemoryFeatureFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkExternalMemoryFeatureFlagBits)VkExternalMemoryFeatureFlagBits_map[it];
    }
}

static void parse_VkExternalFenceHandleTypeFlags(const Json::Value &obj, VkExternalFenceHandleTypeFlags &o)
{
    o                       = (VkExternalFenceHandleTypeFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkExternalFenceHandleTypeFlagBits)VkExternalFenceHandleTypeFlagBits_map[it];
    }
}

static void parse_VkExternalFenceFeatureFlags(const Json::Value &obj, VkExternalFenceFeatureFlags &o)
{
    o                       = (VkExternalFenceFeatureFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkExternalFenceFeatureFlagBits)VkExternalFenceFeatureFlagBits_map[it];
    }
}

static void parse_VkFenceImportFlags(const Json::Value &obj, VkFenceImportFlags &o)
{
    o                       = (VkFenceImportFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkFenceImportFlagBits)VkFenceImportFlagBits_map[it];
    }
}

static void parse_VkSemaphoreImportFlags(const Json::Value &obj, VkSemaphoreImportFlags &o)
{
    o                       = (VkSemaphoreImportFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkSemaphoreImportFlagBits)VkSemaphoreImportFlagBits_map[it];
    }
}

static void parse_VkExternalSemaphoreHandleTypeFlags(const Json::Value &obj, VkExternalSemaphoreHandleTypeFlags &o)
{
    o                       = (VkExternalSemaphoreHandleTypeFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkExternalSemaphoreHandleTypeFlagBits)VkExternalSemaphoreHandleTypeFlagBits_map[it];
    }
}

static void parse_VkExternalSemaphoreFeatureFlags(const Json::Value &obj, VkExternalSemaphoreFeatureFlags &o)
{
    o                       = (VkExternalSemaphoreFeatureFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkExternalSemaphoreFeatureFlagBits)VkExternalSemaphoreFeatureFlagBits_map[it];
    }
}

static void parse_VkPhysicalDeviceSubgroupProperties(const Json::Value &obj, VkPhysicalDeviceSubgroupProperties &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceSubgroupProperties *)parsePNextChain(obj);

    parse_uint32_t(obj["subgroupSize"], (o.subgroupSize));

    parse_VkShaderStageFlags(obj["supportedStages"], (o.supportedStages));

    parse_VkSubgroupFeatureFlags(obj["supportedOperations"], (o.supportedOperations));

    parse_VkBool32(obj["quadOperationsInAllStages"], (o.quadOperationsInAllStages));
}

static void parse_VkBindBufferMemoryInfo(const Json::Value &obj, VkBindBufferMemoryInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkBindBufferMemoryInfo *)parsePNextChain(obj);

    parse_VkDeviceSize(obj["memoryOffset"], (o.memoryOffset));
}

static void parse_VkBindImageMemoryInfo(const Json::Value &obj, VkBindImageMemoryInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkBindImageMemoryInfo *)parsePNextChain(obj);

    parse_VkDeviceSize(obj["memoryOffset"], (o.memoryOffset));
}

static void parse_VkPhysicalDevice16BitStorageFeatures(const Json::Value &obj, VkPhysicalDevice16BitStorageFeatures &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDevice16BitStorageFeatures *)parsePNextChain(obj);

    parse_VkBool32(obj["storageBuffer16BitAccess"], (o.storageBuffer16BitAccess));

    parse_VkBool32(obj["uniformAndStorageBuffer16BitAccess"], (o.uniformAndStorageBuffer16BitAccess));

    parse_VkBool32(obj["storagePushConstant16"], (o.storagePushConstant16));

    parse_VkBool32(obj["storageInputOutput16"], (o.storageInputOutput16));
}

static void parse_VkMemoryDedicatedRequirements(const Json::Value &obj, VkMemoryDedicatedRequirements &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkMemoryDedicatedRequirements *)parsePNextChain(obj);

    parse_VkBool32(obj["prefersDedicatedAllocation"], (o.prefersDedicatedAllocation));

    parse_VkBool32(obj["requiresDedicatedAllocation"], (o.requiresDedicatedAllocation));
}

static void parse_VkMemoryDedicatedAllocateInfo(const Json::Value &obj, VkMemoryDedicatedAllocateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkMemoryDedicatedAllocateInfo *)parsePNextChain(obj);
}

static void parse_VkMemoryAllocateFlagsInfo(const Json::Value &obj, VkMemoryAllocateFlagsInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkMemoryAllocateFlagsInfo *)parsePNextChain(obj);

    parse_VkMemoryAllocateFlags(obj["flags"], (o.flags));

    parse_uint32_t(obj["deviceMask"], (o.deviceMask));
}

static void parse_VkDeviceGroupRenderPassBeginInfo(const Json::Value &obj, VkDeviceGroupRenderPassBeginInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkDeviceGroupRenderPassBeginInfo *)parsePNextChain(obj);

    parse_uint32_t(obj["deviceMask"], (o.deviceMask));

    parse_uint32_t(obj["deviceRenderAreaCount"], (o.deviceRenderAreaCount));

    (o.pDeviceRenderAreas) = (VkRect2D *)s_globalMem.allocate((o.deviceRenderAreaCount), sizeof(VkRect2D));
    const Json::Value &obj_pDeviceRenderAreas = obj["pDeviceRenderAreas"];
    if (obj_pDeviceRenderAreas.size() == 0)
        (o.pDeviceRenderAreas) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.deviceRenderAreaCount); i++)
        {
            parse_VkRect2D(obj_pDeviceRenderAreas[i], const_cast<VkRect2D &>((o.pDeviceRenderAreas[i])));
        }
    }
}

static void parse_VkDeviceGroupCommandBufferBeginInfo(const Json::Value &obj, VkDeviceGroupCommandBufferBeginInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkDeviceGroupCommandBufferBeginInfo *)parsePNextChain(obj);

    parse_uint32_t(obj["deviceMask"], (o.deviceMask));
}

static void parse_VkDeviceGroupSubmitInfo(const Json::Value &obj, VkDeviceGroupSubmitInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkDeviceGroupSubmitInfo *)parsePNextChain(obj);

    parse_uint32_t(obj["waitSemaphoreCount"], (o.waitSemaphoreCount));

    (o.pWaitSemaphoreDeviceIndices) = (uint32_t *)s_globalMem.allocate((o.waitSemaphoreCount), sizeof(uint32_t));
    const Json::Value &obj_pWaitSemaphoreDeviceIndices_arr = obj["pWaitSemaphoreDeviceIndices"];
    for (unsigned int i = 0; i < obj_pWaitSemaphoreDeviceIndices_arr.size(); i++)
    {
        parse_uint32_t(obj_pWaitSemaphoreDeviceIndices_arr[i],
                       const_cast<uint32_t &>((o.pWaitSemaphoreDeviceIndices[i])));
    }

    parse_uint32_t(obj["commandBufferCount"], (o.commandBufferCount));

    (o.pCommandBufferDeviceMasks) = (uint32_t *)s_globalMem.allocate((o.commandBufferCount), sizeof(uint32_t));
    const Json::Value &obj_pCommandBufferDeviceMasks_arr = obj["pCommandBufferDeviceMasks"];
    for (unsigned int i = 0; i < obj_pCommandBufferDeviceMasks_arr.size(); i++)
    {
        parse_uint32_t(obj_pCommandBufferDeviceMasks_arr[i], const_cast<uint32_t &>((o.pCommandBufferDeviceMasks[i])));
    }

    parse_uint32_t(obj["signalSemaphoreCount"], (o.signalSemaphoreCount));

    (o.pSignalSemaphoreDeviceIndices) = (uint32_t *)s_globalMem.allocate((o.signalSemaphoreCount), sizeof(uint32_t));
    const Json::Value &obj_pSignalSemaphoreDeviceIndices_arr = obj["pSignalSemaphoreDeviceIndices"];
    for (unsigned int i = 0; i < obj_pSignalSemaphoreDeviceIndices_arr.size(); i++)
    {
        parse_uint32_t(obj_pSignalSemaphoreDeviceIndices_arr[i],
                       const_cast<uint32_t &>((o.pSignalSemaphoreDeviceIndices[i])));
    }
}

static void parse_VkBindBufferMemoryDeviceGroupInfo(const Json::Value &obj, VkBindBufferMemoryDeviceGroupInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkBindBufferMemoryDeviceGroupInfo *)parsePNextChain(obj);

    parse_uint32_t(obj["deviceIndexCount"], (o.deviceIndexCount));

    (o.pDeviceIndices) = (uint32_t *)s_globalMem.allocate((o.deviceIndexCount), sizeof(uint32_t));
    const Json::Value &obj_pDeviceIndices_arr = obj["pDeviceIndices"];
    for (unsigned int i = 0; i < obj_pDeviceIndices_arr.size(); i++)
    {
        parse_uint32_t(obj_pDeviceIndices_arr[i], const_cast<uint32_t &>((o.pDeviceIndices[i])));
    }
}

static void parse_VkBindImageMemoryDeviceGroupInfo(const Json::Value &obj, VkBindImageMemoryDeviceGroupInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkBindImageMemoryDeviceGroupInfo *)parsePNextChain(obj);

    parse_uint32_t(obj["deviceIndexCount"], (o.deviceIndexCount));

    (o.pDeviceIndices) = (uint32_t *)s_globalMem.allocate((o.deviceIndexCount), sizeof(uint32_t));
    const Json::Value &obj_pDeviceIndices_arr = obj["pDeviceIndices"];
    for (unsigned int i = 0; i < obj_pDeviceIndices_arr.size(); i++)
    {
        parse_uint32_t(obj_pDeviceIndices_arr[i], const_cast<uint32_t &>((o.pDeviceIndices[i])));
    }

    parse_uint32_t(obj["splitInstanceBindRegionCount"], (o.splitInstanceBindRegionCount));

    (o.pSplitInstanceBindRegions) =
        (VkRect2D *)s_globalMem.allocate((o.splitInstanceBindRegionCount), sizeof(VkRect2D));
    const Json::Value &obj_pSplitInstanceBindRegions = obj["pSplitInstanceBindRegions"];
    if (obj_pSplitInstanceBindRegions.size() == 0)
        (o.pSplitInstanceBindRegions) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.splitInstanceBindRegionCount); i++)
        {
            parse_VkRect2D(obj_pSplitInstanceBindRegions[i], const_cast<VkRect2D &>((o.pSplitInstanceBindRegions[i])));
        }
    }
}

static void parse_VkPhysicalDeviceGroupProperties(const Json::Value &obj, VkPhysicalDeviceGroupProperties &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceGroupProperties *)parsePNextChain(obj);

    parse_uint32_t(obj["physicalDeviceCount"], (o.physicalDeviceCount));

    const Json::Value &obj_physicalDevices_arr = obj["physicalDevices"];
    for (unsigned int i = 0; i < obj_physicalDevices_arr.size(); i++)
    {
        parse_VkPhysicalDevice(obj_physicalDevices_arr[i], const_cast<VkPhysicalDevice &>((o.physicalDevices[i])));
    }

    parse_VkBool32(obj["subsetAllocation"], (o.subsetAllocation));
}

static void parse_VkDeviceGroupDeviceCreateInfo(const Json::Value &obj, VkDeviceGroupDeviceCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkDeviceGroupDeviceCreateInfo *)parsePNextChain(obj);

    parse_uint32_t(obj["physicalDeviceCount"], (o.physicalDeviceCount));

    (o.pPhysicalDevices) = (VkPhysicalDevice *)s_globalMem.allocate((o.physicalDeviceCount), sizeof(VkPhysicalDevice));
    const Json::Value &obj_pPhysicalDevices_arr = obj["pPhysicalDevices"];
    for (unsigned int i = 0; i < obj_pPhysicalDevices_arr.size(); i++)
    {
        parse_VkPhysicalDevice(obj_pPhysicalDevices_arr[i], const_cast<VkPhysicalDevice &>((o.pPhysicalDevices[i])));
    }
}

static void parse_VkBufferMemoryRequirementsInfo2(const Json::Value &obj, VkBufferMemoryRequirementsInfo2 &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkBufferMemoryRequirementsInfo2 *)parsePNextChain(obj);
}

static void parse_VkImageMemoryRequirementsInfo2(const Json::Value &obj, VkImageMemoryRequirementsInfo2 &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkImageMemoryRequirementsInfo2 *)parsePNextChain(obj);
}

static void parse_VkMemoryRequirements2(const Json::Value &obj, VkMemoryRequirements2 &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkMemoryRequirements2 *)parsePNextChain(obj);

    parse_VkMemoryRequirements(obj["memoryRequirements"], (o.memoryRequirements));
}

static void parse_VkPhysicalDeviceFeatures2(const Json::Value &obj, VkPhysicalDeviceFeatures2 &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceFeatures2 *)parsePNextChain(obj);

    parse_VkPhysicalDeviceFeatures(obj["features"], (o.features));
}

static void parse_VkPhysicalDeviceProperties2(const Json::Value &obj, VkPhysicalDeviceProperties2 &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceProperties2 *)parsePNextChain(obj);

    parse_VkPhysicalDeviceProperties(obj["properties"], (o.properties));
}

static void parse_VkFormatProperties2(const Json::Value &obj, VkFormatProperties2 &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkFormatProperties2 *)parsePNextChain(obj);

    parse_VkFormatProperties(obj["formatProperties"], (o.formatProperties));
}

static void parse_VkImageFormatProperties2(const Json::Value &obj, VkImageFormatProperties2 &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkImageFormatProperties2 *)parsePNextChain(obj);

    parse_VkImageFormatProperties(obj["imageFormatProperties"], (o.imageFormatProperties));
}

static void parse_VkPhysicalDeviceImageFormatInfo2(const Json::Value &obj, VkPhysicalDeviceImageFormatInfo2 &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceImageFormatInfo2 *)parsePNextChain(obj);

    parse_VkFormat(obj["format"], (o.format));

    parse_VkImageType(obj["type"], (o.type));

    parse_VkImageTiling(obj["tiling"], (o.tiling));

    parse_VkImageUsageFlags(obj["usage"], (o.usage));

    parse_VkImageCreateFlags(obj["flags"], (o.flags));
}

static void parse_VkQueueFamilyProperties2(const Json::Value &obj, VkQueueFamilyProperties2 &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkQueueFamilyProperties2 *)parsePNextChain(obj);

    parse_VkQueueFamilyProperties(obj["queueFamilyProperties"], (o.queueFamilyProperties));
}

static void parse_VkPhysicalDeviceMemoryProperties2(const Json::Value &obj, VkPhysicalDeviceMemoryProperties2 &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceMemoryProperties2 *)parsePNextChain(obj);

    parse_VkPhysicalDeviceMemoryProperties(obj["memoryProperties"], (o.memoryProperties));
}

static void parse_VkPhysicalDevicePointClippingProperties(const Json::Value &obj,
                                                          VkPhysicalDevicePointClippingProperties &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDevicePointClippingProperties *)parsePNextChain(obj);

    parse_VkPointClippingBehavior(obj["pointClippingBehavior"], (o.pointClippingBehavior));
}

static void parse_VkInputAttachmentAspectReference(const Json::Value &obj, VkInputAttachmentAspectReference &o)
{

    parse_uint32_t(obj["subpass"], (o.subpass));

    parse_uint32_t(obj["inputAttachmentIndex"], (o.inputAttachmentIndex));

    parse_VkImageAspectFlags(obj["aspectMask"], (o.aspectMask));
}

static void parse_VkRenderPassInputAttachmentAspectCreateInfo(const Json::Value &obj,
                                                              VkRenderPassInputAttachmentAspectCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkRenderPassInputAttachmentAspectCreateInfo *)parsePNextChain(obj);

    parse_uint32_t(obj["aspectReferenceCount"], (o.aspectReferenceCount));

    (o.pAspectReferences) = (VkInputAttachmentAspectReference *)s_globalMem.allocate(
        (o.aspectReferenceCount), sizeof(VkInputAttachmentAspectReference));
    const Json::Value &obj_pAspectReferences = obj["pAspectReferences"];
    if (obj_pAspectReferences.size() == 0)
        (o.pAspectReferences) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.aspectReferenceCount); i++)
        {
            parse_VkInputAttachmentAspectReference(
                obj_pAspectReferences[i], const_cast<VkInputAttachmentAspectReference &>((o.pAspectReferences[i])));
        }
    }
}

static void parse_VkImageViewUsageCreateInfo(const Json::Value &obj, VkImageViewUsageCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkImageViewUsageCreateInfo *)parsePNextChain(obj);

    parse_VkImageUsageFlags(obj["usage"], (o.usage));
}

static void parse_VkPipelineTessellationDomainOriginStateCreateInfo(
    const Json::Value &obj, VkPipelineTessellationDomainOriginStateCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPipelineTessellationDomainOriginStateCreateInfo *)parsePNextChain(obj);

    parse_VkTessellationDomainOrigin(obj["domainOrigin"], (o.domainOrigin));
}

static void parse_VkRenderPassMultiviewCreateInfo(const Json::Value &obj, VkRenderPassMultiviewCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkRenderPassMultiviewCreateInfo *)parsePNextChain(obj);

    parse_uint32_t(obj["subpassCount"], (o.subpassCount));

    (o.pViewMasks)                        = (uint32_t *)s_globalMem.allocate((o.subpassCount), sizeof(uint32_t));
    const Json::Value &obj_pViewMasks_arr = obj["pViewMasks"];
    for (unsigned int i = 0; i < obj_pViewMasks_arr.size(); i++)
    {
        parse_uint32_t(obj_pViewMasks_arr[i], const_cast<uint32_t &>((o.pViewMasks[i])));
    }

    parse_uint32_t(obj["dependencyCount"], (o.dependencyCount));

    (o.pViewOffsets)                        = (int32_t *)s_globalMem.allocate((o.dependencyCount), sizeof(int32_t));
    const Json::Value &obj_pViewOffsets_arr = obj["pViewOffsets"];
    for (unsigned int i = 0; i < obj_pViewOffsets_arr.size(); i++)
    {
        parse_int32_t(obj_pViewOffsets_arr[i], const_cast<int32_t &>((o.pViewOffsets[i])));
    }

    parse_uint32_t(obj["correlationMaskCount"], (o.correlationMaskCount));

    (o.pCorrelationMasks) = (uint32_t *)s_globalMem.allocate((o.correlationMaskCount), sizeof(uint32_t));
    const Json::Value &obj_pCorrelationMasks_arr = obj["pCorrelationMasks"];
    for (unsigned int i = 0; i < obj_pCorrelationMasks_arr.size(); i++)
    {
        parse_uint32_t(obj_pCorrelationMasks_arr[i], const_cast<uint32_t &>((o.pCorrelationMasks[i])));
    }
}

static void parse_VkPhysicalDeviceMultiviewFeatures(const Json::Value &obj, VkPhysicalDeviceMultiviewFeatures &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceMultiviewFeatures *)parsePNextChain(obj);

    parse_VkBool32(obj["multiview"], (o.multiview));

    parse_VkBool32(obj["multiviewGeometryShader"], (o.multiviewGeometryShader));

    parse_VkBool32(obj["multiviewTessellationShader"], (o.multiviewTessellationShader));
}

static void parse_VkPhysicalDeviceMultiviewProperties(const Json::Value &obj, VkPhysicalDeviceMultiviewProperties &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceMultiviewProperties *)parsePNextChain(obj);

    parse_uint32_t(obj["maxMultiviewViewCount"], (o.maxMultiviewViewCount));

    parse_uint32_t(obj["maxMultiviewInstanceIndex"], (o.maxMultiviewInstanceIndex));
}

static void parse_VkPhysicalDeviceVariablePointersFeatures(const Json::Value &obj,
                                                           VkPhysicalDeviceVariablePointersFeatures &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceVariablePointersFeatures *)parsePNextChain(obj);

    parse_VkBool32(obj["variablePointersStorageBuffer"], (o.variablePointersStorageBuffer));

    parse_VkBool32(obj["variablePointers"], (o.variablePointers));
}

static void parse_VkPhysicalDeviceProtectedMemoryFeatures(const Json::Value &obj,
                                                          VkPhysicalDeviceProtectedMemoryFeatures &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceProtectedMemoryFeatures *)parsePNextChain(obj);

    parse_VkBool32(obj["protectedMemory"], (o.protectedMemory));
}

static void parse_VkPhysicalDeviceProtectedMemoryProperties(const Json::Value &obj,
                                                            VkPhysicalDeviceProtectedMemoryProperties &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceProtectedMemoryProperties *)parsePNextChain(obj);

    parse_VkBool32(obj["protectedNoFault"], (o.protectedNoFault));
}

static void parse_VkDeviceQueueInfo2(const Json::Value &obj, VkDeviceQueueInfo2 &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkDeviceQueueInfo2 *)parsePNextChain(obj);

    parse_VkDeviceQueueCreateFlags(obj["flags"], (o.flags));

    parse_uint32_t(obj["queueFamilyIndex"], (o.queueFamilyIndex));

    parse_uint32_t(obj["queueIndex"], (o.queueIndex));
}

static void parse_VkProtectedSubmitInfo(const Json::Value &obj, VkProtectedSubmitInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkProtectedSubmitInfo *)parsePNextChain(obj);

    parse_VkBool32(obj["protectedSubmit"], (o.protectedSubmit));
}

static void parse_VkSamplerYcbcrConversionCreateInfo(const Json::Value &obj, VkSamplerYcbcrConversionCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkSamplerYcbcrConversionCreateInfo *)parsePNextChain(obj);

    parse_VkFormat(obj["format"], (o.format));

    parse_VkSamplerYcbcrModelConversion(obj["ycbcrModel"], (o.ycbcrModel));

    parse_VkSamplerYcbcrRange(obj["ycbcrRange"], (o.ycbcrRange));

    parse_VkComponentMapping(obj["components"], (o.components));

    parse_VkChromaLocation(obj["xChromaOffset"], (o.xChromaOffset));

    parse_VkChromaLocation(obj["yChromaOffset"], (o.yChromaOffset));

    parse_VkFilter(obj["chromaFilter"], (o.chromaFilter));

    parse_VkBool32(obj["forceExplicitReconstruction"], (o.forceExplicitReconstruction));
}

static void parse_VkSamplerYcbcrConversionInfo(const Json::Value &obj, VkSamplerYcbcrConversionInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkSamplerYcbcrConversionInfo *)parsePNextChain(obj);

    uint64_t conversionInternal = 0;
    parse_uint64_t(obj["conversion"], conversionInternal);
    o.conversion = VkSamplerYcbcrConversion(reinterpret_cast<void *>(conversionInternal));
}

static void parse_VkBindImagePlaneMemoryInfo(const Json::Value &obj, VkBindImagePlaneMemoryInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkBindImagePlaneMemoryInfo *)parsePNextChain(obj);

    parse_VkImageAspectFlagBits(obj["planeAspect"], (o.planeAspect));
}

static void parse_VkImagePlaneMemoryRequirementsInfo(const Json::Value &obj, VkImagePlaneMemoryRequirementsInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkImagePlaneMemoryRequirementsInfo *)parsePNextChain(obj);

    parse_VkImageAspectFlagBits(obj["planeAspect"], (o.planeAspect));
}

static void parse_VkPhysicalDeviceSamplerYcbcrConversionFeatures(const Json::Value &obj,
                                                                 VkPhysicalDeviceSamplerYcbcrConversionFeatures &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceSamplerYcbcrConversionFeatures *)parsePNextChain(obj);

    parse_VkBool32(obj["samplerYcbcrConversion"], (o.samplerYcbcrConversion));
}

static void parse_VkSamplerYcbcrConversionImageFormatProperties(const Json::Value &obj,
                                                                VkSamplerYcbcrConversionImageFormatProperties &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkSamplerYcbcrConversionImageFormatProperties *)parsePNextChain(obj);

    parse_uint32_t(obj["combinedImageSamplerDescriptorCount"], (o.combinedImageSamplerDescriptorCount));
}

static void parse_VkExternalMemoryProperties(const Json::Value &obj, VkExternalMemoryProperties &o)
{

    parse_VkExternalMemoryFeatureFlags(obj["externalMemoryFeatures"], (o.externalMemoryFeatures));

    parse_VkExternalMemoryHandleTypeFlags(obj["exportFromImportedHandleTypes"], (o.exportFromImportedHandleTypes));

    parse_VkExternalMemoryHandleTypeFlags(obj["compatibleHandleTypes"], (o.compatibleHandleTypes));
}

static void parse_VkPhysicalDeviceExternalImageFormatInfo(const Json::Value &obj,
                                                          VkPhysicalDeviceExternalImageFormatInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceExternalImageFormatInfo *)parsePNextChain(obj);

    parse_VkExternalMemoryHandleTypeFlagBits(obj["handleType"], (o.handleType));
}

static void parse_VkExternalImageFormatProperties(const Json::Value &obj, VkExternalImageFormatProperties &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkExternalImageFormatProperties *)parsePNextChain(obj);

    parse_VkExternalMemoryProperties(obj["externalMemoryProperties"], (o.externalMemoryProperties));
}

static void parse_VkPhysicalDeviceExternalBufferInfo(const Json::Value &obj, VkPhysicalDeviceExternalBufferInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceExternalBufferInfo *)parsePNextChain(obj);

    parse_VkBufferCreateFlags(obj["flags"], (o.flags));

    parse_VkBufferUsageFlags(obj["usage"], (o.usage));

    parse_VkExternalMemoryHandleTypeFlagBits(obj["handleType"], (o.handleType));
}

static void parse_VkExternalBufferProperties(const Json::Value &obj, VkExternalBufferProperties &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkExternalBufferProperties *)parsePNextChain(obj);

    parse_VkExternalMemoryProperties(obj["externalMemoryProperties"], (o.externalMemoryProperties));
}

static void parse_VkPhysicalDeviceIDProperties(const Json::Value &obj, VkPhysicalDeviceIDProperties &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceIDProperties *)parsePNextChain(obj);

    const Json::Value &obj_deviceUUID_arr = obj["deviceUUID"];
    for (unsigned int i = 0; i < obj_deviceUUID_arr.size(); i++)
    {
        parse_uint8_t(obj_deviceUUID_arr[i], const_cast<uint8_t &>((o.deviceUUID[i])));
    }

    const Json::Value &obj_driverUUID_arr = obj["driverUUID"];
    for (unsigned int i = 0; i < obj_driverUUID_arr.size(); i++)
    {
        parse_uint8_t(obj_driverUUID_arr[i], const_cast<uint8_t &>((o.driverUUID[i])));
    }

    const Json::Value &obj_deviceLUID_arr = obj["deviceLUID"];
    for (unsigned int i = 0; i < obj_deviceLUID_arr.size(); i++)
    {
        parse_uint8_t(obj_deviceLUID_arr[i], const_cast<uint8_t &>((o.deviceLUID[i])));
    }

    parse_uint32_t(obj["deviceNodeMask"], (o.deviceNodeMask));

    parse_VkBool32(obj["deviceLUIDValid"], (o.deviceLUIDValid));
}

static void parse_VkExternalMemoryImageCreateInfo(const Json::Value &obj, VkExternalMemoryImageCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkExternalMemoryImageCreateInfo *)parsePNextChain(obj);

    parse_VkExternalMemoryHandleTypeFlags(obj["handleTypes"], (o.handleTypes));
}

static void parse_VkExternalMemoryBufferCreateInfo(const Json::Value &obj, VkExternalMemoryBufferCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkExternalMemoryBufferCreateInfo *)parsePNextChain(obj);

    parse_VkExternalMemoryHandleTypeFlags(obj["handleTypes"], (o.handleTypes));
}

static void parse_VkExportMemoryAllocateInfo(const Json::Value &obj, VkExportMemoryAllocateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkExportMemoryAllocateInfo *)parsePNextChain(obj);

    parse_VkExternalMemoryHandleTypeFlags(obj["handleTypes"], (o.handleTypes));
}

static void parse_VkPhysicalDeviceExternalFenceInfo(const Json::Value &obj, VkPhysicalDeviceExternalFenceInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceExternalFenceInfo *)parsePNextChain(obj);

    parse_VkExternalFenceHandleTypeFlagBits(obj["handleType"], (o.handleType));
}

static void parse_VkExternalFenceProperties(const Json::Value &obj, VkExternalFenceProperties &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkExternalFenceProperties *)parsePNextChain(obj);

    parse_VkExternalFenceHandleTypeFlags(obj["exportFromImportedHandleTypes"], (o.exportFromImportedHandleTypes));

    parse_VkExternalFenceHandleTypeFlags(obj["compatibleHandleTypes"], (o.compatibleHandleTypes));

    parse_VkExternalFenceFeatureFlags(obj["externalFenceFeatures"], (o.externalFenceFeatures));
}

static void parse_VkExportFenceCreateInfo(const Json::Value &obj, VkExportFenceCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkExportFenceCreateInfo *)parsePNextChain(obj);

    parse_VkExternalFenceHandleTypeFlags(obj["handleTypes"], (o.handleTypes));
}

static void parse_VkExportSemaphoreCreateInfo(const Json::Value &obj, VkExportSemaphoreCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkExportSemaphoreCreateInfo *)parsePNextChain(obj);

    parse_VkExternalSemaphoreHandleTypeFlags(obj["handleTypes"], (o.handleTypes));
}

static void parse_VkPhysicalDeviceExternalSemaphoreInfo(const Json::Value &obj,
                                                        VkPhysicalDeviceExternalSemaphoreInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceExternalSemaphoreInfo *)parsePNextChain(obj);

    parse_VkExternalSemaphoreHandleTypeFlagBits(obj["handleType"], (o.handleType));
}

static void parse_VkExternalSemaphoreProperties(const Json::Value &obj, VkExternalSemaphoreProperties &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkExternalSemaphoreProperties *)parsePNextChain(obj);

    parse_VkExternalSemaphoreHandleTypeFlags(obj["exportFromImportedHandleTypes"], (o.exportFromImportedHandleTypes));

    parse_VkExternalSemaphoreHandleTypeFlags(obj["compatibleHandleTypes"], (o.compatibleHandleTypes));

    parse_VkExternalSemaphoreFeatureFlags(obj["externalSemaphoreFeatures"], (o.externalSemaphoreFeatures));
}

static void parse_VkPhysicalDeviceMaintenance3Properties(const Json::Value &obj,
                                                         VkPhysicalDeviceMaintenance3Properties &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceMaintenance3Properties *)parsePNextChain(obj);

    parse_uint32_t(obj["maxPerSetDescriptors"], (o.maxPerSetDescriptors));

    parse_VkDeviceSize(obj["maxMemoryAllocationSize"], (o.maxMemoryAllocationSize));
}

static void parse_VkDescriptorSetLayoutSupport(const Json::Value &obj, VkDescriptorSetLayoutSupport &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkDescriptorSetLayoutSupport *)parsePNextChain(obj);

    parse_VkBool32(obj["supported"], (o.supported));
}

static void parse_VkPhysicalDeviceShaderDrawParametersFeatures(const Json::Value &obj,
                                                               VkPhysicalDeviceShaderDrawParametersFeatures &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceShaderDrawParametersFeatures *)parsePNextChain(obj);

    parse_VkBool32(obj["shaderDrawParameters"], (o.shaderDrawParameters));
}

static std::map<std::string, int> VkDriverId_map = {
    std::make_pair("VK_DRIVER_ID_AMD_PROPRIETARY", 1),
    std::make_pair("VK_DRIVER_ID_AMD_OPEN_SOURCE", 2),
    std::make_pair("VK_DRIVER_ID_MESA_RADV", 3),
    std::make_pair("VK_DRIVER_ID_NVIDIA_PROPRIETARY", 4),
    std::make_pair("VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS", 5),
    std::make_pair("VK_DRIVER_ID_INTEL_OPEN_SOURCE_MESA", 6),
    std::make_pair("VK_DRIVER_ID_IMAGINATION_PROPRIETARY", 7),
    std::make_pair("VK_DRIVER_ID_QUALCOMM_PROPRIETARY", 8),
    std::make_pair("VK_DRIVER_ID_ARM_PROPRIETARY", 9),
    std::make_pair("VK_DRIVER_ID_GOOGLE_SWIFTSHADER", 10),
    std::make_pair("VK_DRIVER_ID_GGP_PROPRIETARY", 11),
    std::make_pair("VK_DRIVER_ID_BROADCOM_PROPRIETARY", 12),
    std::make_pair("VK_DRIVER_ID_MESA_LLVMPIPE", 13),
    std::make_pair("VK_DRIVER_ID_MOLTENVK", 14),
    std::make_pair("VK_DRIVER_ID_COREAVI_PROPRIETARY", 15),
    std::make_pair("VK_DRIVER_ID_JUICE_PROPRIETARY", 16),
    std::make_pair("VK_DRIVER_ID_VERISILICON_PROPRIETARY", 17),
    std::make_pair("VK_DRIVER_ID_MESA_TURNIP", 18),
    std::make_pair("VK_DRIVER_ID_MESA_V3DV", 19),
    std::make_pair("VK_DRIVER_ID_MESA_PANVK", 20),
    std::make_pair("VK_DRIVER_ID_SAMSUNG_PROPRIETARY", 21),
    std::make_pair("VK_DRIVER_ID_MESA_VENUS", 22),
    std::make_pair("VK_DRIVER_ID_MESA_DOZEN", 23),
    std::make_pair("VK_DRIVER_ID_MESA_NVK", 24),
    std::make_pair("VK_DRIVER_ID_IMAGINATION_OPEN_SOURCE_MESA", 25),
    std::make_pair("VK_DRIVER_ID_MESA_HONEYKRISP", 26),
    std::make_pair("VK_DRIVER_ID_RESERVED_27", 27),
    std::make_pair("VK_DRIVER_ID_AMD_PROPRIETARY_KHR", 1),
    std::make_pair("VK_DRIVER_ID_AMD_OPEN_SOURCE_KHR", 2),
    std::make_pair("VK_DRIVER_ID_MESA_RADV_KHR", 3),
    std::make_pair("VK_DRIVER_ID_NVIDIA_PROPRIETARY_KHR", 4),
    std::make_pair("VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS_KHR", 5),
    std::make_pair("VK_DRIVER_ID_INTEL_OPEN_SOURCE_MESA_KHR", 6),
    std::make_pair("VK_DRIVER_ID_IMAGINATION_PROPRIETARY_KHR", 7),
    std::make_pair("VK_DRIVER_ID_QUALCOMM_PROPRIETARY_KHR", 8),
    std::make_pair("VK_DRIVER_ID_ARM_PROPRIETARY_KHR", 9),
    std::make_pair("VK_DRIVER_ID_GOOGLE_SWIFTSHADER_KHR", 10),
    std::make_pair("VK_DRIVER_ID_GGP_PROPRIETARY_KHR", 11),
    std::make_pair("VK_DRIVER_ID_BROADCOM_PROPRIETARY_KHR", 12),
};
static void parse_VkDriverId(const Json::Value &obj, VkDriverId &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkDriverId)VkDriverId_map[std::string(_res)];
}

static std::map<std::string, int> VkShaderFloatControlsIndependence_map = {
    std::make_pair("VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_32_BIT_ONLY", 0),
    std::make_pair("VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_ALL", 1),
    std::make_pair("VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_NONE", 2),
    std::make_pair("VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_32_BIT_ONLY_KHR", 0),
    std::make_pair("VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_ALL_KHR", 1),
    std::make_pair("VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_NONE_KHR", 2),
};
static void parse_VkShaderFloatControlsIndependence(const Json::Value &obj, VkShaderFloatControlsIndependence &o)
{
    const std::string &_res = obj.asString();
    o = (VkShaderFloatControlsIndependence)VkShaderFloatControlsIndependence_map[std::string(_res)];
}

static std::map<std::string, int> VkResolveModeFlagBits_map = {
    std::make_pair("VK_RESOLVE_MODE_NONE", 0),
    std::make_pair("VK_RESOLVE_MODE_SAMPLE_ZERO_BIT", 1UL << 0),
    std::make_pair("VK_RESOLVE_MODE_AVERAGE_BIT", 1UL << 1),
    std::make_pair("VK_RESOLVE_MODE_MIN_BIT", 1UL << 2),
    std::make_pair("VK_RESOLVE_MODE_MAX_BIT", 1UL << 3),
    std::make_pair("VK_RESOLVE_MODE_NONE_KHR", 0),
    std::make_pair("VK_RESOLVE_MODE_SAMPLE_ZERO_BIT_KHR", 1UL << 0),
    std::make_pair("VK_RESOLVE_MODE_AVERAGE_BIT_KHR", 1UL << 1),
    std::make_pair("VK_RESOLVE_MODE_MIN_BIT_KHR", 1UL << 2),
    std::make_pair("VK_RESOLVE_MODE_MAX_BIT_KHR", 1UL << 3),
    std::make_pair("VK_RESOLVE_MODE_EXTERNAL_FORMAT_DOWNSAMPLE_ANDROID", 1UL << 4),
};
static void parse_VkResolveModeFlagBits(const Json::Value &obj, VkResolveModeFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkResolveModeFlagBits)VkResolveModeFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkDescriptorBindingFlagBits_map = {
    std::make_pair("VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT", 1UL << 0),
    std::make_pair("VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT", 1UL << 1),
    std::make_pair("VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT", 1UL << 2),
    std::make_pair("VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT", 1UL << 3),
    std::make_pair("VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT", 1UL << 0),
    std::make_pair("VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT_EXT", 1UL << 1),
    std::make_pair("VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT", 1UL << 2),
    std::make_pair("VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT", 1UL << 3),
    std::make_pair("VK_DESCRIPTOR_BINDING_RESERVED_4_BIT_QCOM", 1UL << 4),
};
static void parse_VkDescriptorBindingFlagBits(const Json::Value &obj, VkDescriptorBindingFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkDescriptorBindingFlagBits)VkDescriptorBindingFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkSamplerReductionMode_map = {
    std::make_pair("VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE", 0),
    std::make_pair("VK_SAMPLER_REDUCTION_MODE_MIN", 1),
    std::make_pair("VK_SAMPLER_REDUCTION_MODE_MAX", 2),
    std::make_pair("VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE_EXT", 0),
    std::make_pair("VK_SAMPLER_REDUCTION_MODE_MIN_EXT", 1),
    std::make_pair("VK_SAMPLER_REDUCTION_MODE_MAX_EXT", 2),
    std::make_pair("VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE_RANGECLAMP_QCOM", 1000521000),
};
static void parse_VkSamplerReductionMode(const Json::Value &obj, VkSamplerReductionMode &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkSamplerReductionMode)VkSamplerReductionMode_map[std::string(_res)];
}

static std::map<std::string, int> VkSemaphoreType_map = {
    std::make_pair("VK_SEMAPHORE_TYPE_BINARY", 0),
    std::make_pair("VK_SEMAPHORE_TYPE_TIMELINE", 1),
    std::make_pair("VK_SEMAPHORE_TYPE_BINARY_KHR", 0),
    std::make_pair("VK_SEMAPHORE_TYPE_TIMELINE_KHR", 1),
};
static void parse_VkSemaphoreType(const Json::Value &obj, VkSemaphoreType &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkSemaphoreType)VkSemaphoreType_map[std::string(_res)];
}

static std::map<std::string, int> VkSemaphoreWaitFlagBits_map = {
    std::make_pair("VK_SEMAPHORE_WAIT_ANY_BIT", 1UL << 0),
    std::make_pair("VK_SEMAPHORE_WAIT_ANY_BIT_KHR", 1UL << 0),
};
static void parse_VkSemaphoreWaitFlagBits(const Json::Value &obj, VkSemaphoreWaitFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkSemaphoreWaitFlagBits)VkSemaphoreWaitFlagBits_map[std::string(_res)];
}

static void parse_VkResolveModeFlags(const Json::Value &obj, VkResolveModeFlags &o)
{
    o                       = (VkResolveModeFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkResolveModeFlagBits)VkResolveModeFlagBits_map[it];
    }
}

static void parse_VkDescriptorBindingFlags(const Json::Value &obj, VkDescriptorBindingFlags &o)
{
    o                       = (VkDescriptorBindingFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkDescriptorBindingFlagBits)VkDescriptorBindingFlagBits_map[it];
    }
}

static void parse_VkSemaphoreWaitFlags(const Json::Value &obj, VkSemaphoreWaitFlags &o)
{
    o                       = (VkSemaphoreWaitFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkSemaphoreWaitFlagBits)VkSemaphoreWaitFlagBits_map[it];
    }
}

static void parse_VkPhysicalDeviceVulkan11Features(const Json::Value &obj, VkPhysicalDeviceVulkan11Features &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceVulkan11Features *)parsePNextChain(obj);

    parse_VkBool32(obj["storageBuffer16BitAccess"], (o.storageBuffer16BitAccess));

    parse_VkBool32(obj["uniformAndStorageBuffer16BitAccess"], (o.uniformAndStorageBuffer16BitAccess));

    parse_VkBool32(obj["storagePushConstant16"], (o.storagePushConstant16));

    parse_VkBool32(obj["storageInputOutput16"], (o.storageInputOutput16));

    parse_VkBool32(obj["multiview"], (o.multiview));

    parse_VkBool32(obj["multiviewGeometryShader"], (o.multiviewGeometryShader));

    parse_VkBool32(obj["multiviewTessellationShader"], (o.multiviewTessellationShader));

    parse_VkBool32(obj["variablePointersStorageBuffer"], (o.variablePointersStorageBuffer));

    parse_VkBool32(obj["variablePointers"], (o.variablePointers));

    parse_VkBool32(obj["protectedMemory"], (o.protectedMemory));

    parse_VkBool32(obj["samplerYcbcrConversion"], (o.samplerYcbcrConversion));

    parse_VkBool32(obj["shaderDrawParameters"], (o.shaderDrawParameters));
}

static void parse_VkPhysicalDeviceVulkan11Properties(const Json::Value &obj, VkPhysicalDeviceVulkan11Properties &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceVulkan11Properties *)parsePNextChain(obj);

    const Json::Value &obj_deviceUUID_arr = obj["deviceUUID"];
    for (unsigned int i = 0; i < obj_deviceUUID_arr.size(); i++)
    {
        parse_uint8_t(obj_deviceUUID_arr[i], const_cast<uint8_t &>((o.deviceUUID[i])));
    }

    const Json::Value &obj_driverUUID_arr = obj["driverUUID"];
    for (unsigned int i = 0; i < obj_driverUUID_arr.size(); i++)
    {
        parse_uint8_t(obj_driverUUID_arr[i], const_cast<uint8_t &>((o.driverUUID[i])));
    }

    const Json::Value &obj_deviceLUID_arr = obj["deviceLUID"];
    for (unsigned int i = 0; i < obj_deviceLUID_arr.size(); i++)
    {
        parse_uint8_t(obj_deviceLUID_arr[i], const_cast<uint8_t &>((o.deviceLUID[i])));
    }

    parse_uint32_t(obj["deviceNodeMask"], (o.deviceNodeMask));

    parse_VkBool32(obj["deviceLUIDValid"], (o.deviceLUIDValid));

    parse_uint32_t(obj["subgroupSize"], (o.subgroupSize));

    parse_VkShaderStageFlags(obj["subgroupSupportedStages"], (o.subgroupSupportedStages));

    parse_VkSubgroupFeatureFlags(obj["subgroupSupportedOperations"], (o.subgroupSupportedOperations));

    parse_VkBool32(obj["subgroupQuadOperationsInAllStages"], (o.subgroupQuadOperationsInAllStages));

    parse_VkPointClippingBehavior(obj["pointClippingBehavior"], (o.pointClippingBehavior));

    parse_uint32_t(obj["maxMultiviewViewCount"], (o.maxMultiviewViewCount));

    parse_uint32_t(obj["maxMultiviewInstanceIndex"], (o.maxMultiviewInstanceIndex));

    parse_VkBool32(obj["protectedNoFault"], (o.protectedNoFault));

    parse_uint32_t(obj["maxPerSetDescriptors"], (o.maxPerSetDescriptors));

    parse_VkDeviceSize(obj["maxMemoryAllocationSize"], (o.maxMemoryAllocationSize));
}

static void parse_VkPhysicalDeviceVulkan12Features(const Json::Value &obj, VkPhysicalDeviceVulkan12Features &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceVulkan12Features *)parsePNextChain(obj);

    parse_VkBool32(obj["samplerMirrorClampToEdge"], (o.samplerMirrorClampToEdge));

    parse_VkBool32(obj["drawIndirectCount"], (o.drawIndirectCount));

    parse_VkBool32(obj["storageBuffer8BitAccess"], (o.storageBuffer8BitAccess));

    parse_VkBool32(obj["uniformAndStorageBuffer8BitAccess"], (o.uniformAndStorageBuffer8BitAccess));

    parse_VkBool32(obj["storagePushConstant8"], (o.storagePushConstant8));

    parse_VkBool32(obj["shaderBufferInt64Atomics"], (o.shaderBufferInt64Atomics));

    parse_VkBool32(obj["shaderSharedInt64Atomics"], (o.shaderSharedInt64Atomics));

    parse_VkBool32(obj["shaderFloat16"], (o.shaderFloat16));

    parse_VkBool32(obj["shaderInt8"], (o.shaderInt8));

    parse_VkBool32(obj["descriptorIndexing"], (o.descriptorIndexing));

    parse_VkBool32(obj["shaderInputAttachmentArrayDynamicIndexing"], (o.shaderInputAttachmentArrayDynamicIndexing));

    parse_VkBool32(obj["shaderUniformTexelBufferArrayDynamicIndexing"],
                   (o.shaderUniformTexelBufferArrayDynamicIndexing));

    parse_VkBool32(obj["shaderStorageTexelBufferArrayDynamicIndexing"],
                   (o.shaderStorageTexelBufferArrayDynamicIndexing));

    parse_VkBool32(obj["shaderUniformBufferArrayNonUniformIndexing"], (o.shaderUniformBufferArrayNonUniformIndexing));

    parse_VkBool32(obj["shaderSampledImageArrayNonUniformIndexing"], (o.shaderSampledImageArrayNonUniformIndexing));

    parse_VkBool32(obj["shaderStorageBufferArrayNonUniformIndexing"], (o.shaderStorageBufferArrayNonUniformIndexing));

    parse_VkBool32(obj["shaderStorageImageArrayNonUniformIndexing"], (o.shaderStorageImageArrayNonUniformIndexing));

    parse_VkBool32(obj["shaderInputAttachmentArrayNonUniformIndexing"],
                   (o.shaderInputAttachmentArrayNonUniformIndexing));

    parse_VkBool32(obj["shaderUniformTexelBufferArrayNonUniformIndexing"],
                   (o.shaderUniformTexelBufferArrayNonUniformIndexing));

    parse_VkBool32(obj["shaderStorageTexelBufferArrayNonUniformIndexing"],
                   (o.shaderStorageTexelBufferArrayNonUniformIndexing));

    parse_VkBool32(obj["descriptorBindingUniformBufferUpdateAfterBind"],
                   (o.descriptorBindingUniformBufferUpdateAfterBind));

    parse_VkBool32(obj["descriptorBindingSampledImageUpdateAfterBind"],
                   (o.descriptorBindingSampledImageUpdateAfterBind));

    parse_VkBool32(obj["descriptorBindingStorageImageUpdateAfterBind"],
                   (o.descriptorBindingStorageImageUpdateAfterBind));

    parse_VkBool32(obj["descriptorBindingStorageBufferUpdateAfterBind"],
                   (o.descriptorBindingStorageBufferUpdateAfterBind));

    parse_VkBool32(obj["descriptorBindingUniformTexelBufferUpdateAfterBind"],
                   (o.descriptorBindingUniformTexelBufferUpdateAfterBind));

    parse_VkBool32(obj["descriptorBindingStorageTexelBufferUpdateAfterBind"],
                   (o.descriptorBindingStorageTexelBufferUpdateAfterBind));

    parse_VkBool32(obj["descriptorBindingUpdateUnusedWhilePending"], (o.descriptorBindingUpdateUnusedWhilePending));

    parse_VkBool32(obj["descriptorBindingPartiallyBound"], (o.descriptorBindingPartiallyBound));

    parse_VkBool32(obj["descriptorBindingVariableDescriptorCount"], (o.descriptorBindingVariableDescriptorCount));

    parse_VkBool32(obj["runtimeDescriptorArray"], (o.runtimeDescriptorArray));

    parse_VkBool32(obj["samplerFilterMinmax"], (o.samplerFilterMinmax));

    parse_VkBool32(obj["scalarBlockLayout"], (o.scalarBlockLayout));

    parse_VkBool32(obj["imagelessFramebuffer"], (o.imagelessFramebuffer));

    parse_VkBool32(obj["uniformBufferStandardLayout"], (o.uniformBufferStandardLayout));

    parse_VkBool32(obj["shaderSubgroupExtendedTypes"], (o.shaderSubgroupExtendedTypes));

    parse_VkBool32(obj["separateDepthStencilLayouts"], (o.separateDepthStencilLayouts));

    parse_VkBool32(obj["hostQueryReset"], (o.hostQueryReset));

    parse_VkBool32(obj["timelineSemaphore"], (o.timelineSemaphore));

    parse_VkBool32(obj["bufferDeviceAddress"], (o.bufferDeviceAddress));

    parse_VkBool32(obj["bufferDeviceAddressCaptureReplay"], (o.bufferDeviceAddressCaptureReplay));

    parse_VkBool32(obj["bufferDeviceAddressMultiDevice"], (o.bufferDeviceAddressMultiDevice));

    parse_VkBool32(obj["vulkanMemoryModel"], (o.vulkanMemoryModel));

    parse_VkBool32(obj["vulkanMemoryModelDeviceScope"], (o.vulkanMemoryModelDeviceScope));

    parse_VkBool32(obj["vulkanMemoryModelAvailabilityVisibilityChains"],
                   (o.vulkanMemoryModelAvailabilityVisibilityChains));

    parse_VkBool32(obj["shaderOutputViewportIndex"], (o.shaderOutputViewportIndex));

    parse_VkBool32(obj["shaderOutputLayer"], (o.shaderOutputLayer));

    parse_VkBool32(obj["subgroupBroadcastDynamicId"], (o.subgroupBroadcastDynamicId));
}

static void parse_VkConformanceVersion(const Json::Value &obj, VkConformanceVersion &o)
{

    parse_uint8_t(obj["major"], (o.major));

    parse_uint8_t(obj["minor"], (o.minor));

    parse_uint8_t(obj["subminor"], (o.subminor));

    parse_uint8_t(obj["patch"], (o.patch));
}

static void parse_VkPhysicalDeviceVulkan12Properties(const Json::Value &obj, VkPhysicalDeviceVulkan12Properties &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceVulkan12Properties *)parsePNextChain(obj);

    parse_VkDriverId(obj["driverID"], (o.driverID));

    /** TODO: Handle this - driverName **/

    /** TODO: Handle this - driverInfo **/

    parse_VkConformanceVersion(obj["conformanceVersion"], (o.conformanceVersion));

    parse_VkShaderFloatControlsIndependence(obj["denormBehaviorIndependence"], (o.denormBehaviorIndependence));

    parse_VkShaderFloatControlsIndependence(obj["roundingModeIndependence"], (o.roundingModeIndependence));

    parse_VkBool32(obj["shaderSignedZeroInfNanPreserveFloat16"], (o.shaderSignedZeroInfNanPreserveFloat16));

    parse_VkBool32(obj["shaderSignedZeroInfNanPreserveFloat32"], (o.shaderSignedZeroInfNanPreserveFloat32));

    parse_VkBool32(obj["shaderSignedZeroInfNanPreserveFloat64"], (o.shaderSignedZeroInfNanPreserveFloat64));

    parse_VkBool32(obj["shaderDenormPreserveFloat16"], (o.shaderDenormPreserveFloat16));

    parse_VkBool32(obj["shaderDenormPreserveFloat32"], (o.shaderDenormPreserveFloat32));

    parse_VkBool32(obj["shaderDenormPreserveFloat64"], (o.shaderDenormPreserveFloat64));

    parse_VkBool32(obj["shaderDenormFlushToZeroFloat16"], (o.shaderDenormFlushToZeroFloat16));

    parse_VkBool32(obj["shaderDenormFlushToZeroFloat32"], (o.shaderDenormFlushToZeroFloat32));

    parse_VkBool32(obj["shaderDenormFlushToZeroFloat64"], (o.shaderDenormFlushToZeroFloat64));

    parse_VkBool32(obj["shaderRoundingModeRTEFloat16"], (o.shaderRoundingModeRTEFloat16));

    parse_VkBool32(obj["shaderRoundingModeRTEFloat32"], (o.shaderRoundingModeRTEFloat32));

    parse_VkBool32(obj["shaderRoundingModeRTEFloat64"], (o.shaderRoundingModeRTEFloat64));

    parse_VkBool32(obj["shaderRoundingModeRTZFloat16"], (o.shaderRoundingModeRTZFloat16));

    parse_VkBool32(obj["shaderRoundingModeRTZFloat32"], (o.shaderRoundingModeRTZFloat32));

    parse_VkBool32(obj["shaderRoundingModeRTZFloat64"], (o.shaderRoundingModeRTZFloat64));

    parse_uint32_t(obj["maxUpdateAfterBindDescriptorsInAllPools"], (o.maxUpdateAfterBindDescriptorsInAllPools));

    parse_VkBool32(obj["shaderUniformBufferArrayNonUniformIndexingNative"],
                   (o.shaderUniformBufferArrayNonUniformIndexingNative));

    parse_VkBool32(obj["shaderSampledImageArrayNonUniformIndexingNative"],
                   (o.shaderSampledImageArrayNonUniformIndexingNative));

    parse_VkBool32(obj["shaderStorageBufferArrayNonUniformIndexingNative"],
                   (o.shaderStorageBufferArrayNonUniformIndexingNative));

    parse_VkBool32(obj["shaderStorageImageArrayNonUniformIndexingNative"],
                   (o.shaderStorageImageArrayNonUniformIndexingNative));

    parse_VkBool32(obj["shaderInputAttachmentArrayNonUniformIndexingNative"],
                   (o.shaderInputAttachmentArrayNonUniformIndexingNative));

    parse_VkBool32(obj["robustBufferAccessUpdateAfterBind"], (o.robustBufferAccessUpdateAfterBind));

    parse_VkBool32(obj["quadDivergentImplicitLod"], (o.quadDivergentImplicitLod));

    parse_uint32_t(obj["maxPerStageDescriptorUpdateAfterBindSamplers"],
                   (o.maxPerStageDescriptorUpdateAfterBindSamplers));

    parse_uint32_t(obj["maxPerStageDescriptorUpdateAfterBindUniformBuffers"],
                   (o.maxPerStageDescriptorUpdateAfterBindUniformBuffers));

    parse_uint32_t(obj["maxPerStageDescriptorUpdateAfterBindStorageBuffers"],
                   (o.maxPerStageDescriptorUpdateAfterBindStorageBuffers));

    parse_uint32_t(obj["maxPerStageDescriptorUpdateAfterBindSampledImages"],
                   (o.maxPerStageDescriptorUpdateAfterBindSampledImages));

    parse_uint32_t(obj["maxPerStageDescriptorUpdateAfterBindStorageImages"],
                   (o.maxPerStageDescriptorUpdateAfterBindStorageImages));

    parse_uint32_t(obj["maxPerStageDescriptorUpdateAfterBindInputAttachments"],
                   (o.maxPerStageDescriptorUpdateAfterBindInputAttachments));

    parse_uint32_t(obj["maxPerStageUpdateAfterBindResources"], (o.maxPerStageUpdateAfterBindResources));

    parse_uint32_t(obj["maxDescriptorSetUpdateAfterBindSamplers"], (o.maxDescriptorSetUpdateAfterBindSamplers));

    parse_uint32_t(obj["maxDescriptorSetUpdateAfterBindUniformBuffers"],
                   (o.maxDescriptorSetUpdateAfterBindUniformBuffers));

    parse_uint32_t(obj["maxDescriptorSetUpdateAfterBindUniformBuffersDynamic"],
                   (o.maxDescriptorSetUpdateAfterBindUniformBuffersDynamic));

    parse_uint32_t(obj["maxDescriptorSetUpdateAfterBindStorageBuffers"],
                   (o.maxDescriptorSetUpdateAfterBindStorageBuffers));

    parse_uint32_t(obj["maxDescriptorSetUpdateAfterBindStorageBuffersDynamic"],
                   (o.maxDescriptorSetUpdateAfterBindStorageBuffersDynamic));

    parse_uint32_t(obj["maxDescriptorSetUpdateAfterBindSampledImages"],
                   (o.maxDescriptorSetUpdateAfterBindSampledImages));

    parse_uint32_t(obj["maxDescriptorSetUpdateAfterBindStorageImages"],
                   (o.maxDescriptorSetUpdateAfterBindStorageImages));

    parse_uint32_t(obj["maxDescriptorSetUpdateAfterBindInputAttachments"],
                   (o.maxDescriptorSetUpdateAfterBindInputAttachments));

    parse_VkResolveModeFlags(obj["supportedDepthResolveModes"], (o.supportedDepthResolveModes));

    parse_VkResolveModeFlags(obj["supportedStencilResolveModes"], (o.supportedStencilResolveModes));

    parse_VkBool32(obj["independentResolveNone"], (o.independentResolveNone));

    parse_VkBool32(obj["independentResolve"], (o.independentResolve));

    parse_VkBool32(obj["filterMinmaxSingleComponentFormats"], (o.filterMinmaxSingleComponentFormats));

    parse_VkBool32(obj["filterMinmaxImageComponentMapping"], (o.filterMinmaxImageComponentMapping));

    parse_uint64_t(obj["maxTimelineSemaphoreValueDifference"], (o.maxTimelineSemaphoreValueDifference));

    parse_VkSampleCountFlags(obj["framebufferIntegerColorSampleCounts"], (o.framebufferIntegerColorSampleCounts));
}

static void parse_VkImageFormatListCreateInfo(const Json::Value &obj, VkImageFormatListCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkImageFormatListCreateInfo *)parsePNextChain(obj);

    parse_uint32_t(obj["viewFormatCount"], (o.viewFormatCount));

    (o.pViewFormats)                        = (VkFormat *)s_globalMem.allocate((o.viewFormatCount), sizeof(VkFormat));
    const Json::Value &obj_pViewFormats_arr = obj["pViewFormats"];
    for (unsigned int i = 0; i < obj_pViewFormats_arr.size(); i++)
    {
        parse_VkFormat(obj_pViewFormats_arr[i], const_cast<VkFormat &>((o.pViewFormats[i])));
    }
}

static void parse_VkAttachmentDescription2(const Json::Value &obj, VkAttachmentDescription2 &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkAttachmentDescription2 *)parsePNextChain(obj);

    parse_VkAttachmentDescriptionFlags(obj["flags"], (o.flags));

    parse_VkFormat(obj["format"], (o.format));

    parse_VkSampleCountFlagBits(obj["samples"], (o.samples));

    parse_VkAttachmentLoadOp(obj["loadOp"], (o.loadOp));

    parse_VkAttachmentStoreOp(obj["storeOp"], (o.storeOp));

    parse_VkAttachmentLoadOp(obj["stencilLoadOp"], (o.stencilLoadOp));

    parse_VkAttachmentStoreOp(obj["stencilStoreOp"], (o.stencilStoreOp));

    parse_VkImageLayout(obj["initialLayout"], (o.initialLayout));

    parse_VkImageLayout(obj["finalLayout"], (o.finalLayout));
}

static void parse_VkAttachmentReference2(const Json::Value &obj, VkAttachmentReference2 &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkAttachmentReference2 *)parsePNextChain(obj);

    parse_uint32_t(obj["attachment"], (o.attachment));

    parse_VkImageLayout(obj["layout"], (o.layout));

    parse_VkImageAspectFlags(obj["aspectMask"], (o.aspectMask));
}

static void parse_VkSubpassDescription2(const Json::Value &obj, VkSubpassDescription2 &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkSubpassDescription2 *)parsePNextChain(obj);

    parse_VkSubpassDescriptionFlags(obj["flags"], (o.flags));

    parse_VkPipelineBindPoint(obj["pipelineBindPoint"], (o.pipelineBindPoint));

    parse_uint32_t(obj["viewMask"], (o.viewMask));

    parse_uint32_t(obj["inputAttachmentCount"], (o.inputAttachmentCount));

    (o.pInputAttachments) =
        (VkAttachmentReference2 *)s_globalMem.allocate((o.inputAttachmentCount), sizeof(VkAttachmentReference2));
    const Json::Value &obj_pInputAttachments = obj["pInputAttachments"];
    if (obj_pInputAttachments.size() == 0)
        (o.pInputAttachments) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.inputAttachmentCount); i++)
        {
            parse_VkAttachmentReference2(obj_pInputAttachments[i],
                                         const_cast<VkAttachmentReference2 &>((o.pInputAttachments[i])));
        }
    }

    parse_uint32_t(obj["colorAttachmentCount"], (o.colorAttachmentCount));

    (o.pColorAttachments) =
        (VkAttachmentReference2 *)s_globalMem.allocate((o.colorAttachmentCount), sizeof(VkAttachmentReference2));
    const Json::Value &obj_pColorAttachments = obj["pColorAttachments"];
    if (obj_pColorAttachments.size() == 0)
        (o.pColorAttachments) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.colorAttachmentCount); i++)
        {
            parse_VkAttachmentReference2(obj_pColorAttachments[i],
                                         const_cast<VkAttachmentReference2 &>((o.pColorAttachments[i])));
        }
    }

    (o.pResolveAttachments) =
        (VkAttachmentReference2 *)s_globalMem.allocate((o.colorAttachmentCount), sizeof(VkAttachmentReference2));
    const Json::Value &obj_pResolveAttachments = obj["pResolveAttachments"];
    if (obj_pResolveAttachments.size() == 0)
        (o.pResolveAttachments) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.colorAttachmentCount); i++)
        {
            parse_VkAttachmentReference2(obj_pResolveAttachments[i],
                                         const_cast<VkAttachmentReference2 &>((o.pResolveAttachments[i])));
        }
    }

    {
        const Json::Value &obj_pDepthStencilAttachment = obj["pDepthStencilAttachment"];
        if (obj_pDepthStencilAttachment.size() == 0)
        {
            (o.pDepthStencilAttachment) = nullptr;
        }
        else
        {
            (o.pDepthStencilAttachment) =
                (VkAttachmentReference2 *)s_globalMem.allocate(1, sizeof(VkAttachmentReference2));
            parse_VkAttachmentReference2(obj_pDepthStencilAttachment,
                                         const_cast<VkAttachmentReference2 &>(*(o.pDepthStencilAttachment)));
        }
    }

    parse_uint32_t(obj["preserveAttachmentCount"], (o.preserveAttachmentCount));

    (o.pPreserveAttachments) = (uint32_t *)s_globalMem.allocate((o.preserveAttachmentCount), sizeof(uint32_t));
    const Json::Value &obj_pPreserveAttachments_arr = obj["pPreserveAttachments"];
    for (unsigned int i = 0; i < obj_pPreserveAttachments_arr.size(); i++)
    {
        parse_uint32_t(obj_pPreserveAttachments_arr[i], const_cast<uint32_t &>((o.pPreserveAttachments[i])));
    }
}

static void parse_VkSubpassDependency2(const Json::Value &obj, VkSubpassDependency2 &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkSubpassDependency2 *)parsePNextChain(obj);

    parse_uint32_t(obj["srcSubpass"], (o.srcSubpass));

    parse_uint32_t(obj["dstSubpass"], (o.dstSubpass));

    parse_VkPipelineStageFlags(obj["srcStageMask"], (o.srcStageMask));

    parse_VkPipelineStageFlags(obj["dstStageMask"], (o.dstStageMask));

    parse_VkAccessFlags(obj["srcAccessMask"], (o.srcAccessMask));

    parse_VkAccessFlags(obj["dstAccessMask"], (o.dstAccessMask));

    parse_VkDependencyFlags(obj["dependencyFlags"], (o.dependencyFlags));

    parse_int32_t(obj["viewOffset"], (o.viewOffset));
}

static void parse_VkRenderPassCreateInfo2(const Json::Value &obj, VkRenderPassCreateInfo2 &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkRenderPassCreateInfo2 *)parsePNextChain(obj);

    parse_VkRenderPassCreateFlags(obj["flags"], (o.flags));

    parse_uint32_t(obj["attachmentCount"], (o.attachmentCount));

    (o.pAttachments) =
        (VkAttachmentDescription2 *)s_globalMem.allocate((o.attachmentCount), sizeof(VkAttachmentDescription2));
    const Json::Value &obj_pAttachments = obj["pAttachments"];
    if (obj_pAttachments.size() == 0)
        (o.pAttachments) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.attachmentCount); i++)
        {
            parse_VkAttachmentDescription2(obj_pAttachments[i],
                                           const_cast<VkAttachmentDescription2 &>((o.pAttachments[i])));
        }
    }

    parse_uint32_t(obj["subpassCount"], (o.subpassCount));

    (o.pSubpasses) = (VkSubpassDescription2 *)s_globalMem.allocate((o.subpassCount), sizeof(VkSubpassDescription2));
    const Json::Value &obj_pSubpasses = obj["pSubpasses"];
    if (obj_pSubpasses.size() == 0)
        (o.pSubpasses) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.subpassCount); i++)
        {
            parse_VkSubpassDescription2(obj_pSubpasses[i], const_cast<VkSubpassDescription2 &>((o.pSubpasses[i])));
        }
    }

    parse_uint32_t(obj["dependencyCount"], (o.dependencyCount));

    (o.pDependencies) = (VkSubpassDependency2 *)s_globalMem.allocate((o.dependencyCount), sizeof(VkSubpassDependency2));
    const Json::Value &obj_pDependencies = obj["pDependencies"];
    if (obj_pDependencies.size() == 0)
        (o.pDependencies) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.dependencyCount); i++)
        {
            parse_VkSubpassDependency2(obj_pDependencies[i], const_cast<VkSubpassDependency2 &>((o.pDependencies[i])));
        }
    }

    parse_uint32_t(obj["correlatedViewMaskCount"], (o.correlatedViewMaskCount));

    (o.pCorrelatedViewMasks) = (uint32_t *)s_globalMem.allocate((o.correlatedViewMaskCount), sizeof(uint32_t));
    const Json::Value &obj_pCorrelatedViewMasks_arr = obj["pCorrelatedViewMasks"];
    for (unsigned int i = 0; i < obj_pCorrelatedViewMasks_arr.size(); i++)
    {
        parse_uint32_t(obj_pCorrelatedViewMasks_arr[i], const_cast<uint32_t &>((o.pCorrelatedViewMasks[i])));
    }
}

static void parse_VkSubpassBeginInfo(const Json::Value &obj, VkSubpassBeginInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkSubpassBeginInfo *)parsePNextChain(obj);

    parse_VkSubpassContents(obj["contents"], (o.contents));
}

static void parse_VkSubpassEndInfo(const Json::Value &obj, VkSubpassEndInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkSubpassEndInfo *)parsePNextChain(obj);
}

static void parse_VkPhysicalDevice8BitStorageFeatures(const Json::Value &obj, VkPhysicalDevice8BitStorageFeatures &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDevice8BitStorageFeatures *)parsePNextChain(obj);

    parse_VkBool32(obj["storageBuffer8BitAccess"], (o.storageBuffer8BitAccess));

    parse_VkBool32(obj["uniformAndStorageBuffer8BitAccess"], (o.uniformAndStorageBuffer8BitAccess));

    parse_VkBool32(obj["storagePushConstant8"], (o.storagePushConstant8));
}

static void parse_VkPhysicalDeviceDriverProperties(const Json::Value &obj, VkPhysicalDeviceDriverProperties &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceDriverProperties *)parsePNextChain(obj);

    parse_VkDriverId(obj["driverID"], (o.driverID));

    /** TODO: Handle this - driverName **/

    /** TODO: Handle this - driverInfo **/

    parse_VkConformanceVersion(obj["conformanceVersion"], (o.conformanceVersion));
}

static void parse_VkPhysicalDeviceShaderAtomicInt64Features(const Json::Value &obj,
                                                            VkPhysicalDeviceShaderAtomicInt64Features &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceShaderAtomicInt64Features *)parsePNextChain(obj);

    parse_VkBool32(obj["shaderBufferInt64Atomics"], (o.shaderBufferInt64Atomics));

    parse_VkBool32(obj["shaderSharedInt64Atomics"], (o.shaderSharedInt64Atomics));
}

static void parse_VkPhysicalDeviceShaderFloat16Int8Features(const Json::Value &obj,
                                                            VkPhysicalDeviceShaderFloat16Int8Features &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceShaderFloat16Int8Features *)parsePNextChain(obj);

    parse_VkBool32(obj["shaderFloat16"], (o.shaderFloat16));

    parse_VkBool32(obj["shaderInt8"], (o.shaderInt8));
}

static void parse_VkPhysicalDeviceFloatControlsProperties(const Json::Value &obj,
                                                          VkPhysicalDeviceFloatControlsProperties &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceFloatControlsProperties *)parsePNextChain(obj);

    parse_VkShaderFloatControlsIndependence(obj["denormBehaviorIndependence"], (o.denormBehaviorIndependence));

    parse_VkShaderFloatControlsIndependence(obj["roundingModeIndependence"], (o.roundingModeIndependence));

    parse_VkBool32(obj["shaderSignedZeroInfNanPreserveFloat16"], (o.shaderSignedZeroInfNanPreserveFloat16));

    parse_VkBool32(obj["shaderSignedZeroInfNanPreserveFloat32"], (o.shaderSignedZeroInfNanPreserveFloat32));

    parse_VkBool32(obj["shaderSignedZeroInfNanPreserveFloat64"], (o.shaderSignedZeroInfNanPreserveFloat64));

    parse_VkBool32(obj["shaderDenormPreserveFloat16"], (o.shaderDenormPreserveFloat16));

    parse_VkBool32(obj["shaderDenormPreserveFloat32"], (o.shaderDenormPreserveFloat32));

    parse_VkBool32(obj["shaderDenormPreserveFloat64"], (o.shaderDenormPreserveFloat64));

    parse_VkBool32(obj["shaderDenormFlushToZeroFloat16"], (o.shaderDenormFlushToZeroFloat16));

    parse_VkBool32(obj["shaderDenormFlushToZeroFloat32"], (o.shaderDenormFlushToZeroFloat32));

    parse_VkBool32(obj["shaderDenormFlushToZeroFloat64"], (o.shaderDenormFlushToZeroFloat64));

    parse_VkBool32(obj["shaderRoundingModeRTEFloat16"], (o.shaderRoundingModeRTEFloat16));

    parse_VkBool32(obj["shaderRoundingModeRTEFloat32"], (o.shaderRoundingModeRTEFloat32));

    parse_VkBool32(obj["shaderRoundingModeRTEFloat64"], (o.shaderRoundingModeRTEFloat64));

    parse_VkBool32(obj["shaderRoundingModeRTZFloat16"], (o.shaderRoundingModeRTZFloat16));

    parse_VkBool32(obj["shaderRoundingModeRTZFloat32"], (o.shaderRoundingModeRTZFloat32));

    parse_VkBool32(obj["shaderRoundingModeRTZFloat64"], (o.shaderRoundingModeRTZFloat64));
}

static void parse_VkDescriptorSetLayoutBindingFlagsCreateInfo(const Json::Value &obj,
                                                              VkDescriptorSetLayoutBindingFlagsCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkDescriptorSetLayoutBindingFlagsCreateInfo *)parsePNextChain(obj);

    parse_uint32_t(obj["bindingCount"], (o.bindingCount));

    (o.pBindingFlags) =
        (VkDescriptorBindingFlags *)s_globalMem.allocate((o.bindingCount), sizeof(VkDescriptorBindingFlags));
    const Json::Value &obj_pBindingFlags_arr = obj["pBindingFlags"];
    for (unsigned int i = 0; i < obj_pBindingFlags_arr.size(); i++)
    {
        parse_VkDescriptorBindingFlags(obj_pBindingFlags_arr[i],
                                       const_cast<VkDescriptorBindingFlags &>((o.pBindingFlags[i])));
    }
}

static void parse_VkPhysicalDeviceDescriptorIndexingFeatures(const Json::Value &obj,
                                                             VkPhysicalDeviceDescriptorIndexingFeatures &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceDescriptorIndexingFeatures *)parsePNextChain(obj);

    parse_VkBool32(obj["shaderInputAttachmentArrayDynamicIndexing"], (o.shaderInputAttachmentArrayDynamicIndexing));

    parse_VkBool32(obj["shaderUniformTexelBufferArrayDynamicIndexing"],
                   (o.shaderUniformTexelBufferArrayDynamicIndexing));

    parse_VkBool32(obj["shaderStorageTexelBufferArrayDynamicIndexing"],
                   (o.shaderStorageTexelBufferArrayDynamicIndexing));

    parse_VkBool32(obj["shaderUniformBufferArrayNonUniformIndexing"], (o.shaderUniformBufferArrayNonUniformIndexing));

    parse_VkBool32(obj["shaderSampledImageArrayNonUniformIndexing"], (o.shaderSampledImageArrayNonUniformIndexing));

    parse_VkBool32(obj["shaderStorageBufferArrayNonUniformIndexing"], (o.shaderStorageBufferArrayNonUniformIndexing));

    parse_VkBool32(obj["shaderStorageImageArrayNonUniformIndexing"], (o.shaderStorageImageArrayNonUniformIndexing));

    parse_VkBool32(obj["shaderInputAttachmentArrayNonUniformIndexing"],
                   (o.shaderInputAttachmentArrayNonUniformIndexing));

    parse_VkBool32(obj["shaderUniformTexelBufferArrayNonUniformIndexing"],
                   (o.shaderUniformTexelBufferArrayNonUniformIndexing));

    parse_VkBool32(obj["shaderStorageTexelBufferArrayNonUniformIndexing"],
                   (o.shaderStorageTexelBufferArrayNonUniformIndexing));

    parse_VkBool32(obj["descriptorBindingUniformBufferUpdateAfterBind"],
                   (o.descriptorBindingUniformBufferUpdateAfterBind));

    parse_VkBool32(obj["descriptorBindingSampledImageUpdateAfterBind"],
                   (o.descriptorBindingSampledImageUpdateAfterBind));

    parse_VkBool32(obj["descriptorBindingStorageImageUpdateAfterBind"],
                   (o.descriptorBindingStorageImageUpdateAfterBind));

    parse_VkBool32(obj["descriptorBindingStorageBufferUpdateAfterBind"],
                   (o.descriptorBindingStorageBufferUpdateAfterBind));

    parse_VkBool32(obj["descriptorBindingUniformTexelBufferUpdateAfterBind"],
                   (o.descriptorBindingUniformTexelBufferUpdateAfterBind));

    parse_VkBool32(obj["descriptorBindingStorageTexelBufferUpdateAfterBind"],
                   (o.descriptorBindingStorageTexelBufferUpdateAfterBind));

    parse_VkBool32(obj["descriptorBindingUpdateUnusedWhilePending"], (o.descriptorBindingUpdateUnusedWhilePending));

    parse_VkBool32(obj["descriptorBindingPartiallyBound"], (o.descriptorBindingPartiallyBound));

    parse_VkBool32(obj["descriptorBindingVariableDescriptorCount"], (o.descriptorBindingVariableDescriptorCount));

    parse_VkBool32(obj["runtimeDescriptorArray"], (o.runtimeDescriptorArray));
}

static void parse_VkPhysicalDeviceDescriptorIndexingProperties(const Json::Value &obj,
                                                               VkPhysicalDeviceDescriptorIndexingProperties &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceDescriptorIndexingProperties *)parsePNextChain(obj);

    parse_uint32_t(obj["maxUpdateAfterBindDescriptorsInAllPools"], (o.maxUpdateAfterBindDescriptorsInAllPools));

    parse_VkBool32(obj["shaderUniformBufferArrayNonUniformIndexingNative"],
                   (o.shaderUniformBufferArrayNonUniformIndexingNative));

    parse_VkBool32(obj["shaderSampledImageArrayNonUniformIndexingNative"],
                   (o.shaderSampledImageArrayNonUniformIndexingNative));

    parse_VkBool32(obj["shaderStorageBufferArrayNonUniformIndexingNative"],
                   (o.shaderStorageBufferArrayNonUniformIndexingNative));

    parse_VkBool32(obj["shaderStorageImageArrayNonUniformIndexingNative"],
                   (o.shaderStorageImageArrayNonUniformIndexingNative));

    parse_VkBool32(obj["shaderInputAttachmentArrayNonUniformIndexingNative"],
                   (o.shaderInputAttachmentArrayNonUniformIndexingNative));

    parse_VkBool32(obj["robustBufferAccessUpdateAfterBind"], (o.robustBufferAccessUpdateAfterBind));

    parse_VkBool32(obj["quadDivergentImplicitLod"], (o.quadDivergentImplicitLod));

    parse_uint32_t(obj["maxPerStageDescriptorUpdateAfterBindSamplers"],
                   (o.maxPerStageDescriptorUpdateAfterBindSamplers));

    parse_uint32_t(obj["maxPerStageDescriptorUpdateAfterBindUniformBuffers"],
                   (o.maxPerStageDescriptorUpdateAfterBindUniformBuffers));

    parse_uint32_t(obj["maxPerStageDescriptorUpdateAfterBindStorageBuffers"],
                   (o.maxPerStageDescriptorUpdateAfterBindStorageBuffers));

    parse_uint32_t(obj["maxPerStageDescriptorUpdateAfterBindSampledImages"],
                   (o.maxPerStageDescriptorUpdateAfterBindSampledImages));

    parse_uint32_t(obj["maxPerStageDescriptorUpdateAfterBindStorageImages"],
                   (o.maxPerStageDescriptorUpdateAfterBindStorageImages));

    parse_uint32_t(obj["maxPerStageDescriptorUpdateAfterBindInputAttachments"],
                   (o.maxPerStageDescriptorUpdateAfterBindInputAttachments));

    parse_uint32_t(obj["maxPerStageUpdateAfterBindResources"], (o.maxPerStageUpdateAfterBindResources));

    parse_uint32_t(obj["maxDescriptorSetUpdateAfterBindSamplers"], (o.maxDescriptorSetUpdateAfterBindSamplers));

    parse_uint32_t(obj["maxDescriptorSetUpdateAfterBindUniformBuffers"],
                   (o.maxDescriptorSetUpdateAfterBindUniformBuffers));

    parse_uint32_t(obj["maxDescriptorSetUpdateAfterBindUniformBuffersDynamic"],
                   (o.maxDescriptorSetUpdateAfterBindUniformBuffersDynamic));

    parse_uint32_t(obj["maxDescriptorSetUpdateAfterBindStorageBuffers"],
                   (o.maxDescriptorSetUpdateAfterBindStorageBuffers));

    parse_uint32_t(obj["maxDescriptorSetUpdateAfterBindStorageBuffersDynamic"],
                   (o.maxDescriptorSetUpdateAfterBindStorageBuffersDynamic));

    parse_uint32_t(obj["maxDescriptorSetUpdateAfterBindSampledImages"],
                   (o.maxDescriptorSetUpdateAfterBindSampledImages));

    parse_uint32_t(obj["maxDescriptorSetUpdateAfterBindStorageImages"],
                   (o.maxDescriptorSetUpdateAfterBindStorageImages));

    parse_uint32_t(obj["maxDescriptorSetUpdateAfterBindInputAttachments"],
                   (o.maxDescriptorSetUpdateAfterBindInputAttachments));
}

static void parse_VkDescriptorSetVariableDescriptorCountAllocateInfo(
    const Json::Value &obj, VkDescriptorSetVariableDescriptorCountAllocateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkDescriptorSetVariableDescriptorCountAllocateInfo *)parsePNextChain(obj);

    parse_uint32_t(obj["descriptorSetCount"], (o.descriptorSetCount));

    (o.pDescriptorCounts) = (uint32_t *)s_globalMem.allocate((o.descriptorSetCount), sizeof(uint32_t));
    const Json::Value &obj_pDescriptorCounts_arr = obj["pDescriptorCounts"];
    for (unsigned int i = 0; i < obj_pDescriptorCounts_arr.size(); i++)
    {
        parse_uint32_t(obj_pDescriptorCounts_arr[i], const_cast<uint32_t &>((o.pDescriptorCounts[i])));
    }
}

static void parse_VkDescriptorSetVariableDescriptorCountLayoutSupport(
    const Json::Value &obj, VkDescriptorSetVariableDescriptorCountLayoutSupport &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkDescriptorSetVariableDescriptorCountLayoutSupport *)parsePNextChain(obj);

    parse_uint32_t(obj["maxVariableDescriptorCount"], (o.maxVariableDescriptorCount));
}

static void parse_VkSubpassDescriptionDepthStencilResolve(const Json::Value &obj,
                                                          VkSubpassDescriptionDepthStencilResolve &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkSubpassDescriptionDepthStencilResolve *)parsePNextChain(obj);

    parse_VkResolveModeFlagBits(obj["depthResolveMode"], (o.depthResolveMode));

    parse_VkResolveModeFlagBits(obj["stencilResolveMode"], (o.stencilResolveMode));

    {
        const Json::Value &obj_pDepthStencilResolveAttachment = obj["pDepthStencilResolveAttachment"];
        if (obj_pDepthStencilResolveAttachment.size() == 0)
        {
            (o.pDepthStencilResolveAttachment) = nullptr;
        }
        else
        {
            (o.pDepthStencilResolveAttachment) =
                (VkAttachmentReference2 *)s_globalMem.allocate(1, sizeof(VkAttachmentReference2));
            parse_VkAttachmentReference2(obj_pDepthStencilResolveAttachment,
                                         const_cast<VkAttachmentReference2 &>(*(o.pDepthStencilResolveAttachment)));
        }
    }
}

static void parse_VkPhysicalDeviceDepthStencilResolveProperties(const Json::Value &obj,
                                                                VkPhysicalDeviceDepthStencilResolveProperties &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceDepthStencilResolveProperties *)parsePNextChain(obj);

    parse_VkResolveModeFlags(obj["supportedDepthResolveModes"], (o.supportedDepthResolveModes));

    parse_VkResolveModeFlags(obj["supportedStencilResolveModes"], (o.supportedStencilResolveModes));

    parse_VkBool32(obj["independentResolveNone"], (o.independentResolveNone));

    parse_VkBool32(obj["independentResolve"], (o.independentResolve));
}

static void parse_VkPhysicalDeviceScalarBlockLayoutFeatures(const Json::Value &obj,
                                                            VkPhysicalDeviceScalarBlockLayoutFeatures &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceScalarBlockLayoutFeatures *)parsePNextChain(obj);

    parse_VkBool32(obj["scalarBlockLayout"], (o.scalarBlockLayout));
}

static void parse_VkImageStencilUsageCreateInfo(const Json::Value &obj, VkImageStencilUsageCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkImageStencilUsageCreateInfo *)parsePNextChain(obj);

    parse_VkImageUsageFlags(obj["stencilUsage"], (o.stencilUsage));
}

static void parse_VkSamplerReductionModeCreateInfo(const Json::Value &obj, VkSamplerReductionModeCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkSamplerReductionModeCreateInfo *)parsePNextChain(obj);

    parse_VkSamplerReductionMode(obj["reductionMode"], (o.reductionMode));
}

static void parse_VkPhysicalDeviceSamplerFilterMinmaxProperties(const Json::Value &obj,
                                                                VkPhysicalDeviceSamplerFilterMinmaxProperties &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceSamplerFilterMinmaxProperties *)parsePNextChain(obj);

    parse_VkBool32(obj["filterMinmaxSingleComponentFormats"], (o.filterMinmaxSingleComponentFormats));

    parse_VkBool32(obj["filterMinmaxImageComponentMapping"], (o.filterMinmaxImageComponentMapping));
}

static void parse_VkPhysicalDeviceVulkanMemoryModelFeatures(const Json::Value &obj,
                                                            VkPhysicalDeviceVulkanMemoryModelFeatures &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceVulkanMemoryModelFeatures *)parsePNextChain(obj);

    parse_VkBool32(obj["vulkanMemoryModel"], (o.vulkanMemoryModel));

    parse_VkBool32(obj["vulkanMemoryModelDeviceScope"], (o.vulkanMemoryModelDeviceScope));

    parse_VkBool32(obj["vulkanMemoryModelAvailabilityVisibilityChains"],
                   (o.vulkanMemoryModelAvailabilityVisibilityChains));
}

static void parse_VkPhysicalDeviceImagelessFramebufferFeatures(const Json::Value &obj,
                                                               VkPhysicalDeviceImagelessFramebufferFeatures &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceImagelessFramebufferFeatures *)parsePNextChain(obj);

    parse_VkBool32(obj["imagelessFramebuffer"], (o.imagelessFramebuffer));
}

static void parse_VkFramebufferAttachmentImageInfo(const Json::Value &obj, VkFramebufferAttachmentImageInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkFramebufferAttachmentImageInfo *)parsePNextChain(obj);

    parse_VkImageCreateFlags(obj["flags"], (o.flags));

    parse_VkImageUsageFlags(obj["usage"], (o.usage));

    parse_uint32_t(obj["width"], (o.width));

    parse_uint32_t(obj["height"], (o.height));

    parse_uint32_t(obj["layerCount"], (o.layerCount));

    parse_uint32_t(obj["viewFormatCount"], (o.viewFormatCount));

    (o.pViewFormats)                        = (VkFormat *)s_globalMem.allocate((o.viewFormatCount), sizeof(VkFormat));
    const Json::Value &obj_pViewFormats_arr = obj["pViewFormats"];
    for (unsigned int i = 0; i < obj_pViewFormats_arr.size(); i++)
    {
        parse_VkFormat(obj_pViewFormats_arr[i], const_cast<VkFormat &>((o.pViewFormats[i])));
    }
}

static void parse_VkFramebufferAttachmentsCreateInfo(const Json::Value &obj, VkFramebufferAttachmentsCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkFramebufferAttachmentsCreateInfo *)parsePNextChain(obj);

    parse_uint32_t(obj["attachmentImageInfoCount"], (o.attachmentImageInfoCount));

    (o.pAttachmentImageInfos) = (VkFramebufferAttachmentImageInfo *)s_globalMem.allocate(
        (o.attachmentImageInfoCount), sizeof(VkFramebufferAttachmentImageInfo));
    const Json::Value &obj_pAttachmentImageInfos = obj["pAttachmentImageInfos"];
    if (obj_pAttachmentImageInfos.size() == 0)
        (o.pAttachmentImageInfos) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.attachmentImageInfoCount); i++)
        {
            parse_VkFramebufferAttachmentImageInfo(
                obj_pAttachmentImageInfos[i],
                const_cast<VkFramebufferAttachmentImageInfo &>((o.pAttachmentImageInfos[i])));
        }
    }
}

static void parse_VkRenderPassAttachmentBeginInfo(const Json::Value &obj, VkRenderPassAttachmentBeginInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkRenderPassAttachmentBeginInfo *)parsePNextChain(obj);

    parse_uint32_t(obj["attachmentCount"], (o.attachmentCount));

    (o.pAttachments) = (VkImageView *)s_globalMem.allocate((o.attachmentCount), sizeof(VkImageView));
    const Json::Value &obj_pAttachments_arr = obj["pAttachments"];
    for (unsigned int i = 0; i < obj_pAttachments_arr.size(); i++)
    {
        parse_VkImageView(obj_pAttachments_arr[i], const_cast<VkImageView &>((o.pAttachments[i])));
    }
}

static void parse_VkPhysicalDeviceUniformBufferStandardLayoutFeatures(
    const Json::Value &obj, VkPhysicalDeviceUniformBufferStandardLayoutFeatures &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceUniformBufferStandardLayoutFeatures *)parsePNextChain(obj);

    parse_VkBool32(obj["uniformBufferStandardLayout"], (o.uniformBufferStandardLayout));
}

static void parse_VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures(
    const Json::Value &obj, VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures *)parsePNextChain(obj);

    parse_VkBool32(obj["shaderSubgroupExtendedTypes"], (o.shaderSubgroupExtendedTypes));
}

static void parse_VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures(
    const Json::Value &obj, VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures *)parsePNextChain(obj);

    parse_VkBool32(obj["separateDepthStencilLayouts"], (o.separateDepthStencilLayouts));
}

static void parse_VkAttachmentReferenceStencilLayout(const Json::Value &obj, VkAttachmentReferenceStencilLayout &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkAttachmentReferenceStencilLayout *)parsePNextChain(obj);

    parse_VkImageLayout(obj["stencilLayout"], (o.stencilLayout));
}

static void parse_VkAttachmentDescriptionStencilLayout(const Json::Value &obj, VkAttachmentDescriptionStencilLayout &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkAttachmentDescriptionStencilLayout *)parsePNextChain(obj);

    parse_VkImageLayout(obj["stencilInitialLayout"], (o.stencilInitialLayout));

    parse_VkImageLayout(obj["stencilFinalLayout"], (o.stencilFinalLayout));
}

static void parse_VkPhysicalDeviceHostQueryResetFeatures(const Json::Value &obj,
                                                         VkPhysicalDeviceHostQueryResetFeatures &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceHostQueryResetFeatures *)parsePNextChain(obj);

    parse_VkBool32(obj["hostQueryReset"], (o.hostQueryReset));
}

static void parse_VkPhysicalDeviceTimelineSemaphoreFeatures(const Json::Value &obj,
                                                            VkPhysicalDeviceTimelineSemaphoreFeatures &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceTimelineSemaphoreFeatures *)parsePNextChain(obj);

    parse_VkBool32(obj["timelineSemaphore"], (o.timelineSemaphore));
}

static void parse_VkPhysicalDeviceTimelineSemaphoreProperties(const Json::Value &obj,
                                                              VkPhysicalDeviceTimelineSemaphoreProperties &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceTimelineSemaphoreProperties *)parsePNextChain(obj);

    parse_uint64_t(obj["maxTimelineSemaphoreValueDifference"], (o.maxTimelineSemaphoreValueDifference));
}

static void parse_VkSemaphoreTypeCreateInfo(const Json::Value &obj, VkSemaphoreTypeCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkSemaphoreTypeCreateInfo *)parsePNextChain(obj);

    parse_VkSemaphoreType(obj["semaphoreType"], (o.semaphoreType));

    parse_uint64_t(obj["initialValue"], (o.initialValue));
}

static void parse_VkTimelineSemaphoreSubmitInfo(const Json::Value &obj, VkTimelineSemaphoreSubmitInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkTimelineSemaphoreSubmitInfo *)parsePNextChain(obj);

    parse_uint32_t(obj["waitSemaphoreValueCount"], (o.waitSemaphoreValueCount));

    parse_uint32_t(obj["signalSemaphoreValueCount"], (o.signalSemaphoreValueCount));
}

static void parse_VkSemaphoreWaitInfo(const Json::Value &obj, VkSemaphoreWaitInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkSemaphoreWaitInfo *)parsePNextChain(obj);

    parse_VkSemaphoreWaitFlags(obj["flags"], (o.flags));

    parse_uint32_t(obj["semaphoreCount"], (o.semaphoreCount));

    (o.pSemaphores) = (VkSemaphore *)s_globalMem.allocate((o.semaphoreCount), sizeof(VkSemaphore));
    const Json::Value &obj_pSemaphores_arr = obj["pSemaphores"];
    for (unsigned int i = 0; i < obj_pSemaphores_arr.size(); i++)
    {
        parse_VkSemaphore(obj_pSemaphores_arr[i], const_cast<VkSemaphore &>((o.pSemaphores[i])));
    }

    (o.pValues)                        = (uint64_t *)s_globalMem.allocate((o.semaphoreCount), sizeof(uint64_t));
    const Json::Value &obj_pValues_arr = obj["pValues"];
    for (unsigned int i = 0; i < obj_pValues_arr.size(); i++)
    {
        parse_uint64_t(obj_pValues_arr[i], const_cast<uint64_t &>((o.pValues[i])));
    }
}

static void parse_VkSemaphoreSignalInfo(const Json::Value &obj, VkSemaphoreSignalInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkSemaphoreSignalInfo *)parsePNextChain(obj);

    parse_uint64_t(obj["value"], (o.value));
}

static void parse_VkPhysicalDeviceBufferDeviceAddressFeatures(const Json::Value &obj,
                                                              VkPhysicalDeviceBufferDeviceAddressFeatures &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceBufferDeviceAddressFeatures *)parsePNextChain(obj);

    parse_VkBool32(obj["bufferDeviceAddress"], (o.bufferDeviceAddress));

    parse_VkBool32(obj["bufferDeviceAddressCaptureReplay"], (o.bufferDeviceAddressCaptureReplay));

    parse_VkBool32(obj["bufferDeviceAddressMultiDevice"], (o.bufferDeviceAddressMultiDevice));
}

static void parse_VkBufferDeviceAddressInfo(const Json::Value &obj, VkBufferDeviceAddressInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkBufferDeviceAddressInfo *)parsePNextChain(obj);
}

static void parse_VkBufferOpaqueCaptureAddressCreateInfo(const Json::Value &obj,
                                                         VkBufferOpaqueCaptureAddressCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkBufferOpaqueCaptureAddressCreateInfo *)parsePNextChain(obj);

    parse_uint64_t(obj["opaqueCaptureAddress"], (o.opaqueCaptureAddress));
}

static void parse_VkMemoryOpaqueCaptureAddressAllocateInfo(const Json::Value &obj,
                                                           VkMemoryOpaqueCaptureAddressAllocateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkMemoryOpaqueCaptureAddressAllocateInfo *)parsePNextChain(obj);

    parse_uint64_t(obj["opaqueCaptureAddress"], (o.opaqueCaptureAddress));
}

static void parse_VkDeviceMemoryOpaqueCaptureAddressInfo(const Json::Value &obj,
                                                         VkDeviceMemoryOpaqueCaptureAddressInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkDeviceMemoryOpaqueCaptureAddressInfo *)parsePNextChain(obj);
}

static std::map<std::string, int> VkFaultLevel_map = {
    std::make_pair("VK_FAULT_LEVEL_UNASSIGNED", 0),
    std::make_pair("VK_FAULT_LEVEL_CRITICAL", 1),
    std::make_pair("VK_FAULT_LEVEL_RECOVERABLE", 2),
    std::make_pair("VK_FAULT_LEVEL_WARNING", 3),
};
static void parse_VkFaultLevel(const Json::Value &obj, VkFaultLevel &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkFaultLevel)VkFaultLevel_map[std::string(_res)];
}

static std::map<std::string, int> VkFaultType_map = {
    std::make_pair("VK_FAULT_TYPE_INVALID", 0),           std::make_pair("VK_FAULT_TYPE_UNASSIGNED", 1),
    std::make_pair("VK_FAULT_TYPE_IMPLEMENTATION", 2),    std::make_pair("VK_FAULT_TYPE_SYSTEM", 3),
    std::make_pair("VK_FAULT_TYPE_PHYSICAL_DEVICE", 4),   std::make_pair("VK_FAULT_TYPE_COMMAND_BUFFER_FULL", 5),
    std::make_pair("VK_FAULT_TYPE_INVALID_API_USAGE", 6),
};
static void parse_VkFaultType(const Json::Value &obj, VkFaultType &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkFaultType)VkFaultType_map[std::string(_res)];
}

static std::map<std::string, int> VkFaultQueryBehavior_map = {
    std::make_pair("VK_FAULT_QUERY_BEHAVIOR_GET_AND_CLEAR_ALL_FAULTS", 0),
};
static void parse_VkFaultQueryBehavior(const Json::Value &obj, VkFaultQueryBehavior &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkFaultQueryBehavior)VkFaultQueryBehavior_map[std::string(_res)];
}

static std::map<std::string, int> VkPipelineMatchControl_map = {
    std::make_pair("VK_PIPELINE_MATCH_CONTROL_APPLICATION_UUID_EXACT_MATCH", 0),
};
static void parse_VkPipelineMatchControl(const Json::Value &obj, VkPipelineMatchControl &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkPipelineMatchControl)VkPipelineMatchControl_map[std::string(_res)];
}

static std::map<std::string, int> VkPipelineCacheValidationVersion_map = {
    std::make_pair("VK_PIPELINE_CACHE_VALIDATION_VERSION_SAFETY_CRITICAL_ONE", 1),
};
static void parse_VkPipelineCacheValidationVersion(const Json::Value &obj, VkPipelineCacheValidationVersion &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkPipelineCacheValidationVersion)VkPipelineCacheValidationVersion_map[std::string(_res)];
}

static void parse_VkPhysicalDeviceVulkanSC10Features(const Json::Value &obj, VkPhysicalDeviceVulkanSC10Features &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceVulkanSC10Features *)parsePNextChain(obj);

    parse_VkBool32(obj["shaderAtomicInstructions"], (o.shaderAtomicInstructions));
}

static void parse_VkPhysicalDeviceVulkanSC10Properties(const Json::Value &obj, VkPhysicalDeviceVulkanSC10Properties &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceVulkanSC10Properties *)parsePNextChain(obj);

    parse_VkBool32(obj["deviceNoDynamicHostAllocations"], (o.deviceNoDynamicHostAllocations));

    parse_VkBool32(obj["deviceDestroyFreesMemory"], (o.deviceDestroyFreesMemory));

    parse_VkBool32(obj["commandPoolMultipleCommandBuffersRecording"], (o.commandPoolMultipleCommandBuffersRecording));

    parse_VkBool32(obj["commandPoolResetCommandBuffer"], (o.commandPoolResetCommandBuffer));

    parse_VkBool32(obj["commandBufferSimultaneousUse"], (o.commandBufferSimultaneousUse));

    parse_VkBool32(obj["secondaryCommandBufferNullOrImagelessFramebuffer"],
                   (o.secondaryCommandBufferNullOrImagelessFramebuffer));

    parse_VkBool32(obj["recycleDescriptorSetMemory"], (o.recycleDescriptorSetMemory));

    parse_VkBool32(obj["recyclePipelineMemory"], (o.recyclePipelineMemory));

    parse_uint32_t(obj["maxRenderPassSubpasses"], (o.maxRenderPassSubpasses));

    parse_uint32_t(obj["maxRenderPassDependencies"], (o.maxRenderPassDependencies));

    parse_uint32_t(obj["maxSubpassInputAttachments"], (o.maxSubpassInputAttachments));

    parse_uint32_t(obj["maxSubpassPreserveAttachments"], (o.maxSubpassPreserveAttachments));

    parse_uint32_t(obj["maxFramebufferAttachments"], (o.maxFramebufferAttachments));

    parse_uint32_t(obj["maxDescriptorSetLayoutBindings"], (o.maxDescriptorSetLayoutBindings));

    parse_uint32_t(obj["maxQueryFaultCount"], (o.maxQueryFaultCount));

    parse_uint32_t(obj["maxCallbackFaultCount"], (o.maxCallbackFaultCount));

    parse_uint32_t(obj["maxCommandPoolCommandBuffers"], (o.maxCommandPoolCommandBuffers));

    parse_VkDeviceSize(obj["maxCommandBufferSize"], (o.maxCommandBufferSize));
}

static void parse_VkPipelinePoolSize(const Json::Value &obj, VkPipelinePoolSize &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPipelinePoolSize *)parsePNextChain(obj);

    parse_VkDeviceSize(obj["poolEntrySize"], (o.poolEntrySize));

    parse_uint32_t(obj["poolEntryCount"], (o.poolEntryCount));
}

static void parse_VkDeviceObjectReservationCreateInfo(const Json::Value &obj, VkDeviceObjectReservationCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkDeviceObjectReservationCreateInfo *)parsePNextChain(obj);

    parse_uint32_t(obj["pipelineCacheCreateInfoCount"], (o.pipelineCacheCreateInfoCount));

    (o.pPipelineCacheCreateInfos) = (VkPipelineCacheCreateInfo *)s_globalMem.allocate(
        (o.pipelineCacheCreateInfoCount), sizeof(VkPipelineCacheCreateInfo));
    const Json::Value &obj_pPipelineCacheCreateInfos = obj["pPipelineCacheCreateInfos"];
    if (obj_pPipelineCacheCreateInfos.size() == 0)
        (o.pPipelineCacheCreateInfos) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.pipelineCacheCreateInfoCount); i++)
        {
            parse_VkPipelineCacheCreateInfo(obj_pPipelineCacheCreateInfos[i],
                                            const_cast<VkPipelineCacheCreateInfo &>((o.pPipelineCacheCreateInfos[i])));
        }
    }

    parse_uint32_t(obj["pipelinePoolSizeCount"], (o.pipelinePoolSizeCount));

    (o.pPipelinePoolSizes) =
        (VkPipelinePoolSize *)s_globalMem.allocate((o.pipelinePoolSizeCount), sizeof(VkPipelinePoolSize));
    const Json::Value &obj_pPipelinePoolSizes = obj["pPipelinePoolSizes"];
    if (obj_pPipelinePoolSizes.size() == 0)
        (o.pPipelinePoolSizes) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.pipelinePoolSizeCount); i++)
        {
            parse_VkPipelinePoolSize(obj_pPipelinePoolSizes[i],
                                     const_cast<VkPipelinePoolSize &>((o.pPipelinePoolSizes[i])));
        }
    }

    parse_uint32_t(obj["semaphoreRequestCount"], (o.semaphoreRequestCount));

    parse_uint32_t(obj["commandBufferRequestCount"], (o.commandBufferRequestCount));

    parse_uint32_t(obj["fenceRequestCount"], (o.fenceRequestCount));

    parse_uint32_t(obj["deviceMemoryRequestCount"], (o.deviceMemoryRequestCount));

    parse_uint32_t(obj["bufferRequestCount"], (o.bufferRequestCount));

    parse_uint32_t(obj["imageRequestCount"], (o.imageRequestCount));

    parse_uint32_t(obj["eventRequestCount"], (o.eventRequestCount));

    parse_uint32_t(obj["queryPoolRequestCount"], (o.queryPoolRequestCount));

    parse_uint32_t(obj["bufferViewRequestCount"], (o.bufferViewRequestCount));

    parse_uint32_t(obj["imageViewRequestCount"], (o.imageViewRequestCount));

    parse_uint32_t(obj["layeredImageViewRequestCount"], (o.layeredImageViewRequestCount));

    parse_uint32_t(obj["pipelineCacheRequestCount"], (o.pipelineCacheRequestCount));

    parse_uint32_t(obj["pipelineLayoutRequestCount"], (o.pipelineLayoutRequestCount));

    parse_uint32_t(obj["renderPassRequestCount"], (o.renderPassRequestCount));

    parse_uint32_t(obj["graphicsPipelineRequestCount"], (o.graphicsPipelineRequestCount));

    parse_uint32_t(obj["computePipelineRequestCount"], (o.computePipelineRequestCount));

    parse_uint32_t(obj["descriptorSetLayoutRequestCount"], (o.descriptorSetLayoutRequestCount));

    parse_uint32_t(obj["samplerRequestCount"], (o.samplerRequestCount));

    parse_uint32_t(obj["descriptorPoolRequestCount"], (o.descriptorPoolRequestCount));

    parse_uint32_t(obj["descriptorSetRequestCount"], (o.descriptorSetRequestCount));

    parse_uint32_t(obj["framebufferRequestCount"], (o.framebufferRequestCount));

    parse_uint32_t(obj["commandPoolRequestCount"], (o.commandPoolRequestCount));

    parse_uint32_t(obj["samplerYcbcrConversionRequestCount"], (o.samplerYcbcrConversionRequestCount));

    parse_uint32_t(obj["surfaceRequestCount"], (o.surfaceRequestCount));

    parse_uint32_t(obj["swapchainRequestCount"], (o.swapchainRequestCount));

    parse_uint32_t(obj["displayModeRequestCount"], (o.displayModeRequestCount));

    parse_uint32_t(obj["subpassDescriptionRequestCount"], (o.subpassDescriptionRequestCount));

    parse_uint32_t(obj["attachmentDescriptionRequestCount"], (o.attachmentDescriptionRequestCount));

    parse_uint32_t(obj["descriptorSetLayoutBindingRequestCount"], (o.descriptorSetLayoutBindingRequestCount));

    parse_uint32_t(obj["descriptorSetLayoutBindingLimit"], (o.descriptorSetLayoutBindingLimit));

    parse_uint32_t(obj["maxImageViewMipLevels"], (o.maxImageViewMipLevels));

    parse_uint32_t(obj["maxImageViewArrayLayers"], (o.maxImageViewArrayLayers));

    parse_uint32_t(obj["maxLayeredImageViewMipLevels"], (o.maxLayeredImageViewMipLevels));

    parse_uint32_t(obj["maxOcclusionQueriesPerPool"], (o.maxOcclusionQueriesPerPool));

    parse_uint32_t(obj["maxPipelineStatisticsQueriesPerPool"], (o.maxPipelineStatisticsQueriesPerPool));

    parse_uint32_t(obj["maxTimestampQueriesPerPool"], (o.maxTimestampQueriesPerPool));

    parse_uint32_t(obj["maxImmutableSamplersPerDescriptorSetLayout"], (o.maxImmutableSamplersPerDescriptorSetLayout));
}

static void parse_VkCommandPoolMemoryReservationCreateInfo(const Json::Value &obj,
                                                           VkCommandPoolMemoryReservationCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkCommandPoolMemoryReservationCreateInfo *)parsePNextChain(obj);

    parse_VkDeviceSize(obj["commandPoolReservedSize"], (o.commandPoolReservedSize));

    parse_uint32_t(obj["commandPoolMaxCommandBuffers"], (o.commandPoolMaxCommandBuffers));
}

static void parse_VkCommandPoolMemoryConsumption(const Json::Value &obj, VkCommandPoolMemoryConsumption &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkCommandPoolMemoryConsumption *)parsePNextChain(obj);

    parse_VkDeviceSize(obj["commandPoolAllocated"], (o.commandPoolAllocated));

    parse_VkDeviceSize(obj["commandPoolReservedSize"], (o.commandPoolReservedSize));

    parse_VkDeviceSize(obj["commandBufferAllocated"], (o.commandBufferAllocated));
}

static void parse_VkFaultData(const Json::Value &obj, VkFaultData &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkFaultData *)parsePNextChain(obj);

    parse_VkFaultLevel(obj["faultLevel"], (o.faultLevel));

    parse_VkFaultType(obj["faultType"], (o.faultType));
}

static void parse_VkFaultCallbackInfo(const Json::Value &obj, VkFaultCallbackInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkFaultCallbackInfo *)parsePNextChain(obj);

    parse_uint32_t(obj["faultCount"], (o.faultCount));

    (o.pFaults)                    = (VkFaultData *)s_globalMem.allocate((o.faultCount), sizeof(VkFaultData));
    const Json::Value &obj_pFaults = obj["pFaults"];
    if (obj_pFaults.size() == 0)
        (o.pFaults) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.faultCount); i++)
        {
            parse_VkFaultData(obj_pFaults[i], const_cast<VkFaultData &>((o.pFaults[i])));
        }
    }

    /** Note: Ignoring function pointer (PFN_vkFaultCallbackFunction). **/
}

static void parse_VkPipelineOfflineCreateInfo(const Json::Value &obj, VkPipelineOfflineCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPipelineOfflineCreateInfo *)parsePNextChain(obj);

    const Json::Value &obj_pipelineIdentifier_arr = obj["pipelineIdentifier"];
    for (unsigned int i = 0; i < obj_pipelineIdentifier_arr.size(); i++)
    {
        parse_uint8_t(obj_pipelineIdentifier_arr[i], const_cast<uint8_t &>((o.pipelineIdentifier[i])));
    }

    parse_VkPipelineMatchControl(obj["matchControl"], (o.matchControl));

    parse_VkDeviceSize(obj["poolEntrySize"], (o.poolEntrySize));
}

static void parse_VkPipelineCacheStageValidationIndexEntry(const Json::Value &obj,
                                                           VkPipelineCacheStageValidationIndexEntry &o)
{

    parse_uint64_t(obj["codeSize"], (o.codeSize));

    parse_uint64_t(obj["codeOffset"], (o.codeOffset));
}

static void parse_VkPipelineCacheSafetyCriticalIndexEntry(const Json::Value &obj,
                                                          VkPipelineCacheSafetyCriticalIndexEntry &o)
{

    const Json::Value &obj_pipelineIdentifier_arr = obj["pipelineIdentifier"];
    for (unsigned int i = 0; i < obj_pipelineIdentifier_arr.size(); i++)
    {
        parse_uint8_t(obj_pipelineIdentifier_arr[i], const_cast<uint8_t &>((o.pipelineIdentifier[i])));
    }

    parse_uint64_t(obj["pipelineMemorySize"], (o.pipelineMemorySize));

    parse_uint64_t(obj["jsonSize"], (o.jsonSize));

    parse_uint64_t(obj["jsonOffset"], (o.jsonOffset));

    parse_uint32_t(obj["stageIndexCount"], (o.stageIndexCount));

    parse_uint32_t(obj["stageIndexStride"], (o.stageIndexStride));

    parse_uint64_t(obj["stageIndexOffset"], (o.stageIndexOffset));
}

static void parse_VkPipelineCacheHeaderVersionSafetyCriticalOne(const Json::Value &obj,
                                                                VkPipelineCacheHeaderVersionSafetyCriticalOne &o)
{

    parse_VkPipelineCacheHeaderVersionOne(obj["headerVersionOne"], (o.headerVersionOne));

    parse_VkPipelineCacheValidationVersion(obj["validationVersion"], (o.validationVersion));

    parse_uint32_t(obj["implementationData"], (o.implementationData));

    parse_uint32_t(obj["pipelineIndexCount"], (o.pipelineIndexCount));

    parse_uint32_t(obj["pipelineIndexStride"], (o.pipelineIndexStride));

    parse_uint64_t(obj["pipelineIndexOffset"], (o.pipelineIndexOffset));
}

static void parse_VkSurfaceKHR(const Json::Value &obj, VkSurfaceKHR &o)
{
    //    const std::string& _res = obj.asString();
}

static std::map<std::string, int> VkSurfaceTransformFlagBitsKHR_map = {
    std::make_pair("VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR", 1UL << 0),
    std::make_pair("VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR", 1UL << 1),
    std::make_pair("VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR", 1UL << 2),
    std::make_pair("VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR", 1UL << 3),
    std::make_pair("VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_BIT_KHR", 1UL << 4),
    std::make_pair("VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90_BIT_KHR", 1UL << 5),
    std::make_pair("VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_180_BIT_KHR", 1UL << 6),
    std::make_pair("VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270_BIT_KHR", 1UL << 7),
    std::make_pair("VK_SURFACE_TRANSFORM_INHERIT_BIT_KHR", 1UL << 8),
};
static void parse_VkSurfaceTransformFlagBitsKHR(const Json::Value &obj, VkSurfaceTransformFlagBitsKHR &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkSurfaceTransformFlagBitsKHR)VkSurfaceTransformFlagBitsKHR_map[std::string(_res)];
}

static std::map<std::string, int> VkPresentModeKHR_map = {
    std::make_pair("VK_PRESENT_MODE_IMMEDIATE_KHR", 0),
    std::make_pair("VK_PRESENT_MODE_MAILBOX_KHR", 1),
    std::make_pair("VK_PRESENT_MODE_FIFO_KHR", 2),
    std::make_pair("VK_PRESENT_MODE_FIFO_RELAXED_KHR", 3),
    std::make_pair("VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR", 1000111000),
    std::make_pair("VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR", 1000111001),
};
static void parse_VkPresentModeKHR(const Json::Value &obj, VkPresentModeKHR &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkPresentModeKHR)VkPresentModeKHR_map[std::string(_res)];
}

static std::map<std::string, int> VkColorSpaceKHR_map = {
    std::make_pair("VK_COLOR_SPACE_SRGB_NONLINEAR_KHR", 0),
    std::make_pair("VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT", 1000104001),
    std::make_pair("VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT", 1000104002),
    std::make_pair("VK_COLOR_SPACE_DISPLAY_P3_LINEAR_EXT", 1000104003),
    std::make_pair("VK_COLOR_SPACE_DCI_P3_NONLINEAR_EXT", 1000104004),
    std::make_pair("VK_COLOR_SPACE_BT709_LINEAR_EXT", 1000104005),
    std::make_pair("VK_COLOR_SPACE_BT709_NONLINEAR_EXT", 1000104006),
    std::make_pair("VK_COLOR_SPACE_BT2020_LINEAR_EXT", 1000104007),
    std::make_pair("VK_COLOR_SPACE_HDR10_ST2084_EXT", 1000104008),
    std::make_pair("VK_COLOR_SPACE_DOLBYVISION_EXT", 1000104009),
    std::make_pair("VK_COLOR_SPACE_HDR10_HLG_EXT", 1000104010),
    std::make_pair("VK_COLOR_SPACE_ADOBERGB_LINEAR_EXT", 1000104011),
    std::make_pair("VK_COLOR_SPACE_ADOBERGB_NONLINEAR_EXT", 1000104012),
    std::make_pair("VK_COLOR_SPACE_PASS_THROUGH_EXT", 1000104013),
    std::make_pair("VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT", 1000104014),
    std::make_pair("VK_COLOR_SPACE_DISPLAY_NATIVE_AMD", 1000213000),
};
static void parse_VkColorSpaceKHR(const Json::Value &obj, VkColorSpaceKHR &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkColorSpaceKHR)VkColorSpaceKHR_map[std::string(_res)];
}

static std::map<std::string, int> VkCompositeAlphaFlagBitsKHR_map = {
    std::make_pair("VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR", 1UL << 0),
    std::make_pair("VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR", 1UL << 1),
    std::make_pair("VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR", 1UL << 2),
    std::make_pair("VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR", 1UL << 3),
};
static void parse_VkCompositeAlphaFlagBitsKHR(const Json::Value &obj, VkCompositeAlphaFlagBitsKHR &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkCompositeAlphaFlagBitsKHR)VkCompositeAlphaFlagBitsKHR_map[std::string(_res)];
}

static void parse_VkCompositeAlphaFlagsKHR(const Json::Value &obj, VkCompositeAlphaFlagsKHR &o)
{
    o                       = (VkCompositeAlphaFlagsKHR)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkCompositeAlphaFlagBitsKHR)VkCompositeAlphaFlagBitsKHR_map[it];
    }
}

static void parse_VkSurfaceTransformFlagsKHR(const Json::Value &obj, VkSurfaceTransformFlagsKHR &o)
{
    o                       = (VkSurfaceTransformFlagsKHR)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkSurfaceTransformFlagBitsKHR)VkSurfaceTransformFlagBitsKHR_map[it];
    }
}

static void parse_VkSurfaceCapabilitiesKHR(const Json::Value &obj, VkSurfaceCapabilitiesKHR &o)
{

    parse_uint32_t(obj["minImageCount"], (o.minImageCount));

    parse_uint32_t(obj["maxImageCount"], (o.maxImageCount));

    parse_VkExtent2D(obj["currentExtent"], (o.currentExtent));

    parse_VkExtent2D(obj["minImageExtent"], (o.minImageExtent));

    parse_VkExtent2D(obj["maxImageExtent"], (o.maxImageExtent));

    parse_uint32_t(obj["maxImageArrayLayers"], (o.maxImageArrayLayers));

    parse_VkSurfaceTransformFlagsKHR(obj["supportedTransforms"], (o.supportedTransforms));

    parse_VkSurfaceTransformFlagBitsKHR(obj["currentTransform"], (o.currentTransform));

    parse_VkCompositeAlphaFlagsKHR(obj["supportedCompositeAlpha"], (o.supportedCompositeAlpha));

    parse_VkImageUsageFlags(obj["supportedUsageFlags"], (o.supportedUsageFlags));
}

static void parse_VkSurfaceFormatKHR(const Json::Value &obj, VkSurfaceFormatKHR &o)
{

    parse_VkFormat(obj["format"], (o.format));

    parse_VkColorSpaceKHR(obj["colorSpace"], (o.colorSpace));
}

static void parse_VkSwapchainKHR(const Json::Value &obj, VkSwapchainKHR &o)
{
    //    const std::string& _res = obj.asString();
}

static std::map<std::string, int> VkSwapchainCreateFlagBitsKHR_map = {
    std::make_pair("VK_SWAPCHAIN_CREATE_SPLIT_INSTANCE_BIND_REGIONS_BIT_KHR", 1UL << 0),
    std::make_pair("VK_SWAPCHAIN_CREATE_PROTECTED_BIT_KHR", 1UL << 1),
    std::make_pair("VK_SWAPCHAIN_CREATE_SPLIT_INSTANCE_BIND_REGIONS_BIT_KHR", 1UL << 0),
    std::make_pair("VK_SWAPCHAIN_CREATE_MUTABLE_FORMAT_BIT_KHR", 1UL << 2),
    std::make_pair("VK_SWAPCHAIN_CREATE_DEFERRED_MEMORY_ALLOCATION_BIT_EXT", 1UL << 3),
    std::make_pair("VK_SWAPCHAIN_CREATE_RESERVED_4_BIT_EXT", 1UL << 4),
};
static void parse_VkSwapchainCreateFlagBitsKHR(const Json::Value &obj, VkSwapchainCreateFlagBitsKHR &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkSwapchainCreateFlagBitsKHR)VkSwapchainCreateFlagBitsKHR_map[std::string(_res)];
}

static std::map<std::string, int> VkDeviceGroupPresentModeFlagBitsKHR_map = {
    std::make_pair("VK_DEVICE_GROUP_PRESENT_MODE_LOCAL_BIT_KHR", 1UL << 0),
    std::make_pair("VK_DEVICE_GROUP_PRESENT_MODE_REMOTE_BIT_KHR", 1UL << 1),
    std::make_pair("VK_DEVICE_GROUP_PRESENT_MODE_SUM_BIT_KHR", 1UL << 2),
    std::make_pair("VK_DEVICE_GROUP_PRESENT_MODE_LOCAL_MULTI_DEVICE_BIT_KHR", 1UL << 3),
};
static void parse_VkDeviceGroupPresentModeFlagBitsKHR(const Json::Value &obj, VkDeviceGroupPresentModeFlagBitsKHR &o)
{
    const std::string &_res = obj.asString();
    o = (VkDeviceGroupPresentModeFlagBitsKHR)VkDeviceGroupPresentModeFlagBitsKHR_map[std::string(_res)];
}

static void parse_VkSwapchainCreateFlagsKHR(const Json::Value &obj, VkSwapchainCreateFlagsKHR &o)
{
    o                       = (VkSwapchainCreateFlagsKHR)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkSwapchainCreateFlagBitsKHR)VkSwapchainCreateFlagBitsKHR_map[it];
    }
}

static void parse_VkDeviceGroupPresentModeFlagsKHR(const Json::Value &obj, VkDeviceGroupPresentModeFlagsKHR &o)
{
    o                       = (VkDeviceGroupPresentModeFlagsKHR)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkDeviceGroupPresentModeFlagBitsKHR)VkDeviceGroupPresentModeFlagBitsKHR_map[it];
    }
}

static void parse_VkSwapchainCreateInfoKHR(const Json::Value &obj, VkSwapchainCreateInfoKHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkSwapchainCreateInfoKHR *)parsePNextChain(obj);

    parse_VkSwapchainCreateFlagsKHR(obj["flags"], (o.flags));

    parse_uint32_t(obj["minImageCount"], (o.minImageCount));

    parse_VkFormat(obj["imageFormat"], (o.imageFormat));

    parse_VkColorSpaceKHR(obj["imageColorSpace"], (o.imageColorSpace));

    parse_VkExtent2D(obj["imageExtent"], (o.imageExtent));

    parse_uint32_t(obj["imageArrayLayers"], (o.imageArrayLayers));

    parse_VkImageUsageFlags(obj["imageUsage"], (o.imageUsage));

    parse_VkSharingMode(obj["imageSharingMode"], (o.imageSharingMode));

    parse_uint32_t(obj["queueFamilyIndexCount"], (o.queueFamilyIndexCount));

    (o.pQueueFamilyIndices) = (uint32_t *)s_globalMem.allocate((o.queueFamilyIndexCount), sizeof(uint32_t));
    const Json::Value &obj_pQueueFamilyIndices_arr = obj["pQueueFamilyIndices"];
    for (unsigned int i = 0; i < obj_pQueueFamilyIndices_arr.size(); i++)
    {
        parse_uint32_t(obj_pQueueFamilyIndices_arr[i], const_cast<uint32_t &>((o.pQueueFamilyIndices[i])));
    }

    parse_VkSurfaceTransformFlagBitsKHR(obj["preTransform"], (o.preTransform));

    parse_VkCompositeAlphaFlagBitsKHR(obj["compositeAlpha"], (o.compositeAlpha));

    parse_VkPresentModeKHR(obj["presentMode"], (o.presentMode));

    parse_VkBool32(obj["clipped"], (o.clipped));
}

static void parse_VkPresentInfoKHR(const Json::Value &obj, VkPresentInfoKHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPresentInfoKHR *)parsePNextChain(obj);

    parse_uint32_t(obj["waitSemaphoreCount"], (o.waitSemaphoreCount));

    (o.pWaitSemaphores) = (VkSemaphore *)s_globalMem.allocate((o.waitSemaphoreCount), sizeof(VkSemaphore));
    const Json::Value &obj_pWaitSemaphores_arr = obj["pWaitSemaphores"];
    for (unsigned int i = 0; i < obj_pWaitSemaphores_arr.size(); i++)
    {
        parse_VkSemaphore(obj_pWaitSemaphores_arr[i], const_cast<VkSemaphore &>((o.pWaitSemaphores[i])));
    }

    parse_uint32_t(obj["swapchainCount"], (o.swapchainCount));

    (o.pSwapchains) = (VkSwapchainKHR *)s_globalMem.allocate((o.swapchainCount), sizeof(VkSwapchainKHR));
    const Json::Value &obj_pSwapchains_arr = obj["pSwapchains"];
    for (unsigned int i = 0; i < obj_pSwapchains_arr.size(); i++)
    {
        parse_VkSwapchainKHR(obj_pSwapchains_arr[i], const_cast<VkSwapchainKHR &>((o.pSwapchains[i])));
    }

    (o.pImageIndices)                        = (uint32_t *)s_globalMem.allocate((o.swapchainCount), sizeof(uint32_t));
    const Json::Value &obj_pImageIndices_arr = obj["pImageIndices"];
    for (unsigned int i = 0; i < obj_pImageIndices_arr.size(); i++)
    {
        parse_uint32_t(obj_pImageIndices_arr[i], const_cast<uint32_t &>((o.pImageIndices[i])));
    }
}

static void parse_VkImageSwapchainCreateInfoKHR(const Json::Value &obj, VkImageSwapchainCreateInfoKHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkImageSwapchainCreateInfoKHR *)parsePNextChain(obj);
}

static void parse_VkBindImageMemorySwapchainInfoKHR(const Json::Value &obj, VkBindImageMemorySwapchainInfoKHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkBindImageMemorySwapchainInfoKHR *)parsePNextChain(obj);

    parse_uint32_t(obj["imageIndex"], (o.imageIndex));
}

static void parse_VkAcquireNextImageInfoKHR(const Json::Value &obj, VkAcquireNextImageInfoKHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkAcquireNextImageInfoKHR *)parsePNextChain(obj);

    parse_uint64_t(obj["timeout"], (o.timeout));

    parse_uint32_t(obj["deviceMask"], (o.deviceMask));
}

static void parse_VkDeviceGroupPresentCapabilitiesKHR(const Json::Value &obj, VkDeviceGroupPresentCapabilitiesKHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkDeviceGroupPresentCapabilitiesKHR *)parsePNextChain(obj);

    const Json::Value &obj_presentMask_arr = obj["presentMask"];
    for (unsigned int i = 0; i < obj_presentMask_arr.size(); i++)
    {
        parse_uint32_t(obj_presentMask_arr[i], const_cast<uint32_t &>((o.presentMask[i])));
    }

    parse_VkDeviceGroupPresentModeFlagsKHR(obj["modes"], (o.modes));
}

static void parse_VkDeviceGroupPresentInfoKHR(const Json::Value &obj, VkDeviceGroupPresentInfoKHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkDeviceGroupPresentInfoKHR *)parsePNextChain(obj);

    parse_uint32_t(obj["swapchainCount"], (o.swapchainCount));

    (o.pDeviceMasks)                        = (uint32_t *)s_globalMem.allocate((o.swapchainCount), sizeof(uint32_t));
    const Json::Value &obj_pDeviceMasks_arr = obj["pDeviceMasks"];
    for (unsigned int i = 0; i < obj_pDeviceMasks_arr.size(); i++)
    {
        parse_uint32_t(obj_pDeviceMasks_arr[i], const_cast<uint32_t &>((o.pDeviceMasks[i])));
    }

    parse_VkDeviceGroupPresentModeFlagBitsKHR(obj["mode"], (o.mode));
}

static void parse_VkDeviceGroupSwapchainCreateInfoKHR(const Json::Value &obj, VkDeviceGroupSwapchainCreateInfoKHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkDeviceGroupSwapchainCreateInfoKHR *)parsePNextChain(obj);

    parse_VkDeviceGroupPresentModeFlagsKHR(obj["modes"], (o.modes));
}

static void parse_VkDisplayKHR(const Json::Value &obj, VkDisplayKHR &o)
{
    //    const std::string& _res = obj.asString();
}

static void parse_VkDisplayModeKHR(const Json::Value &obj, VkDisplayModeKHR &o)
{
    //    const std::string& _res = obj.asString();
}

static std::map<std::string, int> VkDisplayPlaneAlphaFlagBitsKHR_map = {
    std::make_pair("VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR", 1UL << 0),
    std::make_pair("VK_DISPLAY_PLANE_ALPHA_GLOBAL_BIT_KHR", 1UL << 1),
    std::make_pair("VK_DISPLAY_PLANE_ALPHA_PER_PIXEL_BIT_KHR", 1UL << 2),
    std::make_pair("VK_DISPLAY_PLANE_ALPHA_PER_PIXEL_PREMULTIPLIED_BIT_KHR", 1UL << 3),
};
static void parse_VkDisplayPlaneAlphaFlagBitsKHR(const Json::Value &obj, VkDisplayPlaneAlphaFlagBitsKHR &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkDisplayPlaneAlphaFlagBitsKHR)VkDisplayPlaneAlphaFlagBitsKHR_map[std::string(_res)];
}

static void parse_VkDisplayModeCreateFlagsKHR(const Json::Value &obj, VkDisplayModeCreateFlagsKHR &o)
{
    if (obj.isString())
    {
        const std::string &_res = obj.asString();
        sscanf(_res.c_str(), "%u", &o);
    }
    else
    {
        o = obj.asUInt();
    }
}

static void parse_VkDisplayPlaneAlphaFlagsKHR(const Json::Value &obj, VkDisplayPlaneAlphaFlagsKHR &o)
{
    o                       = (VkDisplayPlaneAlphaFlagsKHR)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkDisplayPlaneAlphaFlagBitsKHR)VkDisplayPlaneAlphaFlagBitsKHR_map[it];
    }
}

static void parse_VkDisplaySurfaceCreateFlagsKHR(const Json::Value &obj, VkDisplaySurfaceCreateFlagsKHR &o)
{
    if (obj.isString())
    {
        const std::string &_res = obj.asString();
        sscanf(_res.c_str(), "%u", &o);
    }
    else
    {
        o = obj.asUInt();
    }
}

static void parse_VkDisplayModeParametersKHR(const Json::Value &obj, VkDisplayModeParametersKHR &o)
{

    parse_VkExtent2D(obj["visibleRegion"], (o.visibleRegion));

    parse_uint32_t(obj["refreshRate"], (o.refreshRate));
}

static void parse_VkDisplayModeCreateInfoKHR(const Json::Value &obj, VkDisplayModeCreateInfoKHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkDisplayModeCreateInfoKHR *)parsePNextChain(obj);

    parse_VkDisplayModeCreateFlagsKHR(obj["flags"], (o.flags));

    parse_VkDisplayModeParametersKHR(obj["parameters"], (o.parameters));
}

static void parse_VkDisplayModePropertiesKHR(const Json::Value &obj, VkDisplayModePropertiesKHR &o)
{

    parse_VkDisplayModeParametersKHR(obj["parameters"], (o.parameters));
}

static void parse_VkDisplayPlaneCapabilitiesKHR(const Json::Value &obj, VkDisplayPlaneCapabilitiesKHR &o)
{

    parse_VkDisplayPlaneAlphaFlagsKHR(obj["supportedAlpha"], (o.supportedAlpha));

    parse_VkOffset2D(obj["minSrcPosition"], (o.minSrcPosition));

    parse_VkOffset2D(obj["maxSrcPosition"], (o.maxSrcPosition));

    parse_VkExtent2D(obj["minSrcExtent"], (o.minSrcExtent));

    parse_VkExtent2D(obj["maxSrcExtent"], (o.maxSrcExtent));

    parse_VkOffset2D(obj["minDstPosition"], (o.minDstPosition));

    parse_VkOffset2D(obj["maxDstPosition"], (o.maxDstPosition));

    parse_VkExtent2D(obj["minDstExtent"], (o.minDstExtent));

    parse_VkExtent2D(obj["maxDstExtent"], (o.maxDstExtent));
}

static void parse_VkDisplayPlanePropertiesKHR(const Json::Value &obj, VkDisplayPlanePropertiesKHR &o)
{

    parse_uint32_t(obj["currentStackIndex"], (o.currentStackIndex));
}

static void parse_VkDisplayPropertiesKHR(const Json::Value &obj, VkDisplayPropertiesKHR &o)
{

    /** TODO: Handle this - displayName **/

    parse_VkExtent2D(obj["physicalDimensions"], (o.physicalDimensions));

    parse_VkExtent2D(obj["physicalResolution"], (o.physicalResolution));

    parse_VkSurfaceTransformFlagsKHR(obj["supportedTransforms"], (o.supportedTransforms));

    parse_VkBool32(obj["planeReorderPossible"], (o.planeReorderPossible));

    parse_VkBool32(obj["persistentContent"], (o.persistentContent));
}

static void parse_VkDisplaySurfaceCreateInfoKHR(const Json::Value &obj, VkDisplaySurfaceCreateInfoKHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkDisplaySurfaceCreateInfoKHR *)parsePNextChain(obj);

    parse_VkDisplaySurfaceCreateFlagsKHR(obj["flags"], (o.flags));

    parse_uint32_t(obj["planeIndex"], (o.planeIndex));

    parse_uint32_t(obj["planeStackIndex"], (o.planeStackIndex));

    parse_VkSurfaceTransformFlagBitsKHR(obj["transform"], (o.transform));

    parse_float(obj["globalAlpha"], (o.globalAlpha));

    parse_VkDisplayPlaneAlphaFlagBitsKHR(obj["alphaMode"], (o.alphaMode));

    parse_VkExtent2D(obj["imageExtent"], (o.imageExtent));
}

static void parse_VkDisplayPresentInfoKHR(const Json::Value &obj, VkDisplayPresentInfoKHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkDisplayPresentInfoKHR *)parsePNextChain(obj);

    parse_VkRect2D(obj["srcRect"], (o.srcRect));

    parse_VkRect2D(obj["dstRect"], (o.dstRect));

    parse_VkBool32(obj["persistent"], (o.persistent));
}

static void parse_VkImportMemoryFdInfoKHR(const Json::Value &obj, VkImportMemoryFdInfoKHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkImportMemoryFdInfoKHR *)parsePNextChain(obj);

    parse_VkExternalMemoryHandleTypeFlagBits(obj["handleType"], (o.handleType));

    parse_int(obj["fd"], (o.fd));
}

static void parse_VkMemoryFdPropertiesKHR(const Json::Value &obj, VkMemoryFdPropertiesKHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkMemoryFdPropertiesKHR *)parsePNextChain(obj);

    parse_uint32_t(obj["memoryTypeBits"], (o.memoryTypeBits));
}

static void parse_VkMemoryGetFdInfoKHR(const Json::Value &obj, VkMemoryGetFdInfoKHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkMemoryGetFdInfoKHR *)parsePNextChain(obj);

    parse_VkExternalMemoryHandleTypeFlagBits(obj["handleType"], (o.handleType));
}

static void parse_VkImportSemaphoreFdInfoKHR(const Json::Value &obj, VkImportSemaphoreFdInfoKHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkImportSemaphoreFdInfoKHR *)parsePNextChain(obj);

    parse_VkSemaphoreImportFlags(obj["flags"], (o.flags));

    parse_VkExternalSemaphoreHandleTypeFlagBits(obj["handleType"], (o.handleType));

    parse_int(obj["fd"], (o.fd));
}

static void parse_VkSemaphoreGetFdInfoKHR(const Json::Value &obj, VkSemaphoreGetFdInfoKHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkSemaphoreGetFdInfoKHR *)parsePNextChain(obj);

    parse_VkExternalSemaphoreHandleTypeFlagBits(obj["handleType"], (o.handleType));
}

static void parse_VkRectLayerKHR(const Json::Value &obj, VkRectLayerKHR &o)
{

    parse_VkOffset2D(obj["offset"], (o.offset));

    parse_VkExtent2D(obj["extent"], (o.extent));

    parse_uint32_t(obj["layer"], (o.layer));
}

static void parse_VkPresentRegionKHR(const Json::Value &obj, VkPresentRegionKHR &o)
{

    parse_uint32_t(obj["rectangleCount"], (o.rectangleCount));

    (o.pRectangles) = (VkRectLayerKHR *)s_globalMem.allocate((o.rectangleCount), sizeof(VkRectLayerKHR));
    const Json::Value &obj_pRectangles = obj["pRectangles"];
    if (obj_pRectangles.size() == 0)
        (o.pRectangles) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.rectangleCount); i++)
        {
            parse_VkRectLayerKHR(obj_pRectangles[i], const_cast<VkRectLayerKHR &>((o.pRectangles[i])));
        }
    }
}

static void parse_VkPresentRegionsKHR(const Json::Value &obj, VkPresentRegionsKHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPresentRegionsKHR *)parsePNextChain(obj);

    parse_uint32_t(obj["swapchainCount"], (o.swapchainCount));

    (o.pRegions) = (VkPresentRegionKHR *)s_globalMem.allocate((o.swapchainCount), sizeof(VkPresentRegionKHR));
    const Json::Value &obj_pRegions = obj["pRegions"];
    if (obj_pRegions.size() == 0)
        (o.pRegions) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.swapchainCount); i++)
        {
            parse_VkPresentRegionKHR(obj_pRegions[i], const_cast<VkPresentRegionKHR &>((o.pRegions[i])));
        }
    }
}

static void parse_VkSharedPresentSurfaceCapabilitiesKHR(const Json::Value &obj,
                                                        VkSharedPresentSurfaceCapabilitiesKHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkSharedPresentSurfaceCapabilitiesKHR *)parsePNextChain(obj);

    parse_VkImageUsageFlags(obj["sharedPresentSupportedUsageFlags"], (o.sharedPresentSupportedUsageFlags));
}

static void parse_VkImportFenceFdInfoKHR(const Json::Value &obj, VkImportFenceFdInfoKHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkImportFenceFdInfoKHR *)parsePNextChain(obj);

    parse_VkFenceImportFlags(obj["flags"], (o.flags));

    parse_VkExternalFenceHandleTypeFlagBits(obj["handleType"], (o.handleType));

    parse_int(obj["fd"], (o.fd));
}

static void parse_VkFenceGetFdInfoKHR(const Json::Value &obj, VkFenceGetFdInfoKHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkFenceGetFdInfoKHR *)parsePNextChain(obj);

    parse_VkExternalFenceHandleTypeFlagBits(obj["handleType"], (o.handleType));
}

static std::map<std::string, int> VkPerformanceCounterUnitKHR_map = {
    std::make_pair("VK_PERFORMANCE_COUNTER_UNIT_GENERIC_KHR", 0),
    std::make_pair("VK_PERFORMANCE_COUNTER_UNIT_PERCENTAGE_KHR", 1),
    std::make_pair("VK_PERFORMANCE_COUNTER_UNIT_NANOSECONDS_KHR", 2),
    std::make_pair("VK_PERFORMANCE_COUNTER_UNIT_BYTES_KHR", 3),
    std::make_pair("VK_PERFORMANCE_COUNTER_UNIT_BYTES_PER_SECOND_KHR", 4),
    std::make_pair("VK_PERFORMANCE_COUNTER_UNIT_KELVIN_KHR", 5),
    std::make_pair("VK_PERFORMANCE_COUNTER_UNIT_WATTS_KHR", 6),
    std::make_pair("VK_PERFORMANCE_COUNTER_UNIT_VOLTS_KHR", 7),
    std::make_pair("VK_PERFORMANCE_COUNTER_UNIT_AMPS_KHR", 8),
    std::make_pair("VK_PERFORMANCE_COUNTER_UNIT_HERTZ_KHR", 9),
    std::make_pair("VK_PERFORMANCE_COUNTER_UNIT_CYCLES_KHR", 10),
};
static void parse_VkPerformanceCounterUnitKHR(const Json::Value &obj, VkPerformanceCounterUnitKHR &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkPerformanceCounterUnitKHR)VkPerformanceCounterUnitKHR_map[std::string(_res)];
}

static std::map<std::string, int> VkPerformanceCounterScopeKHR_map = {
    std::make_pair("VK_PERFORMANCE_COUNTER_SCOPE_COMMAND_BUFFER_KHR", 0),
    std::make_pair("VK_PERFORMANCE_COUNTER_SCOPE_RENDER_PASS_KHR", 1),
    std::make_pair("VK_PERFORMANCE_COUNTER_SCOPE_COMMAND_KHR", 2),
    std::make_pair("VK_QUERY_SCOPE_COMMAND_BUFFER_KHR", 0),
    std::make_pair("VK_QUERY_SCOPE_RENDER_PASS_KHR", 1),
    std::make_pair("VK_QUERY_SCOPE_COMMAND_KHR", 2),
};
static void parse_VkPerformanceCounterScopeKHR(const Json::Value &obj, VkPerformanceCounterScopeKHR &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkPerformanceCounterScopeKHR)VkPerformanceCounterScopeKHR_map[std::string(_res)];
}

static std::map<std::string, int> VkPerformanceCounterStorageKHR_map = {
    std::make_pair("VK_PERFORMANCE_COUNTER_STORAGE_INT32_KHR", 0),
    std::make_pair("VK_PERFORMANCE_COUNTER_STORAGE_INT64_KHR", 1),
    std::make_pair("VK_PERFORMANCE_COUNTER_STORAGE_UINT32_KHR", 2),
    std::make_pair("VK_PERFORMANCE_COUNTER_STORAGE_UINT64_KHR", 3),
    std::make_pair("VK_PERFORMANCE_COUNTER_STORAGE_FLOAT32_KHR", 4),
    std::make_pair("VK_PERFORMANCE_COUNTER_STORAGE_FLOAT64_KHR", 5),
};
static void parse_VkPerformanceCounterStorageKHR(const Json::Value &obj, VkPerformanceCounterStorageKHR &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkPerformanceCounterStorageKHR)VkPerformanceCounterStorageKHR_map[std::string(_res)];
}

static std::map<std::string, int> VkPerformanceCounterDescriptionFlagBitsKHR_map = {
    std::make_pair("VK_PERFORMANCE_COUNTER_DESCRIPTION_PERFORMANCE_IMPACTING_BIT_KHR", 1UL << 0),
    std::make_pair("VK_PERFORMANCE_COUNTER_DESCRIPTION_PERFORMANCE_IMPACTING_KHR", 1UL << 0),
    std::make_pair("VK_PERFORMANCE_COUNTER_DESCRIPTION_CONCURRENTLY_IMPACTED_BIT_KHR", 1UL << 1),
    std::make_pair("VK_PERFORMANCE_COUNTER_DESCRIPTION_CONCURRENTLY_IMPACTED_KHR", 1UL << 1),
};
static void parse_VkPerformanceCounterDescriptionFlagBitsKHR(const Json::Value &obj,
                                                             VkPerformanceCounterDescriptionFlagBitsKHR &o)
{
    const std::string &_res = obj.asString();
    o = (VkPerformanceCounterDescriptionFlagBitsKHR)VkPerformanceCounterDescriptionFlagBitsKHR_map[std::string(_res)];
}

static std::map<std::string, int> VkAcquireProfilingLockFlagBitsKHR_map = {};
static void parse_VkAcquireProfilingLockFlagBitsKHR(const Json::Value &obj, VkAcquireProfilingLockFlagBitsKHR &o)
{
    const std::string &_res = obj.asString();
    o = (VkAcquireProfilingLockFlagBitsKHR)VkAcquireProfilingLockFlagBitsKHR_map[std::string(_res)];
}

static void parse_VkPerformanceCounterDescriptionFlagsKHR(const Json::Value &obj,
                                                          VkPerformanceCounterDescriptionFlagsKHR &o)
{
    o                       = (VkPerformanceCounterDescriptionFlagsKHR)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkPerformanceCounterDescriptionFlagBitsKHR)VkPerformanceCounterDescriptionFlagBitsKHR_map[it];
    }
}

static void parse_VkAcquireProfilingLockFlagsKHR(const Json::Value &obj, VkAcquireProfilingLockFlagsKHR &o)
{
    o                       = (VkAcquireProfilingLockFlagsKHR)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkAcquireProfilingLockFlagBitsKHR)VkAcquireProfilingLockFlagBitsKHR_map[it];
    }
}

static void parse_VkPhysicalDevicePerformanceQueryFeaturesKHR(const Json::Value &obj,
                                                              VkPhysicalDevicePerformanceQueryFeaturesKHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDevicePerformanceQueryFeaturesKHR *)parsePNextChain(obj);

    parse_VkBool32(obj["performanceCounterQueryPools"], (o.performanceCounterQueryPools));

    parse_VkBool32(obj["performanceCounterMultipleQueryPools"], (o.performanceCounterMultipleQueryPools));
}

static void parse_VkPhysicalDevicePerformanceQueryPropertiesKHR(const Json::Value &obj,
                                                                VkPhysicalDevicePerformanceQueryPropertiesKHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDevicePerformanceQueryPropertiesKHR *)parsePNextChain(obj);

    parse_VkBool32(obj["allowCommandBufferQueryCopies"], (o.allowCommandBufferQueryCopies));
}

static void parse_VkPerformanceCounterKHR(const Json::Value &obj, VkPerformanceCounterKHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPerformanceCounterKHR *)parsePNextChain(obj);

    parse_VkPerformanceCounterUnitKHR(obj["unit"], (o.unit));

    parse_VkPerformanceCounterScopeKHR(obj["scope"], (o.scope));

    parse_VkPerformanceCounterStorageKHR(obj["storage"], (o.storage));

    const Json::Value &obj_uuid_arr = obj["uuid"];
    for (unsigned int i = 0; i < obj_uuid_arr.size(); i++)
    {
        parse_uint8_t(obj_uuid_arr[i], const_cast<uint8_t &>((o.uuid[i])));
    }
}

static void parse_VkPerformanceCounterDescriptionKHR(const Json::Value &obj, VkPerformanceCounterDescriptionKHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPerformanceCounterDescriptionKHR *)parsePNextChain(obj);

    parse_VkPerformanceCounterDescriptionFlagsKHR(obj["flags"], (o.flags));

    /** TODO: Handle this - name **/

    /** TODO: Handle this - category **/

    /** TODO: Handle this - description **/
}

static void parse_VkQueryPoolPerformanceCreateInfoKHR(const Json::Value &obj, VkQueryPoolPerformanceCreateInfoKHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkQueryPoolPerformanceCreateInfoKHR *)parsePNextChain(obj);

    parse_uint32_t(obj["queueFamilyIndex"], (o.queueFamilyIndex));

    parse_uint32_t(obj["counterIndexCount"], (o.counterIndexCount));

    (o.pCounterIndices) = (uint32_t *)s_globalMem.allocate((o.counterIndexCount), sizeof(uint32_t));
    const Json::Value &obj_pCounterIndices_arr = obj["pCounterIndices"];
    for (unsigned int i = 0; i < obj_pCounterIndices_arr.size(); i++)
    {
        parse_uint32_t(obj_pCounterIndices_arr[i], const_cast<uint32_t &>((o.pCounterIndices[i])));
    }
}

static void parse_VkPerformanceCounterResultKHR(const Json::Value &obj, VkPerformanceCounterResultKHR &o)
{

    parse_int32_t(obj["int32"], (o.int32));

    parse_int64_t(obj["int64"], (o.int64));

    parse_uint32_t(obj["uint32"], (o.uint32));

    parse_uint64_t(obj["uint64"], (o.uint64));

    parse_float(obj["float32"], (o.float32));

    parse_double(obj["float64"], (o.float64));
}

static void parse_VkAcquireProfilingLockInfoKHR(const Json::Value &obj, VkAcquireProfilingLockInfoKHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkAcquireProfilingLockInfoKHR *)parsePNextChain(obj);

    parse_VkAcquireProfilingLockFlagsKHR(obj["flags"], (o.flags));

    parse_uint64_t(obj["timeout"], (o.timeout));
}

static void parse_VkPerformanceQuerySubmitInfoKHR(const Json::Value &obj, VkPerformanceQuerySubmitInfoKHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPerformanceQuerySubmitInfoKHR *)parsePNextChain(obj);

    parse_uint32_t(obj["counterPassIndex"], (o.counterPassIndex));
}

static void parse_VkPerformanceQueryReservationInfoKHR(const Json::Value &obj, VkPerformanceQueryReservationInfoKHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPerformanceQueryReservationInfoKHR *)parsePNextChain(obj);

    parse_uint32_t(obj["maxPerformanceQueriesPerPool"], (o.maxPerformanceQueriesPerPool));
}

static void parse_VkPhysicalDeviceSurfaceInfo2KHR(const Json::Value &obj, VkPhysicalDeviceSurfaceInfo2KHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceSurfaceInfo2KHR *)parsePNextChain(obj);
}

static void parse_VkSurfaceCapabilities2KHR(const Json::Value &obj, VkSurfaceCapabilities2KHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkSurfaceCapabilities2KHR *)parsePNextChain(obj);

    parse_VkSurfaceCapabilitiesKHR(obj["surfaceCapabilities"], (o.surfaceCapabilities));
}

static void parse_VkSurfaceFormat2KHR(const Json::Value &obj, VkSurfaceFormat2KHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkSurfaceFormat2KHR *)parsePNextChain(obj);

    parse_VkSurfaceFormatKHR(obj["surfaceFormat"], (o.surfaceFormat));
}

static void parse_VkDisplayProperties2KHR(const Json::Value &obj, VkDisplayProperties2KHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkDisplayProperties2KHR *)parsePNextChain(obj);

    parse_VkDisplayPropertiesKHR(obj["displayProperties"], (o.displayProperties));
}

static void parse_VkDisplayPlaneProperties2KHR(const Json::Value &obj, VkDisplayPlaneProperties2KHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkDisplayPlaneProperties2KHR *)parsePNextChain(obj);

    parse_VkDisplayPlanePropertiesKHR(obj["displayPlaneProperties"], (o.displayPlaneProperties));
}

static void parse_VkDisplayModeProperties2KHR(const Json::Value &obj, VkDisplayModeProperties2KHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkDisplayModeProperties2KHR *)parsePNextChain(obj);

    parse_VkDisplayModePropertiesKHR(obj["displayModeProperties"], (o.displayModeProperties));
}

static void parse_VkDisplayPlaneInfo2KHR(const Json::Value &obj, VkDisplayPlaneInfo2KHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkDisplayPlaneInfo2KHR *)parsePNextChain(obj);

    parse_uint32_t(obj["planeIndex"], (o.planeIndex));
}

static void parse_VkDisplayPlaneCapabilities2KHR(const Json::Value &obj, VkDisplayPlaneCapabilities2KHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkDisplayPlaneCapabilities2KHR *)parsePNextChain(obj);

    parse_VkDisplayPlaneCapabilitiesKHR(obj["capabilities"], (o.capabilities));
}

static void parse_VkPhysicalDeviceShaderClockFeaturesKHR(const Json::Value &obj,
                                                         VkPhysicalDeviceShaderClockFeaturesKHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceShaderClockFeaturesKHR *)parsePNextChain(obj);

    parse_VkBool32(obj["shaderSubgroupClock"], (o.shaderSubgroupClock));

    parse_VkBool32(obj["shaderDeviceClock"], (o.shaderDeviceClock));
}

static std::map<std::string, int> VkQueueGlobalPriority_map = {
    std::make_pair("VK_QUEUE_GLOBAL_PRIORITY_LOW", 128),
    std::make_pair("VK_QUEUE_GLOBAL_PRIORITY_MEDIUM", 256),
    std::make_pair("VK_QUEUE_GLOBAL_PRIORITY_HIGH", 512),
    std::make_pair("VK_QUEUE_GLOBAL_PRIORITY_REALTIME", 1024),
    std::make_pair("VK_QUEUE_GLOBAL_PRIORITY_LOW_KHR", 128),
    std::make_pair("VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_KHR", 256),
    std::make_pair("VK_QUEUE_GLOBAL_PRIORITY_HIGH_KHR", 512),
    std::make_pair("VK_QUEUE_GLOBAL_PRIORITY_REALTIME_KHR", 1024),
    std::make_pair("VK_QUEUE_GLOBAL_PRIORITY_LOW_EXT", 128),
    std::make_pair("VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_EXT", 256),
    std::make_pair("VK_QUEUE_GLOBAL_PRIORITY_HIGH_EXT", 512),
    std::make_pair("VK_QUEUE_GLOBAL_PRIORITY_REALTIME_EXT", 1024),
};
static void parse_VkQueueGlobalPriority(const Json::Value &obj, VkQueueGlobalPriority &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkQueueGlobalPriority)VkQueueGlobalPriority_map[std::string(_res)];
}

static void parse_VkDeviceQueueGlobalPriorityCreateInfoKHR(const Json::Value &obj,
                                                           VkDeviceQueueGlobalPriorityCreateInfoKHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkDeviceQueueGlobalPriorityCreateInfoKHR *)parsePNextChain(obj);

    parse_VkQueueGlobalPriority(obj["globalPriority"], (o.globalPriority));
}

static void parse_VkPhysicalDeviceGlobalPriorityQueryFeaturesKHR(const Json::Value &obj,
                                                                 VkPhysicalDeviceGlobalPriorityQueryFeaturesKHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceGlobalPriorityQueryFeaturesKHR *)parsePNextChain(obj);

    parse_VkBool32(obj["globalPriorityQuery"], (o.globalPriorityQuery));
}

static void parse_VkQueueFamilyGlobalPriorityPropertiesKHR(const Json::Value &obj,
                                                           VkQueueFamilyGlobalPriorityPropertiesKHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkQueueFamilyGlobalPriorityPropertiesKHR *)parsePNextChain(obj);

    parse_uint32_t(obj["priorityCount"], (o.priorityCount));

    const Json::Value &obj_priorities_arr = obj["priorities"];
    for (unsigned int i = 0; i < obj_priorities_arr.size(); i++)
    {
        parse_VkQueueGlobalPriority(obj_priorities_arr[i], const_cast<VkQueueGlobalPriority &>((o.priorities[i])));
    }
}

static void parse_VkPhysicalDeviceShaderTerminateInvocationFeatures(
    const Json::Value &obj, VkPhysicalDeviceShaderTerminateInvocationFeatures &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceShaderTerminateInvocationFeatures *)parsePNextChain(obj);

    parse_VkBool32(obj["shaderTerminateInvocation"], (o.shaderTerminateInvocation));
}

static std::map<std::string, int> VkFragmentShadingRateCombinerOpKHR_map = {
    std::make_pair("VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR", 0),
    std::make_pair("VK_FRAGMENT_SHADING_RATE_COMBINER_OP_REPLACE_KHR", 1),
    std::make_pair("VK_FRAGMENT_SHADING_RATE_COMBINER_OP_MIN_KHR", 2),
    std::make_pair("VK_FRAGMENT_SHADING_RATE_COMBINER_OP_MAX_KHR", 3),
    std::make_pair("VK_FRAGMENT_SHADING_RATE_COMBINER_OP_MUL_KHR", 4),
};
static void parse_VkFragmentShadingRateCombinerOpKHR(const Json::Value &obj, VkFragmentShadingRateCombinerOpKHR &o)
{
    const std::string &_res = obj.asString();
    o = (VkFragmentShadingRateCombinerOpKHR)VkFragmentShadingRateCombinerOpKHR_map[std::string(_res)];
}

static void parse_VkFragmentShadingRateAttachmentInfoKHR(const Json::Value &obj,
                                                         VkFragmentShadingRateAttachmentInfoKHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkFragmentShadingRateAttachmentInfoKHR *)parsePNextChain(obj);

    {
        const Json::Value &obj_pFragmentShadingRateAttachment = obj["pFragmentShadingRateAttachment"];
        if (obj_pFragmentShadingRateAttachment.size() == 0)
        {
            (o.pFragmentShadingRateAttachment) = nullptr;
        }
        else
        {
            (o.pFragmentShadingRateAttachment) =
                (VkAttachmentReference2 *)s_globalMem.allocate(1, sizeof(VkAttachmentReference2));
            parse_VkAttachmentReference2(obj_pFragmentShadingRateAttachment,
                                         const_cast<VkAttachmentReference2 &>(*(o.pFragmentShadingRateAttachment)));
        }
    }

    parse_VkExtent2D(obj["shadingRateAttachmentTexelSize"], (o.shadingRateAttachmentTexelSize));
}

static void parse_VkPipelineFragmentShadingRateStateCreateInfoKHR(const Json::Value &obj,
                                                                  VkPipelineFragmentShadingRateStateCreateInfoKHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPipelineFragmentShadingRateStateCreateInfoKHR *)parsePNextChain(obj);

    parse_VkExtent2D(obj["fragmentSize"], (o.fragmentSize));

    const Json::Value &obj_combinerOps_arr = obj["combinerOps"];
    for (unsigned int i = 0; i < obj_combinerOps_arr.size(); i++)
    {
        parse_VkFragmentShadingRateCombinerOpKHR(obj_combinerOps_arr[i],
                                                 const_cast<VkFragmentShadingRateCombinerOpKHR &>((o.combinerOps[i])));
    }
}

static void parse_VkPhysicalDeviceFragmentShadingRateFeaturesKHR(const Json::Value &obj,
                                                                 VkPhysicalDeviceFragmentShadingRateFeaturesKHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceFragmentShadingRateFeaturesKHR *)parsePNextChain(obj);

    parse_VkBool32(obj["pipelineFragmentShadingRate"], (o.pipelineFragmentShadingRate));

    parse_VkBool32(obj["primitiveFragmentShadingRate"], (o.primitiveFragmentShadingRate));

    parse_VkBool32(obj["attachmentFragmentShadingRate"], (o.attachmentFragmentShadingRate));
}

static void parse_VkPhysicalDeviceFragmentShadingRatePropertiesKHR(const Json::Value &obj,
                                                                   VkPhysicalDeviceFragmentShadingRatePropertiesKHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceFragmentShadingRatePropertiesKHR *)parsePNextChain(obj);

    parse_VkExtent2D(obj["minFragmentShadingRateAttachmentTexelSize"], (o.minFragmentShadingRateAttachmentTexelSize));

    parse_VkExtent2D(obj["maxFragmentShadingRateAttachmentTexelSize"], (o.maxFragmentShadingRateAttachmentTexelSize));

    parse_uint32_t(obj["maxFragmentShadingRateAttachmentTexelSizeAspectRatio"],
                   (o.maxFragmentShadingRateAttachmentTexelSizeAspectRatio));

    parse_VkBool32(obj["primitiveFragmentShadingRateWithMultipleViewports"],
                   (o.primitiveFragmentShadingRateWithMultipleViewports));

    parse_VkBool32(obj["layeredShadingRateAttachments"], (o.layeredShadingRateAttachments));

    parse_VkBool32(obj["fragmentShadingRateNonTrivialCombinerOps"], (o.fragmentShadingRateNonTrivialCombinerOps));

    parse_VkExtent2D(obj["maxFragmentSize"], (o.maxFragmentSize));

    parse_uint32_t(obj["maxFragmentSizeAspectRatio"], (o.maxFragmentSizeAspectRatio));

    parse_uint32_t(obj["maxFragmentShadingRateCoverageSamples"], (o.maxFragmentShadingRateCoverageSamples));

    parse_VkSampleCountFlagBits(obj["maxFragmentShadingRateRasterizationSamples"],
                                (o.maxFragmentShadingRateRasterizationSamples));

    parse_VkBool32(obj["fragmentShadingRateWithShaderDepthStencilWrites"],
                   (o.fragmentShadingRateWithShaderDepthStencilWrites));

    parse_VkBool32(obj["fragmentShadingRateWithSampleMask"], (o.fragmentShadingRateWithSampleMask));

    parse_VkBool32(obj["fragmentShadingRateWithShaderSampleMask"], (o.fragmentShadingRateWithShaderSampleMask));

    parse_VkBool32(obj["fragmentShadingRateWithConservativeRasterization"],
                   (o.fragmentShadingRateWithConservativeRasterization));

    parse_VkBool32(obj["fragmentShadingRateWithFragmentShaderInterlock"],
                   (o.fragmentShadingRateWithFragmentShaderInterlock));

    parse_VkBool32(obj["fragmentShadingRateWithCustomSampleLocations"],
                   (o.fragmentShadingRateWithCustomSampleLocations));

    parse_VkBool32(obj["fragmentShadingRateStrictMultiplyCombiner"], (o.fragmentShadingRateStrictMultiplyCombiner));
}

static void parse_VkPhysicalDeviceFragmentShadingRateKHR(const Json::Value &obj,
                                                         VkPhysicalDeviceFragmentShadingRateKHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceFragmentShadingRateKHR *)parsePNextChain(obj);

    parse_VkSampleCountFlags(obj["sampleCounts"], (o.sampleCounts));

    parse_VkExtent2D(obj["fragmentSize"], (o.fragmentSize));
}

static std::map<std::string, int> VkRefreshObjectFlagBitsKHR_map = {};
static void parse_VkRefreshObjectFlagBitsKHR(const Json::Value &obj, VkRefreshObjectFlagBitsKHR &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkRefreshObjectFlagBitsKHR)VkRefreshObjectFlagBitsKHR_map[std::string(_res)];
}

static void parse_VkRefreshObjectFlagsKHR(const Json::Value &obj, VkRefreshObjectFlagsKHR &o)
{
    o                       = (VkRefreshObjectFlagsKHR)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkRefreshObjectFlagBitsKHR)VkRefreshObjectFlagBitsKHR_map[it];
    }
}

static void parse_VkRefreshObjectKHR(const Json::Value &obj, VkRefreshObjectKHR &o)
{

    parse_VkObjectType(obj["objectType"], (o.objectType));

    parse_uint64_t(obj["objectHandle"], (o.objectHandle));

    parse_VkRefreshObjectFlagsKHR(obj["flags"], (o.flags));
}

static void parse_VkRefreshObjectListKHR(const Json::Value &obj, VkRefreshObjectListKHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkRefreshObjectListKHR *)parsePNextChain(obj);

    parse_uint32_t(obj["objectCount"], (o.objectCount));

    (o.pObjects) = (VkRefreshObjectKHR *)s_globalMem.allocate((o.objectCount), sizeof(VkRefreshObjectKHR));
    const Json::Value &obj_pObjects = obj["pObjects"];
    if (obj_pObjects.size() == 0)
        (o.pObjects) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.objectCount); i++)
        {
            parse_VkRefreshObjectKHR(obj_pObjects[i], const_cast<VkRefreshObjectKHR &>((o.pObjects[i])));
        }
    }
}

static void parse_VkFlags64(const Json::Value &obj, VkFlags64 &o)
{
    const std::string &_res = obj.asString();
    sscanf(_res.c_str(), "%" SCNd64, &o);
}

static std::map<std::string, uint64_t> VkPipelineStageFlagBits2_map = {
    std::make_pair("VK_PIPELINE_STAGE_2_NONE", 0),
    std::make_pair("VK_PIPELINE_STAGE_2_NONE_KHR", 0),
    std::make_pair("VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT", 1ULL << 0),
    std::make_pair("VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR", 1ULL << 0),
    std::make_pair("VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT", 1ULL << 1),
    std::make_pair("VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT_KHR", 1ULL << 1),
    std::make_pair("VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT", 1ULL << 2),
    std::make_pair("VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT_KHR", 1ULL << 2),
    std::make_pair("VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT", 1ULL << 3),
    std::make_pair("VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT_KHR", 1ULL << 3),
    std::make_pair("VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT", 1ULL << 4),
    std::make_pair("VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT_KHR", 1ULL << 4),
    std::make_pair("VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT", 1ULL << 5),
    std::make_pair("VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT_KHR", 1ULL << 5),
    std::make_pair("VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT", 1ULL << 6),
    std::make_pair("VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT_KHR", 1ULL << 6),
    std::make_pair("VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT", 1ULL << 7),
    std::make_pair("VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR", 1ULL << 7),
    std::make_pair("VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT", 1ULL << 8),
    std::make_pair("VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT_KHR", 1ULL << 8),
    std::make_pair("VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT", 1ULL << 9),
    std::make_pair("VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT_KHR", 1ULL << 9),
    std::make_pair("VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT", 1ULL << 10),
    std::make_pair("VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR", 1ULL << 10),
    std::make_pair("VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT", 1ULL << 11),
    std::make_pair("VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR", 1ULL << 11),
    std::make_pair("VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT", 1ULL << 12),
    std::make_pair("VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT_KHR", 1ULL << 12),
    std::make_pair("VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR", 1ULL << 12),
    std::make_pair("VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT", 1ULL << 13),
    std::make_pair("VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR", 1ULL << 13),
    std::make_pair("VK_PIPELINE_STAGE_2_HOST_BIT", 1ULL << 14),
    std::make_pair("VK_PIPELINE_STAGE_2_HOST_BIT_KHR", 1ULL << 14),
    std::make_pair("VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT", 1ULL << 15),
    std::make_pair("VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT_KHR", 1ULL << 15),
    std::make_pair("VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT", 1ULL << 16),
    std::make_pair("VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR", 1ULL << 16),
    std::make_pair("VK_PIPELINE_STAGE_2_COPY_BIT", 1ULL << 32),
    std::make_pair("VK_PIPELINE_STAGE_2_COPY_BIT_KHR", 1ULL << 32),
    std::make_pair("VK_PIPELINE_STAGE_2_RESOLVE_BIT", 1ULL << 33),
    std::make_pair("VK_PIPELINE_STAGE_2_RESOLVE_BIT_KHR", 1ULL << 33),
    std::make_pair("VK_PIPELINE_STAGE_2_BLIT_BIT", 1ULL << 34),
    std::make_pair("VK_PIPELINE_STAGE_2_BLIT_BIT_KHR", 1ULL << 34),
    std::make_pair("VK_PIPELINE_STAGE_2_CLEAR_BIT", 1ULL << 35),
    std::make_pair("VK_PIPELINE_STAGE_2_CLEAR_BIT_KHR", 1ULL << 35),
    std::make_pair("VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT", 1ULL << 36),
    std::make_pair("VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT_KHR", 1ULL << 36),
    std::make_pair("VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT", 1ULL << 37),
    std::make_pair("VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT_KHR", 1ULL << 37),
    std::make_pair("VK_PIPELINE_STAGE_2_PRE_RASTERIZATION_SHADERS_BIT", 1ULL << 38),
    std::make_pair("VK_PIPELINE_STAGE_2_PRE_RASTERIZATION_SHADERS_BIT_KHR", 1ULL << 38),
    std::make_pair("VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR", 1ULL << 26),
    std::make_pair("VK_PIPELINE_STAGE_2_VIDEO_ENCODE_BIT_KHR", 1ULL << 27),
    std::make_pair("VK_PIPELINE_STAGE_2_TRANSFORM_FEEDBACK_BIT_EXT", 1ULL << 24),
    std::make_pair("VK_PIPELINE_STAGE_2_CONDITIONAL_RENDERING_BIT_EXT", 1ULL << 18),
    std::make_pair("VK_PIPELINE_STAGE_2_COMMAND_PREPROCESS_BIT_NV", 1ULL << 17),
    std::make_pair("VK_PIPELINE_STAGE_2_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR", 1ULL << 22),
    std::make_pair("VK_PIPELINE_STAGE_2_SHADING_RATE_IMAGE_BIT_NV", 1ULL << 22),
    std::make_pair("VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR", 1ULL << 25),
    std::make_pair("VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR", 1ULL << 21),
    std::make_pair("VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_NV", 1ULL << 21),
    std::make_pair("VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_NV", 1ULL << 25),
    std::make_pair("VK_PIPELINE_STAGE_2_FRAGMENT_DENSITY_PROCESS_BIT_EXT", 1ULL << 23),
    std::make_pair("VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_NV", 1ULL << 19),
    std::make_pair("VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_NV", 1ULL << 20),
    std::make_pair("VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT", 1ULL << 19),
    std::make_pair("VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT", 1ULL << 20),
    std::make_pair("VK_PIPELINE_STAGE_2_SUBPASS_SHADER_BIT_HUAWEI", 1ULL << 39),
    std::make_pair("VK_PIPELINE_STAGE_2_SUBPASS_SHADING_BIT_HUAWEI", 1ULL << 39),
    std::make_pair("VK_PIPELINE_STAGE_2_INVOCATION_MASK_BIT_HUAWEI", 1ULL << 40),
    std::make_pair("VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_COPY_BIT_KHR", 1ULL << 28),
    std::make_pair("VK_PIPELINE_STAGE_2_MICROMAP_BUILD_BIT_EXT", 1ULL << 30),
    std::make_pair("VK_PIPELINE_STAGE_2_CLUSTER_CULLING_SHADER_BIT_HUAWEI", 1ULL << 41),
    std::make_pair("VK_PIPELINE_STAGE_2_RESERVED_43_BIT_ARM", 1ULL << 43),
    std::make_pair("VK_PIPELINE_STAGE_2_OPTICAL_FLOW_BIT_NV", 1ULL << 29),
    std::make_pair("VK_PIPELINE_STAGE_2_RESERVED_44_BIT_NV", 1ULL << 44),
    std::make_pair("VK_PIPELINE_STAGE_2_RESERVED_42_BIT_EXT", 1ULL << 42),
    std::make_pair("VK_PIPELINE_STAGE_2_RESERVED_45_BIT_NV", 1ULL << 45),
};
static void parse_VkPipelineStageFlagBits2(const Json::Value &obj, VkPipelineStageFlagBits2 &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkPipelineStageFlagBits2)VkPipelineStageFlagBits2_map[std::string(_res)];
}

static std::map<std::string, uint64_t> VkPipelineStageFlagBits2KHR_map = {
    std::make_pair("VK_PIPELINE_STAGE_2_NONE", 0),
    std::make_pair("VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT", 1ULL << 0),
    std::make_pair("VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT", 1ULL << 1),
    std::make_pair("VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT", 1ULL << 2),
    std::make_pair("VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT", 1ULL << 3),
    std::make_pair("VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT", 1ULL << 4),
    std::make_pair("VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT", 1ULL << 5),
    std::make_pair("VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT", 1ULL << 6),
    std::make_pair("VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT", 1ULL << 7),
    std::make_pair("VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT", 1ULL << 8),
    std::make_pair("VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT", 1ULL << 9),
    std::make_pair("VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT", 1ULL << 10),
    std::make_pair("VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT", 1ULL << 11),
    std::make_pair("VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT", 1ULL << 12),
    std::make_pair("VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT", 1ULL << 13),
    std::make_pair("VK_PIPELINE_STAGE_2_HOST_BIT", 1ULL << 14),
    std::make_pair("VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT", 1ULL << 15),
    std::make_pair("VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT", 1ULL << 16),
    std::make_pair("VK_PIPELINE_STAGE_2_COPY_BIT", 1ULL << 32),
    std::make_pair("VK_PIPELINE_STAGE_2_RESOLVE_BIT", 1ULL << 33),
    std::make_pair("VK_PIPELINE_STAGE_2_BLIT_BIT", 1ULL << 34),
    std::make_pair("VK_PIPELINE_STAGE_2_CLEAR_BIT", 1ULL << 35),
    std::make_pair("VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT", 1ULL << 36),
    std::make_pair("VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT", 1ULL << 37),
    std::make_pair("VK_PIPELINE_STAGE_2_PRE_RASTERIZATION_SHADERS_BIT", 1ULL << 38),
    std::make_pair("VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR", 1ULL << 26),
    std::make_pair("VK_PIPELINE_STAGE_2_VIDEO_ENCODE_BIT_KHR", 1ULL << 27),
    std::make_pair("VK_PIPELINE_STAGE_2_TRANSFORM_FEEDBACK_BIT_EXT", 1ULL << 24),
    std::make_pair("VK_PIPELINE_STAGE_2_CONDITIONAL_RENDERING_BIT_EXT", 1ULL << 18),
    std::make_pair("VK_PIPELINE_STAGE_2_COMMAND_PREPROCESS_BIT_NV", 1ULL << 17),
    std::make_pair("VK_PIPELINE_STAGE_2_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR", 1ULL << 22),
    std::make_pair("VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR", 1ULL << 25),
    std::make_pair("VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR", 1ULL << 21),
    std::make_pair("VK_PIPELINE_STAGE_2_FRAGMENT_DENSITY_PROCESS_BIT_EXT", 1ULL << 23),
    std::make_pair("VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT", 1ULL << 19),
    std::make_pair("VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT", 1ULL << 20),
    std::make_pair("VK_PIPELINE_STAGE_2_SUBPASS_SHADER_BIT_HUAWEI", 1ULL << 39),
    std::make_pair("VK_PIPELINE_STAGE_2_INVOCATION_MASK_BIT_HUAWEI", 1ULL << 40),
    std::make_pair("VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_COPY_BIT_KHR", 1ULL << 28),
    std::make_pair("VK_PIPELINE_STAGE_2_MICROMAP_BUILD_BIT_EXT", 1ULL << 30),
    std::make_pair("VK_PIPELINE_STAGE_2_CLUSTER_CULLING_SHADER_BIT_HUAWEI", 1ULL << 41),
    std::make_pair("VK_PIPELINE_STAGE_2_RESERVED_43_BIT_ARM", 1ULL << 43),
    std::make_pair("VK_PIPELINE_STAGE_2_OPTICAL_FLOW_BIT_NV", 1ULL << 29),
    std::make_pair("VK_PIPELINE_STAGE_2_RESERVED_44_BIT_NV", 1ULL << 44),
    std::make_pair("VK_PIPELINE_STAGE_2_RESERVED_42_BIT_EXT", 1ULL << 42),
    std::make_pair("VK_PIPELINE_STAGE_2_RESERVED_45_BIT_NV", 1ULL << 45),
};
static void parse_VkPipelineStageFlagBits2KHR(const Json::Value &obj, VkPipelineStageFlagBits2KHR &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkPipelineStageFlagBits2KHR)VkPipelineStageFlagBits2KHR_map[std::string(_res)];
}

static std::map<std::string, uint64_t> VkAccessFlagBits2_map = {
    std::make_pair("VK_ACCESS_2_NONE", 0),
    std::make_pair("VK_ACCESS_2_NONE_KHR", 0),
    std::make_pair("VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT", 1ULL << 0),
    std::make_pair("VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT_KHR", 1ULL << 0),
    std::make_pair("VK_ACCESS_2_INDEX_READ_BIT", 1ULL << 1),
    std::make_pair("VK_ACCESS_2_INDEX_READ_BIT_KHR", 1ULL << 1),
    std::make_pair("VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT", 1ULL << 2),
    std::make_pair("VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT_KHR", 1ULL << 2),
    std::make_pair("VK_ACCESS_2_UNIFORM_READ_BIT", 1ULL << 3),
    std::make_pair("VK_ACCESS_2_UNIFORM_READ_BIT_KHR", 1ULL << 3),
    std::make_pair("VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT", 1ULL << 4),
    std::make_pair("VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT_KHR", 1ULL << 4),
    std::make_pair("VK_ACCESS_2_SHADER_READ_BIT", 1ULL << 5),
    std::make_pair("VK_ACCESS_2_SHADER_READ_BIT_KHR", 1ULL << 5),
    std::make_pair("VK_ACCESS_2_SHADER_WRITE_BIT", 1ULL << 6),
    std::make_pair("VK_ACCESS_2_SHADER_WRITE_BIT_KHR", 1ULL << 6),
    std::make_pair("VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT", 1ULL << 7),
    std::make_pair("VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT_KHR", 1ULL << 7),
    std::make_pair("VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT", 1ULL << 8),
    std::make_pair("VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR", 1ULL << 8),
    std::make_pair("VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT", 1ULL << 9),
    std::make_pair("VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT_KHR", 1ULL << 9),
    std::make_pair("VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT", 1ULL << 10),
    std::make_pair("VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT_KHR", 1ULL << 10),
    std::make_pair("VK_ACCESS_2_TRANSFER_READ_BIT", 1ULL << 11),
    std::make_pair("VK_ACCESS_2_TRANSFER_READ_BIT_KHR", 1ULL << 11),
    std::make_pair("VK_ACCESS_2_TRANSFER_WRITE_BIT", 1ULL << 12),
    std::make_pair("VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR", 1ULL << 12),
    std::make_pair("VK_ACCESS_2_HOST_READ_BIT", 1ULL << 13),
    std::make_pair("VK_ACCESS_2_HOST_READ_BIT_KHR", 1ULL << 13),
    std::make_pair("VK_ACCESS_2_HOST_WRITE_BIT", 1ULL << 14),
    std::make_pair("VK_ACCESS_2_HOST_WRITE_BIT_KHR", 1ULL << 14),
    std::make_pair("VK_ACCESS_2_MEMORY_READ_BIT", 1ULL << 15),
    std::make_pair("VK_ACCESS_2_MEMORY_READ_BIT_KHR", 1ULL << 15),
    std::make_pair("VK_ACCESS_2_MEMORY_WRITE_BIT", 1ULL << 16),
    std::make_pair("VK_ACCESS_2_MEMORY_WRITE_BIT_KHR", 1ULL << 16),
    std::make_pair("VK_ACCESS_2_SHADER_SAMPLED_READ_BIT", 1ULL << 32),
    std::make_pair("VK_ACCESS_2_SHADER_SAMPLED_READ_BIT_KHR", 1ULL << 32),
    std::make_pair("VK_ACCESS_2_SHADER_STORAGE_READ_BIT", 1ULL << 33),
    std::make_pair("VK_ACCESS_2_SHADER_STORAGE_READ_BIT_KHR", 1ULL << 33),
    std::make_pair("VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT", 1ULL << 34),
    std::make_pair("VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT_KHR", 1ULL << 34),
    std::make_pair("VK_ACCESS_2_VIDEO_DECODE_READ_BIT_KHR", 1ULL << 35),
    std::make_pair("VK_ACCESS_2_VIDEO_DECODE_WRITE_BIT_KHR", 1ULL << 36),
    std::make_pair("VK_ACCESS_2_RESERVED_57_BIT_KHR", 1ULL << 57),
    std::make_pair("VK_ACCESS_2_RESERVED_58_BIT_KHR", 1ULL << 58),
    std::make_pair("VK_ACCESS_2_RESERVED_59_BIT_KHR", 1ULL << 59),
    std::make_pair("VK_ACCESS_2_RESERVED_46_BIT_INTEL", 1ULL << 46),
    std::make_pair("VK_ACCESS_2_VIDEO_ENCODE_READ_BIT_KHR", 1ULL << 37),
    std::make_pair("VK_ACCESS_2_VIDEO_ENCODE_WRITE_BIT_KHR", 1ULL << 38),
    std::make_pair("VK_ACCESS_2_RESERVED_51_BIT_QCOM", 1ULL << 51),
    std::make_pair("VK_ACCESS_2_RESERVED_52_BIT_QCOM", 1ULL << 52),
    std::make_pair("VK_ACCESS_2_RESERVED_53_BIT_QCOM", 1ULL << 53),
    std::make_pair("VK_ACCESS_2_RESERVED_54_BIT_QCOM", 1ULL << 54),
    std::make_pair("VK_ACCESS_2_TRANSFORM_FEEDBACK_WRITE_BIT_EXT", 1ULL << 25),
    std::make_pair("VK_ACCESS_2_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT", 1ULL << 26),
    std::make_pair("VK_ACCESS_2_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT", 1ULL << 27),
    std::make_pair("VK_ACCESS_2_CONDITIONAL_RENDERING_READ_BIT_EXT", 1ULL << 20),
    std::make_pair("VK_ACCESS_2_COMMAND_PREPROCESS_READ_BIT_NV", 1ULL << 17),
    std::make_pair("VK_ACCESS_2_COMMAND_PREPROCESS_WRITE_BIT_NV", 1ULL << 18),
    std::make_pair("VK_ACCESS_2_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR", 1ULL << 23),
    std::make_pair("VK_ACCESS_2_SHADING_RATE_IMAGE_READ_BIT_NV", 1ULL << 23),
    std::make_pair("VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR", 1ULL << 21),
    std::make_pair("VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR", 1ULL << 22),
    std::make_pair("VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_NV", 1ULL << 21),
    std::make_pair("VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_NV", 1ULL << 22),
    std::make_pair("VK_ACCESS_2_FRAGMENT_DENSITY_MAP_READ_BIT_EXT", 1ULL << 24),
    std::make_pair("VK_ACCESS_2_COLOR_ATTACHMENT_READ_NONCOHERENT_BIT_EXT", 1ULL << 19),
    std::make_pair("VK_ACCESS_2_DESCRIPTOR_BUFFER_READ_BIT_EXT", 1ULL << 41),
    std::make_pair("VK_ACCESS_2_INVOCATION_MASK_READ_BIT_HUAWEI", 1ULL << 39),
    std::make_pair("VK_ACCESS_2_SHADER_BINDING_TABLE_READ_BIT_KHR", 1ULL << 40),
    std::make_pair("VK_ACCESS_2_MICROMAP_READ_BIT_EXT", 1ULL << 44),
    std::make_pair("VK_ACCESS_2_MICROMAP_WRITE_BIT_EXT", 1ULL << 45),
    std::make_pair("VK_ACCESS_2_RESERVED_49_BIT_ARM", 1ULL << 49),
    std::make_pair("VK_ACCESS_2_RESERVED_50_BIT_ARM", 1ULL << 50),
    std::make_pair("VK_ACCESS_2_OPTICAL_FLOW_READ_BIT_NV", 1ULL << 42),
    std::make_pair("VK_ACCESS_2_OPTICAL_FLOW_WRITE_BIT_NV", 1ULL << 43),
    std::make_pair("VK_ACCESS_2_RESERVED_47_BIT_EXT", 1ULL << 47),
    std::make_pair("VK_ACCESS_2_RESERVED_48_BIT_EXT", 1ULL << 48),
    std::make_pair("VK_ACCESS_2_RESERVED_55_BIT_NV", 1ULL << 55),
    std::make_pair("VK_ACCESS_2_RESERVED_56_BIT_NV", 1ULL << 56),
};
static void parse_VkAccessFlagBits2(const Json::Value &obj, VkAccessFlagBits2 &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkAccessFlagBits2)VkAccessFlagBits2_map[std::string(_res)];
}

static std::map<std::string, uint64_t> VkAccessFlagBits2KHR_map = {
    std::make_pair("VK_ACCESS_2_NONE", 0),
    std::make_pair("VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT", 1ULL << 0),
    std::make_pair("VK_ACCESS_2_INDEX_READ_BIT", 1ULL << 1),
    std::make_pair("VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT", 1ULL << 2),
    std::make_pair("VK_ACCESS_2_UNIFORM_READ_BIT", 1ULL << 3),
    std::make_pair("VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT", 1ULL << 4),
    std::make_pair("VK_ACCESS_2_SHADER_READ_BIT", 1ULL << 5),
    std::make_pair("VK_ACCESS_2_SHADER_WRITE_BIT", 1ULL << 6),
    std::make_pair("VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT", 1ULL << 7),
    std::make_pair("VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT", 1ULL << 8),
    std::make_pair("VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT", 1ULL << 9),
    std::make_pair("VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT", 1ULL << 10),
    std::make_pair("VK_ACCESS_2_TRANSFER_READ_BIT", 1ULL << 11),
    std::make_pair("VK_ACCESS_2_TRANSFER_WRITE_BIT", 1ULL << 12),
    std::make_pair("VK_ACCESS_2_HOST_READ_BIT", 1ULL << 13),
    std::make_pair("VK_ACCESS_2_HOST_WRITE_BIT", 1ULL << 14),
    std::make_pair("VK_ACCESS_2_MEMORY_READ_BIT", 1ULL << 15),
    std::make_pair("VK_ACCESS_2_MEMORY_WRITE_BIT", 1ULL << 16),
    std::make_pair("VK_ACCESS_2_SHADER_SAMPLED_READ_BIT", 1ULL << 32),
    std::make_pair("VK_ACCESS_2_SHADER_STORAGE_READ_BIT", 1ULL << 33),
    std::make_pair("VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT", 1ULL << 34),
    std::make_pair("VK_ACCESS_2_VIDEO_DECODE_READ_BIT_KHR", 1ULL << 35),
    std::make_pair("VK_ACCESS_2_VIDEO_DECODE_WRITE_BIT_KHR", 1ULL << 36),
    std::make_pair("VK_ACCESS_2_RESERVED_57_BIT_KHR", 1ULL << 57),
    std::make_pair("VK_ACCESS_2_RESERVED_58_BIT_KHR", 1ULL << 58),
    std::make_pair("VK_ACCESS_2_RESERVED_59_BIT_KHR", 1ULL << 59),
    std::make_pair("VK_ACCESS_2_RESERVED_46_BIT_INTEL", 1ULL << 46),
    std::make_pair("VK_ACCESS_2_VIDEO_ENCODE_READ_BIT_KHR", 1ULL << 37),
    std::make_pair("VK_ACCESS_2_VIDEO_ENCODE_WRITE_BIT_KHR", 1ULL << 38),
    std::make_pair("VK_ACCESS_2_RESERVED_51_BIT_QCOM", 1ULL << 51),
    std::make_pair("VK_ACCESS_2_RESERVED_52_BIT_QCOM", 1ULL << 52),
    std::make_pair("VK_ACCESS_2_RESERVED_53_BIT_QCOM", 1ULL << 53),
    std::make_pair("VK_ACCESS_2_RESERVED_54_BIT_QCOM", 1ULL << 54),
    std::make_pair("VK_ACCESS_2_TRANSFORM_FEEDBACK_WRITE_BIT_EXT", 1ULL << 25),
    std::make_pair("VK_ACCESS_2_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT", 1ULL << 26),
    std::make_pair("VK_ACCESS_2_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT", 1ULL << 27),
    std::make_pair("VK_ACCESS_2_CONDITIONAL_RENDERING_READ_BIT_EXT", 1ULL << 20),
    std::make_pair("VK_ACCESS_2_COMMAND_PREPROCESS_READ_BIT_NV", 1ULL << 17),
    std::make_pair("VK_ACCESS_2_COMMAND_PREPROCESS_WRITE_BIT_NV", 1ULL << 18),
    std::make_pair("VK_ACCESS_2_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR", 1ULL << 23),
    std::make_pair("VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR", 1ULL << 21),
    std::make_pair("VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR", 1ULL << 22),
    std::make_pair("VK_ACCESS_2_FRAGMENT_DENSITY_MAP_READ_BIT_EXT", 1ULL << 24),
    std::make_pair("VK_ACCESS_2_COLOR_ATTACHMENT_READ_NONCOHERENT_BIT_EXT", 1ULL << 19),
    std::make_pair("VK_ACCESS_2_DESCRIPTOR_BUFFER_READ_BIT_EXT", 1ULL << 41),
    std::make_pair("VK_ACCESS_2_INVOCATION_MASK_READ_BIT_HUAWEI", 1ULL << 39),
    std::make_pair("VK_ACCESS_2_SHADER_BINDING_TABLE_READ_BIT_KHR", 1ULL << 40),
    std::make_pair("VK_ACCESS_2_MICROMAP_READ_BIT_EXT", 1ULL << 44),
    std::make_pair("VK_ACCESS_2_MICROMAP_WRITE_BIT_EXT", 1ULL << 45),
    std::make_pair("VK_ACCESS_2_RESERVED_49_BIT_ARM", 1ULL << 49),
    std::make_pair("VK_ACCESS_2_RESERVED_50_BIT_ARM", 1ULL << 50),
    std::make_pair("VK_ACCESS_2_OPTICAL_FLOW_READ_BIT_NV", 1ULL << 42),
    std::make_pair("VK_ACCESS_2_OPTICAL_FLOW_WRITE_BIT_NV", 1ULL << 43),
    std::make_pair("VK_ACCESS_2_RESERVED_47_BIT_EXT", 1ULL << 47),
    std::make_pair("VK_ACCESS_2_RESERVED_48_BIT_EXT", 1ULL << 48),
    std::make_pair("VK_ACCESS_2_RESERVED_55_BIT_NV", 1ULL << 55),
    std::make_pair("VK_ACCESS_2_RESERVED_56_BIT_NV", 1ULL << 56),
};
static void parse_VkAccessFlagBits2KHR(const Json::Value &obj, VkAccessFlagBits2KHR &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkAccessFlagBits2KHR)VkAccessFlagBits2KHR_map[std::string(_res)];
}

static std::map<std::string, int> VkSubmitFlagBits_map = {
    std::make_pair("VK_SUBMIT_PROTECTED_BIT", 1UL << 0),
    std::make_pair("VK_SUBMIT_PROTECTED_BIT_KHR", 1UL << 0),
};
static void parse_VkSubmitFlagBits(const Json::Value &obj, VkSubmitFlagBits &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkSubmitFlagBits)VkSubmitFlagBits_map[std::string(_res)];
}

static std::map<std::string, int> VkSubmitFlagBitsKHR_map = {
    std::make_pair("VK_SUBMIT_PROTECTED_BIT", 1UL << 0),
};
static void parse_VkSubmitFlagBitsKHR(const Json::Value &obj, VkSubmitFlagBitsKHR &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkSubmitFlagBitsKHR)VkSubmitFlagBitsKHR_map[std::string(_res)];
}

static void parse_VkPipelineStageFlags2(const Json::Value &obj, VkPipelineStageFlags2 &o)
{
    if (obj.isString())
    {
        const std::string &_res = obj.asString();
        sscanf(_res.c_str(), "%" SCNd64, &o);
    }
    else
    {
        o = obj.asUInt();
    }
}

static void parse_VkAccessFlags2(const Json::Value &obj, VkAccessFlags2 &o)
{
    if (obj.isString())
    {
        const std::string &_res = obj.asString();
        sscanf(_res.c_str(), "%" SCNd64, &o);
    }
    else
    {
        o = obj.asUInt();
    }
}

static void parse_VkSubmitFlags(const Json::Value &obj, VkSubmitFlags &o)
{
    o                       = (VkSubmitFlags)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkSubmitFlagBits)VkSubmitFlagBits_map[it];
    }
}

static void parse_VkMemoryBarrier2(const Json::Value &obj, VkMemoryBarrier2 &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkMemoryBarrier2 *)parsePNextChain(obj);

    parse_VkPipelineStageFlags2(obj["srcStageMask"], (o.srcStageMask));

    parse_VkAccessFlags2(obj["srcAccessMask"], (o.srcAccessMask));

    parse_VkPipelineStageFlags2(obj["dstStageMask"], (o.dstStageMask));

    parse_VkAccessFlags2(obj["dstAccessMask"], (o.dstAccessMask));
}

static void parse_VkBufferMemoryBarrier2(const Json::Value &obj, VkBufferMemoryBarrier2 &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkBufferMemoryBarrier2 *)parsePNextChain(obj);

    parse_VkPipelineStageFlags2(obj["srcStageMask"], (o.srcStageMask));

    parse_VkAccessFlags2(obj["srcAccessMask"], (o.srcAccessMask));

    parse_VkPipelineStageFlags2(obj["dstStageMask"], (o.dstStageMask));

    parse_VkAccessFlags2(obj["dstAccessMask"], (o.dstAccessMask));

    parse_uint32_t(obj["srcQueueFamilyIndex"], (o.srcQueueFamilyIndex));

    parse_uint32_t(obj["dstQueueFamilyIndex"], (o.dstQueueFamilyIndex));

    parse_VkDeviceSize(obj["offset"], (o.offset));

    parse_VkDeviceSize(obj["size"], (o.size));
}

static void parse_VkImageMemoryBarrier2(const Json::Value &obj, VkImageMemoryBarrier2 &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkImageMemoryBarrier2 *)parsePNextChain(obj);

    parse_VkPipelineStageFlags2(obj["srcStageMask"], (o.srcStageMask));

    parse_VkAccessFlags2(obj["srcAccessMask"], (o.srcAccessMask));

    parse_VkPipelineStageFlags2(obj["dstStageMask"], (o.dstStageMask));

    parse_VkAccessFlags2(obj["dstAccessMask"], (o.dstAccessMask));

    parse_VkImageLayout(obj["oldLayout"], (o.oldLayout));

    parse_VkImageLayout(obj["newLayout"], (o.newLayout));

    parse_uint32_t(obj["srcQueueFamilyIndex"], (o.srcQueueFamilyIndex));

    parse_uint32_t(obj["dstQueueFamilyIndex"], (o.dstQueueFamilyIndex));

    parse_VkImageSubresourceRange(obj["subresourceRange"], (o.subresourceRange));
}

static void parse_VkDependencyInfo(const Json::Value &obj, VkDependencyInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkDependencyInfo *)parsePNextChain(obj);

    parse_VkDependencyFlags(obj["dependencyFlags"], (o.dependencyFlags));

    parse_uint32_t(obj["memoryBarrierCount"], (o.memoryBarrierCount));

    (o.pMemoryBarriers) = (VkMemoryBarrier2 *)s_globalMem.allocate((o.memoryBarrierCount), sizeof(VkMemoryBarrier2));
    const Json::Value &obj_pMemoryBarriers = obj["pMemoryBarriers"];
    if (obj_pMemoryBarriers.size() == 0)
        (o.pMemoryBarriers) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.memoryBarrierCount); i++)
        {
            parse_VkMemoryBarrier2(obj_pMemoryBarriers[i], const_cast<VkMemoryBarrier2 &>((o.pMemoryBarriers[i])));
        }
    }

    parse_uint32_t(obj["bufferMemoryBarrierCount"], (o.bufferMemoryBarrierCount));

    (o.pBufferMemoryBarriers) =
        (VkBufferMemoryBarrier2 *)s_globalMem.allocate((o.bufferMemoryBarrierCount), sizeof(VkBufferMemoryBarrier2));
    const Json::Value &obj_pBufferMemoryBarriers = obj["pBufferMemoryBarriers"];
    if (obj_pBufferMemoryBarriers.size() == 0)
        (o.pBufferMemoryBarriers) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.bufferMemoryBarrierCount); i++)
        {
            parse_VkBufferMemoryBarrier2(obj_pBufferMemoryBarriers[i],
                                         const_cast<VkBufferMemoryBarrier2 &>((o.pBufferMemoryBarriers[i])));
        }
    }

    parse_uint32_t(obj["imageMemoryBarrierCount"], (o.imageMemoryBarrierCount));

    (o.pImageMemoryBarriers) =
        (VkImageMemoryBarrier2 *)s_globalMem.allocate((o.imageMemoryBarrierCount), sizeof(VkImageMemoryBarrier2));
    const Json::Value &obj_pImageMemoryBarriers = obj["pImageMemoryBarriers"];
    if (obj_pImageMemoryBarriers.size() == 0)
        (o.pImageMemoryBarriers) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.imageMemoryBarrierCount); i++)
        {
            parse_VkImageMemoryBarrier2(obj_pImageMemoryBarriers[i],
                                        const_cast<VkImageMemoryBarrier2 &>((o.pImageMemoryBarriers[i])));
        }
    }
}

static void parse_VkSemaphoreSubmitInfo(const Json::Value &obj, VkSemaphoreSubmitInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkSemaphoreSubmitInfo *)parsePNextChain(obj);

    parse_uint64_t(obj["value"], (o.value));

    parse_VkPipelineStageFlags2(obj["stageMask"], (o.stageMask));

    parse_uint32_t(obj["deviceIndex"], (o.deviceIndex));
}

static void parse_VkCommandBufferSubmitInfo(const Json::Value &obj, VkCommandBufferSubmitInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkCommandBufferSubmitInfo *)parsePNextChain(obj);

    parse_uint32_t(obj["deviceMask"], (o.deviceMask));
}

static void parse_VkSubmitInfo2(const Json::Value &obj, VkSubmitInfo2 &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkSubmitInfo2 *)parsePNextChain(obj);

    parse_VkSubmitFlags(obj["flags"], (o.flags));

    parse_uint32_t(obj["waitSemaphoreInfoCount"], (o.waitSemaphoreInfoCount));

    (o.pWaitSemaphoreInfos) =
        (VkSemaphoreSubmitInfo *)s_globalMem.allocate((o.waitSemaphoreInfoCount), sizeof(VkSemaphoreSubmitInfo));
    const Json::Value &obj_pWaitSemaphoreInfos = obj["pWaitSemaphoreInfos"];
    if (obj_pWaitSemaphoreInfos.size() == 0)
        (o.pWaitSemaphoreInfos) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.waitSemaphoreInfoCount); i++)
        {
            parse_VkSemaphoreSubmitInfo(obj_pWaitSemaphoreInfos[i],
                                        const_cast<VkSemaphoreSubmitInfo &>((o.pWaitSemaphoreInfos[i])));
        }
    }

    parse_uint32_t(obj["commandBufferInfoCount"], (o.commandBufferInfoCount));

    (o.pCommandBufferInfos) = (VkCommandBufferSubmitInfo *)s_globalMem.allocate((o.commandBufferInfoCount),
                                                                                sizeof(VkCommandBufferSubmitInfo));
    const Json::Value &obj_pCommandBufferInfos = obj["pCommandBufferInfos"];
    if (obj_pCommandBufferInfos.size() == 0)
        (o.pCommandBufferInfos) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.commandBufferInfoCount); i++)
        {
            parse_VkCommandBufferSubmitInfo(obj_pCommandBufferInfos[i],
                                            const_cast<VkCommandBufferSubmitInfo &>((o.pCommandBufferInfos[i])));
        }
    }

    parse_uint32_t(obj["signalSemaphoreInfoCount"], (o.signalSemaphoreInfoCount));

    (o.pSignalSemaphoreInfos) =
        (VkSemaphoreSubmitInfo *)s_globalMem.allocate((o.signalSemaphoreInfoCount), sizeof(VkSemaphoreSubmitInfo));
    const Json::Value &obj_pSignalSemaphoreInfos = obj["pSignalSemaphoreInfos"];
    if (obj_pSignalSemaphoreInfos.size() == 0)
        (o.pSignalSemaphoreInfos) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.signalSemaphoreInfoCount); i++)
        {
            parse_VkSemaphoreSubmitInfo(obj_pSignalSemaphoreInfos[i],
                                        const_cast<VkSemaphoreSubmitInfo &>((o.pSignalSemaphoreInfos[i])));
        }
    }
}

static void parse_VkPhysicalDeviceSynchronization2Features(const Json::Value &obj,
                                                           VkPhysicalDeviceSynchronization2Features &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceSynchronization2Features *)parsePNextChain(obj);

    parse_VkBool32(obj["synchronization2"], (o.synchronization2));
}

static void parse_VkQueueFamilyCheckpointProperties2NV(const Json::Value &obj, VkQueueFamilyCheckpointProperties2NV &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkQueueFamilyCheckpointProperties2NV *)parsePNextChain(obj);

    parse_VkPipelineStageFlags2(obj["checkpointExecutionStageMask"], (o.checkpointExecutionStageMask));
}

static void parse_VkCheckpointData2NV(const Json::Value &obj, VkCheckpointData2NV &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkCheckpointData2NV *)parsePNextChain(obj);

    parse_VkPipelineStageFlags2(obj["stage"], (o.stage));
}

static void parse_VkBufferCopy2(const Json::Value &obj, VkBufferCopy2 &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkBufferCopy2 *)parsePNextChain(obj);

    parse_VkDeviceSize(obj["srcOffset"], (o.srcOffset));

    parse_VkDeviceSize(obj["dstOffset"], (o.dstOffset));

    parse_VkDeviceSize(obj["size"], (o.size));
}

static void parse_VkCopyBufferInfo2(const Json::Value &obj, VkCopyBufferInfo2 &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkCopyBufferInfo2 *)parsePNextChain(obj);

    parse_uint32_t(obj["regionCount"], (o.regionCount));

    (o.pRegions)                    = (VkBufferCopy2 *)s_globalMem.allocate((o.regionCount), sizeof(VkBufferCopy2));
    const Json::Value &obj_pRegions = obj["pRegions"];
    if (obj_pRegions.size() == 0)
        (o.pRegions) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.regionCount); i++)
        {
            parse_VkBufferCopy2(obj_pRegions[i], const_cast<VkBufferCopy2 &>((o.pRegions[i])));
        }
    }
}

static void parse_VkImageCopy2(const Json::Value &obj, VkImageCopy2 &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkImageCopy2 *)parsePNextChain(obj);

    parse_VkImageSubresourceLayers(obj["srcSubresource"], (o.srcSubresource));

    parse_VkOffset3D(obj["srcOffset"], (o.srcOffset));

    parse_VkImageSubresourceLayers(obj["dstSubresource"], (o.dstSubresource));

    parse_VkOffset3D(obj["dstOffset"], (o.dstOffset));

    parse_VkExtent3D(obj["extent"], (o.extent));
}

static void parse_VkCopyImageInfo2(const Json::Value &obj, VkCopyImageInfo2 &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkCopyImageInfo2 *)parsePNextChain(obj);

    parse_VkImageLayout(obj["srcImageLayout"], (o.srcImageLayout));

    parse_VkImageLayout(obj["dstImageLayout"], (o.dstImageLayout));

    parse_uint32_t(obj["regionCount"], (o.regionCount));

    (o.pRegions)                    = (VkImageCopy2 *)s_globalMem.allocate((o.regionCount), sizeof(VkImageCopy2));
    const Json::Value &obj_pRegions = obj["pRegions"];
    if (obj_pRegions.size() == 0)
        (o.pRegions) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.regionCount); i++)
        {
            parse_VkImageCopy2(obj_pRegions[i], const_cast<VkImageCopy2 &>((o.pRegions[i])));
        }
    }
}

static void parse_VkBufferImageCopy2(const Json::Value &obj, VkBufferImageCopy2 &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkBufferImageCopy2 *)parsePNextChain(obj);

    parse_VkDeviceSize(obj["bufferOffset"], (o.bufferOffset));

    parse_uint32_t(obj["bufferRowLength"], (o.bufferRowLength));

    parse_uint32_t(obj["bufferImageHeight"], (o.bufferImageHeight));

    parse_VkImageSubresourceLayers(obj["imageSubresource"], (o.imageSubresource));

    parse_VkOffset3D(obj["imageOffset"], (o.imageOffset));

    parse_VkExtent3D(obj["imageExtent"], (o.imageExtent));
}

static void parse_VkCopyBufferToImageInfo2(const Json::Value &obj, VkCopyBufferToImageInfo2 &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkCopyBufferToImageInfo2 *)parsePNextChain(obj);

    parse_VkImageLayout(obj["dstImageLayout"], (o.dstImageLayout));

    parse_uint32_t(obj["regionCount"], (o.regionCount));

    (o.pRegions) = (VkBufferImageCopy2 *)s_globalMem.allocate((o.regionCount), sizeof(VkBufferImageCopy2));
    const Json::Value &obj_pRegions = obj["pRegions"];
    if (obj_pRegions.size() == 0)
        (o.pRegions) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.regionCount); i++)
        {
            parse_VkBufferImageCopy2(obj_pRegions[i], const_cast<VkBufferImageCopy2 &>((o.pRegions[i])));
        }
    }
}

static void parse_VkCopyImageToBufferInfo2(const Json::Value &obj, VkCopyImageToBufferInfo2 &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkCopyImageToBufferInfo2 *)parsePNextChain(obj);

    parse_VkImageLayout(obj["srcImageLayout"], (o.srcImageLayout));

    parse_uint32_t(obj["regionCount"], (o.regionCount));

    (o.pRegions) = (VkBufferImageCopy2 *)s_globalMem.allocate((o.regionCount), sizeof(VkBufferImageCopy2));
    const Json::Value &obj_pRegions = obj["pRegions"];
    if (obj_pRegions.size() == 0)
        (o.pRegions) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.regionCount); i++)
        {
            parse_VkBufferImageCopy2(obj_pRegions[i], const_cast<VkBufferImageCopy2 &>((o.pRegions[i])));
        }
    }
}

static void parse_VkImageBlit2(const Json::Value &obj, VkImageBlit2 &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkImageBlit2 *)parsePNextChain(obj);

    parse_VkImageSubresourceLayers(obj["srcSubresource"], (o.srcSubresource));

    const Json::Value &obj_srcOffsets_arr = obj["srcOffsets"];
    for (unsigned int i = 0; i < obj_srcOffsets_arr.size(); i++)
    {
        parse_VkOffset3D(obj_srcOffsets_arr[i], const_cast<VkOffset3D &>((o.srcOffsets[i])));
    }

    parse_VkImageSubresourceLayers(obj["dstSubresource"], (o.dstSubresource));

    const Json::Value &obj_dstOffsets_arr = obj["dstOffsets"];
    for (unsigned int i = 0; i < obj_dstOffsets_arr.size(); i++)
    {
        parse_VkOffset3D(obj_dstOffsets_arr[i], const_cast<VkOffset3D &>((o.dstOffsets[i])));
    }
}

static void parse_VkBlitImageInfo2(const Json::Value &obj, VkBlitImageInfo2 &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkBlitImageInfo2 *)parsePNextChain(obj);

    parse_VkImageLayout(obj["srcImageLayout"], (o.srcImageLayout));

    parse_VkImageLayout(obj["dstImageLayout"], (o.dstImageLayout));

    parse_uint32_t(obj["regionCount"], (o.regionCount));

    (o.pRegions)                    = (VkImageBlit2 *)s_globalMem.allocate((o.regionCount), sizeof(VkImageBlit2));
    const Json::Value &obj_pRegions = obj["pRegions"];
    if (obj_pRegions.size() == 0)
        (o.pRegions) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.regionCount); i++)
        {
            parse_VkImageBlit2(obj_pRegions[i], const_cast<VkImageBlit2 &>((o.pRegions[i])));
        }
    }

    parse_VkFilter(obj["filter"], (o.filter));
}

static void parse_VkImageResolve2(const Json::Value &obj, VkImageResolve2 &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkImageResolve2 *)parsePNextChain(obj);

    parse_VkImageSubresourceLayers(obj["srcSubresource"], (o.srcSubresource));

    parse_VkOffset3D(obj["srcOffset"], (o.srcOffset));

    parse_VkImageSubresourceLayers(obj["dstSubresource"], (o.dstSubresource));

    parse_VkOffset3D(obj["dstOffset"], (o.dstOffset));

    parse_VkExtent3D(obj["extent"], (o.extent));
}

static void parse_VkResolveImageInfo2(const Json::Value &obj, VkResolveImageInfo2 &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkResolveImageInfo2 *)parsePNextChain(obj);

    parse_VkImageLayout(obj["srcImageLayout"], (o.srcImageLayout));

    parse_VkImageLayout(obj["dstImageLayout"], (o.dstImageLayout));

    parse_uint32_t(obj["regionCount"], (o.regionCount));

    (o.pRegions)                    = (VkImageResolve2 *)s_globalMem.allocate((o.regionCount), sizeof(VkImageResolve2));
    const Json::Value &obj_pRegions = obj["pRegions"];
    if (obj_pRegions.size() == 0)
        (o.pRegions) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.regionCount); i++)
        {
            parse_VkImageResolve2(obj_pRegions[i], const_cast<VkImageResolve2 &>((o.pRegions[i])));
        }
    }
}

static void parse_VkPhysicalDeviceVertexAttributeDivisorPropertiesKHR(
    const Json::Value &obj, VkPhysicalDeviceVertexAttributeDivisorPropertiesKHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceVertexAttributeDivisorPropertiesKHR *)parsePNextChain(obj);

    parse_uint32_t(obj["maxVertexAttribDivisor"], (o.maxVertexAttribDivisor));

    parse_VkBool32(obj["supportsNonZeroFirstInstance"], (o.supportsNonZeroFirstInstance));
}

static void parse_VkVertexInputBindingDivisorDescriptionKHR(const Json::Value &obj,
                                                            VkVertexInputBindingDivisorDescriptionKHR &o)
{

    parse_uint32_t(obj["binding"], (o.binding));

    parse_uint32_t(obj["divisor"], (o.divisor));
}

static void parse_VkPipelineVertexInputDivisorStateCreateInfoKHR(const Json::Value &obj,
                                                                 VkPipelineVertexInputDivisorStateCreateInfoKHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPipelineVertexInputDivisorStateCreateInfoKHR *)parsePNextChain(obj);

    parse_uint32_t(obj["vertexBindingDivisorCount"], (o.vertexBindingDivisorCount));

    (o.pVertexBindingDivisors) = (VkVertexInputBindingDivisorDescriptionKHR *)s_globalMem.allocate(
        (o.vertexBindingDivisorCount), sizeof(VkVertexInputBindingDivisorDescriptionKHR));
    const Json::Value &obj_pVertexBindingDivisors = obj["pVertexBindingDivisors"];
    if (obj_pVertexBindingDivisors.size() == 0)
        (o.pVertexBindingDivisors) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.vertexBindingDivisorCount); i++)
        {
            parse_VkVertexInputBindingDivisorDescriptionKHR(
                obj_pVertexBindingDivisors[i],
                const_cast<VkVertexInputBindingDivisorDescriptionKHR &>((o.pVertexBindingDivisors[i])));
        }
    }
}

static void parse_VkPhysicalDeviceVertexAttributeDivisorFeaturesKHR(
    const Json::Value &obj, VkPhysicalDeviceVertexAttributeDivisorFeaturesKHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceVertexAttributeDivisorFeaturesKHR *)parsePNextChain(obj);

    parse_VkBool32(obj["vertexAttributeInstanceRateDivisor"], (o.vertexAttributeInstanceRateDivisor));

    parse_VkBool32(obj["vertexAttributeInstanceRateZeroDivisor"], (o.vertexAttributeInstanceRateZeroDivisor));
}

static void parse_VkPhysicalDeviceIndexTypeUint8FeaturesKHR(const Json::Value &obj,
                                                            VkPhysicalDeviceIndexTypeUint8FeaturesKHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceIndexTypeUint8FeaturesKHR *)parsePNextChain(obj);

    parse_VkBool32(obj["indexTypeUint8"], (o.indexTypeUint8));
}

static std::map<std::string, int> VkLineRasterizationMode_map = {
    std::make_pair("VK_LINE_RASTERIZATION_MODE_DEFAULT", 0),
    std::make_pair("VK_LINE_RASTERIZATION_MODE_DEFAULT_KHR", 0),
    std::make_pair("VK_LINE_RASTERIZATION_MODE_DEFAULT_EXT", 0),
    std::make_pair("VK_LINE_RASTERIZATION_MODE_RECTANGULAR", 1),
    std::make_pair("VK_LINE_RASTERIZATION_MODE_RECTANGULAR_KHR", 1),
    std::make_pair("VK_LINE_RASTERIZATION_MODE_RECTANGULAR_EXT", 1),
    std::make_pair("VK_LINE_RASTERIZATION_MODE_BRESENHAM", 2),
    std::make_pair("VK_LINE_RASTERIZATION_MODE_BRESENHAM_KHR", 2),
    std::make_pair("VK_LINE_RASTERIZATION_MODE_BRESENHAM_EXT", 2),
    std::make_pair("VK_LINE_RASTERIZATION_MODE_RECTANGULAR_SMOOTH", 3),
    std::make_pair("VK_LINE_RASTERIZATION_MODE_RECTANGULAR_SMOOTH_KHR", 3),
    std::make_pair("VK_LINE_RASTERIZATION_MODE_RECTANGULAR_SMOOTH_EXT", 3),
};
static void parse_VkLineRasterizationMode(const Json::Value &obj, VkLineRasterizationMode &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkLineRasterizationMode)VkLineRasterizationMode_map[std::string(_res)];
}

static void parse_VkPhysicalDeviceLineRasterizationFeaturesKHR(const Json::Value &obj,
                                                               VkPhysicalDeviceLineRasterizationFeaturesKHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceLineRasterizationFeaturesKHR *)parsePNextChain(obj);

    parse_VkBool32(obj["rectangularLines"], (o.rectangularLines));

    parse_VkBool32(obj["bresenhamLines"], (o.bresenhamLines));

    parse_VkBool32(obj["smoothLines"], (o.smoothLines));

    parse_VkBool32(obj["stippledRectangularLines"], (o.stippledRectangularLines));

    parse_VkBool32(obj["stippledBresenhamLines"], (o.stippledBresenhamLines));

    parse_VkBool32(obj["stippledSmoothLines"], (o.stippledSmoothLines));
}

static void parse_VkPhysicalDeviceLineRasterizationPropertiesKHR(const Json::Value &obj,
                                                                 VkPhysicalDeviceLineRasterizationPropertiesKHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceLineRasterizationPropertiesKHR *)parsePNextChain(obj);

    parse_uint32_t(obj["lineSubPixelPrecisionBits"], (o.lineSubPixelPrecisionBits));
}

static void parse_VkPipelineRasterizationLineStateCreateInfoKHR(const Json::Value &obj,
                                                                VkPipelineRasterizationLineStateCreateInfoKHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPipelineRasterizationLineStateCreateInfoKHR *)parsePNextChain(obj);

    parse_VkLineRasterizationMode(obj["lineRasterizationMode"], (o.lineRasterizationMode));

    parse_VkBool32(obj["stippledLineEnable"], (o.stippledLineEnable));

    parse_uint32_t(obj["lineStippleFactor"], (o.lineStippleFactor));

    parse_uint16_t(obj["lineStipplePattern"], (o.lineStipplePattern));
}

static std::map<std::string, int> VkTimeDomainKHR_map = {
    std::make_pair("VK_TIME_DOMAIN_DEVICE_KHR", 0),
    std::make_pair("VK_TIME_DOMAIN_CLOCK_MONOTONIC_KHR", 1),
    std::make_pair("VK_TIME_DOMAIN_CLOCK_MONOTONIC_RAW_KHR", 2),
    std::make_pair("VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_KHR", 3),
    std::make_pair("VK_TIME_DOMAIN_DEVICE_EXT", 0),
    std::make_pair("VK_TIME_DOMAIN_CLOCK_MONOTONIC_EXT", 1),
    std::make_pair("VK_TIME_DOMAIN_CLOCK_MONOTONIC_RAW_EXT", 2),
    std::make_pair("VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_EXT", 3),
};
static void parse_VkTimeDomainKHR(const Json::Value &obj, VkTimeDomainKHR &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkTimeDomainKHR)VkTimeDomainKHR_map[std::string(_res)];
}

static void parse_VkCalibratedTimestampInfoKHR(const Json::Value &obj, VkCalibratedTimestampInfoKHR &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkCalibratedTimestampInfoKHR *)parsePNextChain(obj);

    parse_VkTimeDomainKHR(obj["timeDomain"], (o.timeDomain));
}

static void parse_VkPhysicalDeviceTextureCompressionASTCHDRFeatures(
    const Json::Value &obj, VkPhysicalDeviceTextureCompressionASTCHDRFeatures &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceTextureCompressionASTCHDRFeatures *)parsePNextChain(obj);

    parse_VkBool32(obj["textureCompressionASTC_HDR"], (o.textureCompressionASTC_HDR));
}

static void parse_VkImageViewASTCDecodeModeEXT(const Json::Value &obj, VkImageViewASTCDecodeModeEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkImageViewASTCDecodeModeEXT *)parsePNextChain(obj);

    parse_VkFormat(obj["decodeMode"], (o.decodeMode));
}

static void parse_VkPhysicalDeviceASTCDecodeFeaturesEXT(const Json::Value &obj,
                                                        VkPhysicalDeviceASTCDecodeFeaturesEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceASTCDecodeFeaturesEXT *)parsePNextChain(obj);

    parse_VkBool32(obj["decodeModeSharedExponent"], (o.decodeModeSharedExponent));
}

static std::map<std::string, int> VkSurfaceCounterFlagBitsEXT_map = {
    std::make_pair("VK_SURFACE_COUNTER_VBLANK_BIT_EXT", 1UL << 0),
    std::make_pair("VK_SURFACE_COUNTER_VBLANK_EXT", 1UL << 0),
};
static void parse_VkSurfaceCounterFlagBitsEXT(const Json::Value &obj, VkSurfaceCounterFlagBitsEXT &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkSurfaceCounterFlagBitsEXT)VkSurfaceCounterFlagBitsEXT_map[std::string(_res)];
}

static void parse_VkSurfaceCounterFlagsEXT(const Json::Value &obj, VkSurfaceCounterFlagsEXT &o)
{
    o                       = (VkSurfaceCounterFlagsEXT)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkSurfaceCounterFlagBitsEXT)VkSurfaceCounterFlagBitsEXT_map[it];
    }
}

static void parse_VkSurfaceCapabilities2EXT(const Json::Value &obj, VkSurfaceCapabilities2EXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkSurfaceCapabilities2EXT *)parsePNextChain(obj);

    parse_uint32_t(obj["minImageCount"], (o.minImageCount));

    parse_uint32_t(obj["maxImageCount"], (o.maxImageCount));

    parse_VkExtent2D(obj["currentExtent"], (o.currentExtent));

    parse_VkExtent2D(obj["minImageExtent"], (o.minImageExtent));

    parse_VkExtent2D(obj["maxImageExtent"], (o.maxImageExtent));

    parse_uint32_t(obj["maxImageArrayLayers"], (o.maxImageArrayLayers));

    parse_VkSurfaceTransformFlagsKHR(obj["supportedTransforms"], (o.supportedTransforms));

    parse_VkSurfaceTransformFlagBitsKHR(obj["currentTransform"], (o.currentTransform));

    parse_VkCompositeAlphaFlagsKHR(obj["supportedCompositeAlpha"], (o.supportedCompositeAlpha));

    parse_VkImageUsageFlags(obj["supportedUsageFlags"], (o.supportedUsageFlags));

    parse_VkSurfaceCounterFlagsEXT(obj["supportedSurfaceCounters"], (o.supportedSurfaceCounters));
}

static std::map<std::string, int> VkDisplayPowerStateEXT_map = {
    std::make_pair("VK_DISPLAY_POWER_STATE_OFF_EXT", 0),
    std::make_pair("VK_DISPLAY_POWER_STATE_SUSPEND_EXT", 1),
    std::make_pair("VK_DISPLAY_POWER_STATE_ON_EXT", 2),
};
static void parse_VkDisplayPowerStateEXT(const Json::Value &obj, VkDisplayPowerStateEXT &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkDisplayPowerStateEXT)VkDisplayPowerStateEXT_map[std::string(_res)];
}

static std::map<std::string, int> VkDeviceEventTypeEXT_map = {
    std::make_pair("VK_DEVICE_EVENT_TYPE_DISPLAY_HOTPLUG_EXT", 0),
};
static void parse_VkDeviceEventTypeEXT(const Json::Value &obj, VkDeviceEventTypeEXT &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkDeviceEventTypeEXT)VkDeviceEventTypeEXT_map[std::string(_res)];
}

static std::map<std::string, int> VkDisplayEventTypeEXT_map = {
    std::make_pair("VK_DISPLAY_EVENT_TYPE_FIRST_PIXEL_OUT_EXT", 0),
};
static void parse_VkDisplayEventTypeEXT(const Json::Value &obj, VkDisplayEventTypeEXT &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkDisplayEventTypeEXT)VkDisplayEventTypeEXT_map[std::string(_res)];
}

static void parse_VkDisplayPowerInfoEXT(const Json::Value &obj, VkDisplayPowerInfoEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkDisplayPowerInfoEXT *)parsePNextChain(obj);

    parse_VkDisplayPowerStateEXT(obj["powerState"], (o.powerState));
}

static void parse_VkDeviceEventInfoEXT(const Json::Value &obj, VkDeviceEventInfoEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkDeviceEventInfoEXT *)parsePNextChain(obj);

    parse_VkDeviceEventTypeEXT(obj["deviceEvent"], (o.deviceEvent));
}

static void parse_VkDisplayEventInfoEXT(const Json::Value &obj, VkDisplayEventInfoEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkDisplayEventInfoEXT *)parsePNextChain(obj);

    parse_VkDisplayEventTypeEXT(obj["displayEvent"], (o.displayEvent));
}

static void parse_VkSwapchainCounterCreateInfoEXT(const Json::Value &obj, VkSwapchainCounterCreateInfoEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkSwapchainCounterCreateInfoEXT *)parsePNextChain(obj);

    parse_VkSurfaceCounterFlagsEXT(obj["surfaceCounters"], (o.surfaceCounters));
}

static std::map<std::string, int> VkDiscardRectangleModeEXT_map = {
    std::make_pair("VK_DISCARD_RECTANGLE_MODE_INCLUSIVE_EXT", 0),
    std::make_pair("VK_DISCARD_RECTANGLE_MODE_EXCLUSIVE_EXT", 1),
};
static void parse_VkDiscardRectangleModeEXT(const Json::Value &obj, VkDiscardRectangleModeEXT &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkDiscardRectangleModeEXT)VkDiscardRectangleModeEXT_map[std::string(_res)];
}

static void parse_VkPipelineDiscardRectangleStateCreateFlagsEXT(const Json::Value &obj,
                                                                VkPipelineDiscardRectangleStateCreateFlagsEXT &o)
{
    if (obj.isString())
    {
        const std::string &_res = obj.asString();
        sscanf(_res.c_str(), "%u", &o);
    }
    else
    {
        o = obj.asUInt();
    }
}

static void parse_VkPhysicalDeviceDiscardRectanglePropertiesEXT(const Json::Value &obj,
                                                                VkPhysicalDeviceDiscardRectanglePropertiesEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceDiscardRectanglePropertiesEXT *)parsePNextChain(obj);

    parse_uint32_t(obj["maxDiscardRectangles"], (o.maxDiscardRectangles));
}

static void parse_VkPipelineDiscardRectangleStateCreateInfoEXT(const Json::Value &obj,
                                                               VkPipelineDiscardRectangleStateCreateInfoEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPipelineDiscardRectangleStateCreateInfoEXT *)parsePNextChain(obj);

    parse_VkPipelineDiscardRectangleStateCreateFlagsEXT(obj["flags"], (o.flags));

    parse_VkDiscardRectangleModeEXT(obj["discardRectangleMode"], (o.discardRectangleMode));

    parse_uint32_t(obj["discardRectangleCount"], (o.discardRectangleCount));

    (o.pDiscardRectangles) = (VkRect2D *)s_globalMem.allocate((o.discardRectangleCount), sizeof(VkRect2D));
    const Json::Value &obj_pDiscardRectangles = obj["pDiscardRectangles"];
    if (obj_pDiscardRectangles.size() == 0)
        (o.pDiscardRectangles) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.discardRectangleCount); i++)
        {
            parse_VkRect2D(obj_pDiscardRectangles[i], const_cast<VkRect2D &>((o.pDiscardRectangles[i])));
        }
    }
}

static std::map<std::string, int> VkConservativeRasterizationModeEXT_map = {
    std::make_pair("VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT", 0),
    std::make_pair("VK_CONSERVATIVE_RASTERIZATION_MODE_OVERESTIMATE_EXT", 1),
    std::make_pair("VK_CONSERVATIVE_RASTERIZATION_MODE_UNDERESTIMATE_EXT", 2),
};
static void parse_VkConservativeRasterizationModeEXT(const Json::Value &obj, VkConservativeRasterizationModeEXT &o)
{
    const std::string &_res = obj.asString();
    o = (VkConservativeRasterizationModeEXT)VkConservativeRasterizationModeEXT_map[std::string(_res)];
}

static void parse_VkPipelineRasterizationConservativeStateCreateFlagsEXT(
    const Json::Value &obj, VkPipelineRasterizationConservativeStateCreateFlagsEXT &o)
{
    if (obj.isString())
    {
        const std::string &_res = obj.asString();
        sscanf(_res.c_str(), "%u", &o);
    }
    else
    {
        o = obj.asUInt();
    }
}

static void parse_VkPhysicalDeviceConservativeRasterizationPropertiesEXT(
    const Json::Value &obj, VkPhysicalDeviceConservativeRasterizationPropertiesEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceConservativeRasterizationPropertiesEXT *)parsePNextChain(obj);

    parse_float(obj["primitiveOverestimationSize"], (o.primitiveOverestimationSize));

    parse_float(obj["maxExtraPrimitiveOverestimationSize"], (o.maxExtraPrimitiveOverestimationSize));

    parse_float(obj["extraPrimitiveOverestimationSizeGranularity"], (o.extraPrimitiveOverestimationSizeGranularity));

    parse_VkBool32(obj["primitiveUnderestimation"], (o.primitiveUnderestimation));

    parse_VkBool32(obj["conservativePointAndLineRasterization"], (o.conservativePointAndLineRasterization));

    parse_VkBool32(obj["degenerateTrianglesRasterized"], (o.degenerateTrianglesRasterized));

    parse_VkBool32(obj["degenerateLinesRasterized"], (o.degenerateLinesRasterized));

    parse_VkBool32(obj["fullyCoveredFragmentShaderInputVariable"], (o.fullyCoveredFragmentShaderInputVariable));

    parse_VkBool32(obj["conservativeRasterizationPostDepthCoverage"], (o.conservativeRasterizationPostDepthCoverage));
}

static void parse_VkPipelineRasterizationConservativeStateCreateInfoEXT(
    const Json::Value &obj, VkPipelineRasterizationConservativeStateCreateInfoEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPipelineRasterizationConservativeStateCreateInfoEXT *)parsePNextChain(obj);

    parse_VkPipelineRasterizationConservativeStateCreateFlagsEXT(obj["flags"], (o.flags));

    parse_VkConservativeRasterizationModeEXT(obj["conservativeRasterizationMode"], (o.conservativeRasterizationMode));

    parse_float(obj["extraPrimitiveOverestimationSize"], (o.extraPrimitiveOverestimationSize));
}

static void parse_VkPipelineRasterizationDepthClipStateCreateFlagsEXT(
    const Json::Value &obj, VkPipelineRasterizationDepthClipStateCreateFlagsEXT &o)
{
    if (obj.isString())
    {
        const std::string &_res = obj.asString();
        sscanf(_res.c_str(), "%u", &o);
    }
    else
    {
        o = obj.asUInt();
    }
}

static void parse_VkPhysicalDeviceDepthClipEnableFeaturesEXT(const Json::Value &obj,
                                                             VkPhysicalDeviceDepthClipEnableFeaturesEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceDepthClipEnableFeaturesEXT *)parsePNextChain(obj);

    parse_VkBool32(obj["depthClipEnable"], (o.depthClipEnable));
}

static void parse_VkPipelineRasterizationDepthClipStateCreateInfoEXT(
    const Json::Value &obj, VkPipelineRasterizationDepthClipStateCreateInfoEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPipelineRasterizationDepthClipStateCreateInfoEXT *)parsePNextChain(obj);

    parse_VkPipelineRasterizationDepthClipStateCreateFlagsEXT(obj["flags"], (o.flags));

    parse_VkBool32(obj["depthClipEnable"], (o.depthClipEnable));
}

static void parse_VkXYColorEXT(const Json::Value &obj, VkXYColorEXT &o)
{

    parse_float(obj["x"], (o.x));

    parse_float(obj["y"], (o.y));
}

static void parse_VkHdrMetadataEXT(const Json::Value &obj, VkHdrMetadataEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkHdrMetadataEXT *)parsePNextChain(obj);

    parse_VkXYColorEXT(obj["displayPrimaryRed"], (o.displayPrimaryRed));

    parse_VkXYColorEXT(obj["displayPrimaryGreen"], (o.displayPrimaryGreen));

    parse_VkXYColorEXT(obj["displayPrimaryBlue"], (o.displayPrimaryBlue));

    parse_VkXYColorEXT(obj["whitePoint"], (o.whitePoint));

    parse_float(obj["maxLuminance"], (o.maxLuminance));

    parse_float(obj["minLuminance"], (o.minLuminance));

    parse_float(obj["maxContentLightLevel"], (o.maxContentLightLevel));

    parse_float(obj["maxFrameAverageLightLevel"], (o.maxFrameAverageLightLevel));
}

static void parse_VkDebugUtilsMessengerEXT(const Json::Value &obj, VkDebugUtilsMessengerEXT &o)
{
    //    const std::string& _res = obj.asString();
}

static std::map<std::string, int> VkDebugUtilsMessageSeverityFlagBitsEXT_map = {
    std::make_pair("VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT", 1UL << 0),
    std::make_pair("VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT", 1UL << 4),
    std::make_pair("VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT", 1UL << 8),
    std::make_pair("VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT", 1UL << 12),
};
static void parse_VkDebugUtilsMessageSeverityFlagBitsEXT(const Json::Value &obj,
                                                         VkDebugUtilsMessageSeverityFlagBitsEXT &o)
{
    const std::string &_res = obj.asString();
    o = (VkDebugUtilsMessageSeverityFlagBitsEXT)VkDebugUtilsMessageSeverityFlagBitsEXT_map[std::string(_res)];
}

static std::map<std::string, int> VkDebugUtilsMessageTypeFlagBitsEXT_map = {
    std::make_pair("VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT", 1UL << 0),
    std::make_pair("VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT", 1UL << 1),
    std::make_pair("VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT", 1UL << 2),
    std::make_pair("VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT", 1UL << 3),
};
static void parse_VkDebugUtilsMessageTypeFlagBitsEXT(const Json::Value &obj, VkDebugUtilsMessageTypeFlagBitsEXT &o)
{
    const std::string &_res = obj.asString();
    o = (VkDebugUtilsMessageTypeFlagBitsEXT)VkDebugUtilsMessageTypeFlagBitsEXT_map[std::string(_res)];
}

static void parse_VkDebugUtilsMessengerCallbackDataFlagsEXT(const Json::Value &obj,
                                                            VkDebugUtilsMessengerCallbackDataFlagsEXT &o)
{
    if (obj.isString())
    {
        const std::string &_res = obj.asString();
        sscanf(_res.c_str(), "%u", &o);
    }
    else
    {
        o = obj.asUInt();
    }
}

static void parse_VkDebugUtilsMessageTypeFlagsEXT(const Json::Value &obj, VkDebugUtilsMessageTypeFlagsEXT &o)
{
    o                       = (VkDebugUtilsMessageTypeFlagsEXT)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkDebugUtilsMessageTypeFlagBitsEXT)VkDebugUtilsMessageTypeFlagBitsEXT_map[it];
    }
}

static void parse_VkDebugUtilsMessageSeverityFlagsEXT(const Json::Value &obj, VkDebugUtilsMessageSeverityFlagsEXT &o)
{
    o                       = (VkDebugUtilsMessageSeverityFlagsEXT)0;
    const std::string &_res = obj.asString();
    std::vector<std::string> bitmasks;
    std::istringstream inputStream(_res);
    std::string tempStr;
    while (getline(inputStream, tempStr, '|'))
    {
        tempStr.erase(std::remove_if(tempStr.begin(), tempStr.end(), isspace), tempStr.end());
        bitmasks.push_back(tempStr);
    }
    for (auto &it : bitmasks)
    {
        o |= (VkDebugUtilsMessageSeverityFlagBitsEXT)VkDebugUtilsMessageSeverityFlagBitsEXT_map[it];
    }
}

static void parse_VkDebugUtilsMessengerCreateFlagsEXT(const Json::Value &obj, VkDebugUtilsMessengerCreateFlagsEXT &o)
{
    if (obj.isString())
    {
        const std::string &_res = obj.asString();
        sscanf(_res.c_str(), "%u", &o);
    }
    else
    {
        o = obj.asUInt();
    }
}

static void parse_VkDebugUtilsLabelEXT(const Json::Value &obj, VkDebugUtilsLabelEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkDebugUtilsLabelEXT *)parsePNextChain(obj);

    /** TODO: Handle this - pLabelName **/

    const Json::Value &obj_color_arr = obj["color"];
    for (unsigned int i = 0; i < obj_color_arr.size(); i++)
    {
        parse_float(obj_color_arr[i], const_cast<float &>((o.color[i])));
    }
}

static void parse_VkDebugUtilsObjectNameInfoEXT(const Json::Value &obj, VkDebugUtilsObjectNameInfoEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkDebugUtilsObjectNameInfoEXT *)parsePNextChain(obj);

    parse_VkObjectType(obj["objectType"], (o.objectType));

    parse_uint64_t(obj["objectHandle"], (o.objectHandle));

    /** TODO: Handle this - pObjectName **/
}

static void parse_VkDebugUtilsMessengerCallbackDataEXT(const Json::Value &obj, VkDebugUtilsMessengerCallbackDataEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkDebugUtilsMessengerCallbackDataEXT *)parsePNextChain(obj);

    parse_VkDebugUtilsMessengerCallbackDataFlagsEXT(obj["flags"], (o.flags));

    /** TODO: Handle this - pMessageIdName **/

    parse_int32_t(obj["messageIdNumber"], (o.messageIdNumber));

    /** TODO: Handle this - pMessage **/

    parse_uint32_t(obj["queueLabelCount"], (o.queueLabelCount));

    (o.pQueueLabels) = (VkDebugUtilsLabelEXT *)s_globalMem.allocate((o.queueLabelCount), sizeof(VkDebugUtilsLabelEXT));
    const Json::Value &obj_pQueueLabels = obj["pQueueLabels"];
    if (obj_pQueueLabels.size() == 0)
        (o.pQueueLabels) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.queueLabelCount); i++)
        {
            parse_VkDebugUtilsLabelEXT(obj_pQueueLabels[i], const_cast<VkDebugUtilsLabelEXT &>((o.pQueueLabels[i])));
        }
    }

    parse_uint32_t(obj["cmdBufLabelCount"], (o.cmdBufLabelCount));

    (o.pCmdBufLabels) =
        (VkDebugUtilsLabelEXT *)s_globalMem.allocate((o.cmdBufLabelCount), sizeof(VkDebugUtilsLabelEXT));
    const Json::Value &obj_pCmdBufLabels = obj["pCmdBufLabels"];
    if (obj_pCmdBufLabels.size() == 0)
        (o.pCmdBufLabels) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.cmdBufLabelCount); i++)
        {
            parse_VkDebugUtilsLabelEXT(obj_pCmdBufLabels[i], const_cast<VkDebugUtilsLabelEXT &>((o.pCmdBufLabels[i])));
        }
    }

    parse_uint32_t(obj["objectCount"], (o.objectCount));

    (o.pObjects) =
        (VkDebugUtilsObjectNameInfoEXT *)s_globalMem.allocate((o.objectCount), sizeof(VkDebugUtilsObjectNameInfoEXT));
    const Json::Value &obj_pObjects = obj["pObjects"];
    if (obj_pObjects.size() == 0)
        (o.pObjects) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.objectCount); i++)
        {
            parse_VkDebugUtilsObjectNameInfoEXT(obj_pObjects[i],
                                                const_cast<VkDebugUtilsObjectNameInfoEXT &>((o.pObjects[i])));
        }
    }
}

static void parse_VkDebugUtilsMessengerCreateInfoEXT(const Json::Value &obj, VkDebugUtilsMessengerCreateInfoEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkDebugUtilsMessengerCreateInfoEXT *)parsePNextChain(obj);

    parse_VkDebugUtilsMessengerCreateFlagsEXT(obj["flags"], (o.flags));

    parse_VkDebugUtilsMessageSeverityFlagsEXT(obj["messageSeverity"], (o.messageSeverity));

    parse_VkDebugUtilsMessageTypeFlagsEXT(obj["messageType"], (o.messageType));

    /** Note: Ignoring function pointer (PFN_vkDebugUtilsMessengerCallbackEXT). **/
}

static void parse_VkDebugUtilsObjectTagInfoEXT(const Json::Value &obj, VkDebugUtilsObjectTagInfoEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkDebugUtilsObjectTagInfoEXT *)parsePNextChain(obj);

    parse_VkObjectType(obj["objectType"], (o.objectType));

    parse_uint64_t(obj["objectHandle"], (o.objectHandle));

    parse_uint64_t(obj["tagName"], (o.tagName));

    parse_size_t(obj["tagSize"], (o.tagSize));
}

static void parse_VkSampleLocationEXT(const Json::Value &obj, VkSampleLocationEXT &o)
{

    parse_float(obj["x"], (o.x));

    parse_float(obj["y"], (o.y));
}

static void parse_VkSampleLocationsInfoEXT(const Json::Value &obj, VkSampleLocationsInfoEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkSampleLocationsInfoEXT *)parsePNextChain(obj);

    parse_VkSampleCountFlagBits(obj["sampleLocationsPerPixel"], (o.sampleLocationsPerPixel));

    parse_VkExtent2D(obj["sampleLocationGridSize"], (o.sampleLocationGridSize));

    parse_uint32_t(obj["sampleLocationsCount"], (o.sampleLocationsCount));

    (o.pSampleLocations) =
        (VkSampleLocationEXT *)s_globalMem.allocate((o.sampleLocationsCount), sizeof(VkSampleLocationEXT));
    const Json::Value &obj_pSampleLocations = obj["pSampleLocations"];
    if (obj_pSampleLocations.size() == 0)
        (o.pSampleLocations) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.sampleLocationsCount); i++)
        {
            parse_VkSampleLocationEXT(obj_pSampleLocations[i],
                                      const_cast<VkSampleLocationEXT &>((o.pSampleLocations[i])));
        }
    }
}

static void parse_VkAttachmentSampleLocationsEXT(const Json::Value &obj, VkAttachmentSampleLocationsEXT &o)
{

    parse_uint32_t(obj["attachmentIndex"], (o.attachmentIndex));

    parse_VkSampleLocationsInfoEXT(obj["sampleLocationsInfo"], (o.sampleLocationsInfo));
}

static void parse_VkSubpassSampleLocationsEXT(const Json::Value &obj, VkSubpassSampleLocationsEXT &o)
{

    parse_uint32_t(obj["subpassIndex"], (o.subpassIndex));

    parse_VkSampleLocationsInfoEXT(obj["sampleLocationsInfo"], (o.sampleLocationsInfo));
}

static void parse_VkRenderPassSampleLocationsBeginInfoEXT(const Json::Value &obj,
                                                          VkRenderPassSampleLocationsBeginInfoEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkRenderPassSampleLocationsBeginInfoEXT *)parsePNextChain(obj);

    parse_uint32_t(obj["attachmentInitialSampleLocationsCount"], (o.attachmentInitialSampleLocationsCount));

    (o.pAttachmentInitialSampleLocations) = (VkAttachmentSampleLocationsEXT *)s_globalMem.allocate(
        (o.attachmentInitialSampleLocationsCount), sizeof(VkAttachmentSampleLocationsEXT));
    const Json::Value &obj_pAttachmentInitialSampleLocations = obj["pAttachmentInitialSampleLocations"];
    if (obj_pAttachmentInitialSampleLocations.size() == 0)
        (o.pAttachmentInitialSampleLocations) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.attachmentInitialSampleLocationsCount); i++)
        {
            parse_VkAttachmentSampleLocationsEXT(
                obj_pAttachmentInitialSampleLocations[i],
                const_cast<VkAttachmentSampleLocationsEXT &>((o.pAttachmentInitialSampleLocations[i])));
        }
    }

    parse_uint32_t(obj["postSubpassSampleLocationsCount"], (o.postSubpassSampleLocationsCount));

    (o.pPostSubpassSampleLocations) = (VkSubpassSampleLocationsEXT *)s_globalMem.allocate(
        (o.postSubpassSampleLocationsCount), sizeof(VkSubpassSampleLocationsEXT));
    const Json::Value &obj_pPostSubpassSampleLocations = obj["pPostSubpassSampleLocations"];
    if (obj_pPostSubpassSampleLocations.size() == 0)
        (o.pPostSubpassSampleLocations) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.postSubpassSampleLocationsCount); i++)
        {
            parse_VkSubpassSampleLocationsEXT(
                obj_pPostSubpassSampleLocations[i],
                const_cast<VkSubpassSampleLocationsEXT &>((o.pPostSubpassSampleLocations[i])));
        }
    }
}

static void parse_VkPipelineSampleLocationsStateCreateInfoEXT(const Json::Value &obj,
                                                              VkPipelineSampleLocationsStateCreateInfoEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPipelineSampleLocationsStateCreateInfoEXT *)parsePNextChain(obj);

    parse_VkBool32(obj["sampleLocationsEnable"], (o.sampleLocationsEnable));

    parse_VkSampleLocationsInfoEXT(obj["sampleLocationsInfo"], (o.sampleLocationsInfo));
}

static void parse_VkPhysicalDeviceSampleLocationsPropertiesEXT(const Json::Value &obj,
                                                               VkPhysicalDeviceSampleLocationsPropertiesEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceSampleLocationsPropertiesEXT *)parsePNextChain(obj);

    parse_VkSampleCountFlags(obj["sampleLocationSampleCounts"], (o.sampleLocationSampleCounts));

    parse_VkExtent2D(obj["maxSampleLocationGridSize"], (o.maxSampleLocationGridSize));

    const Json::Value &obj_sampleLocationCoordinateRange_arr = obj["sampleLocationCoordinateRange"];
    for (unsigned int i = 0; i < obj_sampleLocationCoordinateRange_arr.size(); i++)
    {
        parse_float(obj_sampleLocationCoordinateRange_arr[i],
                    const_cast<float &>((o.sampleLocationCoordinateRange[i])));
    }

    parse_uint32_t(obj["sampleLocationSubPixelBits"], (o.sampleLocationSubPixelBits));

    parse_VkBool32(obj["variableSampleLocations"], (o.variableSampleLocations));
}

static void parse_VkMultisamplePropertiesEXT(const Json::Value &obj, VkMultisamplePropertiesEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkMultisamplePropertiesEXT *)parsePNextChain(obj);

    parse_VkExtent2D(obj["maxSampleLocationGridSize"], (o.maxSampleLocationGridSize));
}

static std::map<std::string, int> VkBlendOverlapEXT_map = {
    std::make_pair("VK_BLEND_OVERLAP_UNCORRELATED_EXT", 0),
    std::make_pair("VK_BLEND_OVERLAP_DISJOINT_EXT", 1),
    std::make_pair("VK_BLEND_OVERLAP_CONJOINT_EXT", 2),
};
static void parse_VkBlendOverlapEXT(const Json::Value &obj, VkBlendOverlapEXT &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkBlendOverlapEXT)VkBlendOverlapEXT_map[std::string(_res)];
}

static void parse_VkPhysicalDeviceBlendOperationAdvancedFeaturesEXT(
    const Json::Value &obj, VkPhysicalDeviceBlendOperationAdvancedFeaturesEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceBlendOperationAdvancedFeaturesEXT *)parsePNextChain(obj);

    parse_VkBool32(obj["advancedBlendCoherentOperations"], (o.advancedBlendCoherentOperations));
}

static void parse_VkPhysicalDeviceBlendOperationAdvancedPropertiesEXT(
    const Json::Value &obj, VkPhysicalDeviceBlendOperationAdvancedPropertiesEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceBlendOperationAdvancedPropertiesEXT *)parsePNextChain(obj);

    parse_uint32_t(obj["advancedBlendMaxColorAttachments"], (o.advancedBlendMaxColorAttachments));

    parse_VkBool32(obj["advancedBlendIndependentBlend"], (o.advancedBlendIndependentBlend));

    parse_VkBool32(obj["advancedBlendNonPremultipliedSrcColor"], (o.advancedBlendNonPremultipliedSrcColor));

    parse_VkBool32(obj["advancedBlendNonPremultipliedDstColor"], (o.advancedBlendNonPremultipliedDstColor));

    parse_VkBool32(obj["advancedBlendCorrelatedOverlap"], (o.advancedBlendCorrelatedOverlap));

    parse_VkBool32(obj["advancedBlendAllOperations"], (o.advancedBlendAllOperations));
}

static void parse_VkPipelineColorBlendAdvancedStateCreateInfoEXT(const Json::Value &obj,
                                                                 VkPipelineColorBlendAdvancedStateCreateInfoEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPipelineColorBlendAdvancedStateCreateInfoEXT *)parsePNextChain(obj);

    parse_VkBool32(obj["srcPremultiplied"], (o.srcPremultiplied));

    parse_VkBool32(obj["dstPremultiplied"], (o.dstPremultiplied));

    parse_VkBlendOverlapEXT(obj["blendOverlap"], (o.blendOverlap));
}

static std::map<std::string, uint64_t> VkFormatFeatureFlagBits2_map = {
    std::make_pair("VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_BIT", 1ULL << 0),
    std::make_pair("VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_BIT_KHR", 1ULL << 0),
    std::make_pair("VK_FORMAT_FEATURE_2_STORAGE_IMAGE_BIT", 1ULL << 1),
    std::make_pair("VK_FORMAT_FEATURE_2_STORAGE_IMAGE_BIT_KHR", 1ULL << 1),
    std::make_pair("VK_FORMAT_FEATURE_2_STORAGE_IMAGE_ATOMIC_BIT", 1ULL << 2),
    std::make_pair("VK_FORMAT_FEATURE_2_STORAGE_IMAGE_ATOMIC_BIT_KHR", 1ULL << 2),
    std::make_pair("VK_FORMAT_FEATURE_2_UNIFORM_TEXEL_BUFFER_BIT", 1ULL << 3),
    std::make_pair("VK_FORMAT_FEATURE_2_UNIFORM_TEXEL_BUFFER_BIT_KHR", 1ULL << 3),
    std::make_pair("VK_FORMAT_FEATURE_2_STORAGE_TEXEL_BUFFER_BIT", 1ULL << 4),
    std::make_pair("VK_FORMAT_FEATURE_2_STORAGE_TEXEL_BUFFER_BIT_KHR", 1ULL << 4),
    std::make_pair("VK_FORMAT_FEATURE_2_STORAGE_TEXEL_BUFFER_ATOMIC_BIT", 1ULL << 5),
    std::make_pair("VK_FORMAT_FEATURE_2_STORAGE_TEXEL_BUFFER_ATOMIC_BIT_KHR", 1ULL << 5),
    std::make_pair("VK_FORMAT_FEATURE_2_VERTEX_BUFFER_BIT", 1ULL << 6),
    std::make_pair("VK_FORMAT_FEATURE_2_VERTEX_BUFFER_BIT_KHR", 1ULL << 6),
    std::make_pair("VK_FORMAT_FEATURE_2_COLOR_ATTACHMENT_BIT", 1ULL << 7),
    std::make_pair("VK_FORMAT_FEATURE_2_COLOR_ATTACHMENT_BIT_KHR", 1ULL << 7),
    std::make_pair("VK_FORMAT_FEATURE_2_COLOR_ATTACHMENT_BLEND_BIT", 1ULL << 8),
    std::make_pair("VK_FORMAT_FEATURE_2_COLOR_ATTACHMENT_BLEND_BIT_KHR", 1ULL << 8),
    std::make_pair("VK_FORMAT_FEATURE_2_DEPTH_STENCIL_ATTACHMENT_BIT", 1ULL << 9),
    std::make_pair("VK_FORMAT_FEATURE_2_DEPTH_STENCIL_ATTACHMENT_BIT_KHR", 1ULL << 9),
    std::make_pair("VK_FORMAT_FEATURE_2_BLIT_SRC_BIT", 1ULL << 10),
    std::make_pair("VK_FORMAT_FEATURE_2_BLIT_SRC_BIT_KHR", 1ULL << 10),
    std::make_pair("VK_FORMAT_FEATURE_2_BLIT_DST_BIT", 1ULL << 11),
    std::make_pair("VK_FORMAT_FEATURE_2_BLIT_DST_BIT_KHR", 1ULL << 11),
    std::make_pair("VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_FILTER_LINEAR_BIT", 1ULL << 12),
    std::make_pair("VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_FILTER_LINEAR_BIT_KHR", 1ULL << 12),
    std::make_pair("VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_FILTER_CUBIC_BIT", 1ULL << 13),
    std::make_pair("VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_FILTER_CUBIC_BIT_EXT", 1ULL << 13),
    std::make_pair("VK_FORMAT_FEATURE_2_TRANSFER_SRC_BIT", 1ULL << 14),
    std::make_pair("VK_FORMAT_FEATURE_2_TRANSFER_SRC_BIT_KHR", 1ULL << 14),
    std::make_pair("VK_FORMAT_FEATURE_2_TRANSFER_DST_BIT", 1ULL << 15),
    std::make_pair("VK_FORMAT_FEATURE_2_TRANSFER_DST_BIT_KHR", 1ULL << 15),
    std::make_pair("VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_FILTER_MINMAX_BIT", 1ULL << 16),
    std::make_pair("VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_FILTER_MINMAX_BIT_KHR", 1ULL << 16),
    std::make_pair("VK_FORMAT_FEATURE_2_MIDPOINT_CHROMA_SAMPLES_BIT", 1ULL << 17),
    std::make_pair("VK_FORMAT_FEATURE_2_MIDPOINT_CHROMA_SAMPLES_BIT_KHR", 1ULL << 17),
    std::make_pair("VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER_BIT", 1ULL << 18),
    std::make_pair("VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER_BIT_KHR", 1ULL << 18),
    std::make_pair("VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_YCBCR_CONVERSION_SEPARATE_RECONSTRUCTION_FILTER_BIT", 1ULL << 19),
    std::make_pair("VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_YCBCR_CONVERSION_SEPARATE_RECONSTRUCTION_FILTER_BIT_KHR",
                   1ULL << 19),
    std::make_pair("VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_BIT", 1ULL << 20),
    std::make_pair("VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_BIT_KHR",
                   1ULL << 20),
    std::make_pair("VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_FORCEABLE_BIT",
                   1ULL << 21),
    std::make_pair(
        "VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_YCBCR_CONVERSION_CHROMA_RECONSTRUCTION_EXPLICIT_FORCEABLE_BIT_KHR",
        1ULL << 21),
    std::make_pair("VK_FORMAT_FEATURE_2_DISJOINT_BIT", 1ULL << 22),
    std::make_pair("VK_FORMAT_FEATURE_2_DISJOINT_BIT_KHR", 1ULL << 22),
    std::make_pair("VK_FORMAT_FEATURE_2_COSITED_CHROMA_SAMPLES_BIT", 1ULL << 23),
    std::make_pair("VK_FORMAT_FEATURE_2_COSITED_CHROMA_SAMPLES_BIT_KHR", 1ULL << 23),
    std::make_pair("VK_FORMAT_FEATURE_2_STORAGE_READ_WITHOUT_FORMAT_BIT", 1ULL << 31),
    std::make_pair("VK_FORMAT_FEATURE_2_STORAGE_READ_WITHOUT_FORMAT_BIT_KHR", 1ULL << 31),
    std::make_pair("VK_FORMAT_FEATURE_2_STORAGE_WRITE_WITHOUT_FORMAT_BIT", 1ULL << 32),
    std::make_pair("VK_FORMAT_FEATURE_2_STORAGE_WRITE_WITHOUT_FORMAT_BIT_KHR", 1ULL << 32),
    std::make_pair("VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_DEPTH_COMPARISON_BIT", 1ULL << 33),
    std::make_pair("VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_DEPTH_COMPARISON_BIT_KHR", 1ULL << 33),
    std::make_pair("VK_FORMAT_FEATURE_2_VIDEO_DECODE_OUTPUT_BIT_KHR", 1ULL << 25),
    std::make_pair("VK_FORMAT_FEATURE_2_VIDEO_DECODE_DPB_BIT_KHR", 1ULL << 26),
    std::make_pair("VK_FORMAT_FEATURE_2_ACCELERATION_STRUCTURE_VERTEX_BUFFER_BIT_KHR", 1ULL << 29),
    std::make_pair("VK_FORMAT_FEATURE_2_FRAGMENT_DENSITY_MAP_BIT_EXT", 1ULL << 24),
    std::make_pair("VK_FORMAT_FEATURE_2_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR", 1ULL << 30),
    std::make_pair("VK_FORMAT_FEATURE_2_RESERVED_44_BIT_EXT", 1ULL << 44),
    std::make_pair("VK_FORMAT_FEATURE_2_RESERVED_45_BIT_EXT", 1ULL << 45),
    std::make_pair("VK_FORMAT_FEATURE_2_HOST_IMAGE_TRANSFER_BIT_EXT", 1ULL << 46),
    std::make_pair("VK_FORMAT_FEATURE_2_VIDEO_ENCODE_INPUT_BIT_KHR", 1ULL << 27),
    std::make_pair("VK_FORMAT_FEATURE_2_VIDEO_ENCODE_DPB_BIT_KHR", 1ULL << 28),
    std::make_pair("VK_FORMAT_FEATURE_2_RESERVED_51_BIT_EXT", 1ULL << 51),
    std::make_pair("VK_FORMAT_FEATURE_2_LINEAR_COLOR_ATTACHMENT_BIT_NV", 1ULL << 38),
    std::make_pair("VK_FORMAT_FEATURE_2_WEIGHT_IMAGE_BIT_QCOM", 1ULL << 34),
    std::make_pair("VK_FORMAT_FEATURE_2_WEIGHT_SAMPLED_IMAGE_BIT_QCOM", 1ULL << 35),
    std::make_pair("VK_FORMAT_FEATURE_2_BLOCK_MATCHING_BIT_QCOM", 1ULL << 36),
    std::make_pair("VK_FORMAT_FEATURE_2_BOX_FILTER_SAMPLED_BIT_QCOM", 1ULL << 37),
    std::make_pair("VK_FORMAT_FEATURE_2_RESERVED_47_BIT_ARM", 1ULL << 47),
    std::make_pair("VK_FORMAT_FEATURE_2_RESERVED_39_BIT_EXT", 1ULL << 39),
    std::make_pair("VK_FORMAT_FEATURE_2_OPTICAL_FLOW_IMAGE_BIT_NV", 1ULL << 40),
    std::make_pair("VK_FORMAT_FEATURE_2_OPTICAL_FLOW_VECTOR_BIT_NV", 1ULL << 41),
    std::make_pair("VK_FORMAT_FEATURE_2_OPTICAL_FLOW_COST_BIT_NV", 1ULL << 42),
    std::make_pair("VK_FORMAT_FEATURE_2_RESERVED_48_BIT_EXT", 1ULL << 48),
    std::make_pair("VK_FORMAT_FEATURE_2_RESERVED_49_BIT_KHR", 1ULL << 49),
    std::make_pair("VK_FORMAT_FEATURE_2_RESERVED_50_BIT_KHR", 1ULL << 50),
};
static void parse_VkFormatFeatureFlagBits2(const Json::Value &obj, VkFormatFeatureFlagBits2 &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkFormatFeatureFlagBits2)VkFormatFeatureFlagBits2_map[std::string(_res)];
}

static void parse_VkFormatFeatureFlags2(const Json::Value &obj, VkFormatFeatureFlags2 &o)
{
    if (obj.isString())
    {
        const std::string &_res = obj.asString();
        sscanf(_res.c_str(), "%" SCNd64, &o);
    }
    else
    {
        o = obj.asUInt();
    }
}

static void parse_VkDrmFormatModifierPropertiesEXT(const Json::Value &obj, VkDrmFormatModifierPropertiesEXT &o)
{

    parse_uint64_t(obj["drmFormatModifier"], (o.drmFormatModifier));

    parse_uint32_t(obj["drmFormatModifierPlaneCount"], (o.drmFormatModifierPlaneCount));

    parse_VkFormatFeatureFlags(obj["drmFormatModifierTilingFeatures"], (o.drmFormatModifierTilingFeatures));
}

static void parse_VkDrmFormatModifierPropertiesListEXT(const Json::Value &obj, VkDrmFormatModifierPropertiesListEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkDrmFormatModifierPropertiesListEXT *)parsePNextChain(obj);

    parse_uint32_t(obj["drmFormatModifierCount"], (o.drmFormatModifierCount));

    (o.pDrmFormatModifierProperties) = (VkDrmFormatModifierPropertiesEXT *)s_globalMem.allocate(
        (o.drmFormatModifierCount), sizeof(VkDrmFormatModifierPropertiesEXT));
    const Json::Value &obj_pDrmFormatModifierProperties = obj["pDrmFormatModifierProperties"];
    if (obj_pDrmFormatModifierProperties.size() == 0)
        (o.pDrmFormatModifierProperties) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.drmFormatModifierCount); i++)
        {
            parse_VkDrmFormatModifierPropertiesEXT(
                obj_pDrmFormatModifierProperties[i],
                const_cast<VkDrmFormatModifierPropertiesEXT &>((o.pDrmFormatModifierProperties[i])));
        }
    }
}

static void parse_VkPhysicalDeviceImageDrmFormatModifierInfoEXT(const Json::Value &obj,
                                                                VkPhysicalDeviceImageDrmFormatModifierInfoEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceImageDrmFormatModifierInfoEXT *)parsePNextChain(obj);

    parse_uint64_t(obj["drmFormatModifier"], (o.drmFormatModifier));

    parse_VkSharingMode(obj["sharingMode"], (o.sharingMode));

    parse_uint32_t(obj["queueFamilyIndexCount"], (o.queueFamilyIndexCount));

    (o.pQueueFamilyIndices) = (uint32_t *)s_globalMem.allocate((o.queueFamilyIndexCount), sizeof(uint32_t));
    const Json::Value &obj_pQueueFamilyIndices_arr = obj["pQueueFamilyIndices"];
    for (unsigned int i = 0; i < obj_pQueueFamilyIndices_arr.size(); i++)
    {
        parse_uint32_t(obj_pQueueFamilyIndices_arr[i], const_cast<uint32_t &>((o.pQueueFamilyIndices[i])));
    }
}

static void parse_VkImageDrmFormatModifierListCreateInfoEXT(const Json::Value &obj,
                                                            VkImageDrmFormatModifierListCreateInfoEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkImageDrmFormatModifierListCreateInfoEXT *)parsePNextChain(obj);

    parse_uint32_t(obj["drmFormatModifierCount"], (o.drmFormatModifierCount));

    (o.pDrmFormatModifiers) = (uint64_t *)s_globalMem.allocate((o.drmFormatModifierCount), sizeof(uint64_t));
    const Json::Value &obj_pDrmFormatModifiers_arr = obj["pDrmFormatModifiers"];
    for (unsigned int i = 0; i < obj_pDrmFormatModifiers_arr.size(); i++)
    {
        parse_uint64_t(obj_pDrmFormatModifiers_arr[i], const_cast<uint64_t &>((o.pDrmFormatModifiers[i])));
    }
}

static void parse_VkImageDrmFormatModifierExplicitCreateInfoEXT(const Json::Value &obj,
                                                                VkImageDrmFormatModifierExplicitCreateInfoEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkImageDrmFormatModifierExplicitCreateInfoEXT *)parsePNextChain(obj);

    parse_uint64_t(obj["drmFormatModifier"], (o.drmFormatModifier));

    parse_uint32_t(obj["drmFormatModifierPlaneCount"], (o.drmFormatModifierPlaneCount));

    (o.pPlaneLayouts) =
        (VkSubresourceLayout *)s_globalMem.allocate((o.drmFormatModifierPlaneCount), sizeof(VkSubresourceLayout));
    const Json::Value &obj_pPlaneLayouts = obj["pPlaneLayouts"];
    if (obj_pPlaneLayouts.size() == 0)
        (o.pPlaneLayouts) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.drmFormatModifierPlaneCount); i++)
        {
            parse_VkSubresourceLayout(obj_pPlaneLayouts[i], const_cast<VkSubresourceLayout &>((o.pPlaneLayouts[i])));
        }
    }
}

static void parse_VkImageDrmFormatModifierPropertiesEXT(const Json::Value &obj,
                                                        VkImageDrmFormatModifierPropertiesEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkImageDrmFormatModifierPropertiesEXT *)parsePNextChain(obj);

    parse_uint64_t(obj["drmFormatModifier"], (o.drmFormatModifier));
}

static void parse_VkDrmFormatModifierProperties2EXT(const Json::Value &obj, VkDrmFormatModifierProperties2EXT &o)
{

    parse_uint64_t(obj["drmFormatModifier"], (o.drmFormatModifier));

    parse_uint32_t(obj["drmFormatModifierPlaneCount"], (o.drmFormatModifierPlaneCount));

    parse_VkFormatFeatureFlags2(obj["drmFormatModifierTilingFeatures"], (o.drmFormatModifierTilingFeatures));
}

static void parse_VkDrmFormatModifierPropertiesList2EXT(const Json::Value &obj,
                                                        VkDrmFormatModifierPropertiesList2EXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkDrmFormatModifierPropertiesList2EXT *)parsePNextChain(obj);

    parse_uint32_t(obj["drmFormatModifierCount"], (o.drmFormatModifierCount));

    (o.pDrmFormatModifierProperties) = (VkDrmFormatModifierProperties2EXT *)s_globalMem.allocate(
        (o.drmFormatModifierCount), sizeof(VkDrmFormatModifierProperties2EXT));
    const Json::Value &obj_pDrmFormatModifierProperties = obj["pDrmFormatModifierProperties"];
    if (obj_pDrmFormatModifierProperties.size() == 0)
        (o.pDrmFormatModifierProperties) = nullptr;
    else
    {
        for (unsigned int i = 0; i < (o.drmFormatModifierCount); i++)
        {
            parse_VkDrmFormatModifierProperties2EXT(
                obj_pDrmFormatModifierProperties[i],
                const_cast<VkDrmFormatModifierProperties2EXT &>((o.pDrmFormatModifierProperties[i])));
        }
    }
}

static void parse_VkPhysicalDeviceImageViewImageFormatInfoEXT(const Json::Value &obj,
                                                              VkPhysicalDeviceImageViewImageFormatInfoEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceImageViewImageFormatInfoEXT *)parsePNextChain(obj);

    parse_VkImageViewType(obj["imageViewType"], (o.imageViewType));
}

static void parse_VkFilterCubicImageViewImageFormatPropertiesEXT(const Json::Value &obj,
                                                                 VkFilterCubicImageViewImageFormatPropertiesEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkFilterCubicImageViewImageFormatPropertiesEXT *)parsePNextChain(obj);

    parse_VkBool32(obj["filterCubic"], (o.filterCubic));

    parse_VkBool32(obj["filterCubicMinmax"], (o.filterCubicMinmax));
}

static void parse_VkImportMemoryHostPointerInfoEXT(const Json::Value &obj, VkImportMemoryHostPointerInfoEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkImportMemoryHostPointerInfoEXT *)parsePNextChain(obj);

    parse_VkExternalMemoryHandleTypeFlagBits(obj["handleType"], (o.handleType));
}

static void parse_VkMemoryHostPointerPropertiesEXT(const Json::Value &obj, VkMemoryHostPointerPropertiesEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkMemoryHostPointerPropertiesEXT *)parsePNextChain(obj);

    parse_uint32_t(obj["memoryTypeBits"], (o.memoryTypeBits));
}

static void parse_VkPhysicalDeviceExternalMemoryHostPropertiesEXT(const Json::Value &obj,
                                                                  VkPhysicalDeviceExternalMemoryHostPropertiesEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceExternalMemoryHostPropertiesEXT *)parsePNextChain(obj);

    parse_VkDeviceSize(obj["minImportedHostPointerAlignment"], (o.minImportedHostPointerAlignment));
}

static void parse_VkPhysicalDevicePCIBusInfoPropertiesEXT(const Json::Value &obj,
                                                          VkPhysicalDevicePCIBusInfoPropertiesEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDevicePCIBusInfoPropertiesEXT *)parsePNextChain(obj);

    parse_uint32_t(obj["pciDomain"], (o.pciDomain));

    parse_uint32_t(obj["pciBus"], (o.pciBus));

    parse_uint32_t(obj["pciDevice"], (o.pciDevice));

    parse_uint32_t(obj["pciFunction"], (o.pciFunction));
}

static void parse_VkPhysicalDeviceSubgroupSizeControlFeatures(const Json::Value &obj,
                                                              VkPhysicalDeviceSubgroupSizeControlFeatures &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceSubgroupSizeControlFeatures *)parsePNextChain(obj);

    parse_VkBool32(obj["subgroupSizeControl"], (o.subgroupSizeControl));

    parse_VkBool32(obj["computeFullSubgroups"], (o.computeFullSubgroups));
}

static void parse_VkPhysicalDeviceSubgroupSizeControlProperties(const Json::Value &obj,
                                                                VkPhysicalDeviceSubgroupSizeControlProperties &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceSubgroupSizeControlProperties *)parsePNextChain(obj);

    parse_uint32_t(obj["minSubgroupSize"], (o.minSubgroupSize));

    parse_uint32_t(obj["maxSubgroupSize"], (o.maxSubgroupSize));

    parse_uint32_t(obj["maxComputeWorkgroupSubgroups"], (o.maxComputeWorkgroupSubgroups));

    parse_VkShaderStageFlags(obj["requiredSubgroupSizeStages"], (o.requiredSubgroupSizeStages));
}

static void parse_VkPipelineShaderStageRequiredSubgroupSizeCreateInfo(
    const Json::Value &obj, VkPipelineShaderStageRequiredSubgroupSizeCreateInfo &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPipelineShaderStageRequiredSubgroupSizeCreateInfo *)parsePNextChain(obj);

    parse_uint32_t(obj["requiredSubgroupSize"], (o.requiredSubgroupSize));
}

static void parse_VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT(
    const Json::Value &obj, VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT *)parsePNextChain(obj);

    parse_VkBool32(obj["shaderImageInt64Atomics"], (o.shaderImageInt64Atomics));

    parse_VkBool32(obj["sparseImageInt64Atomics"], (o.sparseImageInt64Atomics));
}

static void parse_VkPhysicalDeviceMemoryBudgetPropertiesEXT(const Json::Value &obj,
                                                            VkPhysicalDeviceMemoryBudgetPropertiesEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceMemoryBudgetPropertiesEXT *)parsePNextChain(obj);

    const Json::Value &obj_heapBudget_arr = obj["heapBudget"];
    for (unsigned int i = 0; i < obj_heapBudget_arr.size(); i++)
    {
        parse_VkDeviceSize(obj_heapBudget_arr[i], const_cast<VkDeviceSize &>((o.heapBudget[i])));
    }

    const Json::Value &obj_heapUsage_arr = obj["heapUsage"];
    for (unsigned int i = 0; i < obj_heapUsage_arr.size(); i++)
    {
        parse_VkDeviceSize(obj_heapUsage_arr[i], const_cast<VkDeviceSize &>((o.heapUsage[i])));
    }
}

static std::map<std::string, int> VkValidationFeatureEnableEXT_map = {
    std::make_pair("VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT", 0),
    std::make_pair("VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT", 1),
    std::make_pair("VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT", 2),
    std::make_pair("VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT", 3),
    std::make_pair("VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT", 4),
};
static void parse_VkValidationFeatureEnableEXT(const Json::Value &obj, VkValidationFeatureEnableEXT &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkValidationFeatureEnableEXT)VkValidationFeatureEnableEXT_map[std::string(_res)];
}

static std::map<std::string, int> VkValidationFeatureDisableEXT_map = {
    std::make_pair("VK_VALIDATION_FEATURE_DISABLE_ALL_EXT", 0),
    std::make_pair("VK_VALIDATION_FEATURE_DISABLE_SHADERS_EXT", 1),
    std::make_pair("VK_VALIDATION_FEATURE_DISABLE_THREAD_SAFETY_EXT", 2),
    std::make_pair("VK_VALIDATION_FEATURE_DISABLE_API_PARAMETERS_EXT", 3),
    std::make_pair("VK_VALIDATION_FEATURE_DISABLE_OBJECT_LIFETIMES_EXT", 4),
    std::make_pair("VK_VALIDATION_FEATURE_DISABLE_CORE_CHECKS_EXT", 5),
    std::make_pair("VK_VALIDATION_FEATURE_DISABLE_UNIQUE_HANDLES_EXT", 6),
    std::make_pair("VK_VALIDATION_FEATURE_DISABLE_SHADER_VALIDATION_CACHE_EXT", 7),
};
static void parse_VkValidationFeatureDisableEXT(const Json::Value &obj, VkValidationFeatureDisableEXT &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkValidationFeatureDisableEXT)VkValidationFeatureDisableEXT_map[std::string(_res)];
}

static void parse_VkValidationFeaturesEXT(const Json::Value &obj, VkValidationFeaturesEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkValidationFeaturesEXT *)parsePNextChain(obj);

    parse_uint32_t(obj["enabledValidationFeatureCount"], (o.enabledValidationFeatureCount));

    (o.pEnabledValidationFeatures) = (VkValidationFeatureEnableEXT *)s_globalMem.allocate(
        (o.enabledValidationFeatureCount), sizeof(VkValidationFeatureEnableEXT));
    const Json::Value &obj_pEnabledValidationFeatures_arr = obj["pEnabledValidationFeatures"];
    for (unsigned int i = 0; i < obj_pEnabledValidationFeatures_arr.size(); i++)
    {
        parse_VkValidationFeatureEnableEXT(
            obj_pEnabledValidationFeatures_arr[i],
            const_cast<VkValidationFeatureEnableEXT &>((o.pEnabledValidationFeatures[i])));
    }

    parse_uint32_t(obj["disabledValidationFeatureCount"], (o.disabledValidationFeatureCount));

    (o.pDisabledValidationFeatures) = (VkValidationFeatureDisableEXT *)s_globalMem.allocate(
        (o.disabledValidationFeatureCount), sizeof(VkValidationFeatureDisableEXT));
    const Json::Value &obj_pDisabledValidationFeatures_arr = obj["pDisabledValidationFeatures"];
    for (unsigned int i = 0; i < obj_pDisabledValidationFeatures_arr.size(); i++)
    {
        parse_VkValidationFeatureDisableEXT(
            obj_pDisabledValidationFeatures_arr[i],
            const_cast<VkValidationFeatureDisableEXT &>((o.pDisabledValidationFeatures[i])));
    }
}

static void parse_VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT(
    const Json::Value &obj, VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT *)parsePNextChain(obj);

    parse_VkBool32(obj["fragmentShaderSampleInterlock"], (o.fragmentShaderSampleInterlock));

    parse_VkBool32(obj["fragmentShaderPixelInterlock"], (o.fragmentShaderPixelInterlock));

    parse_VkBool32(obj["fragmentShaderShadingRateInterlock"], (o.fragmentShaderShadingRateInterlock));
}

static void parse_VkPhysicalDeviceYcbcrImageArraysFeaturesEXT(const Json::Value &obj,
                                                              VkPhysicalDeviceYcbcrImageArraysFeaturesEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceYcbcrImageArraysFeaturesEXT *)parsePNextChain(obj);

    parse_VkBool32(obj["ycbcrImageArrays"], (o.ycbcrImageArrays));
}

static void parse_VkHeadlessSurfaceCreateFlagsEXT(const Json::Value &obj, VkHeadlessSurfaceCreateFlagsEXT &o)
{
    if (obj.isString())
    {
        const std::string &_res = obj.asString();
        sscanf(_res.c_str(), "%u", &o);
    }
    else
    {
        o = obj.asUInt();
    }
}

static void parse_VkHeadlessSurfaceCreateInfoEXT(const Json::Value &obj, VkHeadlessSurfaceCreateInfoEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkHeadlessSurfaceCreateInfoEXT *)parsePNextChain(obj);

    parse_VkHeadlessSurfaceCreateFlagsEXT(obj["flags"], (o.flags));
}

static std::map<std::string, int> VkLineRasterizationModeEXT_map = {
    std::make_pair("VK_LINE_RASTERIZATION_MODE_DEFAULT_KHR", 0),
    std::make_pair("VK_LINE_RASTERIZATION_MODE_RECTANGULAR_KHR", 1),
    std::make_pair("VK_LINE_RASTERIZATION_MODE_BRESENHAM_KHR", 2),
    std::make_pair("VK_LINE_RASTERIZATION_MODE_RECTANGULAR_SMOOTH_KHR", 3),
};
static void parse_VkLineRasterizationModeEXT(const Json::Value &obj, VkLineRasterizationModeEXT &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkLineRasterizationModeEXT)VkLineRasterizationModeEXT_map[std::string(_res)];
}

static void parse_VkPhysicalDeviceShaderAtomicFloatFeaturesEXT(const Json::Value &obj,
                                                               VkPhysicalDeviceShaderAtomicFloatFeaturesEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceShaderAtomicFloatFeaturesEXT *)parsePNextChain(obj);

    parse_VkBool32(obj["shaderBufferFloat32Atomics"], (o.shaderBufferFloat32Atomics));

    parse_VkBool32(obj["shaderBufferFloat32AtomicAdd"], (o.shaderBufferFloat32AtomicAdd));

    parse_VkBool32(obj["shaderBufferFloat64Atomics"], (o.shaderBufferFloat64Atomics));

    parse_VkBool32(obj["shaderBufferFloat64AtomicAdd"], (o.shaderBufferFloat64AtomicAdd));

    parse_VkBool32(obj["shaderSharedFloat32Atomics"], (o.shaderSharedFloat32Atomics));

    parse_VkBool32(obj["shaderSharedFloat32AtomicAdd"], (o.shaderSharedFloat32AtomicAdd));

    parse_VkBool32(obj["shaderSharedFloat64Atomics"], (o.shaderSharedFloat64Atomics));

    parse_VkBool32(obj["shaderSharedFloat64AtomicAdd"], (o.shaderSharedFloat64AtomicAdd));

    parse_VkBool32(obj["shaderImageFloat32Atomics"], (o.shaderImageFloat32Atomics));

    parse_VkBool32(obj["shaderImageFloat32AtomicAdd"], (o.shaderImageFloat32AtomicAdd));

    parse_VkBool32(obj["sparseImageFloat32Atomics"], (o.sparseImageFloat32Atomics));

    parse_VkBool32(obj["sparseImageFloat32AtomicAdd"], (o.sparseImageFloat32AtomicAdd));
}

static void parse_VkPhysicalDeviceExtendedDynamicStateFeaturesEXT(const Json::Value &obj,
                                                                  VkPhysicalDeviceExtendedDynamicStateFeaturesEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceExtendedDynamicStateFeaturesEXT *)parsePNextChain(obj);

    parse_VkBool32(obj["extendedDynamicState"], (o.extendedDynamicState));
}

static void parse_VkPhysicalDeviceShaderDemoteToHelperInvocationFeatures(
    const Json::Value &obj, VkPhysicalDeviceShaderDemoteToHelperInvocationFeatures &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceShaderDemoteToHelperInvocationFeatures *)parsePNextChain(obj);

    parse_VkBool32(obj["shaderDemoteToHelperInvocation"], (o.shaderDemoteToHelperInvocation));
}

static void parse_VkPhysicalDeviceTexelBufferAlignmentFeaturesEXT(const Json::Value &obj,
                                                                  VkPhysicalDeviceTexelBufferAlignmentFeaturesEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceTexelBufferAlignmentFeaturesEXT *)parsePNextChain(obj);

    parse_VkBool32(obj["texelBufferAlignment"], (o.texelBufferAlignment));
}

static void parse_VkPhysicalDeviceTexelBufferAlignmentProperties(const Json::Value &obj,
                                                                 VkPhysicalDeviceTexelBufferAlignmentProperties &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceTexelBufferAlignmentProperties *)parsePNextChain(obj);

    parse_VkDeviceSize(obj["storageTexelBufferOffsetAlignmentBytes"], (o.storageTexelBufferOffsetAlignmentBytes));

    parse_VkBool32(obj["storageTexelBufferOffsetSingleTexelAlignment"],
                   (o.storageTexelBufferOffsetSingleTexelAlignment));

    parse_VkDeviceSize(obj["uniformTexelBufferOffsetAlignmentBytes"], (o.uniformTexelBufferOffsetAlignmentBytes));

    parse_VkBool32(obj["uniformTexelBufferOffsetSingleTexelAlignment"],
                   (o.uniformTexelBufferOffsetSingleTexelAlignment));
}

static void parse_VkPhysicalDeviceRobustness2FeaturesEXT(const Json::Value &obj,
                                                         VkPhysicalDeviceRobustness2FeaturesEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceRobustness2FeaturesEXT *)parsePNextChain(obj);

    parse_VkBool32(obj["robustBufferAccess2"], (o.robustBufferAccess2));

    parse_VkBool32(obj["robustImageAccess2"], (o.robustImageAccess2));

    parse_VkBool32(obj["nullDescriptor"], (o.nullDescriptor));
}

static void parse_VkPhysicalDeviceRobustness2PropertiesEXT(const Json::Value &obj,
                                                           VkPhysicalDeviceRobustness2PropertiesEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceRobustness2PropertiesEXT *)parsePNextChain(obj);

    parse_VkDeviceSize(obj["robustStorageBufferAccessSizeAlignment"], (o.robustStorageBufferAccessSizeAlignment));

    parse_VkDeviceSize(obj["robustUniformBufferAccessSizeAlignment"], (o.robustUniformBufferAccessSizeAlignment));
}

static void parse_VkSamplerCustomBorderColorCreateInfoEXT(const Json::Value &obj,
                                                          VkSamplerCustomBorderColorCreateInfoEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkSamplerCustomBorderColorCreateInfoEXT *)parsePNextChain(obj);

    parse_VkClearColorValue(obj["customBorderColor"], (o.customBorderColor));

    parse_VkFormat(obj["format"], (o.format));
}

static void parse_VkPhysicalDeviceCustomBorderColorPropertiesEXT(const Json::Value &obj,
                                                                 VkPhysicalDeviceCustomBorderColorPropertiesEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceCustomBorderColorPropertiesEXT *)parsePNextChain(obj);

    parse_uint32_t(obj["maxCustomBorderColorSamplers"], (o.maxCustomBorderColorSamplers));
}

static void parse_VkPhysicalDeviceCustomBorderColorFeaturesEXT(const Json::Value &obj,
                                                               VkPhysicalDeviceCustomBorderColorFeaturesEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceCustomBorderColorFeaturesEXT *)parsePNextChain(obj);

    parse_VkBool32(obj["customBorderColors"], (o.customBorderColors));

    parse_VkBool32(obj["customBorderColorWithoutFormat"], (o.customBorderColorWithoutFormat));
}

static void parse_VkPhysicalDeviceYcbcr2Plane444FormatsFeaturesEXT(const Json::Value &obj,
                                                                   VkPhysicalDeviceYcbcr2Plane444FormatsFeaturesEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceYcbcr2Plane444FormatsFeaturesEXT *)parsePNextChain(obj);

    parse_VkBool32(obj["ycbcr2plane444Formats"], (o.ycbcr2plane444Formats));
}

static void parse_VkPhysicalDeviceImageRobustnessFeatures(const Json::Value &obj,
                                                          VkPhysicalDeviceImageRobustnessFeatures &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceImageRobustnessFeatures *)parsePNextChain(obj);

    parse_VkBool32(obj["robustImageAccess"], (o.robustImageAccess));
}

static void parse_VkPhysicalDevice4444FormatsFeaturesEXT(const Json::Value &obj,
                                                         VkPhysicalDevice4444FormatsFeaturesEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDevice4444FormatsFeaturesEXT *)parsePNextChain(obj);

    parse_VkBool32(obj["formatA4R4G4B4"], (o.formatA4R4G4B4));

    parse_VkBool32(obj["formatA4B4G4R4"], (o.formatA4B4G4R4));
}

static void parse_VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT(
    const Json::Value &obj, VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT *)parsePNextChain(obj);

    parse_VkBool32(obj["vertexInputDynamicState"], (o.vertexInputDynamicState));
}

static void parse_VkVertexInputBindingDescription2EXT(const Json::Value &obj, VkVertexInputBindingDescription2EXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkVertexInputBindingDescription2EXT *)parsePNextChain(obj);

    parse_uint32_t(obj["binding"], (o.binding));

    parse_uint32_t(obj["stride"], (o.stride));

    parse_VkVertexInputRate(obj["inputRate"], (o.inputRate));

    parse_uint32_t(obj["divisor"], (o.divisor));
}

static void parse_VkVertexInputAttributeDescription2EXT(const Json::Value &obj,
                                                        VkVertexInputAttributeDescription2EXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkVertexInputAttributeDescription2EXT *)parsePNextChain(obj);

    parse_uint32_t(obj["location"], (o.location));

    parse_uint32_t(obj["binding"], (o.binding));

    parse_VkFormat(obj["format"], (o.format));

    parse_uint32_t(obj["offset"], (o.offset));
}

static std::map<std::string, int> VkSciSyncClientTypeNV_map = {
    std::make_pair("VK_SCI_SYNC_CLIENT_TYPE_SIGNALER_NV", 0),
    std::make_pair("VK_SCI_SYNC_CLIENT_TYPE_WAITER_NV", 1),
    std::make_pair("VK_SCI_SYNC_CLIENT_TYPE_SIGNALER_WAITER_NV", 2),
};
static void parse_VkSciSyncClientTypeNV(const Json::Value &obj, VkSciSyncClientTypeNV &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkSciSyncClientTypeNV)VkSciSyncClientTypeNV_map[std::string(_res)];
}

static std::map<std::string, int> VkSciSyncPrimitiveTypeNV_map = {
    std::make_pair("VK_SCI_SYNC_PRIMITIVE_TYPE_FENCE_NV", 0),
    std::make_pair("VK_SCI_SYNC_PRIMITIVE_TYPE_SEMAPHORE_NV", 1),
};
static void parse_VkSciSyncPrimitiveTypeNV(const Json::Value &obj, VkSciSyncPrimitiveTypeNV &o)
{
    const std::string &_res = obj.asString();
    o                       = (VkSciSyncPrimitiveTypeNV)VkSciSyncPrimitiveTypeNV_map[std::string(_res)];
}

static void parse_VkExportFenceSciSyncInfoNV(const Json::Value &obj, VkExportFenceSciSyncInfoNV &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkExportFenceSciSyncInfoNV *)parsePNextChain(obj);

    parse_NvSciSyncAttrList(obj["pAttributes"], (o.pAttributes));
}

static void parse_VkImportFenceSciSyncInfoNV(const Json::Value &obj, VkImportFenceSciSyncInfoNV &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkImportFenceSciSyncInfoNV *)parsePNextChain(obj);

    parse_VkExternalFenceHandleTypeFlagBits(obj["handleType"], (o.handleType));
}

static void parse_VkFenceGetSciSyncInfoNV(const Json::Value &obj, VkFenceGetSciSyncInfoNV &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkFenceGetSciSyncInfoNV *)parsePNextChain(obj);

    parse_VkExternalFenceHandleTypeFlagBits(obj["handleType"], (o.handleType));
}

static void parse_VkSciSyncAttributesInfoNV(const Json::Value &obj, VkSciSyncAttributesInfoNV &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkSciSyncAttributesInfoNV *)parsePNextChain(obj);

    parse_VkSciSyncClientTypeNV(obj["clientType"], (o.clientType));

    parse_VkSciSyncPrimitiveTypeNV(obj["primitiveType"], (o.primitiveType));
}

static void parse_VkExportSemaphoreSciSyncInfoNV(const Json::Value &obj, VkExportSemaphoreSciSyncInfoNV &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkExportSemaphoreSciSyncInfoNV *)parsePNextChain(obj);

    parse_NvSciSyncAttrList(obj["pAttributes"], (o.pAttributes));
}

static void parse_VkImportSemaphoreSciSyncInfoNV(const Json::Value &obj, VkImportSemaphoreSciSyncInfoNV &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkImportSemaphoreSciSyncInfoNV *)parsePNextChain(obj);

    parse_VkExternalSemaphoreHandleTypeFlagBits(obj["handleType"], (o.handleType));
}

static void parse_VkSemaphoreGetSciSyncInfoNV(const Json::Value &obj, VkSemaphoreGetSciSyncInfoNV &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkSemaphoreGetSciSyncInfoNV *)parsePNextChain(obj);

    parse_VkExternalSemaphoreHandleTypeFlagBits(obj["handleType"], (o.handleType));
}

static void parse_VkPhysicalDeviceExternalSciSyncFeaturesNV(const Json::Value &obj,
                                                            VkPhysicalDeviceExternalSciSyncFeaturesNV &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceExternalSciSyncFeaturesNV *)parsePNextChain(obj);

    parse_VkBool32(obj["sciSyncFence"], (o.sciSyncFence));

    parse_VkBool32(obj["sciSyncSemaphore"], (o.sciSyncSemaphore));

    parse_VkBool32(obj["sciSyncImport"], (o.sciSyncImport));

    parse_VkBool32(obj["sciSyncExport"], (o.sciSyncExport));
}

static void parse_VkExportMemorySciBufInfoNV(const Json::Value &obj, VkExportMemorySciBufInfoNV &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkExportMemorySciBufInfoNV *)parsePNextChain(obj);

    parse_NvSciBufAttrList(obj["pAttributes"], (o.pAttributes));
}

static void parse_VkImportMemorySciBufInfoNV(const Json::Value &obj, VkImportMemorySciBufInfoNV &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkImportMemorySciBufInfoNV *)parsePNextChain(obj);

    parse_VkExternalMemoryHandleTypeFlagBits(obj["handleType"], (o.handleType));

    parse_NvSciBufObj(obj["handle"], (o.handle));
}

static void parse_VkMemoryGetSciBufInfoNV(const Json::Value &obj, VkMemoryGetSciBufInfoNV &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkMemoryGetSciBufInfoNV *)parsePNextChain(obj);

    parse_VkExternalMemoryHandleTypeFlagBits(obj["handleType"], (o.handleType));
}

static void parse_VkMemorySciBufPropertiesNV(const Json::Value &obj, VkMemorySciBufPropertiesNV &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkMemorySciBufPropertiesNV *)parsePNextChain(obj);

    parse_uint32_t(obj["memoryTypeBits"], (o.memoryTypeBits));
}

static void parse_VkPhysicalDeviceExternalMemorySciBufFeaturesNV(const Json::Value &obj,
                                                                 VkPhysicalDeviceExternalMemorySciBufFeaturesNV &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceExternalMemorySciBufFeaturesNV *)parsePNextChain(obj);

    parse_VkBool32(obj["sciBufImport"], (o.sciBufImport));

    parse_VkBool32(obj["sciBufExport"], (o.sciBufExport));
}

static void parse_VkPhysicalDeviceExtendedDynamicState2FeaturesEXT(const Json::Value &obj,
                                                                   VkPhysicalDeviceExtendedDynamicState2FeaturesEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceExtendedDynamicState2FeaturesEXT *)parsePNextChain(obj);

    parse_VkBool32(obj["extendedDynamicState2"], (o.extendedDynamicState2));

    parse_VkBool32(obj["extendedDynamicState2LogicOp"], (o.extendedDynamicState2LogicOp));

    parse_VkBool32(obj["extendedDynamicState2PatchControlPoints"], (o.extendedDynamicState2PatchControlPoints));
}

static void parse_VkPhysicalDeviceColorWriteEnableFeaturesEXT(const Json::Value &obj,
                                                              VkPhysicalDeviceColorWriteEnableFeaturesEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceColorWriteEnableFeaturesEXT *)parsePNextChain(obj);

    parse_VkBool32(obj["colorWriteEnable"], (o.colorWriteEnable));
}

static void parse_VkPipelineColorWriteCreateInfoEXT(const Json::Value &obj, VkPipelineColorWriteCreateInfoEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPipelineColorWriteCreateInfoEXT *)parsePNextChain(obj);

    parse_uint32_t(obj["attachmentCount"], (o.attachmentCount));

    (o.pColorWriteEnables) = (VkBool32 *)s_globalMem.allocate((o.attachmentCount), sizeof(VkBool32));
    const Json::Value &obj_pColorWriteEnables_arr = obj["pColorWriteEnables"];
    for (unsigned int i = 0; i < obj_pColorWriteEnables_arr.size(); i++)
    {
        parse_VkBool32(obj_pColorWriteEnables_arr[i], const_cast<VkBool32 &>((o.pColorWriteEnables[i])));
    }
}

static void parse_VkApplicationParametersEXT(const Json::Value &obj, VkApplicationParametersEXT &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkApplicationParametersEXT *)parsePNextChain(obj);

    parse_uint32_t(obj["vendorID"], (o.vendorID));

    parse_uint32_t(obj["deviceID"], (o.deviceID));

    parse_uint32_t(obj["key"], (o.key));

    parse_uint64_t(obj["value"], (o.value));
}

static void parse_VkSemaphoreSciSyncPoolNV(const Json::Value &obj, VkSemaphoreSciSyncPoolNV &o)
{
    //    const std::string& _res = obj.asString();
}

static void parse_VkPhysicalDeviceExternalSciSync2FeaturesNV(const Json::Value &obj,
                                                             VkPhysicalDeviceExternalSciSync2FeaturesNV &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceExternalSciSync2FeaturesNV *)parsePNextChain(obj);

    parse_VkBool32(obj["sciSyncFence"], (o.sciSyncFence));

    parse_VkBool32(obj["sciSyncSemaphore2"], (o.sciSyncSemaphore2));

    parse_VkBool32(obj["sciSyncImport"], (o.sciSyncImport));

    parse_VkBool32(obj["sciSyncExport"], (o.sciSyncExport));
}

static void parse_VkSemaphoreSciSyncPoolCreateInfoNV(const Json::Value &obj, VkSemaphoreSciSyncPoolCreateInfoNV &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkSemaphoreSciSyncPoolCreateInfoNV *)parsePNextChain(obj);

    parse_NvSciSyncObj(obj["handle"], (o.handle));
}

static void parse_VkSemaphoreSciSyncCreateInfoNV(const Json::Value &obj, VkSemaphoreSciSyncCreateInfoNV &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkSemaphoreSciSyncCreateInfoNV *)parsePNextChain(obj);

    /** TODO: Handle this - pFence **/
}

static void parse_VkDeviceSemaphoreSciSyncPoolReservationCreateInfoNV(
    const Json::Value &obj, VkDeviceSemaphoreSciSyncPoolReservationCreateInfoNV &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkDeviceSemaphoreSciSyncPoolReservationCreateInfoNV *)parsePNextChain(obj);

    parse_uint32_t(obj["semaphoreSciSyncPoolRequestCount"], (o.semaphoreSciSyncPoolRequestCount));
}

static void parse_VkScreenBufferPropertiesQNX(const Json::Value &obj, VkScreenBufferPropertiesQNX &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkScreenBufferPropertiesQNX *)parsePNextChain(obj);

    parse_VkDeviceSize(obj["allocationSize"], (o.allocationSize));

    parse_uint32_t(obj["memoryTypeBits"], (o.memoryTypeBits));
}

static void parse_VkScreenBufferFormatPropertiesQNX(const Json::Value &obj, VkScreenBufferFormatPropertiesQNX &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkScreenBufferFormatPropertiesQNX *)parsePNextChain(obj);

    parse_VkFormat(obj["format"], (o.format));

    parse_uint64_t(obj["externalFormat"], (o.externalFormat));

    parse_uint64_t(obj["screenUsage"], (o.screenUsage));

    parse_VkFormatFeatureFlags(obj["formatFeatures"], (o.formatFeatures));

    parse_VkComponentMapping(obj["samplerYcbcrConversionComponents"], (o.samplerYcbcrConversionComponents));

    parse_VkSamplerYcbcrModelConversion(obj["suggestedYcbcrModel"], (o.suggestedYcbcrModel));

    parse_VkSamplerYcbcrRange(obj["suggestedYcbcrRange"], (o.suggestedYcbcrRange));

    parse_VkChromaLocation(obj["suggestedXChromaOffset"], (o.suggestedXChromaOffset));

    parse_VkChromaLocation(obj["suggestedYChromaOffset"], (o.suggestedYChromaOffset));
}

static void parse_VkImportScreenBufferInfoQNX(const Json::Value &obj, VkImportScreenBufferInfoQNX &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkImportScreenBufferInfoQNX *)parsePNextChain(obj);

    /** Note: Ignoring _screen_buffer* data from buffer **/
}

static void parse_VkExternalFormatQNX(const Json::Value &obj, VkExternalFormatQNX &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkExternalFormatQNX *)parsePNextChain(obj);

    parse_uint64_t(obj["externalFormat"], (o.externalFormat));
}

static void parse_VkPhysicalDeviceExternalMemoryScreenBufferFeaturesQNX(
    const Json::Value &obj, VkPhysicalDeviceExternalMemoryScreenBufferFeaturesQNX &o)
{

    parse_VkStructureType(obj["sType"], (o.sType));

    o.pNext = (VkPhysicalDeviceExternalMemoryScreenBufferFeaturesQNX *)parsePNextChain(obj);

    parse_VkBool32(obj["screenBufferImport"], (o.screenBufferImport));
}

static void *parsePNextChain(const Json::Value &obj)
{
    VkBaseInStructure o;
    const Json::Value &pNextObj = obj["pNext"];
    if (pNextObj.empty() || (pNextObj.isString() && pNextObj.asString() == "NULL"))
        return nullptr;

    parse_VkStructureType(pNextObj["sType"], (o.sType));
    void *p = nullptr;
    switch (o.sType)
    {
    case VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO:
        p = s_globalMem.allocate(sizeof(VkPipelineLayoutCreateInfo));
        parse_VkPipelineLayoutCreateInfo(pNextObj, *((VkPipelineLayoutCreateInfo *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceSubgroupProperties));
        parse_VkPhysicalDeviceSubgroupProperties(pNextObj, *((VkPhysicalDeviceSubgroupProperties *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES:
        p = s_globalMem.allocate(sizeof(VkPhysicalDevice16BitStorageFeatures));
        parse_VkPhysicalDevice16BitStorageFeatures(pNextObj, *((VkPhysicalDevice16BitStorageFeatures *)p));
        break;

    case VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS:
        p = s_globalMem.allocate(sizeof(VkMemoryDedicatedRequirements));
        parse_VkMemoryDedicatedRequirements(pNextObj, *((VkMemoryDedicatedRequirements *)p));
        break;

    case VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO:
        p = s_globalMem.allocate(sizeof(VkMemoryDedicatedAllocateInfo));
        parse_VkMemoryDedicatedAllocateInfo(pNextObj, *((VkMemoryDedicatedAllocateInfo *)p));
        break;

    case VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO:
        p = s_globalMem.allocate(sizeof(VkMemoryAllocateFlagsInfo));
        parse_VkMemoryAllocateFlagsInfo(pNextObj, *((VkMemoryAllocateFlagsInfo *)p));
        break;

    case VK_STRUCTURE_TYPE_DEVICE_GROUP_RENDER_PASS_BEGIN_INFO:
        p = s_globalMem.allocate(sizeof(VkDeviceGroupRenderPassBeginInfo));
        parse_VkDeviceGroupRenderPassBeginInfo(pNextObj, *((VkDeviceGroupRenderPassBeginInfo *)p));
        break;

    case VK_STRUCTURE_TYPE_DEVICE_GROUP_COMMAND_BUFFER_BEGIN_INFO:
        p = s_globalMem.allocate(sizeof(VkDeviceGroupCommandBufferBeginInfo));
        parse_VkDeviceGroupCommandBufferBeginInfo(pNextObj, *((VkDeviceGroupCommandBufferBeginInfo *)p));
        break;

    case VK_STRUCTURE_TYPE_DEVICE_GROUP_SUBMIT_INFO:
        p = s_globalMem.allocate(sizeof(VkDeviceGroupSubmitInfo));
        parse_VkDeviceGroupSubmitInfo(pNextObj, *((VkDeviceGroupSubmitInfo *)p));
        break;

    case VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_DEVICE_GROUP_INFO:
        p = s_globalMem.allocate(sizeof(VkBindBufferMemoryDeviceGroupInfo));
        parse_VkBindBufferMemoryDeviceGroupInfo(pNextObj, *((VkBindBufferMemoryDeviceGroupInfo *)p));
        break;

    case VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_DEVICE_GROUP_INFO:
        p = s_globalMem.allocate(sizeof(VkBindImageMemoryDeviceGroupInfo));
        parse_VkBindImageMemoryDeviceGroupInfo(pNextObj, *((VkBindImageMemoryDeviceGroupInfo *)p));
        break;

    case VK_STRUCTURE_TYPE_DEVICE_GROUP_DEVICE_CREATE_INFO:
        p = s_globalMem.allocate(sizeof(VkDeviceGroupDeviceCreateInfo));
        parse_VkDeviceGroupDeviceCreateInfo(pNextObj, *((VkDeviceGroupDeviceCreateInfo *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceFeatures2));
        parse_VkPhysicalDeviceFeatures2(pNextObj, *((VkPhysicalDeviceFeatures2 *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_POINT_CLIPPING_PROPERTIES:
        p = s_globalMem.allocate(sizeof(VkPhysicalDevicePointClippingProperties));
        parse_VkPhysicalDevicePointClippingProperties(pNextObj, *((VkPhysicalDevicePointClippingProperties *)p));
        break;

    case VK_STRUCTURE_TYPE_RENDER_PASS_INPUT_ATTACHMENT_ASPECT_CREATE_INFO:
        p = s_globalMem.allocate(sizeof(VkRenderPassInputAttachmentAspectCreateInfo));
        parse_VkRenderPassInputAttachmentAspectCreateInfo(pNextObj,
                                                          *((VkRenderPassInputAttachmentAspectCreateInfo *)p));
        break;

    case VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO:
        p = s_globalMem.allocate(sizeof(VkImageViewUsageCreateInfo));
        parse_VkImageViewUsageCreateInfo(pNextObj, *((VkImageViewUsageCreateInfo *)p));
        break;

    case VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_DOMAIN_ORIGIN_STATE_CREATE_INFO:
        p = s_globalMem.allocate(sizeof(VkPipelineTessellationDomainOriginStateCreateInfo));
        parse_VkPipelineTessellationDomainOriginStateCreateInfo(
            pNextObj, *((VkPipelineTessellationDomainOriginStateCreateInfo *)p));
        break;

    case VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO:
        p = s_globalMem.allocate(sizeof(VkRenderPassMultiviewCreateInfo));
        parse_VkRenderPassMultiviewCreateInfo(pNextObj, *((VkRenderPassMultiviewCreateInfo *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceMultiviewFeatures));
        parse_VkPhysicalDeviceMultiviewFeatures(pNextObj, *((VkPhysicalDeviceMultiviewFeatures *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceMultiviewProperties));
        parse_VkPhysicalDeviceMultiviewProperties(pNextObj, *((VkPhysicalDeviceMultiviewProperties *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VARIABLE_POINTERS_FEATURES:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceVariablePointersFeatures));
        parse_VkPhysicalDeviceVariablePointersFeatures(pNextObj, *((VkPhysicalDeviceVariablePointersFeatures *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceProtectedMemoryFeatures));
        parse_VkPhysicalDeviceProtectedMemoryFeatures(pNextObj, *((VkPhysicalDeviceProtectedMemoryFeatures *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_PROPERTIES:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceProtectedMemoryProperties));
        parse_VkPhysicalDeviceProtectedMemoryProperties(pNextObj, *((VkPhysicalDeviceProtectedMemoryProperties *)p));
        break;

    case VK_STRUCTURE_TYPE_PROTECTED_SUBMIT_INFO:
        p = s_globalMem.allocate(sizeof(VkProtectedSubmitInfo));
        parse_VkProtectedSubmitInfo(pNextObj, *((VkProtectedSubmitInfo *)p));
        break;

    case VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO:
        p = s_globalMem.allocate(sizeof(VkSamplerYcbcrConversionInfo));
        parse_VkSamplerYcbcrConversionInfo(pNextObj, *((VkSamplerYcbcrConversionInfo *)p));
        break;

    case VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO:
        p = s_globalMem.allocate(sizeof(VkBindImagePlaneMemoryInfo));
        parse_VkBindImagePlaneMemoryInfo(pNextObj, *((VkBindImagePlaneMemoryInfo *)p));
        break;

    case VK_STRUCTURE_TYPE_IMAGE_PLANE_MEMORY_REQUIREMENTS_INFO:
        p = s_globalMem.allocate(sizeof(VkImagePlaneMemoryRequirementsInfo));
        parse_VkImagePlaneMemoryRequirementsInfo(pNextObj, *((VkImagePlaneMemoryRequirementsInfo *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceSamplerYcbcrConversionFeatures));
        parse_VkPhysicalDeviceSamplerYcbcrConversionFeatures(pNextObj,
                                                             *((VkPhysicalDeviceSamplerYcbcrConversionFeatures *)p));
        break;

    case VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_IMAGE_FORMAT_PROPERTIES:
        p = s_globalMem.allocate(sizeof(VkSamplerYcbcrConversionImageFormatProperties));
        parse_VkSamplerYcbcrConversionImageFormatProperties(pNextObj,
                                                            *((VkSamplerYcbcrConversionImageFormatProperties *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceExternalImageFormatInfo));
        parse_VkPhysicalDeviceExternalImageFormatInfo(pNextObj, *((VkPhysicalDeviceExternalImageFormatInfo *)p));
        break;

    case VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES:
        p = s_globalMem.allocate(sizeof(VkExternalImageFormatProperties));
        parse_VkExternalImageFormatProperties(pNextObj, *((VkExternalImageFormatProperties *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceIDProperties));
        parse_VkPhysicalDeviceIDProperties(pNextObj, *((VkPhysicalDeviceIDProperties *)p));
        break;

    case VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO:
        p = s_globalMem.allocate(sizeof(VkExternalMemoryImageCreateInfo));
        parse_VkExternalMemoryImageCreateInfo(pNextObj, *((VkExternalMemoryImageCreateInfo *)p));
        break;

    case VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO:
        p = s_globalMem.allocate(sizeof(VkExternalMemoryBufferCreateInfo));
        parse_VkExternalMemoryBufferCreateInfo(pNextObj, *((VkExternalMemoryBufferCreateInfo *)p));
        break;

    case VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO:
        p = s_globalMem.allocate(sizeof(VkExportMemoryAllocateInfo));
        parse_VkExportMemoryAllocateInfo(pNextObj, *((VkExportMemoryAllocateInfo *)p));
        break;

    case VK_STRUCTURE_TYPE_EXPORT_FENCE_CREATE_INFO:
        p = s_globalMem.allocate(sizeof(VkExportFenceCreateInfo));
        parse_VkExportFenceCreateInfo(pNextObj, *((VkExportFenceCreateInfo *)p));
        break;

    case VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO:
        p = s_globalMem.allocate(sizeof(VkExportSemaphoreCreateInfo));
        parse_VkExportSemaphoreCreateInfo(pNextObj, *((VkExportSemaphoreCreateInfo *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceMaintenance3Properties));
        parse_VkPhysicalDeviceMaintenance3Properties(pNextObj, *((VkPhysicalDeviceMaintenance3Properties *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceShaderDrawParametersFeatures));
        parse_VkPhysicalDeviceShaderDrawParametersFeatures(pNextObj,
                                                           *((VkPhysicalDeviceShaderDrawParametersFeatures *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceVulkan11Features));
        parse_VkPhysicalDeviceVulkan11Features(pNextObj, *((VkPhysicalDeviceVulkan11Features *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceVulkan11Properties));
        parse_VkPhysicalDeviceVulkan11Properties(pNextObj, *((VkPhysicalDeviceVulkan11Properties *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceVulkan12Features));
        parse_VkPhysicalDeviceVulkan12Features(pNextObj, *((VkPhysicalDeviceVulkan12Features *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceVulkan12Properties));
        parse_VkPhysicalDeviceVulkan12Properties(pNextObj, *((VkPhysicalDeviceVulkan12Properties *)p));
        break;

    case VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO:
        p = s_globalMem.allocate(sizeof(VkImageFormatListCreateInfo));
        parse_VkImageFormatListCreateInfo(pNextObj, *((VkImageFormatListCreateInfo *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES:
        p = s_globalMem.allocate(sizeof(VkPhysicalDevice8BitStorageFeatures));
        parse_VkPhysicalDevice8BitStorageFeatures(pNextObj, *((VkPhysicalDevice8BitStorageFeatures *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceDriverProperties));
        parse_VkPhysicalDeviceDriverProperties(pNextObj, *((VkPhysicalDeviceDriverProperties *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceShaderAtomicInt64Features));
        parse_VkPhysicalDeviceShaderAtomicInt64Features(pNextObj, *((VkPhysicalDeviceShaderAtomicInt64Features *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceShaderFloat16Int8Features));
        parse_VkPhysicalDeviceShaderFloat16Int8Features(pNextObj, *((VkPhysicalDeviceShaderFloat16Int8Features *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceFloatControlsProperties));
        parse_VkPhysicalDeviceFloatControlsProperties(pNextObj, *((VkPhysicalDeviceFloatControlsProperties *)p));
        break;

    case VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO:
        p = s_globalMem.allocate(sizeof(VkDescriptorSetLayoutBindingFlagsCreateInfo));
        parse_VkDescriptorSetLayoutBindingFlagsCreateInfo(pNextObj,
                                                          *((VkDescriptorSetLayoutBindingFlagsCreateInfo *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceDescriptorIndexingFeatures));
        parse_VkPhysicalDeviceDescriptorIndexingFeatures(pNextObj, *((VkPhysicalDeviceDescriptorIndexingFeatures *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceDescriptorIndexingProperties));
        parse_VkPhysicalDeviceDescriptorIndexingProperties(pNextObj,
                                                           *((VkPhysicalDeviceDescriptorIndexingProperties *)p));
        break;

    case VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO:
        p = s_globalMem.allocate(sizeof(VkDescriptorSetVariableDescriptorCountAllocateInfo));
        parse_VkDescriptorSetVariableDescriptorCountAllocateInfo(
            pNextObj, *((VkDescriptorSetVariableDescriptorCountAllocateInfo *)p));
        break;

    case VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_LAYOUT_SUPPORT:
        p = s_globalMem.allocate(sizeof(VkDescriptorSetVariableDescriptorCountLayoutSupport));
        parse_VkDescriptorSetVariableDescriptorCountLayoutSupport(
            pNextObj, *((VkDescriptorSetVariableDescriptorCountLayoutSupport *)p));
        break;

    case VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE:
        p = s_globalMem.allocate(sizeof(VkSubpassDescriptionDepthStencilResolve));
        parse_VkSubpassDescriptionDepthStencilResolve(pNextObj, *((VkSubpassDescriptionDepthStencilResolve *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_STENCIL_RESOLVE_PROPERTIES:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceDepthStencilResolveProperties));
        parse_VkPhysicalDeviceDepthStencilResolveProperties(pNextObj,
                                                            *((VkPhysicalDeviceDepthStencilResolveProperties *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceScalarBlockLayoutFeatures));
        parse_VkPhysicalDeviceScalarBlockLayoutFeatures(pNextObj, *((VkPhysicalDeviceScalarBlockLayoutFeatures *)p));
        break;

    case VK_STRUCTURE_TYPE_IMAGE_STENCIL_USAGE_CREATE_INFO:
        p = s_globalMem.allocate(sizeof(VkImageStencilUsageCreateInfo));
        parse_VkImageStencilUsageCreateInfo(pNextObj, *((VkImageStencilUsageCreateInfo *)p));
        break;

    case VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO:
        p = s_globalMem.allocate(sizeof(VkSamplerReductionModeCreateInfo));
        parse_VkSamplerReductionModeCreateInfo(pNextObj, *((VkSamplerReductionModeCreateInfo *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_FILTER_MINMAX_PROPERTIES:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceSamplerFilterMinmaxProperties));
        parse_VkPhysicalDeviceSamplerFilterMinmaxProperties(pNextObj,
                                                            *((VkPhysicalDeviceSamplerFilterMinmaxProperties *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceVulkanMemoryModelFeatures));
        parse_VkPhysicalDeviceVulkanMemoryModelFeatures(pNextObj, *((VkPhysicalDeviceVulkanMemoryModelFeatures *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGELESS_FRAMEBUFFER_FEATURES:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceImagelessFramebufferFeatures));
        parse_VkPhysicalDeviceImagelessFramebufferFeatures(pNextObj,
                                                           *((VkPhysicalDeviceImagelessFramebufferFeatures *)p));
        break;

    case VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO:
        p = s_globalMem.allocate(sizeof(VkFramebufferAttachmentsCreateInfo));
        parse_VkFramebufferAttachmentsCreateInfo(pNextObj, *((VkFramebufferAttachmentsCreateInfo *)p));
        break;

    case VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO:
        p = s_globalMem.allocate(sizeof(VkRenderPassAttachmentBeginInfo));
        parse_VkRenderPassAttachmentBeginInfo(pNextObj, *((VkRenderPassAttachmentBeginInfo *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFORM_BUFFER_STANDARD_LAYOUT_FEATURES:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceUniformBufferStandardLayoutFeatures));
        parse_VkPhysicalDeviceUniformBufferStandardLayoutFeatures(
            pNextObj, *((VkPhysicalDeviceUniformBufferStandardLayoutFeatures *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SUBGROUP_EXTENDED_TYPES_FEATURES:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures));
        parse_VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures(
            pNextObj, *((VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures));
        parse_VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures(
            pNextObj, *((VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures *)p));
        break;

    case VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_STENCIL_LAYOUT:
        p = s_globalMem.allocate(sizeof(VkAttachmentReferenceStencilLayout));
        parse_VkAttachmentReferenceStencilLayout(pNextObj, *((VkAttachmentReferenceStencilLayout *)p));
        break;

    case VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_STENCIL_LAYOUT:
        p = s_globalMem.allocate(sizeof(VkAttachmentDescriptionStencilLayout));
        parse_VkAttachmentDescriptionStencilLayout(pNextObj, *((VkAttachmentDescriptionStencilLayout *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceHostQueryResetFeatures));
        parse_VkPhysicalDeviceHostQueryResetFeatures(pNextObj, *((VkPhysicalDeviceHostQueryResetFeatures *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceTimelineSemaphoreFeatures));
        parse_VkPhysicalDeviceTimelineSemaphoreFeatures(pNextObj, *((VkPhysicalDeviceTimelineSemaphoreFeatures *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_PROPERTIES:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceTimelineSemaphoreProperties));
        parse_VkPhysicalDeviceTimelineSemaphoreProperties(pNextObj,
                                                          *((VkPhysicalDeviceTimelineSemaphoreProperties *)p));
        break;

    case VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO:
        p = s_globalMem.allocate(sizeof(VkSemaphoreTypeCreateInfo));
        parse_VkSemaphoreTypeCreateInfo(pNextObj, *((VkSemaphoreTypeCreateInfo *)p));
        break;

    case VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO:
        p = s_globalMem.allocate(sizeof(VkTimelineSemaphoreSubmitInfo));
        parse_VkTimelineSemaphoreSubmitInfo(pNextObj, *((VkTimelineSemaphoreSubmitInfo *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceBufferDeviceAddressFeatures));
        parse_VkPhysicalDeviceBufferDeviceAddressFeatures(pNextObj,
                                                          *((VkPhysicalDeviceBufferDeviceAddressFeatures *)p));
        break;

    case VK_STRUCTURE_TYPE_BUFFER_OPAQUE_CAPTURE_ADDRESS_CREATE_INFO:
        p = s_globalMem.allocate(sizeof(VkBufferOpaqueCaptureAddressCreateInfo));
        parse_VkBufferOpaqueCaptureAddressCreateInfo(pNextObj, *((VkBufferOpaqueCaptureAddressCreateInfo *)p));
        break;

    case VK_STRUCTURE_TYPE_MEMORY_OPAQUE_CAPTURE_ADDRESS_ALLOCATE_INFO:
        p = s_globalMem.allocate(sizeof(VkMemoryOpaqueCaptureAddressAllocateInfo));
        parse_VkMemoryOpaqueCaptureAddressAllocateInfo(pNextObj, *((VkMemoryOpaqueCaptureAddressAllocateInfo *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_SC_1_0_FEATURES:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceVulkanSC10Features));
        parse_VkPhysicalDeviceVulkanSC10Features(pNextObj, *((VkPhysicalDeviceVulkanSC10Features *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_SC_1_0_PROPERTIES:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceVulkanSC10Properties));
        parse_VkPhysicalDeviceVulkanSC10Properties(pNextObj, *((VkPhysicalDeviceVulkanSC10Properties *)p));
        break;

    case VK_STRUCTURE_TYPE_DEVICE_OBJECT_RESERVATION_CREATE_INFO:
        p = s_globalMem.allocate(sizeof(VkDeviceObjectReservationCreateInfo));
        parse_VkDeviceObjectReservationCreateInfo(pNextObj, *((VkDeviceObjectReservationCreateInfo *)p));
        break;

    case VK_STRUCTURE_TYPE_COMMAND_POOL_MEMORY_RESERVATION_CREATE_INFO:
        p = s_globalMem.allocate(sizeof(VkCommandPoolMemoryReservationCreateInfo));
        parse_VkCommandPoolMemoryReservationCreateInfo(pNextObj, *((VkCommandPoolMemoryReservationCreateInfo *)p));
        break;

    case VK_STRUCTURE_TYPE_FAULT_CALLBACK_INFO:
        p = s_globalMem.allocate(sizeof(VkFaultCallbackInfo));
        parse_VkFaultCallbackInfo(pNextObj, *((VkFaultCallbackInfo *)p));
        break;

    case VK_STRUCTURE_TYPE_PIPELINE_OFFLINE_CREATE_INFO:
        p = s_globalMem.allocate(sizeof(VkPipelineOfflineCreateInfo));
        parse_VkPipelineOfflineCreateInfo(pNextObj, *((VkPipelineOfflineCreateInfo *)p));
        break;

    case VK_STRUCTURE_TYPE_IMAGE_SWAPCHAIN_CREATE_INFO_KHR:
        p = s_globalMem.allocate(sizeof(VkImageSwapchainCreateInfoKHR));
        parse_VkImageSwapchainCreateInfoKHR(pNextObj, *((VkImageSwapchainCreateInfoKHR *)p));
        break;

    case VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_SWAPCHAIN_INFO_KHR:
        p = s_globalMem.allocate(sizeof(VkBindImageMemorySwapchainInfoKHR));
        parse_VkBindImageMemorySwapchainInfoKHR(pNextObj, *((VkBindImageMemorySwapchainInfoKHR *)p));
        break;

    case VK_STRUCTURE_TYPE_DEVICE_GROUP_PRESENT_INFO_KHR:
        p = s_globalMem.allocate(sizeof(VkDeviceGroupPresentInfoKHR));
        parse_VkDeviceGroupPresentInfoKHR(pNextObj, *((VkDeviceGroupPresentInfoKHR *)p));
        break;

    case VK_STRUCTURE_TYPE_DEVICE_GROUP_SWAPCHAIN_CREATE_INFO_KHR:
        p = s_globalMem.allocate(sizeof(VkDeviceGroupSwapchainCreateInfoKHR));
        parse_VkDeviceGroupSwapchainCreateInfoKHR(pNextObj, *((VkDeviceGroupSwapchainCreateInfoKHR *)p));
        break;

    case VK_STRUCTURE_TYPE_DISPLAY_PRESENT_INFO_KHR:
        p = s_globalMem.allocate(sizeof(VkDisplayPresentInfoKHR));
        parse_VkDisplayPresentInfoKHR(pNextObj, *((VkDisplayPresentInfoKHR *)p));
        break;

    case VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR:
        p = s_globalMem.allocate(sizeof(VkImportMemoryFdInfoKHR));
        parse_VkImportMemoryFdInfoKHR(pNextObj, *((VkImportMemoryFdInfoKHR *)p));
        break;

    case VK_STRUCTURE_TYPE_PRESENT_REGIONS_KHR:
        p = s_globalMem.allocate(sizeof(VkPresentRegionsKHR));
        parse_VkPresentRegionsKHR(pNextObj, *((VkPresentRegionsKHR *)p));
        break;

    case VK_STRUCTURE_TYPE_SHARED_PRESENT_SURFACE_CAPABILITIES_KHR:
        p = s_globalMem.allocate(sizeof(VkSharedPresentSurfaceCapabilitiesKHR));
        parse_VkSharedPresentSurfaceCapabilitiesKHR(pNextObj, *((VkSharedPresentSurfaceCapabilitiesKHR *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PERFORMANCE_QUERY_FEATURES_KHR:
        p = s_globalMem.allocate(sizeof(VkPhysicalDevicePerformanceQueryFeaturesKHR));
        parse_VkPhysicalDevicePerformanceQueryFeaturesKHR(pNextObj,
                                                          *((VkPhysicalDevicePerformanceQueryFeaturesKHR *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PERFORMANCE_QUERY_PROPERTIES_KHR:
        p = s_globalMem.allocate(sizeof(VkPhysicalDevicePerformanceQueryPropertiesKHR));
        parse_VkPhysicalDevicePerformanceQueryPropertiesKHR(pNextObj,
                                                            *((VkPhysicalDevicePerformanceQueryPropertiesKHR *)p));
        break;

    case VK_STRUCTURE_TYPE_QUERY_POOL_PERFORMANCE_CREATE_INFO_KHR:
        p = s_globalMem.allocate(sizeof(VkQueryPoolPerformanceCreateInfoKHR));
        parse_VkQueryPoolPerformanceCreateInfoKHR(pNextObj, *((VkQueryPoolPerformanceCreateInfoKHR *)p));
        break;

    case VK_STRUCTURE_TYPE_PERFORMANCE_QUERY_SUBMIT_INFO_KHR:
        p = s_globalMem.allocate(sizeof(VkPerformanceQuerySubmitInfoKHR));
        parse_VkPerformanceQuerySubmitInfoKHR(pNextObj, *((VkPerformanceQuerySubmitInfoKHR *)p));
        break;

    case VK_STRUCTURE_TYPE_PERFORMANCE_QUERY_RESERVATION_INFO_KHR:
        p = s_globalMem.allocate(sizeof(VkPerformanceQueryReservationInfoKHR));
        parse_VkPerformanceQueryReservationInfoKHR(pNextObj, *((VkPerformanceQueryReservationInfoKHR *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CLOCK_FEATURES_KHR:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceShaderClockFeaturesKHR));
        parse_VkPhysicalDeviceShaderClockFeaturesKHR(pNextObj, *((VkPhysicalDeviceShaderClockFeaturesKHR *)p));
        break;

    case VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO_KHR:
        p = s_globalMem.allocate(sizeof(VkDeviceQueueGlobalPriorityCreateInfoKHR));
        parse_VkDeviceQueueGlobalPriorityCreateInfoKHR(pNextObj, *((VkDeviceQueueGlobalPriorityCreateInfoKHR *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GLOBAL_PRIORITY_QUERY_FEATURES_KHR:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceGlobalPriorityQueryFeaturesKHR));
        parse_VkPhysicalDeviceGlobalPriorityQueryFeaturesKHR(pNextObj,
                                                             *((VkPhysicalDeviceGlobalPriorityQueryFeaturesKHR *)p));
        break;

    case VK_STRUCTURE_TYPE_QUEUE_FAMILY_GLOBAL_PRIORITY_PROPERTIES_KHR:
        p = s_globalMem.allocate(sizeof(VkQueueFamilyGlobalPriorityPropertiesKHR));
        parse_VkQueueFamilyGlobalPriorityPropertiesKHR(pNextObj, *((VkQueueFamilyGlobalPriorityPropertiesKHR *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_TERMINATE_INVOCATION_FEATURES:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceShaderTerminateInvocationFeatures));
        parse_VkPhysicalDeviceShaderTerminateInvocationFeatures(
            pNextObj, *((VkPhysicalDeviceShaderTerminateInvocationFeatures *)p));
        break;

    case VK_STRUCTURE_TYPE_FRAGMENT_SHADING_RATE_ATTACHMENT_INFO_KHR:
        p = s_globalMem.allocate(sizeof(VkFragmentShadingRateAttachmentInfoKHR));
        parse_VkFragmentShadingRateAttachmentInfoKHR(pNextObj, *((VkFragmentShadingRateAttachmentInfoKHR *)p));
        break;

    case VK_STRUCTURE_TYPE_PIPELINE_FRAGMENT_SHADING_RATE_STATE_CREATE_INFO_KHR:
        p = s_globalMem.allocate(sizeof(VkPipelineFragmentShadingRateStateCreateInfoKHR));
        parse_VkPipelineFragmentShadingRateStateCreateInfoKHR(pNextObj,
                                                              *((VkPipelineFragmentShadingRateStateCreateInfoKHR *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceFragmentShadingRateFeaturesKHR));
        parse_VkPhysicalDeviceFragmentShadingRateFeaturesKHR(pNextObj,
                                                             *((VkPhysicalDeviceFragmentShadingRateFeaturesKHR *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_PROPERTIES_KHR:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceFragmentShadingRatePropertiesKHR));
        parse_VkPhysicalDeviceFragmentShadingRatePropertiesKHR(
            pNextObj, *((VkPhysicalDeviceFragmentShadingRatePropertiesKHR *)p));
        break;

    case VK_STRUCTURE_TYPE_MEMORY_BARRIER_2:
        p = s_globalMem.allocate(sizeof(VkMemoryBarrier2));
        parse_VkMemoryBarrier2(pNextObj, *((VkMemoryBarrier2 *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceSynchronization2Features));
        parse_VkPhysicalDeviceSynchronization2Features(pNextObj, *((VkPhysicalDeviceSynchronization2Features *)p));
        break;

    case VK_STRUCTURE_TYPE_QUEUE_FAMILY_CHECKPOINT_PROPERTIES_2_NV:
        p = s_globalMem.allocate(sizeof(VkQueueFamilyCheckpointProperties2NV));
        parse_VkQueueFamilyCheckpointProperties2NV(pNextObj, *((VkQueueFamilyCheckpointProperties2NV *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_PROPERTIES_KHR:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceVertexAttributeDivisorPropertiesKHR));
        parse_VkPhysicalDeviceVertexAttributeDivisorPropertiesKHR(
            pNextObj, *((VkPhysicalDeviceVertexAttributeDivisorPropertiesKHR *)p));
        break;

    case VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_DIVISOR_STATE_CREATE_INFO_KHR:
        p = s_globalMem.allocate(sizeof(VkPipelineVertexInputDivisorStateCreateInfoKHR));
        parse_VkPipelineVertexInputDivisorStateCreateInfoKHR(pNextObj,
                                                             *((VkPipelineVertexInputDivisorStateCreateInfoKHR *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES_KHR:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceVertexAttributeDivisorFeaturesKHR));
        parse_VkPhysicalDeviceVertexAttributeDivisorFeaturesKHR(
            pNextObj, *((VkPhysicalDeviceVertexAttributeDivisorFeaturesKHR *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INDEX_TYPE_UINT8_FEATURES_KHR:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceIndexTypeUint8FeaturesKHR));
        parse_VkPhysicalDeviceIndexTypeUint8FeaturesKHR(pNextObj, *((VkPhysicalDeviceIndexTypeUint8FeaturesKHR *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_FEATURES_KHR:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceLineRasterizationFeaturesKHR));
        parse_VkPhysicalDeviceLineRasterizationFeaturesKHR(pNextObj,
                                                           *((VkPhysicalDeviceLineRasterizationFeaturesKHR *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_PROPERTIES_KHR:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceLineRasterizationPropertiesKHR));
        parse_VkPhysicalDeviceLineRasterizationPropertiesKHR(pNextObj,
                                                             *((VkPhysicalDeviceLineRasterizationPropertiesKHR *)p));
        break;

    case VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_LINE_STATE_CREATE_INFO_KHR:
        p = s_globalMem.allocate(sizeof(VkPipelineRasterizationLineStateCreateInfoKHR));
        parse_VkPipelineRasterizationLineStateCreateInfoKHR(pNextObj,
                                                            *((VkPipelineRasterizationLineStateCreateInfoKHR *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TEXTURE_COMPRESSION_ASTC_HDR_FEATURES:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceTextureCompressionASTCHDRFeatures));
        parse_VkPhysicalDeviceTextureCompressionASTCHDRFeatures(
            pNextObj, *((VkPhysicalDeviceTextureCompressionASTCHDRFeatures *)p));
        break;

    case VK_STRUCTURE_TYPE_IMAGE_VIEW_ASTC_DECODE_MODE_EXT:
        p = s_globalMem.allocate(sizeof(VkImageViewASTCDecodeModeEXT));
        parse_VkImageViewASTCDecodeModeEXT(pNextObj, *((VkImageViewASTCDecodeModeEXT *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ASTC_DECODE_FEATURES_EXT:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceASTCDecodeFeaturesEXT));
        parse_VkPhysicalDeviceASTCDecodeFeaturesEXT(pNextObj, *((VkPhysicalDeviceASTCDecodeFeaturesEXT *)p));
        break;

    case VK_STRUCTURE_TYPE_SWAPCHAIN_COUNTER_CREATE_INFO_EXT:
        p = s_globalMem.allocate(sizeof(VkSwapchainCounterCreateInfoEXT));
        parse_VkSwapchainCounterCreateInfoEXT(pNextObj, *((VkSwapchainCounterCreateInfoEXT *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DISCARD_RECTANGLE_PROPERTIES_EXT:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceDiscardRectanglePropertiesEXT));
        parse_VkPhysicalDeviceDiscardRectanglePropertiesEXT(pNextObj,
                                                            *((VkPhysicalDeviceDiscardRectanglePropertiesEXT *)p));
        break;

    case VK_STRUCTURE_TYPE_PIPELINE_DISCARD_RECTANGLE_STATE_CREATE_INFO_EXT:
        p = s_globalMem.allocate(sizeof(VkPipelineDiscardRectangleStateCreateInfoEXT));
        parse_VkPipelineDiscardRectangleStateCreateInfoEXT(pNextObj,
                                                           *((VkPipelineDiscardRectangleStateCreateInfoEXT *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CONSERVATIVE_RASTERIZATION_PROPERTIES_EXT:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceConservativeRasterizationPropertiesEXT));
        parse_VkPhysicalDeviceConservativeRasterizationPropertiesEXT(
            pNextObj, *((VkPhysicalDeviceConservativeRasterizationPropertiesEXT *)p));
        break;

    case VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT:
        p = s_globalMem.allocate(sizeof(VkPipelineRasterizationConservativeStateCreateInfoEXT));
        parse_VkPipelineRasterizationConservativeStateCreateInfoEXT(
            pNextObj, *((VkPipelineRasterizationConservativeStateCreateInfoEXT *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_ENABLE_FEATURES_EXT:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceDepthClipEnableFeaturesEXT));
        parse_VkPhysicalDeviceDepthClipEnableFeaturesEXT(pNextObj, *((VkPhysicalDeviceDepthClipEnableFeaturesEXT *)p));
        break;

    case VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_DEPTH_CLIP_STATE_CREATE_INFO_EXT:
        p = s_globalMem.allocate(sizeof(VkPipelineRasterizationDepthClipStateCreateInfoEXT));
        parse_VkPipelineRasterizationDepthClipStateCreateInfoEXT(
            pNextObj, *((VkPipelineRasterizationDepthClipStateCreateInfoEXT *)p));
        break;

    case VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT:
        p = s_globalMem.allocate(sizeof(VkDebugUtilsObjectNameInfoEXT));
        parse_VkDebugUtilsObjectNameInfoEXT(pNextObj, *((VkDebugUtilsObjectNameInfoEXT *)p));
        break;

    case VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT:
        p = s_globalMem.allocate(sizeof(VkDebugUtilsMessengerCreateInfoEXT));
        parse_VkDebugUtilsMessengerCreateInfoEXT(pNextObj, *((VkDebugUtilsMessengerCreateInfoEXT *)p));
        break;

    case VK_STRUCTURE_TYPE_SAMPLE_LOCATIONS_INFO_EXT:
        p = s_globalMem.allocate(sizeof(VkSampleLocationsInfoEXT));
        parse_VkSampleLocationsInfoEXT(pNextObj, *((VkSampleLocationsInfoEXT *)p));
        break;

    case VK_STRUCTURE_TYPE_RENDER_PASS_SAMPLE_LOCATIONS_BEGIN_INFO_EXT:
        p = s_globalMem.allocate(sizeof(VkRenderPassSampleLocationsBeginInfoEXT));
        parse_VkRenderPassSampleLocationsBeginInfoEXT(pNextObj, *((VkRenderPassSampleLocationsBeginInfoEXT *)p));
        break;

    case VK_STRUCTURE_TYPE_PIPELINE_SAMPLE_LOCATIONS_STATE_CREATE_INFO_EXT:
        p = s_globalMem.allocate(sizeof(VkPipelineSampleLocationsStateCreateInfoEXT));
        parse_VkPipelineSampleLocationsStateCreateInfoEXT(pNextObj,
                                                          *((VkPipelineSampleLocationsStateCreateInfoEXT *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLE_LOCATIONS_PROPERTIES_EXT:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceSampleLocationsPropertiesEXT));
        parse_VkPhysicalDeviceSampleLocationsPropertiesEXT(pNextObj,
                                                           *((VkPhysicalDeviceSampleLocationsPropertiesEXT *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BLEND_OPERATION_ADVANCED_FEATURES_EXT:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceBlendOperationAdvancedFeaturesEXT));
        parse_VkPhysicalDeviceBlendOperationAdvancedFeaturesEXT(
            pNextObj, *((VkPhysicalDeviceBlendOperationAdvancedFeaturesEXT *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BLEND_OPERATION_ADVANCED_PROPERTIES_EXT:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceBlendOperationAdvancedPropertiesEXT));
        parse_VkPhysicalDeviceBlendOperationAdvancedPropertiesEXT(
            pNextObj, *((VkPhysicalDeviceBlendOperationAdvancedPropertiesEXT *)p));
        break;

    case VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_ADVANCED_STATE_CREATE_INFO_EXT:
        p = s_globalMem.allocate(sizeof(VkPipelineColorBlendAdvancedStateCreateInfoEXT));
        parse_VkPipelineColorBlendAdvancedStateCreateInfoEXT(pNextObj,
                                                             *((VkPipelineColorBlendAdvancedStateCreateInfoEXT *)p));
        break;

    case VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT:
        p = s_globalMem.allocate(sizeof(VkDrmFormatModifierPropertiesListEXT));
        parse_VkDrmFormatModifierPropertiesListEXT(pNextObj, *((VkDrmFormatModifierPropertiesListEXT *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceImageDrmFormatModifierInfoEXT));
        parse_VkPhysicalDeviceImageDrmFormatModifierInfoEXT(pNextObj,
                                                            *((VkPhysicalDeviceImageDrmFormatModifierInfoEXT *)p));
        break;

    case VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT:
        p = s_globalMem.allocate(sizeof(VkImageDrmFormatModifierListCreateInfoEXT));
        parse_VkImageDrmFormatModifierListCreateInfoEXT(pNextObj, *((VkImageDrmFormatModifierListCreateInfoEXT *)p));
        break;

    case VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT:
        p = s_globalMem.allocate(sizeof(VkImageDrmFormatModifierExplicitCreateInfoEXT));
        parse_VkImageDrmFormatModifierExplicitCreateInfoEXT(pNextObj,
                                                            *((VkImageDrmFormatModifierExplicitCreateInfoEXT *)p));
        break;

    case VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_2_EXT:
        p = s_globalMem.allocate(sizeof(VkDrmFormatModifierPropertiesList2EXT));
        parse_VkDrmFormatModifierPropertiesList2EXT(pNextObj, *((VkDrmFormatModifierPropertiesList2EXT *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_VIEW_IMAGE_FORMAT_INFO_EXT:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceImageViewImageFormatInfoEXT));
        parse_VkPhysicalDeviceImageViewImageFormatInfoEXT(pNextObj,
                                                          *((VkPhysicalDeviceImageViewImageFormatInfoEXT *)p));
        break;

    case VK_STRUCTURE_TYPE_FILTER_CUBIC_IMAGE_VIEW_IMAGE_FORMAT_PROPERTIES_EXT:
        p = s_globalMem.allocate(sizeof(VkFilterCubicImageViewImageFormatPropertiesEXT));
        parse_VkFilterCubicImageViewImageFormatPropertiesEXT(pNextObj,
                                                             *((VkFilterCubicImageViewImageFormatPropertiesEXT *)p));
        break;

    case VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT:
        p = s_globalMem.allocate(sizeof(VkImportMemoryHostPointerInfoEXT));
        parse_VkImportMemoryHostPointerInfoEXT(pNextObj, *((VkImportMemoryHostPointerInfoEXT *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceExternalMemoryHostPropertiesEXT));
        parse_VkPhysicalDeviceExternalMemoryHostPropertiesEXT(pNextObj,
                                                              *((VkPhysicalDeviceExternalMemoryHostPropertiesEXT *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PCI_BUS_INFO_PROPERTIES_EXT:
        p = s_globalMem.allocate(sizeof(VkPhysicalDevicePCIBusInfoPropertiesEXT));
        parse_VkPhysicalDevicePCIBusInfoPropertiesEXT(pNextObj, *((VkPhysicalDevicePCIBusInfoPropertiesEXT *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceSubgroupSizeControlFeatures));
        parse_VkPhysicalDeviceSubgroupSizeControlFeatures(pNextObj,
                                                          *((VkPhysicalDeviceSubgroupSizeControlFeatures *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceSubgroupSizeControlProperties));
        parse_VkPhysicalDeviceSubgroupSizeControlProperties(pNextObj,
                                                            *((VkPhysicalDeviceSubgroupSizeControlProperties *)p));
        break;

    case VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO:
        p = s_globalMem.allocate(sizeof(VkPipelineShaderStageRequiredSubgroupSizeCreateInfo));
        parse_VkPipelineShaderStageRequiredSubgroupSizeCreateInfo(
            pNextObj, *((VkPipelineShaderStageRequiredSubgroupSizeCreateInfo *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_IMAGE_ATOMIC_INT64_FEATURES_EXT:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT));
        parse_VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT(
            pNextObj, *((VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceMemoryBudgetPropertiesEXT));
        parse_VkPhysicalDeviceMemoryBudgetPropertiesEXT(pNextObj, *((VkPhysicalDeviceMemoryBudgetPropertiesEXT *)p));
        break;

    case VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT:
        p = s_globalMem.allocate(sizeof(VkValidationFeaturesEXT));
        parse_VkValidationFeaturesEXT(pNextObj, *((VkValidationFeaturesEXT *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT));
        parse_VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT(
            pNextObj, *((VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_YCBCR_IMAGE_ARRAYS_FEATURES_EXT:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceYcbcrImageArraysFeaturesEXT));
        parse_VkPhysicalDeviceYcbcrImageArraysFeaturesEXT(pNextObj,
                                                          *((VkPhysicalDeviceYcbcrImageArraysFeaturesEXT *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceShaderAtomicFloatFeaturesEXT));
        parse_VkPhysicalDeviceShaderAtomicFloatFeaturesEXT(pNextObj,
                                                           *((VkPhysicalDeviceShaderAtomicFloatFeaturesEXT *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceExtendedDynamicStateFeaturesEXT));
        parse_VkPhysicalDeviceExtendedDynamicStateFeaturesEXT(pNextObj,
                                                              *((VkPhysicalDeviceExtendedDynamicStateFeaturesEXT *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DEMOTE_TO_HELPER_INVOCATION_FEATURES:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceShaderDemoteToHelperInvocationFeatures));
        parse_VkPhysicalDeviceShaderDemoteToHelperInvocationFeatures(
            pNextObj, *((VkPhysicalDeviceShaderDemoteToHelperInvocationFeatures *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TEXEL_BUFFER_ALIGNMENT_FEATURES_EXT:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceTexelBufferAlignmentFeaturesEXT));
        parse_VkPhysicalDeviceTexelBufferAlignmentFeaturesEXT(pNextObj,
                                                              *((VkPhysicalDeviceTexelBufferAlignmentFeaturesEXT *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TEXEL_BUFFER_ALIGNMENT_PROPERTIES:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceTexelBufferAlignmentProperties));
        parse_VkPhysicalDeviceTexelBufferAlignmentProperties(pNextObj,
                                                             *((VkPhysicalDeviceTexelBufferAlignmentProperties *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceRobustness2FeaturesEXT));
        parse_VkPhysicalDeviceRobustness2FeaturesEXT(pNextObj, *((VkPhysicalDeviceRobustness2FeaturesEXT *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_PROPERTIES_EXT:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceRobustness2PropertiesEXT));
        parse_VkPhysicalDeviceRobustness2PropertiesEXT(pNextObj, *((VkPhysicalDeviceRobustness2PropertiesEXT *)p));
        break;

    case VK_STRUCTURE_TYPE_SAMPLER_CUSTOM_BORDER_COLOR_CREATE_INFO_EXT:
        p = s_globalMem.allocate(sizeof(VkSamplerCustomBorderColorCreateInfoEXT));
        parse_VkSamplerCustomBorderColorCreateInfoEXT(pNextObj, *((VkSamplerCustomBorderColorCreateInfoEXT *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_PROPERTIES_EXT:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceCustomBorderColorPropertiesEXT));
        parse_VkPhysicalDeviceCustomBorderColorPropertiesEXT(pNextObj,
                                                             *((VkPhysicalDeviceCustomBorderColorPropertiesEXT *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceCustomBorderColorFeaturesEXT));
        parse_VkPhysicalDeviceCustomBorderColorFeaturesEXT(pNextObj,
                                                           *((VkPhysicalDeviceCustomBorderColorFeaturesEXT *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_YCBCR_2_PLANE_444_FORMATS_FEATURES_EXT:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceYcbcr2Plane444FormatsFeaturesEXT));
        parse_VkPhysicalDeviceYcbcr2Plane444FormatsFeaturesEXT(
            pNextObj, *((VkPhysicalDeviceYcbcr2Plane444FormatsFeaturesEXT *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_ROBUSTNESS_FEATURES:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceImageRobustnessFeatures));
        parse_VkPhysicalDeviceImageRobustnessFeatures(pNextObj, *((VkPhysicalDeviceImageRobustnessFeatures *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_4444_FORMATS_FEATURES_EXT:
        p = s_globalMem.allocate(sizeof(VkPhysicalDevice4444FormatsFeaturesEXT));
        parse_VkPhysicalDevice4444FormatsFeaturesEXT(pNextObj, *((VkPhysicalDevice4444FormatsFeaturesEXT *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_INPUT_DYNAMIC_STATE_FEATURES_EXT:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT));
        parse_VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT(
            pNextObj, *((VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT *)p));
        break;

    case VK_STRUCTURE_TYPE_EXPORT_FENCE_SCI_SYNC_INFO_NV:
        p = s_globalMem.allocate(sizeof(VkExportFenceSciSyncInfoNV));
        parse_VkExportFenceSciSyncInfoNV(pNextObj, *((VkExportFenceSciSyncInfoNV *)p));
        break;

    case VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_SCI_SYNC_INFO_NV:
        p = s_globalMem.allocate(sizeof(VkExportSemaphoreSciSyncInfoNV));
        parse_VkExportSemaphoreSciSyncInfoNV(pNextObj, *((VkExportSemaphoreSciSyncInfoNV *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SCI_SYNC_FEATURES_NV:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceExternalSciSyncFeaturesNV));
        parse_VkPhysicalDeviceExternalSciSyncFeaturesNV(pNextObj, *((VkPhysicalDeviceExternalSciSyncFeaturesNV *)p));
        break;

    case VK_STRUCTURE_TYPE_EXPORT_MEMORY_SCI_BUF_INFO_NV:
        p = s_globalMem.allocate(sizeof(VkExportMemorySciBufInfoNV));
        parse_VkExportMemorySciBufInfoNV(pNextObj, *((VkExportMemorySciBufInfoNV *)p));
        break;

    case VK_STRUCTURE_TYPE_IMPORT_MEMORY_SCI_BUF_INFO_NV:
        p = s_globalMem.allocate(sizeof(VkImportMemorySciBufInfoNV));
        parse_VkImportMemorySciBufInfoNV(pNextObj, *((VkImportMemorySciBufInfoNV *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_SCI_BUF_FEATURES_NV:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceExternalMemorySciBufFeaturesNV));
        parse_VkPhysicalDeviceExternalMemorySciBufFeaturesNV(pNextObj,
                                                             *((VkPhysicalDeviceExternalMemorySciBufFeaturesNV *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_2_FEATURES_EXT:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceExtendedDynamicState2FeaturesEXT));
        parse_VkPhysicalDeviceExtendedDynamicState2FeaturesEXT(
            pNextObj, *((VkPhysicalDeviceExtendedDynamicState2FeaturesEXT *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COLOR_WRITE_ENABLE_FEATURES_EXT:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceColorWriteEnableFeaturesEXT));
        parse_VkPhysicalDeviceColorWriteEnableFeaturesEXT(pNextObj,
                                                          *((VkPhysicalDeviceColorWriteEnableFeaturesEXT *)p));
        break;

    case VK_STRUCTURE_TYPE_PIPELINE_COLOR_WRITE_CREATE_INFO_EXT:
        p = s_globalMem.allocate(sizeof(VkPipelineColorWriteCreateInfoEXT));
        parse_VkPipelineColorWriteCreateInfoEXT(pNextObj, *((VkPipelineColorWriteCreateInfoEXT *)p));
        break;

    case VK_STRUCTURE_TYPE_APPLICATION_PARAMETERS_EXT:
        p = s_globalMem.allocate(sizeof(VkApplicationParametersEXT));
        parse_VkApplicationParametersEXT(pNextObj, *((VkApplicationParametersEXT *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SCI_SYNC_2_FEATURES_NV:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceExternalSciSync2FeaturesNV));
        parse_VkPhysicalDeviceExternalSciSync2FeaturesNV(pNextObj, *((VkPhysicalDeviceExternalSciSync2FeaturesNV *)p));
        break;

    case VK_STRUCTURE_TYPE_SEMAPHORE_SCI_SYNC_CREATE_INFO_NV:
        p = s_globalMem.allocate(sizeof(VkSemaphoreSciSyncCreateInfoNV));
        parse_VkSemaphoreSciSyncCreateInfoNV(pNextObj, *((VkSemaphoreSciSyncCreateInfoNV *)p));
        break;

    case VK_STRUCTURE_TYPE_DEVICE_SEMAPHORE_SCI_SYNC_POOL_RESERVATION_CREATE_INFO_NV:
        p = s_globalMem.allocate(sizeof(VkDeviceSemaphoreSciSyncPoolReservationCreateInfoNV));
        parse_VkDeviceSemaphoreSciSyncPoolReservationCreateInfoNV(
            pNextObj, *((VkDeviceSemaphoreSciSyncPoolReservationCreateInfoNV *)p));
        break;

    case VK_STRUCTURE_TYPE_SCREEN_BUFFER_FORMAT_PROPERTIES_QNX:
        p = s_globalMem.allocate(sizeof(VkScreenBufferFormatPropertiesQNX));
        parse_VkScreenBufferFormatPropertiesQNX(pNextObj, *((VkScreenBufferFormatPropertiesQNX *)p));
        break;

    case VK_STRUCTURE_TYPE_IMPORT_SCREEN_BUFFER_INFO_QNX:
        p = s_globalMem.allocate(sizeof(VkImportScreenBufferInfoQNX));
        parse_VkImportScreenBufferInfoQNX(pNextObj, *((VkImportScreenBufferInfoQNX *)p));
        break;

    case VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_QNX:
        p = s_globalMem.allocate(sizeof(VkExternalFormatQNX));
        parse_VkExternalFormatQNX(pNextObj, *((VkExternalFormatQNX *)p));
        break;

    case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_SCREEN_BUFFER_FEATURES_QNX:
        p = s_globalMem.allocate(sizeof(VkPhysicalDeviceExternalMemoryScreenBufferFeaturesQNX));
        parse_VkPhysicalDeviceExternalMemoryScreenBufferFeaturesQNX(
            pNextObj, *((VkPhysicalDeviceExternalMemoryScreenBufferFeaturesQNX *)p));
        break;

    default:
        break;
    }
    return p;
}

} //End of namespace vk_json_parser

#endif // _VULKAN_JSON_PARSER_HPP
