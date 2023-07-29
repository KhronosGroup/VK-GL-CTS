#ifndef _VKTVIDEOSESSIONNVUTILS_HPP
#define _VKTVIDEOSESSIONNVUTILS_HPP
/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
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
 * \brief Video Session NV Utils
 *//*--------------------------------------------------------------------*/

#include "deUniquePtr.hpp"
#include "extNvidiaVideoParserIf.hpp"
#include "vkPlatform.hpp"

namespace vkt
{
namespace video
{

class IfcVulkanVideoDecodeParser
{
public:
    virtual bool parseByteStream(uint8_t *pData, int64_t size)                                        = 0;
    virtual bool initialize(NvidiaVulkanParserVideoDecodeClient *nvidiaVulkanParserVideoDecodeClient) = 0;
    virtual bool deinitialize(void)                                                                   = 0;

    virtual ~IfcVulkanVideoDecodeParser()
    {
    }
};

class IfcNvFunctions
{
public:
    virtual IfcVulkanVideoDecodeParser *createIfcVulkanVideoDecodeParser(
        VkVideoCodecOperationFlagBitsKHR codecOperation, const VkExtensionProperties *stdExtensionVersion) = 0;
    virtual ~IfcNvFunctions(){};
};

de::MovePtr<IfcNvFunctions> createIfcNvFunctions(const vk::Platform &platform);

} // namespace video
} // namespace vkt

#endif // _VKTVIDEOSESSIONNVUTILS_HPP
