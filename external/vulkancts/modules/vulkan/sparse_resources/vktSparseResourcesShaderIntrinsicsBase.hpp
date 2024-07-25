#ifndef _VKTSPARSERESOURCESSHADERINTRINSICSBASE_HPP
#define _VKTSPARSERESOURCESSHADERINTRINSICSBASE_HPP
/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \file  vktSparseResourcesShaderIntrinsicsBase.hpp
 * \brief Sparse Resources Shader Intrinsics Base Classes
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktSparseResourcesBase.hpp"
#include "vktSparseResourcesTestsUtil.hpp"

#include "vkDefs.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkRefUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkDebugReportUtil.hpp"
#include "tcuTextureUtil.hpp"

#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"
#include "deSharedPtr.hpp"

#include <string>
#include <vector>

#include <deMath.h>

namespace vkt
{
namespace sparse
{

enum
{
    MEMORY_BLOCK_BOUND      = 0u,
    MEMORY_BLOCK_NOT_BOUND  = 1u,
    MEMORY_BLOCK_TYPE_COUNT = 2u
};

enum
{
    MEMORY_BLOCK_BOUND_VALUE     = 1u,
    MEMORY_BLOCK_NOT_BOUND_VALUE = 2u
};

enum
{
    BINDING_IMAGE_SPARSE    = 0u,
    BINDING_IMAGE_TEXELS    = 1u,
    BINDING_IMAGE_RESIDENCY = 2u
};

enum SpirVFunction
{
    SPARSE_FETCH = 0u,
    SPARSE_READ,
    SPARSE_SAMPLE_EXPLICIT_LOD,
    SPARSE_SAMPLE_IMPLICIT_LOD,
    SPARSE_GATHER,
    SPARSE_SPIRV_FUNCTION_TYPE_LAST
};

std::string getOpTypeImageComponent(const tcu::TextureFormat &format);
std::string getOpTypeImageComponent(const vk::PlanarFormatDescription &description);
std::string getImageComponentTypeName(const tcu::TextureFormat &format);
std::string getImageComponentTypeName(const vk::PlanarFormatDescription &description);
std::string getImageComponentVec4TypeName(const tcu::TextureFormat &format);
std::string getImageComponentVec4TypeName(const vk::PlanarFormatDescription &description);
std::string getOpTypeImageSparse(const ImageType imageType, const tcu::TextureFormat &format,
                                 const std::string &componentType, const bool requiresSampler);
std::string getOpTypeImageSparse(const ImageType imageType, const vk::VkFormat format, const std::string &componentType,
                                 const bool requiresSampler);
std::string getOpTypeImageResidency(const ImageType imageType);

class SparseShaderIntrinsicsCaseBase : public TestCase
{
public:
    SparseShaderIntrinsicsCaseBase(tcu::TestContext &testCtx, const std::string &name, const SpirVFunction function,
                                   const ImageType imageType, const tcu::UVec3 &imageSize, const vk::VkFormat format,
                                   const std::string &operand)
        : TestCase(testCtx, name)
        , m_function(function)
        , m_imageType(imageType)
        , m_imageSize(imageSize)
        , m_format(format)
        , m_operand(operand)
    {
    }

    virtual void checkSupport(Context &context) const
    {
        const vk::InstanceInterface &instance     = context.getInstanceInterface();
        const vk::VkPhysicalDevice physicalDevice = context.getPhysicalDevice();

        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SHADER_RESOURCE_RESIDENCY);

        // Check if image size does not exceed device limits
        if (!isImageSizeSupported(instance, physicalDevice, m_imageType, m_imageSize))
            TCU_THROW(NotSupportedError, "Image size not supported for device");

        // Check if device supports sparse operations for image type
        if (!checkSparseSupportForImageType(instance, physicalDevice, m_imageType))
            TCU_THROW(NotSupportedError, "Sparse residency for image type is not supported");

        if ((m_operand.find("Nontemporal") != std::string::npos) && (context.getUsedApiVersion() < VK_API_VERSION_1_3))
            TCU_THROW(NotSupportedError, "Vulkan 1.3 or higher is required for this test to run");
    }

protected:
    const SpirVFunction m_function;
    const ImageType m_imageType;
    const tcu::UVec3 m_imageSize;
    const vk::VkFormat m_format;
    const std::string m_operand;
};

class SparseShaderIntrinsicsInstanceBase : public SparseResourcesBaseInstance
{
public:
    SparseShaderIntrinsicsInstanceBase(Context &context, const SpirVFunction function, const ImageType imageType,
                                       const tcu::UVec3 &imageSize, const vk::VkFormat format)
        : SparseResourcesBaseInstance(context)
        , m_function(function)
        , m_imageType(imageType)
        , m_imageSize(imageSize)
        , m_format(format)
        , m_residencyFormat(tcu::TextureFormat::R, tcu::TextureFormat::UNSIGNED_INT32)
    {
    }

    tcu::TestStatus iterate(void);

    virtual vk::VkImageUsageFlags imageSparseUsageFlags(void) const = 0;
    virtual vk::VkImageUsageFlags imageOutputUsageFlags(void) const = 0;

    virtual vk::VkQueueFlags getQueueFlags(void) const = 0;

    virtual void recordCommands(const vk::VkCommandBuffer commandBuffer, const vk::VkImageCreateInfo &imageSparseInfo,
                                const vk::VkImage imageSparse, const vk::VkImage imageTexels,
                                const vk::VkImage imageResidency) = 0;
    virtual void checkSupport(vk::VkImageCreateInfo imageSparseInfo) const;

protected:
    const SpirVFunction m_function;
    const ImageType m_imageType;
    const tcu::UVec3 m_imageSize;
    const vk::VkFormat m_format;
    const tcu::TextureFormat m_residencyFormat;

    typedef de::SharedPtr<vk::Unique<vk::VkPipeline>> SharedVkPipeline;
    std::vector<SharedVkPipeline> pipelines;
    vk::Move<vk::VkPipelineLayout> pipelineLayout;

    typedef de::SharedPtr<vk::Unique<vk::VkImageView>> SharedVkImageView;
    std::vector<SharedVkImageView> imageSparseViews;
    std::vector<SharedVkImageView> imageTexelsViews;
    std::vector<SharedVkImageView> imageResidencyViews;

    vk::Move<vk::VkDescriptorPool> descriptorPool;

    typedef de::SharedPtr<vk::Unique<vk::VkDescriptorSet>> SharedVkDescriptorSet;
    std::vector<SharedVkDescriptorSet> descriptorSets;
};

} // namespace sparse
} // namespace vkt

#endif // _VKTSPARSERESOURCESSHADERINTRINSICSBASE_HPP
