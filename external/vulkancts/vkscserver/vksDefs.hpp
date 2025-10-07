#ifndef _VKSDEFS_HPP
#define _VKSDEFS_HPP

/*-------------------------------------------------------------------------
 * Vulkan CTS Framework
 * --------------------
 *
 * Copyright (c) 2025 The Khronos Group Inc.
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
 * \file
 * \brief Additional defines that are not part of VkSC but are needed by vulkan_json_parser.hpp
 *-------------------------------------------------------------------------*/

#include "vkDefs.hpp"

namespace vk
{

#ifndef VK_NV_device_diagnostic_checkpoints
#define VK_NV_device_diagnostic_checkpoints 1
#define VK_NV_DEVICE_DIAGNOSTIC_CHECKPOINTS_SPEC_VERSION 2
#define VK_NV_DEVICE_DIAGNOSTIC_CHECKPOINTS_EXTENSION_NAME "VK_NV_device_diagnostic_checkpoints"

struct VkQueueFamilyCheckpointPropertiesNV
{
    VkStructureType sType;
    void *pNext;
    VkPipelineStageFlags checkpointExecutionStageMask;
};

struct VkCheckpointDataNV
{
    VkStructureType sType;
    void *pNext;
    VkPipelineStageFlagBits stage;
    void *pCheckpointMarker;
};

struct VkQueueFamilyCheckpointProperties2NV
{
    VkStructureType sType;
    void *pNext;
    VkPipelineStageFlags2 checkpointExecutionStageMask;
};

struct VkCheckpointData2NV
{
    VkStructureType sType;
    void *pNext;
    VkPipelineStageFlags2 stage;
    void *pCheckpointMarker;
};

#endif // VK_NV_device_diagnostic_checkpoints

} // namespace vk

#endif // _VKSDEFS_HPP
