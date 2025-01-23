/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
 * Copyright (c) 2023 LunarG, Inc.
 * Copyright (c) 2023 Nintendo
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
 * \brief Tests Verifying Graphics Pipeline Libraries
 *//*--------------------------------------------------------------------*/

#include "vktPipelineLibraryTests.hpp"

#include "tcuTextureUtil.hpp"
#include "vkDefs.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkRayTracingUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "tcuCommandLine.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTestLog.hpp"
#include "tcuRGBA.hpp"

#include "../draw/vktDrawCreateInfoUtil.hpp"
#include "deMath.h"
#include "deRandom.hpp"
#include "deClock.h"

#include <vector>
#include <chrono>
#include <set>
#include <any>
#include <limits>

namespace vkt
{
namespace pipeline
{
namespace
{
using namespace vk;
using namespace vkt;
using namespace tcu;

static const uint32_t RENDER_SIZE_WIDTH  = 16u;
static const uint32_t RENDER_SIZE_HEIGHT = 16u;
static const VkColorComponentFlags COLOR_COMPONENTS_NO_RED =
    VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
static const VkColorComponentFlags ALL_COLOR_COMPONENTS =
    VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
static const int numClipDistances                                                   = 5;
static const int numCullDistances                                                   = 3;
static const VkGraphicsPipelineLibraryFlagBitsEXT GRAPHICS_PIPELINE_LIBRARY_FLAGS[] = {
    VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT,
    VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT,
    VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT,
    VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT,
};
static const VkGraphicsPipelineLibraryFlagsEXT ALL_GRAPHICS_PIPELINE_LIBRARY_FLAGS =
    static_cast<VkGraphicsPipelineLibraryFlagsEXT>(VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT) |
    static_cast<VkGraphicsPipelineLibraryFlagsEXT>(VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT) |
    static_cast<VkGraphicsPipelineLibraryFlagsEXT>(VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT) |
    static_cast<VkGraphicsPipelineLibraryFlagsEXT>(VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT);

struct PipelineTreeNode
{
    int32_t parentIndex;
    uint32_t shaderCount;
};

typedef std::vector<PipelineTreeNode> PipelineTreeConfiguration;

struct TestParams
{
    PipelineTreeConfiguration pipelineTreeConfiguration;
    bool optimize;
    bool delayedShaderCreate;
    bool useMaintenance5;
};

struct RuntimePipelineTreeNode
{
    int32_t parentIndex;
    VkGraphicsPipelineLibraryFlagsEXT graphicsPipelineLibraryFlags;
    VkGraphicsPipelineLibraryFlagsEXT subtreeGraphicsPipelineLibraryFlags;
    Move<VkPipeline> pipeline;
    std::vector<VkPipeline> pipelineLibraries;
    // We need to track the linked libraries too, included in VkPipelineLibraryCreateInfoKHR->pLibraries
    std::vector<VkGraphicsPipelineLibraryFlagsEXT> linkedLibraryFlags;
};

typedef std::vector<RuntimePipelineTreeNode> RuntimePipelineTreeConfiguration;

inline UVec4 ivec2uvec(const IVec4 &ivec)
{
    return UVec4{
        static_cast<uint32_t>(ivec[0]),
        static_cast<uint32_t>(ivec[1]),
        static_cast<uint32_t>(ivec[2]),
        static_cast<uint32_t>(ivec[3]),
    };
}

inline std::string getTestName(const PipelineTreeConfiguration &pipelineTreeConfiguration)
{
    std::string result;
    int level = pipelineTreeConfiguration[0].parentIndex;

    for (const auto &node : pipelineTreeConfiguration)
    {
        if (level != node.parentIndex)
        {
            DE_ASSERT(level < node.parentIndex);

            result += '_';

            level = node.parentIndex;
        }

        result += de::toString(node.shaderCount);
    }

    return result;
}

inline VkPipelineCreateFlags calcPipelineCreateFlags(bool optimize, bool buildLibrary)
{
    VkPipelineCreateFlags result = 0;

    if (buildLibrary)
        result |= static_cast<VkPipelineCreateFlags>(VK_PIPELINE_CREATE_LIBRARY_BIT_KHR);

    if (optimize)
    {
        if (buildLibrary)
            result |= static_cast<VkPipelineCreateFlags>(VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT);
        else
            result |= static_cast<VkPipelineCreateFlags>(VK_PIPELINE_CREATE_LINK_TIME_OPTIMIZATION_BIT_EXT);
    }

    return result;
}

inline VkRenderPass getRenderPass(VkGraphicsPipelineLibraryFlagsEXT subset, VkRenderPass renderPass)
{
    static const VkGraphicsPipelineLibraryFlagsEXT subsetRequiresRenderPass =
        static_cast<VkGraphicsPipelineLibraryFlagsEXT>(VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT) |
        static_cast<VkGraphicsPipelineLibraryFlagsEXT>(VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT) |
        static_cast<VkGraphicsPipelineLibraryFlagsEXT>(VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT);
    if ((subsetRequiresRenderPass & subset) != 0)
        return renderPass;

    return VK_NULL_HANDLE;
}

inline VkGraphicsPipelineLibraryCreateInfoEXT makeGraphicsPipelineLibraryCreateInfo(
    const VkGraphicsPipelineLibraryFlagsEXT flags)
{
    const VkGraphicsPipelineLibraryCreateInfoEXT graphicsPipelineLibraryCreateInfo = {
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT, //  VkStructureType sType;
        nullptr,                                                     //  void* pNext;
        flags,                                                       //  VkGraphicsPipelineLibraryFlagsEXT flags;
    };

    return graphicsPipelineLibraryCreateInfo;
}

inline VkPipelineLibraryCreateInfoKHR makePipelineLibraryCreateInfo(const std::vector<VkPipeline> &pipelineLibraries)
{
    const uint32_t libraryCount                                    = static_cast<uint32_t>(pipelineLibraries.size());
    const VkPipeline *libraries                                    = de::dataOrNull(pipelineLibraries);
    const VkPipelineLibraryCreateInfoKHR pipelineLibraryCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR, //  VkStructureType sType;
        nullptr,                                            //  const void* pNext;
        libraryCount,                                       //  uint32_t libraryCount;
        libraries,                                          //  const VkPipeline* pLibraries;
    };

    return pipelineLibraryCreateInfo;
}

inline std::string getGraphicsPipelineLibraryFlagsString(const VkGraphicsPipelineLibraryFlagsEXT flags)
{
    std::string result;

    if ((flags & VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT) != 0)
        result += "VERTEX_INPUT_INTERFACE ";
    if ((flags & VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT) != 0)
        result += "PRE_RASTERIZATION_SHADERS ";
    if ((flags & VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT) != 0)
        result += "FRAGMENT_SHADER ";
    if ((flags & VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT) != 0)
        result += "FRAGMENT_OUTPUT_INTERFACE ";

    if (!result.empty())
        result.resize(result.size() - 1);

    return result;
};

VkImageCreateInfo makeColorImageCreateInfo(const VkFormat format, const uint32_t width, const uint32_t height)
{
    const VkImageUsageFlags usage     = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    const VkImageCreateInfo imageInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, //  VkStructureType sType;
        nullptr,                             //  const void* pNext;
        (VkImageCreateFlags)0,               //  VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                    //  VkImageType imageType;
        format,                              //  VkFormat format;
        makeExtent3D(width, height, 1),      //  VkExtent3D extent;
        1u,                                  //  uint32_t mipLevels;
        1u,                                  //  uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,               //  VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,             //  VkImageTiling tiling;
        usage,                               //  VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           //  VkSharingMode sharingMode;
        0u,                                  //  uint32_t queueFamilyIndexCount;
        nullptr,                             //  const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,           //  VkImageLayout initialLayout;
    };

    return imageInfo;
}

VkImageViewCreateInfo makeImageViewCreateInfo(VkImage image, VkFormat format, VkImageAspectFlags aspectMask)
{
    const VkComponentMapping components = {
        VK_COMPONENT_SWIZZLE_R,
        VK_COMPONENT_SWIZZLE_G,
        VK_COMPONENT_SWIZZLE_B,
        VK_COMPONENT_SWIZZLE_A,
    };
    const VkImageSubresourceRange subresourceRange = {
        aspectMask, //  VkImageAspectFlags aspectMask;
        0,          //  uint32_t baseMipLevel;
        1,          //  uint32_t levelCount;
        0,          //  uint32_t baseArrayLayer;
        1,          //  uint32_t layerCount;
    };
    const VkImageViewCreateInfo result = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, //  VkStructureType sType;
        nullptr,                                  //  const void* pNext;
        0u,                                       //  VkImageViewCreateFlags flags;
        image,                                    //  VkImage image;
        VK_IMAGE_VIEW_TYPE_2D,                    //  VkImageViewType viewType;
        format,                                   //  VkFormat format;
        components,                               //  VkComponentMapping components;
        subresourceRange,                         //  VkImageSubresourceRange subresourceRange;
    };

    return result;
}

VkImageCreateInfo makeDepthImageCreateInfo(const VkFormat format, const uint32_t width, const uint32_t height)
{
    const VkImageUsageFlags usage =
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    const VkImageCreateInfo imageInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, //  VkStructureType sType;
        nullptr,                             //  const void* pNext;
        (VkImageCreateFlags)0,               //  VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                    //  VkImageType imageType;
        format,                              //  VkFormat format;
        makeExtent3D(width, height, 1),      //  VkExtent3D extent;
        1u,                                  //  uint32_t mipLevels;
        1u,                                  //  uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,               //  VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,             //  VkImageTiling tiling;
        usage,                               //  VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           //  VkSharingMode sharingMode;
        0u,                                  //  uint32_t queueFamilyIndexCount;
        nullptr,                             //  const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,           //  VkImageLayout initialLayout;
    };

    return imageInfo;
}

const VkFramebufferCreateInfo makeFramebufferCreateInfo(const VkRenderPass renderPass, const uint32_t attachmentCount,
                                                        const VkImageView *attachments, const uint32_t width,
                                                        const uint32_t height)
{
    const VkFramebufferCreateInfo result = {
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, //  VkStructureType sType;
        nullptr,                                   //  const void* pNext;
        0,                                         //  VkFramebufferCreateFlags flags;
        renderPass,                                //  VkRenderPass renderPass;
        attachmentCount,                           //  uint32_t attachmentCount;
        attachments,                               //  const VkImageView* pAttachments;
        width,                                     //  uint32_t width;
        height,                                    //  uint32_t height;
        1,                                         //  uint32_t layers;
    };

    return result;
}

const VkPipelineMultisampleStateCreateInfo makePipelineMultisampleStateCreateInfo(void)
{
    const VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, //  VkStructureType sType;
        nullptr,                                                  //  const void* pNext;
        0u,                                                       //  VkPipelineMultisampleStateCreateFlags flags;
        VK_SAMPLE_COUNT_1_BIT,                                    //  VkSampleCountFlagBits rasterizationSamples;
        false,                                                    //  VkBool32 sampleShadingEnable;
        0.0f,                                                     //  float minSampleShading;
        nullptr,                                                  //  const VkSampleMask* pSampleMask;
        false,                                                    //  VkBool32 alphaToCoverageEnable;
        false,                                                    //  VkBool32 alphaToOneEnable;
    };

    return pipelineMultisampleStateCreateInfo;
}

class GraphicsPipelineCreateInfo : public ::vkt::Draw::PipelineCreateInfo
{
public:
    GraphicsPipelineCreateInfo(vk::VkPipelineLayout _layout, vk::VkRenderPass _renderPass, int _subpass,
                               vk::VkPipelineCreateFlags _flags)
        : ::vkt::Draw::PipelineCreateInfo(_layout, _renderPass, _subpass, _flags)
        , m_vertexInputBindingDescription()
        , m_vertexInputAttributeDescription()
        , m_shaderModuleCreateInfoCount(0)
        , m_shaderModuleCreateInfo{initVulkanStructure(), initVulkanStructure()}
        , m_pipelineShaderStageCreateInfo()
        , m_vertModule()
        , m_fragModule()
    {
    }

    VkVertexInputBindingDescription m_vertexInputBindingDescription;
    VkVertexInputAttributeDescription m_vertexInputAttributeDescription;
    uint32_t m_shaderModuleCreateInfoCount;
    VkShaderModuleCreateInfo m_shaderModuleCreateInfo[2];
    std::vector<VkPipelineShaderStageCreateInfo> m_pipelineShaderStageCreateInfo;
    Move<VkShaderModule> m_vertModule;
    Move<VkShaderModule> m_fragModule;
    Move<VkShaderModule> m_meshModule;
};

void updateVertexInputInterface(Context &context, GraphicsPipelineCreateInfo &graphicsPipelineCreateInfo,
                                VkPrimitiveTopology topology    = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                                uint32_t vertexDescriptionCount = 1u)
{
    DE_UNREF(context);

    graphicsPipelineCreateInfo.m_vertexInputBindingDescription = {
        0u,                          //  uint32_t binding;
        sizeof(tcu::Vec4),           //  uint32_t strideInBytes;
        VK_VERTEX_INPUT_RATE_VERTEX, //  VkVertexInputRate inputRate;
    };
    graphicsPipelineCreateInfo.m_vertexInputAttributeDescription = {
        0u,                            //  uint32_t location;
        0u,                            //  uint32_t binding;
        VK_FORMAT_R32G32B32A32_SFLOAT, //  VkFormat format;
        0u                             //  uint32_t offsetInBytes;
    };

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                   // const void* pNext;
        0u,                                                        // VkPipelineVertexInputStateCreateFlags flags;
        vertexDescriptionCount,                                    // uint32_t vertexBindingDescriptionCount;
        &graphicsPipelineCreateInfo
             .m_vertexInputBindingDescription, // const VkVertexInputBindingDescription* pVertexBindingDescriptions;
        vertexDescriptionCount,                // uint32_t vertexAttributeDescriptionCount;
        &graphicsPipelineCreateInfo
             .m_vertexInputAttributeDescription, // const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
    };
    const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                     // const void* pNext;
        0u,                                                          // VkPipelineInputAssemblyStateCreateFlags flags;
        topology,                                                    // VkPrimitiveTopology topology;
        VK_FALSE,                                                    // VkBool32 primitiveRestartEnable;
    };

    graphicsPipelineCreateInfo.addState(vertexInputStateCreateInfo);
    graphicsPipelineCreateInfo.addState(inputAssemblyStateCreateInfo);
}

void updatePreRasterization(Context &context, GraphicsPipelineCreateInfo &graphicsPipelineCreateInfo,
                            bool delayedShaderCreate, bool useDynamicViewPort = false, bool useMeshShader = false,
                            VkPolygonMode polygonMode                      = VK_POLYGON_MODE_FILL,
                            const VkSpecializationInfo *specializationInfo = nullptr)
{
    const std::string shaderName      = (useMeshShader ? "mesh" : "vert");
    const ProgramBinary &shaderBinary = context.getBinaryCollection().get(shaderName);
    VkShaderModuleCreateInfo &shaderModuleCreateInfo =
        graphicsPipelineCreateInfo.m_shaderModuleCreateInfo[graphicsPipelineCreateInfo.m_shaderModuleCreateInfoCount];

    DE_ASSERT(graphicsPipelineCreateInfo.m_shaderModuleCreateInfoCount <
              DE_LENGTH_OF_ARRAY(graphicsPipelineCreateInfo.m_shaderModuleCreateInfo));

    shaderModuleCreateInfo = {
        VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, //  VkStructureType sType;
        nullptr,                                     //  const void* pNext;
        0u,                                          //  VkShaderModuleCreateFlags flags;
        (uintptr_t)shaderBinary.getSize(),           //  uintptr_t codeSize;
        (uint32_t *)shaderBinary.getBinary(),        //  const uint32_t* pCode;
    };

    if (!delayedShaderCreate)
    {
        const DeviceInterface &vk = context.getDeviceInterface();
        const VkDevice device     = context.getDevice();

        Move<VkShaderModule> shaderMod = createShaderModule(vk, device, &shaderModuleCreateInfo);
        if (useMeshShader)
            graphicsPipelineCreateInfo.m_meshModule = shaderMod;
        else
            graphicsPipelineCreateInfo.m_vertModule = shaderMod;
    }

    const void *pNext = delayedShaderCreate ? &shaderModuleCreateInfo : nullptr;
    const VkShaderModule shaderModule =
        delayedShaderCreate ?
            VK_NULL_HANDLE :
            (useMeshShader ? *graphicsPipelineCreateInfo.m_meshModule : *graphicsPipelineCreateInfo.m_vertModule);
    const VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType sType;
        pNext,                                               // const void* pNext;
        0u,                                                  // VkPipelineShaderStageCreateFlags flags;
        (useMeshShader ? VK_SHADER_STAGE_MESH_BIT_EXT        // VkShaderStageFlagBits stage;
                         :
                         VK_SHADER_STAGE_VERTEX_BIT),
        shaderModule,      // VkShaderModule module;
        "main",            // const char* pName;
        specializationInfo // const VkSpecializationInfo* pSpecializationInfo;
    };

    shaderBinary.setUsed();

    // Within the VkPipelineLayout, all    bindings that affect the specified shader stages
    const VkViewport viewport = makeViewport(RENDER_SIZE_WIDTH, RENDER_SIZE_HEIGHT);
    const VkRect2D scissor    = makeRect2D(3 * RENDER_SIZE_WIDTH / 4, RENDER_SIZE_HEIGHT);
    const VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                               // const void* pNext;
        0u,                                                    // VkPipelineViewportStateCreateFlags flags;
        1u,                                                    // uint32_t viewportCount;
        &viewport,                                             // const VkViewport* pViewports;
        1u,                                                    // uint32_t scissorCount;
        &scissor                                               // const VkRect2D* pScissors;
    };
    std::vector<VkDynamicState> dynamicStates                   = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    const VkPipelineDynamicStateCreateInfo pipelineDynamicState = {
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                              // const void* pNext;
        0u,                                                   // VkPipelineDynamicStateCreateFlags flags;
        de::sizeU32(dynamicStates),                           // uint32_t dynamicStateCount;
        de::dataOrNull(dynamicStates)                         // const VkDynamicState* pDynamicStates;
    };
    const VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                    // const void* pNext;
        0u,                                                         // VkPipelineRasterizationStateCreateFlags flags;
        VK_FALSE,                                                   // VkBool32 depthClampEnable;
        VK_FALSE,                                                   // VkBool32 rasterizerDiscardEnable;
        polygonMode,                                                // VkPolygonMode polygonMode;
        VK_CULL_MODE_NONE,                                          // VkCullModeFlags cullMode;
        VK_FRONT_FACE_COUNTER_CLOCKWISE,                            // VkFrontFace frontFace;
        VK_FALSE,                                                   // VkBool32 depthBiasEnable;
        0.0f,                                                       // float depthBiasConstantFactor;
        0.0f,                                                       // float depthBiasClamp;
        0.0f,                                                       // float depthBiasSlopeFactor;
        1.0f,                                                       // float lineWidth;
    };

    graphicsPipelineCreateInfo.m_shaderModuleCreateInfoCount++;

    graphicsPipelineCreateInfo.addShader(pipelineShaderStageCreateInfo);
    graphicsPipelineCreateInfo.addState(pipelineViewportStateCreateInfo);
    graphicsPipelineCreateInfo.addState(pipelineRasterizationStateCreateInfo);

    if (useDynamicViewPort)
        graphicsPipelineCreateInfo.addState(pipelineDynamicState);
}

void updatePostRasterization(Context &context, GraphicsPipelineCreateInfo &graphicsPipelineCreateInfo,
                             bool delayedShaderCreate, bool enableDepth = true,
                             const VkSpecializationInfo *specializationInfo = nullptr)
{
    const ProgramBinary &shaderBinary = context.getBinaryCollection().get("frag");
    VkShaderModuleCreateInfo &shaderModuleCreateInfo =
        graphicsPipelineCreateInfo.m_shaderModuleCreateInfo[graphicsPipelineCreateInfo.m_shaderModuleCreateInfoCount];

    DE_ASSERT(graphicsPipelineCreateInfo.m_shaderModuleCreateInfoCount <
              DE_LENGTH_OF_ARRAY(graphicsPipelineCreateInfo.m_shaderModuleCreateInfo));

    shaderModuleCreateInfo = {
        VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, //  VkStructureType sType;
        nullptr,                                     //  const void* pNext;
        0u,                                          //  VkShaderModuleCreateFlags flags;
        (uintptr_t)shaderBinary.getSize(),           //  uintptr_t codeSize;
        (uint32_t *)shaderBinary.getBinary(),        //  const uint32_t* pCode;
    };

    if (!delayedShaderCreate)
    {
        const DeviceInterface &vk = context.getDeviceInterface();
        const VkDevice device     = context.getDevice();

        graphicsPipelineCreateInfo.m_fragModule = createShaderModule(vk, device, &shaderModuleCreateInfo);
    }

    const void *pNext                 = delayedShaderCreate ? &shaderModuleCreateInfo : nullptr;
    const VkShaderModule shaderModule = delayedShaderCreate ? VK_NULL_HANDLE : *graphicsPipelineCreateInfo.m_fragModule;
    const VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType sType;
        pNext,                                               // const void* pNext;
        0u,                                                  // VkPipelineShaderStageCreateFlags flags;
        VK_SHADER_STAGE_FRAGMENT_BIT,                        // VkShaderStageFlagBits stage;
        shaderModule,                                        // VkShaderModule module;
        "main",                                              // const char* pName;
        specializationInfo                                   // const VkSpecializationInfo* pSpecializationInfo;
    };

    shaderBinary.setUsed();

    // Within the VkPipelineLayout, all bindings that affect the fragment shader stage

    const VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, //  VkStructureType sType;
        nullptr,                                                    //  const void* pNext;
        0u,                                                         //  VkPipelineDepthStencilStateCreateFlags flags;
        enableDepth,                                                //  VkBool32 depthTestEnable;
        enableDepth,                                                //  VkBool32 depthWriteEnable;
        VK_COMPARE_OP_LESS_OR_EQUAL,                                //  VkCompareOp depthCompareOp;
        VK_FALSE,                                                   //  VkBool32 depthBoundsTestEnable;
        VK_FALSE,                                                   //  VkBool32 stencilTestEnable;
        {
            //  VkStencilOpState front;
            VK_STENCIL_OP_KEEP,  // VkStencilOp failOp;
            VK_STENCIL_OP_KEEP,  // VkStencilOp passOp;
            VK_STENCIL_OP_KEEP,  // VkStencilOp depthFailOp;
            VK_COMPARE_OP_NEVER, // VkCompareOp compareOp;
            0u,                  // uint32_t compareMask;
            0u,                  // uint32_t writeMask;
            0u,                  // uint32_t reference;
        },
        {
            //  VkStencilOpState back;
            VK_STENCIL_OP_KEEP,  // VkStencilOp failOp;
            VK_STENCIL_OP_KEEP,  // VkStencilOp passOp;
            VK_STENCIL_OP_KEEP,  // VkStencilOp depthFailOp;
            VK_COMPARE_OP_NEVER, // VkCompareOp compareOp;
            0u,                  // uint32_t compareMask;
            0u,                  // uint32_t writeMask;
            0u,                  // uint32_t reference;
        },
        0.0f, //  float minDepthBounds;
        1.0f, //  float maxDepthBounds;
    };

    graphicsPipelineCreateInfo.m_shaderModuleCreateInfoCount++;
    graphicsPipelineCreateInfo.addShader(pipelineShaderStageCreateInfo);

    DE_ASSERT(graphicsPipelineCreateInfo.pDepthStencilState == nullptr);
    graphicsPipelineCreateInfo.addState(pipelineDepthStencilStateCreateInfo);

    if (graphicsPipelineCreateInfo.pMultisampleState == nullptr)
    {
        const VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo =
            makePipelineMultisampleStateCreateInfo();

        graphicsPipelineCreateInfo.addState(pipelineMultisampleStateCreateInfo);
    }
}

void updateFragmentOutputInterface(Context &context, GraphicsPipelineCreateInfo &graphicsPipelineCreateInfo,
                                   VkColorComponentFlags colorWriteMask = COLOR_COMPONENTS_NO_RED)
{
    DE_UNREF(context);

    // Number of blend attachments must equal the number of color attachments during any subpass.
    const VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState = {
        VK_FALSE,             // VkBool32 blendEnable;
        VK_BLEND_FACTOR_ONE,  // VkBlendFactor srcColorBlendFactor;
        VK_BLEND_FACTOR_ZERO, // VkBlendFactor dstColorBlendFactor;
        VK_BLEND_OP_ADD,      // VkBlendOp colorBlendOp;
        VK_BLEND_FACTOR_ONE,  // VkBlendFactor srcAlphaBlendFactor;
        VK_BLEND_FACTOR_ZERO, // VkBlendFactor dstAlphaBlendFactor;
        VK_BLEND_OP_ADD,      // VkBlendOp alphaBlendOp;
        colorWriteMask,       // VkColorComponentFlags colorWriteMask;
    };
    const VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                  // const void* pNext;
        (VkPipelineColorBlendStateCreateFlags)0,                  // VkPipelineColorBlendStateCreateFlags flags;
        VK_FALSE,                                                 // VkBool32 logicOpEnable;
        VK_LOGIC_OP_COPY,                                         // VkLogicOp logicOp;
        1u,                                                       // uint32_t attachmentCount;
        &pipelineColorBlendAttachmentState, // const VkPipelineColorBlendAttachmentState* pAttachments;
        {0.0f, 0.0f, 0.0f, 0.0f},           // float blendConstants[4];
    };

    graphicsPipelineCreateInfo.addState(pipelineColorBlendStateCreateInfo);

    if (graphicsPipelineCreateInfo.pMultisampleState == nullptr)
    {
        const VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo =
            makePipelineMultisampleStateCreateInfo();

        graphicsPipelineCreateInfo.addState(pipelineMultisampleStateCreateInfo);
    }
}

/*
    To test that each of graphics pipeline libraries have influence on final pipeline
    the functions have following features:

    updateVertexInputInterface
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
        VK_VERTEX_INPUT_RATE_VERTEX
        Z is read from uniform and written in shader

    updatePreRasterization
        VkRect2D scissor = makeRect2D(3 * RENDER_SIZE_WIDTH / 4, RENDER_SIZE_HEIGHT);

    updatePostRasterization
        Fragment shader top and bottom colors read from uniform buffer

    updateFragmentOutputInterface
        Cut off red component
*/

class PipelineLibraryTestInstance : public TestInstance
{
public:
    PipelineLibraryTestInstance(Context &context, const TestParams &data);
    ~PipelineLibraryTestInstance(void);
    tcu::TestStatus iterate(void);

protected:
    de::MovePtr<BufferWithMemory> makeVertexBuffer(void);
    de::MovePtr<BufferWithMemory> makeZCoordBuffer(void);
    de::MovePtr<BufferWithMemory> makePaletteBuffer(void);
    Move<VkDescriptorPool> createDescriptorPool(void);
    Move<VkDescriptorSetLayout> createDescriptorSetLayout(const VkBuffer vertShaderBuffer,
                                                          const VkBuffer fragShaderBuffer);
    Move<VkDescriptorSet> createDescriptorSet(const VkDescriptorPool pool, const VkDescriptorSetLayout layout,
                                              const VkBuffer vertShaderBuffer, const VkBuffer fragShaderBuffer);
    bool verifyColorImage(const tcu::ConstPixelBufferAccess &pba);
    bool verifyDepthImage(const tcu::ConstPixelBufferAccess &pba);
    bool runTest(RuntimePipelineTreeConfiguration &runtimePipelineTreeConfiguration, const bool optimize,
                 const bool delayedShaderCreate);

private:
    TestParams m_data;
    std::vector<tcu::Vec4> m_vertexData;
    std::vector<tcu::Vec4> m_paletteData;
    std::vector<tcu::Vec4> m_zCoordData;
};

PipelineLibraryTestInstance::PipelineLibraryTestInstance(Context &context, const TestParams &data)
    : vkt::TestInstance(context)
    , m_data(data)
    , m_vertexData()
    , m_paletteData()
{
    m_vertexData = {
        {-1.0f, -1.0f, 0.0f, 1.0f}, {+1.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, +1.0f, 0.5f, 1.0f},
        {-1.0f, +1.0f, 0.5f, 1.0f}, {+1.0f, -1.0f, 0.5f, 1.0f}, {+1.0f, +1.0f, 1.0f, 1.0f},
    };
    m_paletteData = {
        {0.25f, 1.0f, 0.0f, 1.0f},
        {0.75f, 0.0f, 1.0f, 1.0f},
    };
    m_zCoordData = {
        {0.25f, 0.75f, 0.0f, 1.0f},
    };
}

PipelineLibraryTestInstance::~PipelineLibraryTestInstance(void)
{
}

de::MovePtr<BufferWithMemory> PipelineLibraryTestInstance::makeVertexBuffer(void)
{
    const DeviceInterface &vk                 = m_context.getDeviceInterface();
    const VkDevice device                     = m_context.getDevice();
    Allocator &allocator                      = m_context.getDefaultAllocator();
    const size_t bufferDataSize               = de::dataSize(m_vertexData);
    const VkBufferCreateInfo bufferCreateInfo = makeBufferCreateInfo(bufferDataSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    de::MovePtr<BufferWithMemory> buffer      = de::MovePtr<BufferWithMemory>(
        new BufferWithMemory(vk, device, allocator, bufferCreateInfo, MemoryRequirement::HostVisible));

    deMemcpy(buffer->getAllocation().getHostPtr(), m_vertexData.data(), bufferDataSize);
    flushAlloc(vk, device, buffer->getAllocation());

    return buffer;
}

de::MovePtr<BufferWithMemory> PipelineLibraryTestInstance::makeZCoordBuffer(void)
{
    const DeviceInterface &vk   = m_context.getDeviceInterface();
    const VkDevice device       = m_context.getDevice();
    Allocator &allocator        = m_context.getDefaultAllocator();
    const size_t bufferDataSize = de::dataSize(m_zCoordData);
    const VkBufferCreateInfo bufferCreateInfo =
        makeBufferCreateInfo(bufferDataSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    de::MovePtr<BufferWithMemory> buffer = de::MovePtr<BufferWithMemory>(
        new BufferWithMemory(vk, device, allocator, bufferCreateInfo, MemoryRequirement::HostVisible));

    deMemcpy(buffer->getAllocation().getHostPtr(), m_zCoordData.data(), bufferDataSize);
    flushAlloc(vk, device, buffer->getAllocation());

    return buffer;
}

de::MovePtr<BufferWithMemory> PipelineLibraryTestInstance::makePaletteBuffer(void)
{
    const DeviceInterface &vk   = m_context.getDeviceInterface();
    const VkDevice device       = m_context.getDevice();
    Allocator &allocator        = m_context.getDefaultAllocator();
    const size_t bufferDataSize = de::dataSize(m_paletteData);
    const VkBufferCreateInfo bufferCreateInfo =
        makeBufferCreateInfo(bufferDataSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    de::MovePtr<BufferWithMemory> buffer = de::MovePtr<BufferWithMemory>(
        new BufferWithMemory(vk, device, allocator, bufferCreateInfo, MemoryRequirement::HostVisible));

    deMemcpy(buffer->getAllocation().getHostPtr(), m_paletteData.data(), bufferDataSize);
    flushAlloc(vk, device, buffer->getAllocation());

    return buffer;
}

Move<VkDescriptorPool> PipelineLibraryTestInstance::createDescriptorPool(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();

    return DescriptorPoolBuilder()
        .addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4)
        .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 3);
}

Move<VkDescriptorSetLayout> PipelineLibraryTestInstance::createDescriptorSetLayout(const VkBuffer vertShaderBuffer,
                                                                                   const VkBuffer fragShaderBuffer)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();
    DescriptorSetLayoutBuilder builder;

    if (vertShaderBuffer != VK_NULL_HANDLE)
        builder.addIndexedBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1u, VK_SHADER_STAGE_VERTEX_BIT, 0u, nullptr);

    if (fragShaderBuffer != VK_NULL_HANDLE)
        builder.addIndexedBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1u, VK_SHADER_STAGE_FRAGMENT_BIT, 1u, nullptr);

    return builder.build(vk, device);
}

Move<VkDescriptorSet> PipelineLibraryTestInstance::createDescriptorSet(const VkDescriptorPool pool,
                                                                       const VkDescriptorSetLayout layout,
                                                                       const VkBuffer vertShaderBuffer,
                                                                       const VkBuffer fragShaderBuffer)
{
    const DeviceInterface &vk                   = m_context.getDeviceInterface();
    const VkDevice device                       = m_context.getDevice();
    const VkDescriptorSetAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, //  VkStructureType sType;
        nullptr,                                        //  const void* pNext;
        pool,                                           //  VkDescriptorPool descriptorPool;
        1u,                                             //  uint32_t descriptorSetCount;
        &layout                                         //  const VkDescriptorSetLayout* pSetLayouts;
    };
    Move<VkDescriptorSet> descriptorSet = allocateDescriptorSet(vk, device, &allocInfo);
    DescriptorSetUpdateBuilder builder;

    if (vertShaderBuffer != VK_NULL_HANDLE)
    {
        const VkDeviceSize vertShaderBufferSize = de::dataSize(m_zCoordData);
        const VkDescriptorBufferInfo vertShaderBufferInfo =
            makeDescriptorBufferInfo(vertShaderBuffer, 0u, vertShaderBufferSize);

        builder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &vertShaderBufferInfo);
    }

    if (fragShaderBuffer != VK_NULL_HANDLE)
    {
        const VkDeviceSize fragShaderBufferSize = de::dataSize(m_paletteData);
        const VkDescriptorBufferInfo fragShaderBufferInfo =
            makeDescriptorBufferInfo(fragShaderBuffer, 0u, fragShaderBufferSize);

        builder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u),
                            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &fragShaderBufferInfo);
    }

    builder.update(vk, device);

    return descriptorSet;
}

VkFormat getSupportedDepthFormat(const InstanceInterface &vk, const VkPhysicalDevice physicalDevice)
{
    VkFormatProperties properties;

    const VkFormat DepthFormats[] = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_X8_D24_UNORM_PACK32, VK_FORMAT_D24_UNORM_S8_UINT,
                                     VK_FORMAT_D32_SFLOAT_S8_UINT};

    for (const auto format : DepthFormats)
    {
        vk.getPhysicalDeviceFormatProperties(physicalDevice, format, &properties);

        if (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
            return format;
    }

    TCU_THROW(NotSupportedError, "Depth format is not supported");
}

bool PipelineLibraryTestInstance::runTest(RuntimePipelineTreeConfiguration &runtimePipelineTreeConfiguration,
                                          const bool optimize, const bool delayedShaderCreate)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    Allocator &allocator            = m_context.getDefaultAllocator();
    tcu::TestLog &log               = m_context.getTestContext().getLog();
    const VkFormat colorFormat      = VK_FORMAT_R8G8B8A8_UNORM;
    const VkFormat depthFormat =
        getSupportedDepthFormat(m_context.getInstanceInterface(), m_context.getPhysicalDevice());
    const VkGraphicsPipelineLibraryFlagsEXT vertPipelineFlags =
        static_cast<VkGraphicsPipelineLibraryFlagsEXT>(VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT);
    const VkGraphicsPipelineLibraryFlagsEXT fragPipelineFlags =
        static_cast<VkGraphicsPipelineLibraryFlagsEXT>(VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT);
    const VkGraphicsPipelineLibraryFlagsEXT samePipelineFlags = vertPipelineFlags | fragPipelineFlags;
    const int32_t nodeNdxLast           = static_cast<int32_t>(runtimePipelineTreeConfiguration.size()) - 1;
    const Move<VkRenderPass> renderPass = makeRenderPass(vk, device, colorFormat, depthFormat);
    const de::MovePtr<BufferWithMemory> zCoordBuffer  = makeZCoordBuffer();
    const de::MovePtr<BufferWithMemory> paletteBuffer = makePaletteBuffer();
    const Move<VkDescriptorPool> descriptorPool       = createDescriptorPool();

    const Move<VkDescriptorSetLayout> descriptorSetLayoutVert =
        createDescriptorSetLayout(**zCoordBuffer, VK_NULL_HANDLE);
    const Move<VkDescriptorSetLayout> descriptorSetLayoutFrag =
        createDescriptorSetLayout(VK_NULL_HANDLE, **paletteBuffer);
    const Move<VkDescriptorSetLayout> descriptorSetLayoutBoth =
        createDescriptorSetLayout(**zCoordBuffer, **paletteBuffer);
    const Move<VkDescriptorSet> descriptorSetVert =
        createDescriptorSet(*descriptorPool, *descriptorSetLayoutVert, **zCoordBuffer, VK_NULL_HANDLE);
    const Move<VkDescriptorSet> descriptorSetFrag =
        createDescriptorSet(*descriptorPool, *descriptorSetLayoutFrag, VK_NULL_HANDLE, **paletteBuffer);

    VkDescriptorSet vecDescriptorSetBoth[2] = {*descriptorSetVert, *descriptorSetFrag};

    VkDescriptorSetLayout vecLayoutVert[2] = {*descriptorSetLayoutVert, VK_NULL_HANDLE};
    VkDescriptorSetLayout vecLayoutFrag[2] = {VK_NULL_HANDLE, *descriptorSetLayoutFrag};
    VkDescriptorSetLayout vecLayoutBoth[2] = {*descriptorSetLayoutVert, *descriptorSetLayoutFrag};

    VkPipelineLayoutCreateFlags pipelineLayoutCreateFlag = 0u;
    if (!m_data.useMaintenance5 && (m_data.delayedShaderCreate || (m_data.pipelineTreeConfiguration.size() > 1)))
        pipelineLayoutCreateFlag = VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT;

    const Move<VkCommandPool> cmdPool =
        createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
    const Move<VkCommandBuffer> cmdBuffer =
        allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const Move<VkPipelineLayout> pipelineLayoutSame =
        makePipelineLayout(vk, device, 2, vecLayoutBoth, pipelineLayoutCreateFlag);
    Move<VkPipelineLayout> pipelineLayoutVert;
    Move<VkPipelineLayout> pipelineLayoutFrag;
    Move<VkPipeline> rootPipeline;

    // Go through tree nodes and create library for each up to root
    for (int32_t nodeNdx = nodeNdxLast; nodeNdx >= 0;
         --nodeNdx) // We expect only backward node reference, thus build pipielines from end is safe
    {
        RuntimePipelineTreeNode &node                   = runtimePipelineTreeConfiguration[nodeNdx];
        const bool buildLibrary                         = (nodeNdx != 0);
        const VkPipelineCreateFlags pipelineCreateFlags = calcPipelineCreateFlags(optimize, buildLibrary);
        const VkGraphicsPipelineLibraryFlagsEXT subtreeGraphicsPipelineLibraryFlags =
            node.subtreeGraphicsPipelineLibraryFlags | node.graphicsPipelineLibraryFlags;
        const bool samePipelineLayout = samePipelineFlags == (samePipelineFlags & subtreeGraphicsPipelineLibraryFlags);
        const bool vertPipelineLayout = vertPipelineFlags == (vertPipelineFlags & subtreeGraphicsPipelineLibraryFlags);
        const bool fragPipelineLayout = fragPipelineFlags == (fragPipelineFlags & subtreeGraphicsPipelineLibraryFlags);

        if (samePipelineLayout)
            ; // pipelineLayoutSame is always built before.
        else if (vertPipelineLayout)
        {
            if (!pipelineLayoutVert)
                pipelineLayoutVert = makePipelineLayout(vk, device, 2, vecLayoutVert, pipelineLayoutCreateFlag);
        }
        else if (fragPipelineLayout)
        {
            if (!pipelineLayoutFrag)
                pipelineLayoutFrag = makePipelineLayout(vk, device, 2, vecLayoutFrag, pipelineLayoutCreateFlag);
        }

        const VkPipelineLayout pipelineLayout = samePipelineLayout ? *pipelineLayoutSame :
                                                vertPipelineLayout ? *pipelineLayoutVert :
                                                fragPipelineLayout ? *pipelineLayoutFrag :
                                                                     VK_NULL_HANDLE;
        const VkRenderPass renderPassHandle   = getRenderPass(node.graphicsPipelineLibraryFlags, *renderPass);
        VkGraphicsPipelineLibraryCreateInfoEXT graphicsPipelineLibraryCreateInfo =
            makeGraphicsPipelineLibraryCreateInfo(node.graphicsPipelineLibraryFlags);
        VkPipelineLibraryCreateInfoKHR linkingInfo = makePipelineLibraryCreateInfo(node.pipelineLibraries);
        GraphicsPipelineCreateInfo graphicsPipelineCreateInfo(pipelineLayout, renderPassHandle, 0, pipelineCreateFlags);

        for (const auto subsetFlag : GRAPHICS_PIPELINE_LIBRARY_FLAGS)
        {
            if ((node.graphicsPipelineLibraryFlags & subsetFlag) != 0)
            {
                switch (subsetFlag)
                {
                case VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT:
                    updateVertexInputInterface(m_context, graphicsPipelineCreateInfo);
                    break;
                case VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT:
                    updatePreRasterization(m_context, graphicsPipelineCreateInfo, delayedShaderCreate);
                    break;
                case VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT:
                    updatePostRasterization(m_context, graphicsPipelineCreateInfo, delayedShaderCreate);
                    break;
                case VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT:
                    updateFragmentOutputInterface(m_context, graphicsPipelineCreateInfo);
                    break;
                default:
                    TCU_THROW(InternalError, "Unknown pipeline subset");
                }
            }
        }

        VkGraphicsPipelineLibraryFlagsEXT linkedLibrariesFlags = 0;

        for (auto flag : node.linkedLibraryFlags)
            linkedLibrariesFlags |= flag;

        // When pLibraries have any pipeline library with fragment shader state and current pipeline we try to create doesn't,
        // we need to set a MS info.
        if ((linkedLibrariesFlags & VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT) &&
            !(node.graphicsPipelineLibraryFlags & VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT) &&
            (graphicsPipelineCreateInfo.pMultisampleState == nullptr))
        {
            const VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo =
                makePipelineMultisampleStateCreateInfo();

            graphicsPipelineCreateInfo.addState(pipelineMultisampleStateCreateInfo);
        }

        if (!m_data.useMaintenance5 && linkedLibrariesFlags != ALL_GRAPHICS_PIPELINE_LIBRARY_FLAGS &&
            graphicsPipelineLibraryCreateInfo.flags != 0)
            appendStructurePtrToVulkanChain(&graphicsPipelineCreateInfo.pNext, &graphicsPipelineLibraryCreateInfo);

        if (linkingInfo.libraryCount != 0)
        {
            appendStructurePtrToVulkanChain(&graphicsPipelineCreateInfo.pNext, &linkingInfo);
            graphicsPipelineCreateInfo.layout = *pipelineLayoutSame;
        }

        linkedLibrariesFlags |= node.graphicsPipelineLibraryFlags;

        // if current pipeline that we try to create and pLibraries have all states of pipelines, we are not allowed to create a pipeline library.
        if (linkedLibrariesFlags == ALL_GRAPHICS_PIPELINE_LIBRARY_FLAGS)
        {
            DE_ASSERT(!buildLibrary);
            graphicsPipelineCreateInfo.flags &= ~VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;
        }

        node.pipeline = createGraphicsPipeline(vk, device, VK_NULL_HANDLE, &graphicsPipelineCreateInfo);

        if (buildLibrary)
        {
            DE_ASSERT(de::inBounds(node.parentIndex, 0, static_cast<int32_t>(runtimePipelineTreeConfiguration.size())));

            runtimePipelineTreeConfiguration[node.parentIndex].pipelineLibraries.push_back(*node.pipeline);
            runtimePipelineTreeConfiguration[node.parentIndex].linkedLibraryFlags.push_back(linkedLibrariesFlags);
        }
        else
        {
            DE_ASSERT(node.parentIndex == -1);

            rootPipeline = node.pipeline;
        }
    }

    // Queue commands and read results.
    {
        const tcu::UVec2 renderSize                      = {RENDER_SIZE_WIDTH, RENDER_SIZE_HEIGHT};
        const VkRect2D renderArea                        = makeRect2D(renderSize.x(), renderSize.y());
        const de::MovePtr<BufferWithMemory> vertexBuffer = makeVertexBuffer();
        const uint32_t vertexCount                       = static_cast<uint32_t>(m_vertexData.size());
        const VkDeviceSize vertexBufferOffset            = 0;
        const Vec4 colorClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        const VkImageCreateInfo colorImageCreateInfo =
            makeColorImageCreateInfo(colorFormat, renderSize.x(), renderSize.y());
        const ImageWithMemory colorImage(vk, device, allocator, colorImageCreateInfo, MemoryRequirement::Any);
        const VkImageViewCreateInfo colorImageViewCreateInfo = makeImageViewCreateInfo(
            *colorImage, colorFormat, static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_COLOR_BIT));
        const Move<VkImageView> colorImageView = createImageView(vk, device, &colorImageViewCreateInfo);
        const VkImageCreateInfo depthImageCreateInfo =
            makeDepthImageCreateInfo(depthFormat, renderSize.x(), renderSize.y());
        const ImageWithMemory depthImage(vk, device, allocator, depthImageCreateInfo, MemoryRequirement::Any);
        const VkImageViewCreateInfo depthImageViewCreateInfo = makeImageViewCreateInfo(
            *depthImage, depthFormat, static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_DEPTH_BIT));
        const Move<VkImageView> depthImageView = createImageView(vk, device, &depthImageViewCreateInfo);
        const float depthClearDepth            = 1.0f;
        const uint32_t depthClearStencil       = 0u;
        const VkDeviceSize colorBufferDataSize =
            static_cast<VkDeviceSize>(renderSize.x() * renderSize.y() * tcu::getPixelSize(mapVkFormat(colorFormat)));
        const VkBufferCreateInfo colorBufferCreateInfo = makeBufferCreateInfo(
            colorBufferDataSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        const BufferWithMemory colorBuffer(vk, device, allocator, colorBufferCreateInfo,
                                           MemoryRequirement::HostVisible);
        const VkDeviceSize depthBufferDataSize =
            static_cast<VkDeviceSize>(renderSize.x() * renderSize.y() * tcu::getPixelSize(mapVkFormat(depthFormat)));
        const VkBufferCreateInfo depthBufferCreateInfo = makeBufferCreateInfo(
            depthBufferDataSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        const BufferWithMemory depthBuffer(vk, device, allocator, depthBufferCreateInfo,
                                           MemoryRequirement::HostVisible);
        const VkImageView attachments[]                     = {*colorImageView, *depthImageView};
        const VkFramebufferCreateInfo framebufferCreateInfo = makeFramebufferCreateInfo(
            *renderPass, DE_LENGTH_OF_ARRAY(attachments), attachments, renderSize.x(), renderSize.y());
        const Move<VkFramebuffer> framebuffer = createFramebuffer(vk, device, &framebufferCreateInfo);

        vk::beginCommandBuffer(vk, *cmdBuffer, 0u);
        {
            beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, renderArea, colorClearColor, depthClearDepth,
                            depthClearStencil);
            {
                vk.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &vertexBuffer->get(), &vertexBufferOffset);
                vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *rootPipeline);
                vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayoutSame, 0u, 2u,
                                         vecDescriptorSetBoth, 0u, nullptr);
                vk.cmdDraw(*cmdBuffer, vertexCount, 1u, 0u, 0u);
            }
            endRenderPass(vk, *cmdBuffer);

            const tcu::IVec2 size = {(int32_t)renderSize.x(), (int32_t)renderSize.y()};
            copyImageToBuffer(vk, *cmdBuffer, *colorImage, *colorBuffer, size);
            copyImageToBuffer(vk, *cmdBuffer, *depthImage, *depthBuffer, size,
                              VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                              VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1u, VK_IMAGE_ASPECT_DEPTH_BIT,
                              VK_IMAGE_ASPECT_DEPTH_BIT);
        }
        vk::endCommandBuffer(vk, *cmdBuffer);
        vk::submitCommandsAndWait(vk, device, m_context.getUniversalQueue(), cmdBuffer.get());

        vk::invalidateAlloc(vk, device, colorBuffer.getAllocation());
        vk::invalidateAlloc(vk, device, depthBuffer.getAllocation());

        const tcu::ConstPixelBufferAccess colorPixelAccess(mapVkFormat(colorFormat), renderSize.x(), renderSize.y(), 1,
                                                           colorBuffer.getAllocation().getHostPtr());
        const tcu::ConstPixelBufferAccess depthPixelAccess(mapVkFormat(depthFormat), renderSize.x(), renderSize.y(), 1,
                                                           depthBuffer.getAllocation().getHostPtr());

        if (!verifyColorImage(colorPixelAccess))
        {
            log << tcu::TestLog::Image("color", "Rendered image", colorPixelAccess);

            return false;
        }

        if (!verifyDepthImage(depthPixelAccess))
        {
            log << tcu::TestLog::Image("depth", "Rendered image", depthPixelAccess);

            return false;
        }
    }

    return true;
}

bool PipelineLibraryTestInstance::verifyColorImage(const ConstPixelBufferAccess &pba)
{
    tcu::TestLog &log = m_context.getTestContext().getLog();
    TextureLevel referenceImage(pba.getFormat(), pba.getWidth(), pba.getHeight());
    PixelBufferAccess reference(referenceImage);
    const int horzEdge = 3 * reference.getWidth() / 4;
    const int vertEdge = reference.getHeight() / 2;
    const UVec4 green  = ivec2uvec(RGBA::green().toIVec());
    const UVec4 blue   = ivec2uvec(RGBA::blue().toIVec());
    const UVec4 black  = ivec2uvec(RGBA::black().toIVec());

    for (int y = 0; y < reference.getHeight(); ++y)
    {
        for (int x = 0; x < reference.getWidth(); ++x)
        {
            if (x < horzEdge)
            {
                if (y < vertEdge)
                    reference.setPixel(green, x, y);
                else
                    reference.setPixel(blue, x, y);
            }
            else
                reference.setPixel(black, x, y);
        }
    }

    return intThresholdCompare(log, "colorImage", "colorImage", reference, pba, UVec4(), COMPARE_LOG_RESULT);
}

bool PipelineLibraryTestInstance::verifyDepthImage(const ConstPixelBufferAccess &pba)
{
    tcu::TestLog &log            = m_context.getTestContext().getLog();
    const VkFormat compareFormat = VK_FORMAT_R8_UNORM;
    TextureLevel referenceImage(mapVkFormat(compareFormat), pba.getWidth(), pba.getHeight());
    PixelBufferAccess reference(referenceImage);
    TextureLevel resultImage(mapVkFormat(compareFormat), pba.getWidth(), pba.getHeight());
    PixelBufferAccess result(resultImage);
    const int horzEdge     = 3 * reference.getWidth() / 4;
    const int diagonalEdge = (reference.getWidth() + reference.getHeight()) / 2 - 1;
    const UVec4 red100     = ivec2uvec(RGBA::red().toIVec());
    const UVec4 red025     = UVec4(red100[0] / 4, red100[1] / 4, red100[2] / 4, red100[3]);
    const UVec4 red075     = UVec4(3 * red100[0] / 4, 3 * red100[1] / 4, 3 * red100[2] / 4, red100[3]);

    for (int y = 0; y < result.getHeight(); ++y)
        for (int x = 0; x < result.getWidth(); ++x)
        {
            const UVec4 pix(static_cast<uint32_t>(static_cast<float>(red100[0]) * pba.getPixDepth(x, y)), 0, 0, 0);

            result.setPixel(pix, x, y);
        }

    for (int y = 0; y < reference.getHeight(); ++y)
    {
        for (int x = 0; x < reference.getWidth(); ++x)
        {
            if (x < horzEdge)
            {
                if (x + y < diagonalEdge)
                    reference.setPixel(red025, x, y);
                else
                    reference.setPixel(red075, x, y);
            }
            else
                reference.setPixel(red100, x, y);
        }
    }

    return intThresholdCompare(log, "depthImage", "depthImage", reference, result, UVec4(), COMPARE_LOG_RESULT);
}

tcu::TestStatus PipelineLibraryTestInstance::iterate(void)
{
    VkGraphicsPipelineLibraryFlagBitsEXT graphicsPipelineLibraryFlags[] = {
        VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT,
        VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT,
        VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT,
        VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT,
    };
    const auto graphicsPipelineLibraryFlagsBegin = graphicsPipelineLibraryFlags;
    const auto graphicsPipelineLibraryFlagsEnd =
        graphicsPipelineLibraryFlags + DE_LENGTH_OF_ARRAY(graphicsPipelineLibraryFlags);
    uint32_t permutationId = 0;
    std::set<uint32_t> was;
    bool result = true;

    do
    {
        RuntimePipelineTreeConfiguration runtimePipelineTreeConfiguration(m_data.pipelineTreeConfiguration.size());
        size_t subsetNdxStart         = 0;
        uint32_t uniqueTreeSubsetCode = 0;

        for (size_t nodeNdx = 0; nodeNdx < runtimePipelineTreeConfiguration.size(); ++nodeNdx)
        {
            const uint32_t shaderCount    = m_data.pipelineTreeConfiguration[nodeNdx].shaderCount;
            RuntimePipelineTreeNode &node = runtimePipelineTreeConfiguration[nodeNdx];

            node.parentIndex                  = m_data.pipelineTreeConfiguration[nodeNdx].parentIndex;
            node.graphicsPipelineLibraryFlags = 0u;

            for (size_t subsetNdx = 0; subsetNdx < shaderCount; ++subsetNdx)
                node.graphicsPipelineLibraryFlags |= static_cast<VkGraphicsPipelineLibraryFlagsEXT>(
                    graphicsPipelineLibraryFlags[subsetNdxStart + subsetNdx]);

            if (node.parentIndex > 0)
                runtimePipelineTreeConfiguration[node.parentIndex].subtreeGraphicsPipelineLibraryFlags |=
                    node.graphicsPipelineLibraryFlags;

            // Each shader subset should be tested in each node of tree
            subsetNdxStart += shaderCount;

            uniqueTreeSubsetCode = (uniqueTreeSubsetCode << 4) | node.graphicsPipelineLibraryFlags;
        }

        // Check whether this configuration has been tried
        if (was.find(uniqueTreeSubsetCode) == was.end())
            was.insert(uniqueTreeSubsetCode);
        else
            continue;

        result = result && runTest(runtimePipelineTreeConfiguration, m_data.optimize, m_data.delayedShaderCreate);

        if (!result)
        {
            tcu::TestLog &log = m_context.getTestContext().getLog();
            std::ostringstream ess;

            for (size_t nodeNdx = 0; nodeNdx < runtimePipelineTreeConfiguration.size(); ++nodeNdx)
            {
                const RuntimePipelineTreeNode &node = runtimePipelineTreeConfiguration[nodeNdx];

                ess << node.parentIndex << " {";

                for (size_t subsetNdx = 0; subsetNdx < DE_LENGTH_OF_ARRAY(graphicsPipelineLibraryFlags); ++subsetNdx)
                {
                    if ((node.graphicsPipelineLibraryFlags & graphicsPipelineLibraryFlags[subsetNdx]) == 0)
                        continue;

                    ess << getGraphicsPipelineLibraryFlagsString(graphicsPipelineLibraryFlags[subsetNdx]) << " ";
                }

                ess << "}" << std::endl;
            }

            log << tcu::TestLog::Message << ess.str() << tcu::TestLog::EndMessage;

            return tcu::TestStatus::fail("At permutation " + de::toString(permutationId));
        }

        ++permutationId;
    } while (std::next_permutation(graphicsPipelineLibraryFlagsBegin, graphicsPipelineLibraryFlagsEnd));

    return tcu::TestStatus::pass("OK");
}

class PipelineLibraryTestCase : public TestCase
{
public:
    PipelineLibraryTestCase(tcu::TestContext &context, const char *name, const TestParams data);
    ~PipelineLibraryTestCase(void);

    virtual void checkSupport(Context &context) const;
    virtual void initPrograms(SourceCollections &programCollection) const;
    virtual TestInstance *createInstance(Context &context) const;

private:
    TestParams m_data;
};

PipelineLibraryTestCase::PipelineLibraryTestCase(tcu::TestContext &context, const char *name, const TestParams data)
    : vkt::TestCase(context, name)
    , m_data(data)
{
}

PipelineLibraryTestCase::~PipelineLibraryTestCase(void)
{
}

void PipelineLibraryTestCase::checkSupport(Context &context) const
{
    if (m_data.useMaintenance5)
    {
        context.requireDeviceFunctionality("VK_KHR_maintenance5");
        return;
    }

    context.requireDeviceFunctionality("VK_KHR_pipeline_library");

    if (m_data.delayedShaderCreate || (m_data.pipelineTreeConfiguration.size() > 1))
    {
        context.requireDeviceFunctionality("VK_EXT_graphics_pipeline_library");

        const VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT &graphicsPipelineLibraryFeaturesEXT =
            context.getGraphicsPipelineLibraryFeaturesEXT();

        if (!graphicsPipelineLibraryFeaturesEXT.graphicsPipelineLibrary)
            TCU_THROW(NotSupportedError, "graphicsPipelineLibraryFeaturesEXT.graphicsPipelineLibrary required");
    }
}

void PipelineLibraryTestCase::initPrograms(SourceCollections &programCollection) const
{
    std::string vert = "#version 450\n"
                       "layout(location = 0) in vec4 in_position;\n"
                       "layout(set = 0, binding = 0) uniform buf\n"
                       "{\n"
                       "  vec4 z_coord;\n"
                       "};\n"
                       "\n"
                       "out gl_PerVertex\n"
                       "{\n"
                       "  vec4 gl_Position;\n"
                       "};\n"
                       "\n"
                       "void main()\n"
                       "{\n"
                       "  const float z = gl_VertexIndex < 3 ? z_coord.x : z_coord.y;\n"
                       "  gl_Position = vec4(in_position.x, in_position.y, z, 1.0f);\n"
                       "}\n";

    programCollection.glslSources.add("vert") << glu::VertexSource(vert);

    std::string frag = "#version 450\n"
                       "layout(location = 0) out highp vec4 o_color;\n"
                       "layout(set = 1, binding = 1) uniform buf\n"
                       "{\n"
                       "  vec4 colorTop;\n"
                       "  vec4 colorBot;\n"
                       "};\n"
                       "\n"
                       "void main()\n"
                       "{\n"
                       "  const int middle = " +
                       de::toString(RENDER_SIZE_HEIGHT / 2) +
                       ";\n"
                       "  o_color          = int(gl_FragCoord.y - 0.5f) < middle ? colorTop : colorBot;\n"
                       "}\n";

    programCollection.glslSources.add("frag") << glu::FragmentSource(frag);
}

TestInstance *PipelineLibraryTestCase::createInstance(Context &context) const
{
    return new PipelineLibraryTestInstance(context, m_data);
}

enum class MiscTestMode
{
    INDEPENDENT_PIPELINE_LAYOUT_SETS_FAST_LINKED = 0,
    INDEPENDENT_PIPELINE_LAYOUT_SETS_WITH_LINK_TIME_OPTIMIZATION_UNION_HANDLE,
    BIND_NULL_DESCRIPTOR_SET,
    BIND_NULL_DESCRIPTOR_SET_IN_MONOLITHIC_PIPELINE,
    COMPARE_LINK_TIMES,
    SHADER_MODULE_CREATE_INFO_COMP,
    SHADER_MODULE_CREATE_INFO_RT,
    SHADER_MODULE_CREATE_INFO_RT_LIB,
    NULL_RENDERING_CREATE_INFO,
    COMMON_FRAG_LIBRARY,
    VIEW_INDEX_FROM_DEVICE_INDEX,
    UNUSUAL_MULTISAMPLE_STATE,
};

// params used in BIND_NULL_DESCRIPTOR_SET mode
struct NullDescriptorSetParams
{
    uint32_t layoutsCount;
    uint32_t layoutsBits;

    NullDescriptorSetParams(uint32_t layoutsCount_, uint32_t layoutsBits_)
        : layoutsCount(layoutsCount_)
        , layoutsBits(layoutsBits_)
    {
    }
};

enum class PipelineStateMode
{
    ALL_STAGES = 0,
    PRE_RASTERIZATION,
    FRAGMENT,
};

// params used in VIEW_INDEX_FROM_DEVICE_INDEX
struct ViewIndexFromDeviceIndexParams
{
    PipelineStateMode pipelineStateMode;
    bool useMeshShading;
    bool useLinkTimeOptimization;

    ViewIndexFromDeviceIndexParams(PipelineStateMode pipelineStateMode_, bool useMeshShading_,
                                   bool useLinkTimeOptimization_)
        : pipelineStateMode(pipelineStateMode_)
        , useMeshShading(useMeshShading_)
        , useLinkTimeOptimization(useLinkTimeOptimization_)
    {
    }
};

struct MiscTestParams
{
    // miscellaneous test mode
    MiscTestMode mode;

    // some misc test require different addditional params
    std::any modeParams;

    MiscTestParams(MiscTestMode mode_) : mode(mode_)
    {
    }

    // constructor used for BIND_NULL_DESCRIPTOR_SET test mode
    MiscTestParams(MiscTestMode mode_, uint32_t layoutsCount_, uint32_t layoutsBits_)
        : mode(mode_)
        , modeParams(std::in_place_type<NullDescriptorSetParams>, layoutsCount_, layoutsBits_)
    {
    }

    // constructor used for VIEW_INDEX_FROM_DEVICE_INDEX test mode
    MiscTestParams(MiscTestMode mode_, PipelineStateMode pipelineStateMode_, bool useMeshShading_,
                   bool useLinkTimeOptimization_)
        : mode(mode_)
        , modeParams(std::in_place_type<ViewIndexFromDeviceIndexParams>, pipelineStateMode_, useMeshShading_,
                     useLinkTimeOptimization_)
    {
    }

    template <typename T>
    const T &get() const
    {
        return std::any_cast<const T &>(modeParams);
    }
};

class PipelineLibraryMiscTestInstance : public TestInstance
{
public:
    PipelineLibraryMiscTestInstance(Context &context, const MiscTestParams &params);
    ~PipelineLibraryMiscTestInstance(void) = default;
    tcu::TestStatus iterate(void);

protected:
    tcu::TestStatus runNullDescriptorSet(void);
    tcu::TestStatus runNullDescriptorSetInMonolithicPipeline(void);
    tcu::TestStatus runIndependentPipelineLayoutSets(bool useLinkTimeOptimization = false);
    tcu::TestStatus runCompareLinkTimes(void);
    tcu::TestStatus runCommonFragLibraryTest(void);

    struct VerificationData
    {
        const tcu::IVec2 point;
        const tcu::IVec4 color;
    };
    tcu::TestStatus verifyResult(const std::vector<VerificationData> &verificationData,
                                 const tcu::ConstPixelBufferAccess &colorPixelAccess) const;
    // verification for test mode: COMMON_FRAG_LIBRARY_FAST_LINKED
    bool verifyOnePipelineLibraryResult(const tcu::ConstPixelBufferAccess &colorPixelAccess, const int numBars) const;

private:
    MiscTestParams m_testParams;
    const VkFormat m_colorFormat;
    const Vec4 m_colorClearColor;
    const VkRect2D m_renderArea;

    de::MovePtr<ImageWithMemory> m_colorImage;
    Move<VkImageView> m_colorImageView;

    Move<VkRenderPass> m_renderPass;
    Move<VkFramebuffer> m_framebuffer;

    Move<VkCommandPool> m_cmdPool;
    Move<VkCommandBuffer> m_cmdBuffer;
};

PipelineLibraryMiscTestInstance::PipelineLibraryMiscTestInstance(Context &context, const MiscTestParams &params)
    : vkt::TestInstance(context)
    , m_testParams(params)
    , m_colorFormat(VK_FORMAT_R8G8B8A8_UNORM)
    , m_colorClearColor(0.0f, 0.0f, 0.0f, 1.0f)
    , m_renderArea(makeRect2D(RENDER_SIZE_WIDTH, RENDER_SIZE_HEIGHT))
{
}

tcu::TestStatus PipelineLibraryMiscTestInstance::iterate(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();
    Allocator &allocator      = m_context.getDefaultAllocator();

    // create image and image view that will hold rendered frame
    const VkImageCreateInfo colorImageCreateInfo =
        makeColorImageCreateInfo(m_colorFormat, m_renderArea.extent.width, m_renderArea.extent.height);
    m_colorImage = de::MovePtr<ImageWithMemory>(
        new ImageWithMemory(vk, device, allocator, colorImageCreateInfo, MemoryRequirement::Any));
    const VkImageViewCreateInfo colorImageViewCreateInfo = makeImageViewCreateInfo(
        **m_colorImage, m_colorFormat, static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_COLOR_BIT));
    const Move<VkImageView> colorImageView = createImageView(vk, device, &colorImageViewCreateInfo);

    // create renderpass and framebuffer
    m_renderPass                                        = makeRenderPass(vk, device, m_colorFormat);
    const VkFramebufferCreateInfo framebufferCreateInfo = makeFramebufferCreateInfo(
        *m_renderPass, 1u, &*colorImageView, m_renderArea.extent.width, m_renderArea.extent.height);
    m_framebuffer = createFramebuffer(vk, device, &framebufferCreateInfo);

    // create command pool and command buffer
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    m_cmdPool   = createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);
    m_cmdBuffer = allocateCommandBuffer(vk, device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    // run selected test
    if (m_testParams.mode == MiscTestMode::BIND_NULL_DESCRIPTOR_SET)
        return runNullDescriptorSet();
    else if (m_testParams.mode == MiscTestMode::BIND_NULL_DESCRIPTOR_SET_IN_MONOLITHIC_PIPELINE)
        return runNullDescriptorSetInMonolithicPipeline();
    else if (m_testParams.mode == MiscTestMode::INDEPENDENT_PIPELINE_LAYOUT_SETS_FAST_LINKED)
        return runIndependentPipelineLayoutSets();
    else if (m_testParams.mode ==
             MiscTestMode::INDEPENDENT_PIPELINE_LAYOUT_SETS_WITH_LINK_TIME_OPTIMIZATION_UNION_HANDLE)
        return runIndependentPipelineLayoutSets(true);
    else if (m_testParams.mode == MiscTestMode::COMPARE_LINK_TIMES)
        return runCompareLinkTimes();
    else if (m_testParams.mode == MiscTestMode::COMMON_FRAG_LIBRARY)
        return runCommonFragLibraryTest();

    DE_ASSERT(false);
    return tcu::TestStatus::fail("Fail");
}

tcu::TestStatus PipelineLibraryMiscTestInstance::runNullDescriptorSet(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();
    Allocator &allocator      = m_context.getDefaultAllocator();

    const VkDeviceSize colorBufferDataSize = static_cast<VkDeviceSize>(
        m_renderArea.extent.width * m_renderArea.extent.height * tcu::getPixelSize(mapVkFormat(m_colorFormat)));
    const VkBufferCreateInfo colorBufferCreateInfo = makeBufferCreateInfo(
        colorBufferDataSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const BufferWithMemory colorBuffer(vk, device, allocator, colorBufferCreateInfo, MemoryRequirement::HostVisible);

    VkDeviceSize uniformBufferDataSize = sizeof(tcu::Vec4);
    const VkBufferCreateInfo uniformBufferCreateInfo =
        makeBufferCreateInfo(uniformBufferDataSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    de::MovePtr<BufferWithMemory> uniformBuffer[2];

    // setup data in uniform buffers that will give us expected result for validation
    const tcu::Vec4 uniformBuffData[]{
        {-1.00f, 1.00f, 2.0f, -2.00f},
        {0.00f, 0.20f, 0.6f, 0.75f},
    };

    for (uint32_t i = 0; i < 2; ++i)
    {
        uniformBuffer[i] = de::MovePtr<BufferWithMemory>(
            new BufferWithMemory(vk, device, allocator, uniformBufferCreateInfo, MemoryRequirement::HostVisible));
        deMemcpy(uniformBuffer[i]->getAllocation().getHostPtr(), uniformBuffData[i].getPtr(),
                 (size_t)uniformBufferDataSize);
        flushAlloc(vk, device, uniformBuffer[i]->getAllocation());
    }

    const auto &modeParams      = m_testParams.get<NullDescriptorSetParams>();
    const uint32_t maxBitsCount = 8 * sizeof(modeParams.layoutsBits);
    VkDescriptorSetLayout vertDescriptorSetLayouts[maxBitsCount];
    VkDescriptorSetLayout fragDescriptorSetLayouts[maxBitsCount];
    VkDescriptorSetLayout allDescriptorSetLayouts[maxBitsCount];

    // set all layouts to NULL
    deMemset(&vertDescriptorSetLayouts, 0, maxBitsCount * sizeof(VkDescriptorSetLayout));
    deMemset(&fragDescriptorSetLayouts, 0, maxBitsCount * sizeof(VkDescriptorSetLayout));

    // create used descriptor set layouts
    Move<VkDescriptorSetLayout> usedDescriptorSetLayouts[]{
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
            .build(vk, device),
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(vk, device)};

    // create descriptor set layouts that are not used by shaders in test - finalPipelineLayout,
    // needs to always be the complete pipeline layout with no holes; we can put NULLs in
    // DescriptorSetLayouts used by partial pipelines (vertDescriptorSetLayouts and fragDescriptorSetLayouts)
    Move<VkDescriptorSetLayout> unusedDescriptorSetLayouts[maxBitsCount];
    for (uint32_t i = 0u; i < modeParams.layoutsCount; ++i)
    {
        unusedDescriptorSetLayouts[i] = DescriptorSetLayoutBuilder().build(vk, device);

        // by default allDescriptorSetLayouts is filled with unused layouts but later
        // if test requires this proper indexes are replaced with used layouts
        allDescriptorSetLayouts[i] = *unusedDescriptorSetLayouts[i];
    }

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = initVulkanStructure();
    pipelineLayoutCreateInfo.flags                      = VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT;

    // find set bits
    std::vector<uint32_t> bitsThatAreSet;
    for (uint32_t i = 0u; i < modeParams.layoutsCount; ++i)
    {
        if (modeParams.layoutsBits & (1 << (maxBitsCount - 1 - i)))
            bitsThatAreSet.push_back(i);
    }

    uint32_t usedDescriptorSets = static_cast<uint32_t>(bitsThatAreSet.size());
    DE_ASSERT(usedDescriptorSets && (usedDescriptorSets < 3u));

    uint32_t vertSetIndex                   = bitsThatAreSet[0];
    uint32_t fragSetIndex                   = 0u;
    vertDescriptorSetLayouts[vertSetIndex]  = *usedDescriptorSetLayouts[0];
    allDescriptorSetLayouts[vertSetIndex]   = *usedDescriptorSetLayouts[0];
    pipelineLayoutCreateInfo.setLayoutCount = vertSetIndex + 1u;
    pipelineLayoutCreateInfo.pSetLayouts    = vertDescriptorSetLayouts;

    Move<VkPipelineLayout> vertPipelineLayout = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);
    Move<VkPipelineLayout> fragPipelineLayout;

    if (usedDescriptorSets == 2u)
    {
        fragSetIndex                            = bitsThatAreSet[1];
        fragDescriptorSetLayouts[fragSetIndex]  = *usedDescriptorSetLayouts[1];
        allDescriptorSetLayouts[fragSetIndex]   = *usedDescriptorSetLayouts[1];
        pipelineLayoutCreateInfo.setLayoutCount = fragSetIndex + 1u;
        pipelineLayoutCreateInfo.pSetLayouts    = fragDescriptorSetLayouts;

        fragPipelineLayout = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);
    }
    else
    {
        pipelineLayoutCreateInfo.setLayoutCount = 0u;
        pipelineLayoutCreateInfo.pSetLayouts    = nullptr;
        fragPipelineLayout                      = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);
    }

    // create descriptor pool
    Move<VkDescriptorPool> descriptorPool =
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, usedDescriptorSets)
            .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, usedDescriptorSets);

    const VkDescriptorBufferInfo vertShaderBufferInfo =
        makeDescriptorBufferInfo(**uniformBuffer[0], 0u, uniformBufferDataSize);
    Move<VkDescriptorSet> vertDescriptorSet =
        makeDescriptorSet(vk, device, *descriptorPool, *usedDescriptorSetLayouts[0]);
    Move<VkDescriptorSet> fragDescriptorSet;

    if (usedDescriptorSets == 1u)
    {
        // update single descriptors with actual buffer
        DescriptorSetUpdateBuilder()
            .writeSingle(*vertDescriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                         VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &vertShaderBufferInfo)
            .update(vk, device);
    }
    else
    {
        const VkDescriptorBufferInfo fragShaderBufferInfo =
            makeDescriptorBufferInfo(**uniformBuffer[1], 0u, uniformBufferDataSize);
        fragDescriptorSet = makeDescriptorSet(vk, device, *descriptorPool, *usedDescriptorSetLayouts[1]);

        // update both descriptors with actual buffers
        DescriptorSetUpdateBuilder()
            .writeSingle(*vertDescriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                         VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &vertShaderBufferInfo)
            .writeSingle(*fragDescriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                         VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &fragShaderBufferInfo)
            .update(vk, device);
    }

    pipelineLayoutCreateInfo.setLayoutCount    = modeParams.layoutsCount;
    pipelineLayoutCreateInfo.pSetLayouts       = allDescriptorSetLayouts;
    Move<VkPipelineLayout> finalPipelineLayout = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);

    const uint32_t commonPipelinePartFlags = uint32_t(VK_PIPELINE_CREATE_LIBRARY_BIT_KHR);
    GraphicsPipelineCreateInfo partialPipelineCreateInfo[]{
        {*vertPipelineLayout, *m_renderPass, 0, commonPipelinePartFlags},
        {*fragPipelineLayout, *m_renderPass, 0, commonPipelinePartFlags},
    };

    // fill proper portion of pipeline state
    updateVertexInputInterface(m_context, partialPipelineCreateInfo[0], VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0u);
    updatePreRasterization(m_context, partialPipelineCreateInfo[0], false);
    updatePostRasterization(m_context, partialPipelineCreateInfo[1], false);
    updateFragmentOutputInterface(m_context, partialPipelineCreateInfo[1]);

    Move<VkPipeline> vertPipelinePart;
    Move<VkPipeline> fragPipelinePart;

    // extend pNext chain and create partial pipelines
    {
        VkGraphicsPipelineLibraryCreateInfoEXT libraryCreateInfo =
            makeGraphicsPipelineLibraryCreateInfo(VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT |
                                                  VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT);
        appendStructurePtrToVulkanChain(&partialPipelineCreateInfo[0].pNext, &libraryCreateInfo);
        vertPipelinePart = createGraphicsPipeline(vk, device, VK_NULL_HANDLE, &partialPipelineCreateInfo[0]);

        libraryCreateInfo.flags = VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT |
                                  VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT,
        appendStructurePtrToVulkanChain(&partialPipelineCreateInfo[1].pNext, &libraryCreateInfo);
        fragPipelinePart = createGraphicsPipeline(vk, device, VK_NULL_HANDLE, &partialPipelineCreateInfo[1]);
    }

    // create final pipeline out of two parts
    std::vector<VkPipeline> rawParts               = {*vertPipelinePart, *fragPipelinePart};
    VkPipelineLibraryCreateInfoKHR linkingInfo     = makePipelineLibraryCreateInfo(rawParts);
    VkGraphicsPipelineCreateInfo finalPipelineInfo = initVulkanStructure();

    finalPipelineInfo.layout = *finalPipelineLayout;
    appendStructurePtrToVulkanChain(&finalPipelineInfo.pNext, &linkingInfo);
    Move<VkPipeline> pipeline = createGraphicsPipeline(vk, device, VK_NULL_HANDLE, &finalPipelineInfo);

    vk::beginCommandBuffer(vk, *m_cmdBuffer, 0u);
    {
        // change color image layout
        const VkImageMemoryBarrier initialImageBarrier = makeImageMemoryBarrier(
            0,                                          // VkAccessFlags srcAccessMask;
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,       // VkAccessFlags dstAccessMask;
            VK_IMAGE_LAYOUT_UNDEFINED,                  // VkImageLayout oldLayout;
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,   // VkImageLayout newLayout;
            **m_colorImage,                             // VkImage image;
            {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u} // VkImageSubresourceRange subresourceRange;
        );
        vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                              VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, (VkDependencyFlags)0, 0, nullptr, 0,
                              nullptr, 1, &initialImageBarrier);

        // wait for uniform buffers
        std::vector<VkBufferMemoryBarrier> initialBufferBarriers(
            2u, makeBufferMemoryBarrier(VK_ACCESS_HOST_WRITE_BIT,    // VkAccessFlags2KHR                srcAccessMask
                                        VK_ACCESS_TRANSFER_READ_BIT, // VkAccessFlags2KHR                dstAccessMask
                                        uniformBuffer[0]->get(),     // VkBuffer                            buffer
                                        0u,                          // VkDeviceSize                        offset
                                        uniformBufferDataSize        // VkDeviceSize                        size
                                        ));
        initialBufferBarriers[1].buffer = uniformBuffer[1]->get();
        vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                              (VkDependencyFlags)0, 0, nullptr, 2, initialBufferBarriers.data(), 0, nullptr);

        beginRenderPass(vk, *m_cmdBuffer, *m_renderPass, *m_framebuffer, m_renderArea, m_colorClearColor);

        vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

        vk.cmdBindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *finalPipelineLayout, vertSetIndex, 1u,
                                 &*vertDescriptorSet, 0u, nullptr);
        if (usedDescriptorSets == 2u)
            vk.cmdBindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *finalPipelineLayout, fragSetIndex,
                                     1u, &*fragDescriptorSet, 0u, nullptr);

        vk.cmdDraw(*m_cmdBuffer, 4, 1u, 0u, 0u);

        endRenderPass(vk, *m_cmdBuffer);

        const tcu::IVec2 size{(int32_t)m_renderArea.extent.width, (int32_t)m_renderArea.extent.height};
        copyImageToBuffer(vk, *m_cmdBuffer, **m_colorImage, *colorBuffer, size);
    }
    vk::endCommandBuffer(vk, *m_cmdBuffer);
    vk::submitCommandsAndWait(vk, device, m_context.getUniversalQueue(), *m_cmdBuffer);

    vk::invalidateAlloc(vk, device, colorBuffer.getAllocation());
    const tcu::ConstPixelBufferAccess colorPixelAccess(mapVkFormat(m_colorFormat), m_renderArea.extent.width,
                                                       m_renderArea.extent.height, 1,
                                                       colorBuffer.getAllocation().getHostPtr());

    // verify result
    int32_t width  = (int32_t)m_renderArea.extent.width;
    int32_t height = (int32_t)m_renderArea.extent.height;
    const std::vector<VerificationData> verificationData{
        {{1, 1}, {0, 51, 153, 191}}, // note COLOR_COMPONENTS_NO_RED is used
        {{width / 2, height / 2}, {0, 51, 153, 191}},
        {{width - 2, height - 2}, {0, 0, 0, 255}} // clear color
    };
    return verifyResult(verificationData, colorPixelAccess);
}

tcu::TestStatus PipelineLibraryMiscTestInstance::runNullDescriptorSetInMonolithicPipeline()
{
    // VK_NULL_HANDLE can be used for descriptor set layouts when creating a pipeline layout whether independent sets are used or not,
    // as long as graphics pipeline libraries are enabled; VK_NULL_HANDLE is also alowed for a descriptor set under the same conditions
    // when using vkCmdBindDescriptorSets

    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();
    Allocator &allocator      = m_context.getDefaultAllocator();

    const VkDeviceSize colorBufferDataSize = static_cast<VkDeviceSize>(
        m_renderArea.extent.width * m_renderArea.extent.height * tcu::getPixelSize(mapVkFormat(m_colorFormat)));
    const VkBufferCreateInfo colorBufferCreateInfo = makeBufferCreateInfo(
        colorBufferDataSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const BufferWithMemory colorBuffer(vk, device, allocator, colorBufferCreateInfo, MemoryRequirement::HostVisible);

    const tcu::Vec4 uniformBuffData{0.0f, 0.20f, 0.6f, 0.75f};
    VkDeviceSize uniformBufferDataSize = sizeof(tcu::Vec4);
    const VkBufferCreateInfo uniformBufferCreateInfo =
        makeBufferCreateInfo(uniformBufferDataSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    de::MovePtr<BufferWithMemory> uniformBuffer = de::MovePtr<BufferWithMemory>(
        new BufferWithMemory(vk, device, allocator, uniformBufferCreateInfo, MemoryRequirement::HostVisible));
    deMemcpy(uniformBuffer->getAllocation().getHostPtr(), uniformBuffData.getPtr(), (size_t)uniformBufferDataSize);
    flushAlloc(vk, device, uniformBuffer->getAllocation());

    // create descriptor set layouts - first unused, second used
    Move<VkDescriptorSetLayout> descriptorSetLayout{
        DescriptorSetLayoutBuilder()
            .addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(vk, device)};

    Move<VkDescriptorPool> allDescriptorPool =
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1)
            .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1);

    // create descriptor set
    Move<VkDescriptorSet> descriptorSet = makeDescriptorSet(vk, device, *allDescriptorPool, *descriptorSetLayout);

    // update descriptor with actual buffer
    const VkDescriptorBufferInfo shaderBufferInfo =
        makeDescriptorBufferInfo(**uniformBuffer, 0u, uniformBufferDataSize);
    DescriptorSetUpdateBuilder()
        .writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                     VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &shaderBufferInfo)
        .update(vk, device);

    // create a pipeline layout with its first descriptor set layout as VK_NULL_HANDLE
    // and a second with a valid descriptor set layout containing a buffer
    VkDescriptorSet rawDescriptorSets[]             = {VK_NULL_HANDLE, *descriptorSet};
    VkDescriptorSetLayout rawDescriptorSetLayouts[] = {VK_NULL_HANDLE, *descriptorSetLayout};

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = initVulkanStructure();
    pipelineLayoutCreateInfo.setLayoutCount             = 2u;
    pipelineLayoutCreateInfo.pSetLayouts                = rawDescriptorSetLayouts;
    Move<VkPipelineLayout> pipelineLayout               = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);

    // create monolithic graphics pipeline
    GraphicsPipelineCreateInfo pipelineCreateInfo(*pipelineLayout, *m_renderPass, 0, 0u);
    updateVertexInputInterface(m_context, pipelineCreateInfo, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0u);
    updatePreRasterization(m_context, pipelineCreateInfo, false);
    updatePostRasterization(m_context, pipelineCreateInfo, false);
    updateFragmentOutputInterface(m_context, pipelineCreateInfo);
    Move<VkPipeline> pipeline = createGraphicsPipeline(vk, device, VK_NULL_HANDLE, &pipelineCreateInfo);

    vk::beginCommandBuffer(vk, *m_cmdBuffer, 0u);
    {
        // change color image layout
        const VkImageMemoryBarrier initialImageBarrier = makeImageMemoryBarrier(
            0,                                          // VkAccessFlags srcAccessMask;
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,       // VkAccessFlags dstAccessMask;
            VK_IMAGE_LAYOUT_UNDEFINED,                  // VkImageLayout oldLayout;
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,   // VkImageLayout newLayout;
            **m_colorImage,                             // VkImage image;
            {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u} // VkImageSubresourceRange subresourceRange;
        );
        vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                              VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, (VkDependencyFlags)0, 0, nullptr, 0,
                              nullptr, 1, &initialImageBarrier);

        // wait for uniform buffer
        const VkBufferMemoryBarrier initialBufferBarrier =
            makeBufferMemoryBarrier(VK_ACCESS_HOST_WRITE_BIT,   // VkAccessFlags2KHR                srcAccessMask
                                    VK_ACCESS_UNIFORM_READ_BIT, // VkAccessFlags2KHR                dstAccessMask
                                    uniformBuffer->get(),       // VkBuffer                            buffer
                                    0u,                         // VkDeviceSize                        offset
                                    uniformBufferDataSize       // VkDeviceSize                        size
            );
        vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                              (VkDependencyFlags)0, 0, nullptr, 1, &initialBufferBarrier, 0, nullptr);

        beginRenderPass(vk, *m_cmdBuffer, *m_renderPass, *m_framebuffer, m_renderArea, m_colorClearColor);

        vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
        vk.cmdBindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 2u,
                                 rawDescriptorSets, 0u, nullptr);
        vk.cmdDraw(*m_cmdBuffer, 4, 1u, 0u, 0u);

        endRenderPass(vk, *m_cmdBuffer);

        const tcu::IVec2 size{(int32_t)m_renderArea.extent.width, (int32_t)m_renderArea.extent.height};
        copyImageToBuffer(vk, *m_cmdBuffer, **m_colorImage, *colorBuffer, size);
    }
    vk::endCommandBuffer(vk, *m_cmdBuffer);
    vk::submitCommandsAndWait(vk, device, m_context.getUniversalQueue(), *m_cmdBuffer);

    vk::invalidateAlloc(vk, device, colorBuffer.getAllocation());
    const tcu::ConstPixelBufferAccess colorPixelAccess(mapVkFormat(m_colorFormat), m_renderArea.extent.width,
                                                       m_renderArea.extent.height, 1,
                                                       colorBuffer.getAllocation().getHostPtr());

    // verify result
    int32_t width  = (int32_t)m_renderArea.extent.width;
    int32_t height = (int32_t)m_renderArea.extent.height;
    tcu::IVec4 outColor{0, // r is 0 because COLOR_COMPONENTS_NO_RED is used
                        static_cast<int>(uniformBuffData[1] * 255), static_cast<int>(uniformBuffData[2] * 255),
                        static_cast<int>(uniformBuffData[3] * 255)};
    const std::vector<VerificationData> verificationData{
        {{1, 1}, outColor},
        {{width / 2, height / 2}, outColor},
        {{width - 2, height - 2}, {0, 0, 0, 255}} // clear color
    };

    return verifyResult(verificationData, colorPixelAccess);
}

tcu::TestStatus PipelineLibraryMiscTestInstance::runIndependentPipelineLayoutSets(bool useLinkTimeOptimization)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();
    Allocator &allocator      = m_context.getDefaultAllocator();

    const VkDeviceSize colorBufferDataSize = static_cast<VkDeviceSize>(
        m_renderArea.extent.width * m_renderArea.extent.height * tcu::getPixelSize(mapVkFormat(m_colorFormat)));
    const VkBufferCreateInfo colorBufferCreateInfo = makeBufferCreateInfo(
        colorBufferDataSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const BufferWithMemory colorBuffer(vk, device, allocator, colorBufferCreateInfo, MemoryRequirement::HostVisible);

    VkDeviceSize uniformBufferDataSize = sizeof(tcu::Vec4);
    const VkBufferCreateInfo uniformBufferCreateInfo =
        makeBufferCreateInfo(uniformBufferDataSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    de::MovePtr<BufferWithMemory> uniformBuffer[3];

    // setup data in uniform buffers that will give us expected result for validation
    const tcu::Vec4 uniformBuffData[3]{
        {4.00f, 3.00f, -1.0f, 4.00f},
        {0.10f, 0.25f, -0.5f, 0.05f},
        {-5.00f, -2.00f, 3.0f, -6.00f},
    };

    for (uint32_t i = 0; i < 3; ++i)
    {
        uniformBuffer[i] = de::MovePtr<BufferWithMemory>(
            new BufferWithMemory(vk, device, allocator, uniformBufferCreateInfo, MemoryRequirement::HostVisible));
        deMemcpy(uniformBuffer[i]->getAllocation().getHostPtr(), uniformBuffData[i].getPtr(),
                 (size_t)uniformBufferDataSize);
        flushAlloc(vk, device, uniformBuffer[i]->getAllocation());
    }

    // create three descriptor set layouts
    Move<VkDescriptorSetLayout> descriptorSetLayouts[3];
    descriptorSetLayouts[0] = DescriptorSetLayoutBuilder()
                                  .addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
                                  .build(vk, device);
    descriptorSetLayouts[1] = DescriptorSetLayoutBuilder()
                                  .addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
                                  .build(vk, device);
    descriptorSetLayouts[2] = DescriptorSetLayoutBuilder()
                                  .addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
                                  .build(vk, device);

    // for the link time opt (and when null handle is used) use total pipeline layout recreated without the INDEPENDENT SETS bit
    uint32_t allLayoutsFlag = uint32_t(VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT);
    if (useLinkTimeOptimization)
        allLayoutsFlag = 0u;

    // Pre-rasterization stage library has sets 0, 1, 2
    // * set 0 has descriptors
    // * set 1 has no descriptors
    // * set 2 has descriptors
    // Fragment stage library has sets 0, 1
    // * set 0 has descriptors
    // * set 1 has descriptors
    VkDescriptorSetLayout vertDescriptorSetLayouts[] = {*descriptorSetLayouts[0], VK_NULL_HANDLE,
                                                        *descriptorSetLayouts[2]};
    VkDescriptorSetLayout fragDescriptorSetLayouts[] = {*descriptorSetLayouts[0], *descriptorSetLayouts[1]};
    VkDescriptorSetLayout allDescriptorSetLayouts[]  = {*descriptorSetLayouts[0], *descriptorSetLayouts[1],
                                                        *descriptorSetLayouts[2]};

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = initVulkanStructure();
    pipelineLayoutCreateInfo.flags                      = allLayoutsFlag;
    pipelineLayoutCreateInfo.setLayoutCount             = 3u;
    pipelineLayoutCreateInfo.pSetLayouts                = allDescriptorSetLayouts;
    Move<VkPipelineLayout> allLayouts                   = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);
    pipelineLayoutCreateInfo.flags                      = uint32_t(VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT);
    pipelineLayoutCreateInfo.pSetLayouts                = vertDescriptorSetLayouts;
    Move<VkPipelineLayout> vertLayouts                  = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);
    pipelineLayoutCreateInfo.setLayoutCount             = 2u;
    pipelineLayoutCreateInfo.pSetLayouts                = fragDescriptorSetLayouts;
    Move<VkPipelineLayout> fragLayouts                  = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);

    Move<VkDescriptorPool> allDescriptorPool =
        DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3)
            .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 3);

    // create three descriptor sets
    Move<VkDescriptorSet> descriptorSetA = makeDescriptorSet(vk, device, *allDescriptorPool, *descriptorSetLayouts[0]);
    Move<VkDescriptorSet> descriptorSetB = makeDescriptorSet(vk, device, *allDescriptorPool, *descriptorSetLayouts[1]);
    Move<VkDescriptorSet> descriptorSetC = makeDescriptorSet(vk, device, *allDescriptorPool, *descriptorSetLayouts[2]);
    VkDescriptorSet allDescriptorSets[]  = {*descriptorSetA, *descriptorSetB, *descriptorSetC};

    // update descriptors with actual buffers
    const VkDescriptorBufferInfo shaderBufferAInfo =
        makeDescriptorBufferInfo(**uniformBuffer[0], 0u, uniformBufferDataSize);
    const VkDescriptorBufferInfo shaderBufferBInfo =
        makeDescriptorBufferInfo(**uniformBuffer[1], 0u, uniformBufferDataSize);
    const VkDescriptorBufferInfo shaderBufferCInfo =
        makeDescriptorBufferInfo(**uniformBuffer[2], 0u, uniformBufferDataSize);
    DescriptorSetUpdateBuilder()
        .writeSingle(*descriptorSetA, DescriptorSetUpdateBuilder::Location::binding(0u),
                     VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &shaderBufferAInfo)
        .writeSingle(*descriptorSetB, DescriptorSetUpdateBuilder::Location::binding(0u),
                     VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &shaderBufferBInfo)
        .writeSingle(*descriptorSetC, DescriptorSetUpdateBuilder::Location::binding(0u),
                     VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &shaderBufferCInfo)
        .update(vk, device);

    uint32_t commonPipelinePartFlags = uint32_t(VK_PIPELINE_CREATE_LIBRARY_BIT_KHR);
    uint32_t finalPipelineFlag       = 0u;
    if (useLinkTimeOptimization)
    {
        commonPipelinePartFlags |= uint32_t(VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT);
        finalPipelineFlag = uint32_t(VK_PIPELINE_CREATE_LINK_TIME_OPTIMIZATION_BIT_EXT);
    }

    GraphicsPipelineCreateInfo partialPipelineCreateInfo[]{{VK_NULL_HANDLE, *m_renderPass, 0, commonPipelinePartFlags},
                                                           {*vertLayouts, *m_renderPass, 0, commonPipelinePartFlags},
                                                           {*fragLayouts, *m_renderPass, 0, commonPipelinePartFlags},
                                                           {VK_NULL_HANDLE, *m_renderPass, 0, commonPipelinePartFlags}};

    // fill proper portion of pipeline state
    updateVertexInputInterface(m_context, partialPipelineCreateInfo[0], VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0u);
    updatePreRasterization(m_context, partialPipelineCreateInfo[1], false);
    updatePostRasterization(m_context, partialPipelineCreateInfo[2], false);
    updateFragmentOutputInterface(m_context, partialPipelineCreateInfo[3]);

    // extend pNext chain and create all partial pipelines
    std::vector<VkPipeline> rawParts(4u, VK_NULL_HANDLE);
    std::vector<Move<VkPipeline>> pipelineParts;
    pipelineParts.reserve(4u);
    VkGraphicsPipelineLibraryCreateInfoEXT libraryCreateInfo =
        makeGraphicsPipelineLibraryCreateInfo(VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT);
    for (uint32_t i = 0; i < 4u; ++i)
    {
        libraryCreateInfo.flags = GRAPHICS_PIPELINE_LIBRARY_FLAGS[i];
        appendStructurePtrToVulkanChain(&partialPipelineCreateInfo[i].pNext, &libraryCreateInfo);
        pipelineParts.emplace_back(createGraphicsPipeline(vk, device, VK_NULL_HANDLE, &partialPipelineCreateInfo[i]));
        rawParts[i] = *pipelineParts[i];
    }

    // create final pipeline out of four parts
    VkPipelineLibraryCreateInfoKHR linkingInfo     = makePipelineLibraryCreateInfo(rawParts);
    VkGraphicsPipelineCreateInfo finalPipelineInfo = initVulkanStructure();

    finalPipelineInfo.flags  = finalPipelineFlag;
    finalPipelineInfo.layout = *allLayouts;

    appendStructurePtrToVulkanChain(&finalPipelineInfo.pNext, &linkingInfo);
    Move<VkPipeline> pipeline = createGraphicsPipeline(vk, device, VK_NULL_HANDLE, &finalPipelineInfo);

    vk::beginCommandBuffer(vk, *m_cmdBuffer, 0u);
    {
        // change color image layout
        const VkImageMemoryBarrier initialImageBarrier = makeImageMemoryBarrier(
            0,                                          // VkAccessFlags srcAccessMask;
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,       // VkAccessFlags dstAccessMask;
            VK_IMAGE_LAYOUT_UNDEFINED,                  // VkImageLayout oldLayout;
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,   // VkImageLayout newLayout;
            **m_colorImage,                             // VkImage image;
            {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u} // VkImageSubresourceRange subresourceRange;
        );
        vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                              VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, (VkDependencyFlags)0, 0, nullptr, 0,
                              nullptr, 1, &initialImageBarrier);

        // wait for uniform buffers
        std::vector<VkBufferMemoryBarrier> initialBufferBarriers(
            3u, makeBufferMemoryBarrier(VK_ACCESS_HOST_WRITE_BIT,   // VkAccessFlags2KHR                srcAccessMask
                                        VK_ACCESS_UNIFORM_READ_BIT, // VkAccessFlags2KHR                dstAccessMask
                                        uniformBuffer[0]->get(),    // VkBuffer                            buffer
                                        0u,                         // VkDeviceSize                        offset
                                        uniformBufferDataSize       // VkDeviceSize                        size
                                        ));
        initialBufferBarriers[1].buffer = uniformBuffer[1]->get();
        initialBufferBarriers[2].buffer = uniformBuffer[2]->get();
        vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                              (VkDependencyFlags)0, 0, nullptr, 3, initialBufferBarriers.data(), 0, nullptr);

        beginRenderPass(vk, *m_cmdBuffer, *m_renderPass, *m_framebuffer, m_renderArea, m_colorClearColor);

        vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
        vk.cmdBindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *allLayouts, 0u, 3u, allDescriptorSets,
                                 0u, nullptr);
        vk.cmdDraw(*m_cmdBuffer, 4, 1u, 0u, 0u);

        endRenderPass(vk, *m_cmdBuffer);

        const tcu::IVec2 size{(int32_t)m_renderArea.extent.width, (int32_t)m_renderArea.extent.height};
        copyImageToBuffer(vk, *m_cmdBuffer, **m_colorImage, *colorBuffer, size);
    }
    vk::endCommandBuffer(vk, *m_cmdBuffer);
    vk::submitCommandsAndWait(vk, device, m_context.getUniversalQueue(), *m_cmdBuffer);

    vk::invalidateAlloc(vk, device, colorBuffer.getAllocation());
    const tcu::ConstPixelBufferAccess colorPixelAccess(mapVkFormat(m_colorFormat), m_renderArea.extent.width,
                                                       m_renderArea.extent.height, 1,
                                                       colorBuffer.getAllocation().getHostPtr());

    // verify result
    int32_t width  = (int32_t)m_renderArea.extent.width;
    int32_t height = (int32_t)m_renderArea.extent.height;
    const std::vector<VerificationData> verificationData{
        {{1, 1}, {0, 191, 127, 51}}, // note COLOR_COMPONENTS_NO_RED is used
        {{width / 2, height / 2}, {0, 191, 127, 51}},
        {{width - 2, height - 2}, {0, 0, 0, 255}} // clear color
    };
    return verifyResult(verificationData, colorPixelAccess);
}

tcu::TestStatus PipelineLibraryMiscTestInstance::runCompareLinkTimes(void)
{
    const uint32_t uniqueLibrariesCount = 2u;
    const uint32_t pipelinesCount       = 4u * uniqueLibrariesCount;

    const DeviceInterface &vk                       = m_context.getDeviceInterface();
    const VkDevice device                           = m_context.getDevice();
    tcu::TestLog &log                               = m_context.getTestContext().getLog();
    bool allChecksPassed                            = true;
    VkPipelineLayoutCreateInfo pipelineLayoutParams = initVulkanStructure();
    Move<VkPipelineLayout> layout                   = createPipelineLayout(vk, device, &pipelineLayoutParams);

    GraphicsPipelineCreateInfo partialPipelineCreateInfo[]{
        {*layout, *m_renderPass, 0, VK_PIPELINE_CREATE_LIBRARY_BIT_KHR},
        {*layout, *m_renderPass, 0, VK_PIPELINE_CREATE_LIBRARY_BIT_KHR},
        {*layout, *m_renderPass, 0, VK_PIPELINE_CREATE_LIBRARY_BIT_KHR},
        {*layout, *m_renderPass, 0, VK_PIPELINE_CREATE_LIBRARY_BIT_KHR},
        {*layout, *m_renderPass, 0, VK_PIPELINE_CREATE_LIBRARY_BIT_KHR},
        {*layout, *m_renderPass, 0, VK_PIPELINE_CREATE_LIBRARY_BIT_KHR},
        {*layout, *m_renderPass, 0, VK_PIPELINE_CREATE_LIBRARY_BIT_KHR},
        {*layout, *m_renderPass, 0, VK_PIPELINE_CREATE_LIBRARY_BIT_KHR},
    };

    de::Random rnd(static_cast<uint32_t>(deGetMicroseconds()));

    const uint32_t vertexRandSpecConsts[]   = {rnd.getUint32() * 2, rnd.getUint32() * 2};
    const uint32_t fragmentRandSpecConsts[] = {rnd.getUint32() * 2, rnd.getUint32() * 2};

    const VkSpecializationMapEntry entry = {
        0,              // uint32_t constantID;
        0,              // uint32_t offset;
        sizeof(int32_t) // size_t size;
    };

    const VkSpecializationInfo vertexSpecializationInfos[] = {
        {
            1u,                      // uint32_t mapEntryCount;
            &entry,                  // const VkSpecializationMapEntry* pMapEntries;
            sizeof(int32_t),         // size_t dataSize;
            &vertexRandSpecConsts[0] // const void* pData;
        },
        {
            1u,                      // uint32_t mapEntryCount;
            &entry,                  // const VkSpecializationMapEntry* pMapEntries;
            sizeof(int32_t),         // size_t dataSize;
            &vertexRandSpecConsts[1] // const void* pData;
        }};

    const VkSpecializationInfo fragmentSpecializationInfos[] = {
        {
            1u,                        // uint32_t mapEntryCount;
            &entry,                    // const VkSpecializationMapEntry* pMapEntries;
            sizeof(int32_t),           // size_t dataSize;
            &fragmentRandSpecConsts[0] // const void* pData;
        },
        {
            1u,                        // uint32_t mapEntryCount;
            &entry,                    // const VkSpecializationMapEntry* pMapEntries;
            sizeof(int32_t),           // size_t dataSize;
            &fragmentRandSpecConsts[1] // const void* pData;
        }};

    // fill proper portion of pipeline state - this cant be easily done in a scalable loop
    updateVertexInputInterface(m_context, partialPipelineCreateInfo[0], VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
    updateVertexInputInterface(m_context, partialPipelineCreateInfo[1], VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    updatePreRasterization(m_context, partialPipelineCreateInfo[2], false, false, false, VK_POLYGON_MODE_FILL,
                           &vertexSpecializationInfos[0]);
    updatePreRasterization(m_context, partialPipelineCreateInfo[3], false, false, false, VK_POLYGON_MODE_LINE,
                           &vertexSpecializationInfos[1]);
    updatePostRasterization(m_context, partialPipelineCreateInfo[4], false, true, &fragmentSpecializationInfos[0]);
    updatePostRasterization(m_context, partialPipelineCreateInfo[5], false, false, &fragmentSpecializationInfos[1]);
    updateFragmentOutputInterface(m_context, partialPipelineCreateInfo[6], 0xf);
    updateFragmentOutputInterface(m_context, partialPipelineCreateInfo[7]);

    // construct all pipeline parts and mesure time it took
    struct PipelinePartData
    {
        Move<VkPipeline> pipelineHandle;
        std::chrono::duration<int64_t, std::nano> creationDuration;
    };
    std::vector<PipelinePartData> pipelinePartData(pipelinesCount);
    VkGraphicsPipelineLibraryCreateInfoEXT libraryCreateInfo =
        makeGraphicsPipelineLibraryCreateInfo(VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT);
    for (uint32_t i = 0; i < pipelinesCount; ++i)
    {
        appendStructurePtrToVulkanChain(&partialPipelineCreateInfo[i].pNext, &libraryCreateInfo);
        libraryCreateInfo.flags = GRAPHICS_PIPELINE_LIBRARY_FLAGS[i / 2];

        auto &partData            = pipelinePartData[i];
        auto timeStart            = std::chrono::high_resolution_clock::now();
        partData.pipelineHandle   = createGraphicsPipeline(vk, device, VK_NULL_HANDLE, partialPipelineCreateInfo + i);
        partData.creationDuration = std::chrono::high_resolution_clock::now() - timeStart;
    }

    // iterate over all combinations of parts
    for (uint32_t i = 0u; i < (uint32_t)dePow(4, uniqueLibrariesCount); ++i)
    {
        // select new unique combination of parts
        uint32_t vertexInputIndex      = (i) % 2;
        uint32_t preRasterizationIndex = (i / 2) % 2;
        uint32_t fragmentStateIndex    = (i / 4) % 2;
        uint32_t fragmentOutputIndex   = (i / 8) % 2;

        const auto &vertexInputData      = pipelinePartData[vertexInputIndex];
        const auto &preRasterizationData = pipelinePartData[uniqueLibrariesCount + preRasterizationIndex];
        const auto &fragmentStateData    = pipelinePartData[2 * uniqueLibrariesCount + fragmentStateIndex];
        const auto &fragmentOutputData   = pipelinePartData[3 * uniqueLibrariesCount + fragmentOutputIndex];

        std::vector<VkPipeline> pipelinesToLink{
            *vertexInputData.pipelineHandle,
            *preRasterizationData.pipelineHandle,
            *fragmentStateData.pipelineHandle,
            *fragmentOutputData.pipelineHandle,
        };

        VkPipelineLibraryCreateInfoKHR linkingInfo     = makePipelineLibraryCreateInfo(pipelinesToLink);
        VkGraphicsPipelineCreateInfo finalPipelineInfo = initVulkanStructure();
        finalPipelineInfo.layout                       = *layout;

        appendStructurePtrToVulkanChain(&finalPipelineInfo.pNext, &linkingInfo);

        // link pipeline without the optimised bit, and record the time taken to link it
        auto timeStart            = std::chrono::high_resolution_clock::now();
        Move<VkPipeline> pipeline = createGraphicsPipeline(vk, device, VK_NULL_HANDLE, &finalPipelineInfo);
        const auto linkingTime    = std::chrono::high_resolution_clock::now() - timeStart;
        const auto creationTime   = preRasterizationData.creationDuration + fragmentStateData.creationDuration;

        if (linkingTime > (10 * creationTime))
        {
            allChecksPassed = false;
            log << tcu::TestLog::Message << "Liking time (" << linkingTime.count() << ") of combination " << i
                << " is more then ten times greater than creation of both pre-rasterization and fragment states ("
                << creationTime.count() << ")" << tcu::TestLog::EndMessage;
        }
    }

    if (allChecksPassed)
        return tcu::TestStatus::pass("Pass");

    return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "Liking of one or more combinations took to long");
}

/*
Middle bar should contain clip distance with linear values between 0 and 1.
Cull distance is always 0.5 when enabled.
*/
void makeReferenceImage(tcu::PixelBufferAccess &reference, IVec2 clipRegion, const int numBars, int barIdx,
                        const tcu::Vec4 &clipAreaColor)
{
    for (int y = 0; y < reference.getHeight(); ++y)
        for (int x = 0; x < reference.getWidth(); ++x)
        {
            if (x < clipRegion.x() && y < clipRegion.y())
            {
                reference.setPixel(clipAreaColor, x, y);
                continue;
            }

            const int barWidth   = reference.getWidth() / numBars;
            const bool insideBar = x >= barWidth * barIdx && x < barWidth * (barIdx + 1);
            const float expectedClipDistance =
                insideBar ? (((((float)y + 0.5f) / (float)reference.getHeight()) - 0.5f) * 2.0f) : 0.0f;
            float expectedCullDistance = 0.5f;
            const float height         = (float)reference.getHeight();

            if (y >= (reference.getHeight() / 2))
                expectedCullDistance = expectedCullDistance * (1.0f + (2.0f * (float)y) - height) / height;
            else
                expectedCullDistance = 0.0f;

            const tcu::Vec4 expectedColor = Vec4(1.0, expectedClipDistance, expectedCullDistance, 1.0);
            reference.setPixel(expectedColor, x, y);
        }
}

de::MovePtr<BufferWithMemory> makeVertexBuffer(const DeviceInterface &vk, const VkDevice device, Allocator &allocator,
                                               std::vector<tcu::Vec4> &vertexData, VkBufferUsageFlagBits usageFlags)
{
    const size_t bufferDataSize               = de::dataSize(vertexData);
    const VkBufferCreateInfo bufferCreateInfo = makeBufferCreateInfo(bufferDataSize, usageFlags);
    de::MovePtr<BufferWithMemory> buffer      = de::MovePtr<BufferWithMemory>(
        new BufferWithMemory(vk, device, allocator, bufferCreateInfo, MemoryRequirement::HostVisible));

    deMemcpy(buffer->getAllocation().getHostPtr(), vertexData.data(), bufferDataSize);
    flushAlloc(vk, device, buffer->getAllocation());

    return buffer;
}

/*
Pipeline libraries:
    Compile a fragment only pipeline library L1.
    Compile a mesh only pipeline library L2.
    Compile a vertex only pipeline library L3.
    Fast link L2 & L1.
    Fast link L3 & L1.
Shaders:
    Vertex and mesh shaders write clip distance and cull distance.
    Fragment shader reads clip distance and cull distance.
    Clip and cull tests taken from vktClippingTests.
*/
tcu::TestStatus PipelineLibraryMiscTestInstance::runCommonFragLibraryTest(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();
    Allocator &allocator      = m_context.getDefaultAllocator();

    // create output buffer for verification
    const VkDeviceSize outputBufferDataSize = static_cast<VkDeviceSize>(
        m_renderArea.extent.width * m_renderArea.extent.height * tcu::getPixelSize(mapVkFormat(m_colorFormat)));
    const VkBufferCreateInfo outputBufferCreateInfo = makeBufferCreateInfo(
        outputBufferDataSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const BufferWithMemory outputBuffer(vk, device, allocator, outputBufferCreateInfo, MemoryRequirement::HostVisible);

    const int numBars = numClipDistances + numCullDistances;

    // vertex shader input
    std::vector<Vec4> vertices;
    {
        const float dx = 2.0f / numBars;
        for (int i = 0; i < numBars; ++i)
        {
            const float x = -1.0f + dx * static_cast<float>(i);

            vertices.push_back(Vec4(x, -1.0f, 0.0f, 1.0f));
            vertices.push_back(Vec4(x, 1.0f, 0.0f, 1.0f));
            vertices.push_back(Vec4(x + dx, -1.0f, 0.0f, 1.0f));

            vertices.push_back(Vec4(x, 1.0f, 0.0f, 1.0f));
            vertices.push_back(Vec4(x + dx, 1.0f, 0.0f, 1.0f));
            vertices.push_back(Vec4(x + dx, -1.0f, 0.0f, 1.0f));
        }
    }

    const auto vertexBufferStages = (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_MESH_BIT_EXT);
    const auto vertexBufferSize   = static_cast<VkDeviceSize>(de::dataSize(vertices));
    const auto vertexCount        = de::sizeU32(vertices);
    const auto vertexBufferUsage  = (VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    const auto vertexBufferLoc    = DescriptorSetUpdateBuilder::Location::binding(0u);
    const auto vertexBufferType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

    // Vertex buffer.
    const de::MovePtr<BufferWithMemory> vertexBuffer =
        makeVertexBuffer(vk, device, allocator, vertices, (VkBufferUsageFlagBits)vertexBufferUsage);

    // for the link time opt (and when null handle is used) use total pipeline layout recreated without the INDEPENDENT SETS bit
    const auto allLayoutsFlag = VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT;

    // Set layout.
    DescriptorSetLayoutBuilder layoutBuilder;
    layoutBuilder.addSingleBinding(vertexBufferType, vertexBufferStages);
    const auto descriptorSetLayout = layoutBuilder.build(vk, device);

    // Descriptor pool.
    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(vertexBufferType);
    const auto descriptorPool = poolBuilder.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

    // Descriptor set.
    const auto descriptorSet = makeDescriptorSet(vk, device, descriptorPool.get(), descriptorSetLayout.get());

    // Update descriptor set.
    DescriptorSetUpdateBuilder updateBuilder;
    const auto vertexBufferDescInfo = makeDescriptorBufferInfo(vertexBuffer->get(), 0ull, vertexBufferSize);
    updateBuilder.writeSingle(descriptorSet.get(), vertexBufferLoc, vertexBufferType, &vertexBufferDescInfo);
    updateBuilder.update(vk, device);

    // Setup pipeline libraries
    VkDescriptorSet allDescriptorSets[] = {*descriptorSet};

    VkDescriptorSetLayout meshDescriptorSetLayouts[] = {*descriptorSetLayout};
    VkDescriptorSetLayout allDescriptorSetLayouts[]  = {*descriptorSetLayout};

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = initVulkanStructure();
    pipelineLayoutCreateInfo.flags                      = allLayoutsFlag;
    pipelineLayoutCreateInfo.setLayoutCount             = 1u;
    pipelineLayoutCreateInfo.pSetLayouts                = allDescriptorSetLayouts;
    Move<VkPipelineLayout> allLayouts                   = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);

    pipelineLayoutCreateInfo.pSetLayouts = meshDescriptorSetLayouts;
    Move<VkPipelineLayout> meshLayouts   = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);

    pipelineLayoutCreateInfo.setLayoutCount = 0u;
    pipelineLayoutCreateInfo.pSetLayouts    = nullptr;
    Move<VkPipelineLayout> vertLayouts      = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);
    Move<VkPipelineLayout> fragLayouts      = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);
    Move<VkPipelineLayout> nullLayout       = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo);

    const uint32_t commonPipelinePartFlags = uint32_t(VK_PIPELINE_CREATE_LIBRARY_BIT_KHR);

    enum
    {
        PIPELINE_CREATEINFO_IDX_VII = 0,
        PIPELINE_CREATEINFO_IDX_PRERAST_VERT,
        PIPELINE_CREATEINFO_IDX_PRERAST_MESH,
        PIPELINE_CREATEINFO_IDX_POSTRAST,
        PIPELINE_CREATEINFO_IDX_FO,
        PIPELINE_CREATEINFO_IDX_MAX
    };

    const VkGraphicsPipelineLibraryFlagBitsEXT map_pipeline_createinfo_to_flags[PIPELINE_CREATEINFO_IDX_MAX] = {
        VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT,
        VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT, // pre-rasterization (vert)
        VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT, // pre-rasterization (mesh)
        VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT,
        VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT};

    GraphicsPipelineCreateInfo allPipelineCreateInfos[]{
        {VK_NULL_HANDLE, *m_renderPass, 0,
         commonPipelinePartFlags}, // [PIPELINE_CREATEINFO_IDX_VII]: vertex input interface
        {*vertLayouts, *m_renderPass, 0,
         commonPipelinePartFlags}, // [PIPELINE_CREATEINFO_IDX_PRERAST_VERT]: pre-rasterization (vert)
        {*meshLayouts, *m_renderPass, 0,
         commonPipelinePartFlags}, // [PIPELINE_CREATEINFO_IDX_PRERAST_MESH]: pre-rasterization (mesh)
        {*fragLayouts, *m_renderPass, 0,
         commonPipelinePartFlags}, // [PIPELINE_CREATEINFO_IDX_POSTRAST]: post-rasterization (frag)
        {VK_NULL_HANDLE, *m_renderPass, 0,
         commonPipelinePartFlags}, // [PIPELINE_CREATEINFO_IDX_FO]: frag output interface
    };

    // initialize VkGraphicsPipelineLibraryCreateInfoEXT for each library
    std::vector<VkGraphicsPipelineLibraryCreateInfoEXT> libraryCreateInfos;
    for (uint32_t i = 0; i < PIPELINE_CREATEINFO_IDX_MAX; i++)
    {
        VkGraphicsPipelineLibraryFlagBitsEXT flag = map_pipeline_createinfo_to_flags[i];
        libraryCreateInfos.push_back(makeGraphicsPipelineLibraryCreateInfo(flag));
    }

    // vertex-only pipeline parts
    uint32_t pipelineCreateInfoIdx = PIPELINE_CREATEINFO_IDX_VII;
    updateVertexInputInterface(m_context, allPipelineCreateInfos[pipelineCreateInfoIdx],
                               VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 1u);
    appendStructurePtrToVulkanChain(&allPipelineCreateInfos[pipelineCreateInfoIdx].pNext,
                                    &libraryCreateInfos[pipelineCreateInfoIdx]);

    pipelineCreateInfoIdx = PIPELINE_CREATEINFO_IDX_PRERAST_VERT;
    updatePreRasterization(m_context, allPipelineCreateInfos[pipelineCreateInfoIdx], false, true);
    appendStructurePtrToVulkanChain(&allPipelineCreateInfos[pipelineCreateInfoIdx].pNext,
                                    &libraryCreateInfos[pipelineCreateInfoIdx]);

    // mesh-only pipeline parts
    pipelineCreateInfoIdx = PIPELINE_CREATEINFO_IDX_PRERAST_MESH;
    updatePreRasterization(m_context, allPipelineCreateInfos[pipelineCreateInfoIdx], false, true, true);
    appendStructurePtrToVulkanChain(&allPipelineCreateInfos[pipelineCreateInfoIdx].pNext,
                                    &libraryCreateInfos[pipelineCreateInfoIdx]);

    // fragment-only pipeline parts, always linked
    pipelineCreateInfoIdx = PIPELINE_CREATEINFO_IDX_POSTRAST;
    updatePostRasterization(m_context, allPipelineCreateInfos[PIPELINE_CREATEINFO_IDX_POSTRAST], false, false);
    appendStructurePtrToVulkanChain(&allPipelineCreateInfos[pipelineCreateInfoIdx].pNext,
                                    &libraryCreateInfos[pipelineCreateInfoIdx]);

    pipelineCreateInfoIdx = PIPELINE_CREATEINFO_IDX_FO;
    updateFragmentOutputInterface(m_context, allPipelineCreateInfos[pipelineCreateInfoIdx], ALL_COLOR_COMPONENTS);
    appendStructurePtrToVulkanChain(&allPipelineCreateInfos[pipelineCreateInfoIdx].pNext,
                                    &libraryCreateInfos[pipelineCreateInfoIdx]);

    // final pipeline libraries, pipelines[0]: vertex+frag and pipelines[1]: mesh+frag
    std::vector<Move<VkPipeline>> pipelines;
    pipelines.reserve(2u);

    enum
    {
        PIPELINE_LIB_VERT_FRAG = 0,
        PIPELINE_LIB_MESH_FRAG,
        PIPELINE_LIB_MAX
    };

    // create parts of each of the two final pipelines and then create the final pipelines
    std::vector<VkPipeline> rawParts[PIPELINE_LIB_MAX];
    std::vector<Move<VkPipeline>> pipelineParts[PIPELINE_LIB_MAX];
    for (uint32_t combo = PIPELINE_LIB_VERT_FRAG; combo < PIPELINE_LIB_MAX; combo++)
    {
        uint32_t numParts = 0;
        std::vector<uint32_t> createInfoIndices;
        VkGraphicsPipelineCreateInfo finalPipelineInfo = initVulkanStructure();
        finalPipelineInfo.flags                        = 0u;

        if (combo == PIPELINE_LIB_VERT_FRAG)
        {
            // pipeline parts are 4 for vertex+frag pipeline
            // vertex inout interface, pre-rasterization (vertex), post-rasterization, frag output interface
            numParts                 = 4u;
            finalPipelineInfo.layout = *nullLayout;
            createInfoIndices.push_back(PIPELINE_CREATEINFO_IDX_VII);
            createInfoIndices.push_back(PIPELINE_CREATEINFO_IDX_PRERAST_VERT);
            createInfoIndices.push_back(PIPELINE_CREATEINFO_IDX_POSTRAST);
            createInfoIndices.push_back(PIPELINE_CREATEINFO_IDX_FO);
        }
        else
        {
            // pipeline parts are 3 for mesh+frag pipeline
            // pre-rasterization (mesh), post-rasterization, frag output interface
            numParts                 = 3u;
            finalPipelineInfo.layout = *allLayouts;
            createInfoIndices.push_back(PIPELINE_CREATEINFO_IDX_PRERAST_MESH);
            createInfoIndices.push_back(PIPELINE_CREATEINFO_IDX_POSTRAST);
            createInfoIndices.push_back(PIPELINE_CREATEINFO_IDX_FO);
        }

        // extend pNext chain and create all partial pipelines
        rawParts[combo].resize(numParts, VK_NULL_HANDLE);
        pipelineParts[combo].reserve(numParts);

        uint32_t partsIdx = 0;
        for (const auto &idx : createInfoIndices)
        {
            pipelineParts[combo].emplace_back(
                createGraphicsPipeline(vk, device, VK_NULL_HANDLE, &allPipelineCreateInfos[idx]));
            rawParts[combo][partsIdx] = *(pipelineParts[combo][partsIdx]);
            partsIdx++;
        }

        // create final pipeline out of the parts
        VkPipelineLibraryCreateInfoKHR linkingInfo = makePipelineLibraryCreateInfo(rawParts[combo]);
        appendStructurePtrToVulkanChain(&finalPipelineInfo.pNext, &linkingInfo);
        pipelines.emplace_back(createGraphicsPipeline(vk, device, VK_NULL_HANDLE, &finalPipelineInfo));
    }

    // execute both pipelines one after the other and verify the result of each
    bool testOk               = true;
    const VkViewport viewport = makeViewport(m_renderArea.extent.width, m_renderArea.extent.height);
    const VkRect2D scissor    = makeRect2D(m_renderArea.extent.width, m_renderArea.extent.height);

    for (uint32_t combo = PIPELINE_LIB_VERT_FRAG; (combo < PIPELINE_LIB_MAX) && (testOk != false); combo++)
    {
        // only the render pass is shared between the two pipelines
        const VkImageCreateInfo colorImageCreateInfo =
            makeColorImageCreateInfo(m_colorFormat, m_renderArea.extent.width, m_renderArea.extent.height);
        de::MovePtr<ImageWithMemory> localColorImage = de::MovePtr<ImageWithMemory>(
            new ImageWithMemory(vk, device, allocator, colorImageCreateInfo, MemoryRequirement::Any));
        const VkImageViewCreateInfo colorImageViewCreateInfo = makeImageViewCreateInfo(
            **localColorImage, m_colorFormat, static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_COLOR_BIT));
        const Move<VkImageView> colorImageView = createImageView(vk, device, &colorImageViewCreateInfo);

        const VkFramebufferCreateInfo framebufferCreateInfo = makeFramebufferCreateInfo(
            *m_renderPass, 1u, &*colorImageView, m_renderArea.extent.width, m_renderArea.extent.height);
        Move<VkFramebuffer> localFramebuffer = createFramebuffer(vk, device, &framebufferCreateInfo);

        Move<VkCommandBuffer> localCmdBuffer =
            allocateCommandBuffer(vk, device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        vk::beginCommandBuffer(vk, *localCmdBuffer, 0u);
        {

            const VkDeviceSize zeroOffset = 0ull;
            beginRenderPass(vk, *localCmdBuffer, *m_renderPass, *localFramebuffer, m_renderArea, m_colorClearColor);

            vk.cmdBindPipeline(*localCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelines[combo]);
            vk.cmdSetViewport(*localCmdBuffer, 0, 1, &viewport);
            vk.cmdSetScissor(*localCmdBuffer, 0, 1, &scissor);

            if (combo == PIPELINE_LIB_VERT_FRAG)
            {
                vk.cmdBindVertexBuffers(*localCmdBuffer, 0u, 1u, &vertexBuffer->get(), &zeroOffset);
                vk.cmdDraw(*localCmdBuffer, vertexCount, 1u, 0u, 0u);
            }
            else
            {
                vk.cmdBindDescriptorSets(*localCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *allLayouts, 0u, 1u,
                                         allDescriptorSets, 0u, nullptr);
                uint32_t num_workgroups = 1u;
                vk.cmdDrawMeshTasksEXT(*localCmdBuffer, num_workgroups, 1u, 1u);
            }

            endRenderPass(vk, *localCmdBuffer);

            const tcu::IVec2 size{(int32_t)m_renderArea.extent.width, (int32_t)m_renderArea.extent.height};
            copyImageToBuffer(vk, *localCmdBuffer, **localColorImage, *outputBuffer, size);
        }
        vk::endCommandBuffer(vk, *localCmdBuffer);
        vk::submitCommandsAndWait(vk, device, m_context.getUniversalQueue(), *localCmdBuffer);

        {
            vk::invalidateAlloc(vk, device, outputBuffer.getAllocation());

            const tcu::TextureFormat tcuFormat = vk::mapVkFormat(m_colorFormat);
            const tcu::ConstPixelBufferAccess resultAccess(tcuFormat, m_renderArea.extent.width,
                                                           m_renderArea.extent.height, 1,
                                                           outputBuffer.getAllocation().getHostPtr());
            testOk = verifyOnePipelineLibraryResult(resultAccess, numBars);
        }
    }

    return (testOk == true ? tcu::TestStatus::pass("OK") : tcu::TestStatus::fail("Rendered image(s) are incorrect"));
}

tcu::TestStatus PipelineLibraryMiscTestInstance::verifyResult(const std::vector<VerificationData> &verificationData,
                                                              const tcu::ConstPixelBufferAccess &colorPixelAccess) const
{
    const int32_t epsilon = 1;
    for (const auto &v : verificationData)
    {
        const IVec4 pixel = colorPixelAccess.getPixelInt(v.point.x(), v.point.y());
        const IVec4 diff  = pixel - v.color;
        for (uint32_t compNdx = 0; compNdx < 4u; ++compNdx)
        {
            if (de::abs(diff[compNdx]) > epsilon)
            {
                const Vec4 pixelBias(0.0f);
                const Vec4 pixelScale(1.0f);

                m_context.getTestContext().getLog()
                    << TestLog::Image("Result", "Result", colorPixelAccess, pixelScale, pixelBias)
                    << tcu::TestLog::Message << "For texel " << v.point << " expected color " << v.color
                    << " got: " << pixel << tcu::TestLog::EndMessage;

                return tcu::TestStatus::fail("Fail");
            }
        }
    }

    return tcu::TestStatus::pass("Pass");
}

bool PipelineLibraryMiscTestInstance::verifyOnePipelineLibraryResult(const tcu::ConstPixelBufferAccess &resultAccess,
                                                                     const int numBars) const
{
    bool testOk       = true;
    tcu::TestLog &log = m_context.getTestContext().getLog();

    const tcu::TextureFormat tcuFormat = vk::mapVkFormat(m_colorFormat);
    tcu::TextureLevel referenceLevel(tcuFormat, m_renderArea.extent.width, m_renderArea.extent.height);
    auto referenceAccess          = referenceLevel.getAccess();
    const tcu::Vec4 bgColor       = Vec4(1.0f, 0.0f, 0.0f, 1.0f); // red
    const tcu::Vec4 clipAreaColor = Vec4(0.0f, 0.0f, 0.0f, 1.0f); // black
    const IVec2 clipRegion =
        IVec2(numClipDistances * m_renderArea.extent.width / numBars, m_renderArea.extent.height / 2);
    tcu::clear(referenceAccess, bgColor);
    makeReferenceImage(referenceAccess, clipRegion, numBars, numClipDistances / 2, clipAreaColor);

    const float colorThres = 0.005f; // 1/255 < 0.005 < 2/255
    const tcu::Vec4 threshold(0.0f, colorThres, colorThres, 0.0f);

    if (!tcu::floatThresholdCompare(log, "Result", "Reference", referenceAccess, resultAccess, threshold,
                                    tcu::COMPARE_LOG_ON_ERROR))
        testOk = false;

    return testOk;
}

class PipelineLibraryShaderModuleInfoInstance : public TestInstance
{
public:
    PipelineLibraryShaderModuleInfoInstance(Context &context)
        : TestInstance(context)
        , m_vkd(m_context.getDeviceInterface())
        , m_device(m_context.getDevice())
        , m_alloc(m_context.getDefaultAllocator())
        , m_queueIndex(m_context.getUniversalQueueFamilyIndex())
        , m_queue(m_context.getUniversalQueue())
        , m_outVector(kOutputBufferElements, std::numeric_limits<uint32_t>::max())
        , m_cmdBuffer(nullptr)
    {
    }
    virtual ~PipelineLibraryShaderModuleInfoInstance(void)
    {
    }

    static constexpr size_t kOutputBufferElements = 64u;

protected:
    void prepareOutputBuffer(VkShaderStageFlags stages);
    void allocateCmdBuffers(void);
    void addModule(const std::string &moduleName, VkShaderStageFlagBits stage);
    void recordShaderToHostBarrier(VkPipelineStageFlagBits pipelineStage) const;
    void verifyOutputBuffer(void);

    using BufferWithMemoryPtr = de::MovePtr<BufferWithMemory>;

    // From the context.
    const DeviceInterface &m_vkd;
    const VkDevice m_device;
    Allocator &m_alloc;
    const uint32_t m_queueIndex;
    const VkQueue m_queue;

    Move<VkDescriptorSetLayout> m_setLayout;
    Move<VkDescriptorPool> m_descriptorPool;
    Move<VkDescriptorSet> m_descriptorSet;
    std::vector<uint32_t> m_outVector;
    BufferWithMemoryPtr m_outputBuffer;

    Move<VkCommandPool> m_cmdPool;
    Move<VkCommandBuffer> m_cmdBufferPtr;
    VkCommandBuffer m_cmdBuffer;

    std::vector<VkPipelineShaderStageCreateInfo> m_pipelineStageInfos;
    std::vector<VkShaderModuleCreateInfo> m_shaderModuleInfos;
};

void PipelineLibraryShaderModuleInfoInstance::prepareOutputBuffer(VkShaderStageFlags stages)
{
    const auto descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    const auto poolFlags      = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    // Create set layout.
    DescriptorSetLayoutBuilder layoutBuilder;
    layoutBuilder.addSingleBinding(descriptorType, stages);
    m_setLayout = layoutBuilder.build(m_vkd, m_device);

    // Create pool and set.
    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(descriptorType);
    m_descriptorPool = poolBuilder.build(m_vkd, m_device, poolFlags, 1u);
    m_descriptorSet  = makeDescriptorSet(m_vkd, m_device, m_descriptorPool.get(), m_setLayout.get());

    // Create buffer.
    const auto outputBufferSize       = static_cast<VkDeviceSize>(de::dataSize(m_outVector));
    const auto outputBufferCreateInfo = makeBufferCreateInfo(outputBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    m_outputBuffer                    = BufferWithMemoryPtr(
        new BufferWithMemory(m_vkd, m_device, m_alloc, outputBufferCreateInfo, MemoryRequirement::HostVisible));

    // Update set.
    const auto outputBufferDescInfo = makeDescriptorBufferInfo(m_outputBuffer->get(), 0ull, outputBufferSize);
    DescriptorSetUpdateBuilder updateBuilder;
    updateBuilder.writeSingle(m_descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(0u), descriptorType,
                              &outputBufferDescInfo);
    updateBuilder.update(m_vkd, m_device);
}

void PipelineLibraryShaderModuleInfoInstance::addModule(const std::string &moduleName, VkShaderStageFlagBits stage)
{
    const auto &binary = m_context.getBinaryCollection().get(moduleName);

    const VkShaderModuleCreateInfo modInfo = {
        VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,            // VkStructureType sType;
        nullptr,                                                // const void* pNext;
        0u,                                                     // VkShaderModuleCreateFlags flags;
        binary.getSize(),                                       // size_t codeSize;
        reinterpret_cast<const uint32_t *>(binary.getBinary()), // const uint32_t* pCode;
    };
    m_shaderModuleInfos.push_back(modInfo);

    // Note: the pNext pointer will be updated below.
    const VkPipelineShaderStageCreateInfo stageInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                             // const void* pNext;
        0u,                                                  // VkPipelineShaderStageCreateFlags flags;
        stage,                                               // VkShaderStageFlagBits stage;
        VK_NULL_HANDLE,                                      // VkShaderModule module;
        "main",                                              // const char* pName;
        nullptr,                                             // const VkSpecializationInfo* pSpecializationInfo;
    };
    m_pipelineStageInfos.push_back(stageInfo);

    DE_ASSERT(m_shaderModuleInfos.size() == m_pipelineStageInfos.size());

    // Update pNext pointers after possible reallocation.
    for (size_t i = 0u; i < m_shaderModuleInfos.size(); ++i)
        m_pipelineStageInfos[i].pNext = &(m_shaderModuleInfos[i]);
}

void PipelineLibraryShaderModuleInfoInstance::recordShaderToHostBarrier(VkPipelineStageFlagBits pipelineStage) const
{
    const auto postWriteBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
    cmdPipelineMemoryBarrier(m_vkd, m_cmdBuffer, pipelineStage, VK_PIPELINE_STAGE_HOST_BIT, &postWriteBarrier);
}

void PipelineLibraryShaderModuleInfoInstance::verifyOutputBuffer(void)
{
    auto &allocation = m_outputBuffer->getAllocation();

    invalidateAlloc(m_vkd, m_device, allocation);
    deMemcpy(m_outVector.data(), allocation.getHostPtr(), de::dataSize(m_outVector));

    for (uint32_t i = 0; i < static_cast<uint32_t>(m_outVector.size()); ++i)
    {
        if (m_outVector[i] != i)
        {
            std::ostringstream msg;
            msg << "Unexpected value found at position " << i << ": " << m_outVector[i];
            TCU_FAIL(msg.str());
        }
    }
}

void PipelineLibraryShaderModuleInfoInstance::allocateCmdBuffers(void)
{
    m_cmdPool      = makeCommandPool(m_vkd, m_device, m_queueIndex);
    m_cmdBufferPtr = allocateCommandBuffer(m_vkd, m_device, m_cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    m_cmdBuffer    = m_cmdBufferPtr.get();
}

class PipelineLibraryShaderModuleInfoCompInstance : public PipelineLibraryShaderModuleInfoInstance
{
public:
    PipelineLibraryShaderModuleInfoCompInstance(Context &context) : PipelineLibraryShaderModuleInfoInstance(context)
    {
    }
    virtual ~PipelineLibraryShaderModuleInfoCompInstance(void)
    {
    }

    tcu::TestStatus iterate(void) override;
};

tcu::TestStatus PipelineLibraryShaderModuleInfoCompInstance::iterate(void)
{
    const auto stage     = VK_SHADER_STAGE_COMPUTE_BIT;
    const auto bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;

    prepareOutputBuffer(stage);
    addModule("comp", stage);
    allocateCmdBuffers();

    const auto pipelineLayout = makePipelineLayout(m_vkd, m_device, m_setLayout.get());

    const VkComputePipelineCreateInfo pipelineCreateInfo = {
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                        // const void* pNext;
        0u,                                             // VkPipelineCreateFlags flags;
        m_pipelineStageInfos.at(0u),                    // VkPipelineShaderStageCreateInfo stage;
        pipelineLayout.get(),                           // VkPipelineLayout layout;
        VK_NULL_HANDLE,                                 // VkPipeline basePipelineHandle;
        0,                                              // int32_t basePipelineIndex;
    };

    const auto pipeline = createComputePipeline(m_vkd, m_device, VK_NULL_HANDLE, &pipelineCreateInfo);

    beginCommandBuffer(m_vkd, m_cmdBuffer);
    m_vkd.cmdBindDescriptorSets(m_cmdBuffer, bindPoint, pipelineLayout.get(), 0u, 1u, &m_descriptorSet.get(), 0u,
                                nullptr);
    m_vkd.cmdBindPipeline(m_cmdBuffer, bindPoint, pipeline.get());
    m_vkd.cmdDispatch(m_cmdBuffer, 1u, 1u, 1u);
    recordShaderToHostBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    endCommandBuffer(m_vkd, m_cmdBuffer);
    submitCommandsAndWait(m_vkd, m_device, m_queue, m_cmdBuffer);
    verifyOutputBuffer();

    return tcu::TestStatus::pass("Pass");
}

class PipelineLibraryShaderModuleInfoRTInstance : public PipelineLibraryShaderModuleInfoInstance
{
public:
    PipelineLibraryShaderModuleInfoRTInstance(Context &context, bool withLibrary)
        : PipelineLibraryShaderModuleInfoInstance(context)
        , m_withLibrary(withLibrary)
    {
    }
    virtual ~PipelineLibraryShaderModuleInfoRTInstance(void)
    {
    }

    tcu::TestStatus iterate(void) override;

protected:
    bool m_withLibrary;
};

tcu::TestStatus PipelineLibraryShaderModuleInfoRTInstance::iterate(void)
{
    const auto stage     = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    const auto bindPoint = VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR;

    prepareOutputBuffer(stage);
    addModule("rgen", stage);
    allocateCmdBuffers();

    const auto pipelineLayout = makePipelineLayout(m_vkd, m_device, m_setLayout.get());

    const VkRayTracingShaderGroupCreateInfoKHR shaderGroupInfo = {
        VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, // VkStructureType sType;
        nullptr,                                                    // const void* pNext;
        VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,               // VkRayTracingShaderGroupTypeKHR type;
        0u,                                                         // uint32_t generalShader;
        VK_SHADER_UNUSED_KHR,                                       // uint32_t closestHitShader;
        VK_SHADER_UNUSED_KHR,                                       // uint32_t anyHitShader;
        VK_SHADER_UNUSED_KHR,                                       // uint32_t intersectionShader;
        nullptr,                                                    // const void* pShaderGroupCaptureReplayHandle;
    };

    const VkPipelineCreateFlags createFlags =
        (m_withLibrary ? static_cast<VkPipelineCreateFlags>(VK_PIPELINE_CREATE_LIBRARY_BIT_KHR) : 0u);
    const VkRayTracingPipelineInterfaceCreateInfoKHR libIfaceInfo   = initVulkanStructure();
    const VkRayTracingPipelineInterfaceCreateInfoKHR *pLibraryIface = (m_withLibrary ? &libIfaceInfo : nullptr);

    const VkRayTracingPipelineCreateInfoKHR pipelineCreateInfo = {
        VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR, // VkStructureType sType;
        nullptr,                                                // const void* pNext;
        createFlags,                                            // VkPipelineCreateFlags flags;
        de::sizeU32(m_pipelineStageInfos),                      // uint32_t stageCount;
        de::dataOrNull(m_pipelineStageInfos),                   // const VkPipelineShaderStageCreateInfo* pStages;
        1u,                                                     // uint32_t groupCount;
        &shaderGroupInfo,                                       // const VkRayTracingShaderGroupCreateInfoKHR* pGroups;
        1u,                                                     // uint32_t maxPipelineRayRecursionDepth;
        nullptr,                                                // const VkPipelineLibraryCreateInfoKHR* pLibraryInfo;
        pLibraryIface,        // const VkRayTracingPipelineInterfaceCreateInfoKHR* pLibraryInterface;
        nullptr,              // const VkPipelineDynamicStateCreateInfo* pDynamicState;
        pipelineLayout.get(), // VkPipelineLayout layout;
        VK_NULL_HANDLE,       // VkPipeline basePipelineHandle;
        0,                    // int32_t basePipelineIndex;
    };

    Move<VkPipeline> pipelineLib;
    Move<VkPipeline> pipeline;

    if (m_withLibrary)
    {
        pipelineLib = createRayTracingPipelineKHR(m_vkd, m_device, VK_NULL_HANDLE, VK_NULL_HANDLE, &pipelineCreateInfo);

        const VkPipelineLibraryCreateInfoKHR libraryInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR, // VkStructureType sType;
            nullptr,                                            // const void* pNext;
            1u,                                                 // uint32_t libraryCount;
            &pipelineLib.get(),                                 // const VkPipeline* pLibraries;
        };

        const VkRayTracingPipelineCreateInfoKHR nonLibCreateInfo = {
            VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR, // VkStructureType sType;
            nullptr,                                                // const void* pNext;
            0u,                                                     // VkPipelineCreateFlags flags;
            0u,                                                     // uint32_t stageCount;
            nullptr,                                                // const VkPipelineShaderStageCreateInfo* pStages;
            0u,                                                     // uint32_t groupCount;
            nullptr,              // const VkRayTracingShaderGroupCreateInfoKHR* pGroups;
            1u,                   // uint32_t maxPipelineRayRecursionDepth;
            &libraryInfo,         // const VkPipelineLibraryCreateInfoKHR* pLibraryInfo;
            pLibraryIface,        // const VkRayTracingPipelineInterfaceCreateInfoKHR* pLibraryInterface;
            nullptr,              // const VkPipelineDynamicStateCreateInfo* pDynamicState;
            pipelineLayout.get(), // VkPipelineLayout layout;
            VK_NULL_HANDLE,       // VkPipeline basePipelineHandle;
            0,                    // int32_t basePipelineIndex;
        };
        pipeline = createRayTracingPipelineKHR(m_vkd, m_device, VK_NULL_HANDLE, VK_NULL_HANDLE, &nonLibCreateInfo);
    }
    else
    {
        pipeline = createRayTracingPipelineKHR(m_vkd, m_device, VK_NULL_HANDLE, VK_NULL_HANDLE, &pipelineCreateInfo);
    }

    // Make shader binding table.
    const auto rtProperties = makeRayTracingProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice());
    const auto rtHandleSize = rtProperties->getShaderGroupHandleSize();
    const auto sbtSize      = static_cast<VkDeviceSize>(rtHandleSize);
    const auto sbtMemReqs   = (MemoryRequirement::HostVisible | MemoryRequirement::DeviceAddress);
    const auto sbtCreateInfo = makeBufferCreateInfo(sbtSize, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
                                                                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    BufferWithMemoryPtr sbt =
        BufferWithMemoryPtr(new BufferWithMemory(m_vkd, m_device, m_alloc, sbtCreateInfo, sbtMemReqs));
    auto &sbtAlloc = sbt->getAllocation();
    void *sbtData  = sbtAlloc.getHostPtr();

    // Copy ray gen shader group handle to the start of  the buffer.
    VK_CHECK(m_vkd.getRayTracingShaderGroupHandlesKHR(m_device, pipeline.get(), 0u, 1u, static_cast<size_t>(sbtSize),
                                                      sbtData));
    flushAlloc(m_vkd, m_device, sbtAlloc);

    // Strided device address regions.
    VkStridedDeviceAddressRegionKHR rgenSBTRegion = makeStridedDeviceAddressRegionKHR(
        getBufferDeviceAddress(m_vkd, m_device, sbt->get(), 0), rtHandleSize, rtHandleSize);
    VkStridedDeviceAddressRegionKHR missSBTRegion = makeStridedDeviceAddressRegionKHR(0, 0, 0);
    VkStridedDeviceAddressRegionKHR hitsSBTRegion = makeStridedDeviceAddressRegionKHR(0, 0, 0);
    VkStridedDeviceAddressRegionKHR callSBTRegion = makeStridedDeviceAddressRegionKHR(0, 0, 0);

    beginCommandBuffer(m_vkd, m_cmdBuffer);
    m_vkd.cmdBindDescriptorSets(m_cmdBuffer, bindPoint, pipelineLayout.get(), 0u, 1u, &m_descriptorSet.get(), 0u,
                                nullptr);
    m_vkd.cmdBindPipeline(m_cmdBuffer, bindPoint, pipeline.get());
    m_vkd.cmdTraceRaysKHR(m_cmdBuffer, &rgenSBTRegion, &missSBTRegion, &hitsSBTRegion, &callSBTRegion,
                          kOutputBufferElements, 1u, 1u);
    recordShaderToHostBarrier(VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);
    endCommandBuffer(m_vkd, m_cmdBuffer);
    submitCommandsAndWait(m_vkd, m_device, m_queue, m_cmdBuffer);
    verifyOutputBuffer();

    return tcu::TestStatus::pass("Pass");
}

class NullRenderingCreateInfoInstance : public vkt::TestInstance
{
public:
    NullRenderingCreateInfoInstance(Context &context) : vkt::TestInstance(context)
    {
    }
    virtual ~NullRenderingCreateInfoInstance(void) = default;

    tcu::TestStatus iterate(void) override;
};

tcu::TestStatus NullRenderingCreateInfoInstance::iterate(void)
{
    const auto ctx = m_context.getContextCommonData();
    const tcu::IVec3 colorExtent(1, 1, 1);
    const auto imageExtent = makeExtent3D(colorExtent);
    const auto colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const auto tcuFormat   = mapVkFormat(colorFormat);
    const auto colorUsage  = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const auto colorSRR    = makeDefaultImageSubresourceRange();
    const auto colorSRL    = makeDefaultImageSubresourceLayers();

    // Color buffer and view.
    ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, imageExtent, colorFormat, colorUsage,
                                VK_IMAGE_TYPE_2D);
    const auto colorView =
        makeImageView(ctx.vkd, ctx.device, colorBuffer.getImage(), VK_IMAGE_VIEW_TYPE_2D, colorFormat, colorSRR);

    // Verification buffer.
    const auto verificationBufferSize =
        static_cast<VkDeviceSize>(colorExtent.x() * colorExtent.y() * colorExtent.z() * tcu::getPixelSize(tcuFormat));
    const auto verificationBufferInfo = makeBufferCreateInfo(verificationBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    BufferWithMemory verificationBuffer(ctx.vkd, ctx.device, ctx.allocator, verificationBufferInfo,
                                        MemoryRequirement::HostVisible);
    auto &verificationBufferAlloc = verificationBuffer.getAllocation();
    void *verificationBufferPtr   = verificationBufferAlloc.getHostPtr();

    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device);

    const VkPipelineVertexInputStateCreateInfo vertexInputStateInfo = initVulkanStructure();
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateInfo   = initVulkanStructure();
    inputAssemblyStateInfo.topology                                 = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    const std::vector<VkViewport> viewports(1u, makeViewport(imageExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(imageExtent));

    const auto &binaries  = m_context.getBinaryCollection();
    const auto vertModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("vert"));
    const auto fragModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("frag"));

    // We will use a null-filled pipeline rendering info structure for all substates except the fragment output state.
    VkPipelineRenderingCreateInfo nullRenderingInfo = initVulkanStructure();
    nullRenderingInfo.colorAttachmentCount          = 0;

    VkPipelineRenderingCreateInfo finalRenderingInfo = initVulkanStructure();
    finalRenderingInfo.colorAttachmentCount          = 1u;
    finalRenderingInfo.pColorAttachmentFormats       = &colorFormat;

    const VkPipelineViewportStateCreateInfo viewportStateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                               // const void* pNext;
        0u,                                                    // VkPipelineViewportStateCreateFlags flags;
        de::sizeU32(viewports),                                // uint32_t viewportCount;
        de::dataOrNull(viewports),                             // const VkViewport* pViewports;
        de::sizeU32(scissors),                                 // uint32_t scissorCount;
        de::dataOrNull(scissors),                              // const VkRect2D* pScissors;
    };

    const VkPipelineRasterizationStateCreateInfo rasterizationStateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                    // const void* pNext;
        0u,                                                         // VkPipelineRasterizationStateCreateFlags flags;
        VK_FALSE,                                                   // VkBool32 depthClampEnable;
        VK_FALSE,                                                   // VkBool32 rasterizerDiscardEnable;
        VK_POLYGON_MODE_FILL,                                       // VkPolygonMode polygonMode;
        VK_CULL_MODE_BACK_BIT,                                      // VkCullModeFlags cullMode;
        VK_FRONT_FACE_COUNTER_CLOCKWISE,                            // VkFrontFace frontFace;
        VK_FALSE,                                                   // VkBool32 depthBiasEnable;
        0.0f,                                                       // float depthBiasConstantFactor;
        0.0f,                                                       // float depthBiasClamp;
        0.0f,                                                       // float depthBiasSlopeFactor;
        1.0f,                                                       // float lineWidth;
    };

    const VkPipelineMultisampleStateCreateInfo multisampleStateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                  // const void* pNext;
        0u,                                                       // VkPipelineMultisampleStateCreateFlags flags;
        VK_SAMPLE_COUNT_1_BIT,                                    // VkSampleCountFlagBits rasterizationSamples;
        VK_FALSE,                                                 // VkBool32 sampleShadingEnable;
        1.0f,                                                     // float minSampleShading;
        nullptr,                                                  // const VkSampleMask* pSampleMask;
        VK_FALSE,                                                 // VkBool32 alphaToCoverageEnable;
        VK_FALSE,                                                 // VkBool32 alphaToOneEnable;
    };

    const VkPipelineDepthStencilStateCreateInfo depthStencilStateInfo = initVulkanStructure();

    const VkColorComponentFlags colorComponentFlags =
        (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);

    const VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {
        VK_FALSE,             // VkBool32 blendEnable;
        VK_BLEND_FACTOR_ZERO, // VkBlendFactor srcColorBlendFactor;
        VK_BLEND_FACTOR_ZERO, // VkBlendFactor dstColorBlendFactor;
        VK_BLEND_OP_ADD,      // VkBlendOp colorBlendOp;
        VK_BLEND_FACTOR_ZERO, // VkBlendFactor srcAlphaBlendFactor;
        VK_BLEND_FACTOR_ZERO, // VkBlendFactor dstAlphaBlendFactor;
        VK_BLEND_OP_ADD,      // VkBlendOp alphaBlendOp;
        colorComponentFlags,  // VkColorComponentFlags colorWriteMask;
    };

    const VkPipelineColorBlendStateCreateInfo colorBlendStateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                  // const void* pNext;
        0u,                                                       // VkPipelineColorBlendStateCreateFlags flags;
        VK_FALSE,                                                 // VkBool32 logicOpEnable;
        VK_LOGIC_OP_CLEAR,                                        // VkLogicOp logicOp;
        1u,                                                       // uint32_t attachmentCount;
        &colorBlendAttachmentState, // const VkPipelineColorBlendAttachmentState* pAttachments;
        {0.0f, 0.0f, 0.0f, 0.0f},   // float blendConstants[4];
    };

    // Build the different pipeline pieces.
    Move<VkPipeline> vertexInputLib;
    Move<VkPipeline> preRasterShaderLib;
    Move<VkPipeline> fragShaderLib;
    Move<VkPipeline> fragOutputLib;

    const VkPipelineCreateFlags libCreationFlags = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;
    const VkPipelineCreateFlags linkFlags        = 0u;

    // Vertex input state library.
    {
        VkGraphicsPipelineLibraryCreateInfoEXT vertexInputLibInfo = initVulkanStructure();
        vertexInputLibInfo.flags |= VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT;

        VkGraphicsPipelineCreateInfo vertexInputPipelineInfo = initVulkanStructure(&vertexInputLibInfo);
        vertexInputPipelineInfo.flags                        = libCreationFlags;
        vertexInputPipelineInfo.pVertexInputState            = &vertexInputStateInfo;
        vertexInputPipelineInfo.pInputAssemblyState          = &inputAssemblyStateInfo;

        vertexInputLib = createGraphicsPipeline(ctx.vkd, ctx.device, VK_NULL_HANDLE, &vertexInputPipelineInfo);
    }

    // Pre-rasterization shader state library.
    {
        VkGraphicsPipelineLibraryCreateInfoEXT preRasterShaderLibInfo =
            initVulkanStructure(&nullRenderingInfo); // What we're testing.
        preRasterShaderLibInfo.flags |= VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT;

        VkGraphicsPipelineCreateInfo preRasterShaderPipelineInfo = initVulkanStructure(&preRasterShaderLibInfo);
        preRasterShaderPipelineInfo.flags                        = libCreationFlags;
        preRasterShaderPipelineInfo.layout                       = pipelineLayout.get();
        preRasterShaderPipelineInfo.pViewportState               = &viewportStateInfo;
        preRasterShaderPipelineInfo.pRasterizationState          = &rasterizationStateInfo;

        const auto vertShaderInfo = makePipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, vertModule.get());
        preRasterShaderPipelineInfo.stageCount = 1u;
        preRasterShaderPipelineInfo.pStages    = &vertShaderInfo;

        preRasterShaderLib = createGraphicsPipeline(ctx.vkd, ctx.device, VK_NULL_HANDLE, &preRasterShaderPipelineInfo);
    }

    // Fragment shader stage library.
    {
        VkGraphicsPipelineLibraryCreateInfoEXT fragShaderLibInfo =
            initVulkanStructure(&nullRenderingInfo); // What we're testing.
        fragShaderLibInfo.flags |= VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT;

        VkGraphicsPipelineCreateInfo fragShaderPipelineInfo = initVulkanStructure(&fragShaderLibInfo);
        fragShaderPipelineInfo.flags                        = libCreationFlags;
        fragShaderPipelineInfo.layout                       = pipelineLayout.get();
        fragShaderPipelineInfo.pMultisampleState            = &multisampleStateInfo;
        fragShaderPipelineInfo.pDepthStencilState           = &depthStencilStateInfo;

        const auto fragShaderInfo = makePipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, fragModule.get());
        fragShaderPipelineInfo.stageCount = 1u;
        fragShaderPipelineInfo.pStages    = &fragShaderInfo;

        fragShaderLib = createGraphicsPipeline(ctx.vkd, ctx.device, VK_NULL_HANDLE, &fragShaderPipelineInfo);
    }

    // Fragment output library.
    {
        VkGraphicsPipelineLibraryCreateInfoEXT fragOutputLibInfo =
            initVulkanStructure(&finalRenderingInfo); // Good info only in the fragment output substate.
        fragOutputLibInfo.flags |= VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT;

        VkGraphicsPipelineCreateInfo fragOutputPipelineInfo = initVulkanStructure(&fragOutputLibInfo);
        fragOutputPipelineInfo.flags                        = libCreationFlags;
        fragOutputPipelineInfo.pColorBlendState             = &colorBlendStateInfo;
        fragOutputPipelineInfo.pMultisampleState            = &multisampleStateInfo;

        fragOutputLib = createGraphicsPipeline(ctx.vkd, ctx.device, VK_NULL_HANDLE, &fragOutputPipelineInfo);
    }

    // Linked pipeline.
    const std::vector<VkPipeline> libraryHandles{
        vertexInputLib.get(),
        preRasterShaderLib.get(),
        fragShaderLib.get(),
        fragOutputLib.get(),
    };

    VkPipelineLibraryCreateInfoKHR linkedPipelineLibraryInfo = initVulkanStructure();
    linkedPipelineLibraryInfo.libraryCount                   = de::sizeU32(libraryHandles);
    linkedPipelineLibraryInfo.pLibraries                     = de::dataOrNull(libraryHandles);

    VkGraphicsPipelineCreateInfo linkedPipelineInfo = initVulkanStructure(&linkedPipelineLibraryInfo);
    linkedPipelineInfo.flags                        = linkFlags;
    linkedPipelineInfo.layout                       = pipelineLayout.get();

    const auto pipeline = createGraphicsPipeline(ctx.vkd, ctx.device, VK_NULL_HANDLE, &linkedPipelineInfo);

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = cmd.cmdBuffer.get();

    const auto clearValue = makeClearValueColorF32(0.0f, 0.0f, 0.0f, 1.0f);

    beginCommandBuffer(ctx.vkd, cmdBuffer);

    const auto preRenderBarrier = makeImageMemoryBarrier(
        0u, (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT), VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, colorBuffer.getImage(), colorSRR);
    cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, &preRenderBarrier);

    beginRendering(ctx.vkd, cmdBuffer, colorView.get(), scissors.at(0u), clearValue,
                   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    ctx.vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());
    ctx.vkd.cmdDraw(cmdBuffer, 3u, 1u, 0u, 0u);
    endRendering(ctx.vkd, cmdBuffer);

    const auto color2Transfer = makeImageMemoryBarrier(
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, colorBuffer.getImage(), colorSRR);
    cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                  VK_PIPELINE_STAGE_TRANSFER_BIT, &color2Transfer);
    const auto copyRegion = makeBufferImageCopy(imageExtent, colorSRL);
    ctx.vkd.cmdCopyImageToBuffer(cmdBuffer, colorBuffer.getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                 verificationBuffer.get(), 1u, &copyRegion);

    const auto transfer2Host = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
    cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                             &transfer2Host);

    endCommandBuffer(ctx.vkd, cmdBuffer);

    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);
    invalidateAlloc(ctx.vkd, ctx.device, verificationBufferAlloc);

    auto &testLog = m_context.getTestContext().getLog();
    const tcu::Vec4 expectedColor(0.0f, 0.0f, 1.0f, 1.0f); // Must match frag shader.
    const tcu::Vec4 threshold(0.0f, 0.0f, 0.0f, 0.0f);
    tcu::ConstPixelBufferAccess resultAccess(tcuFormat, colorExtent, verificationBufferPtr);

    if (!tcu::floatThresholdCompare(testLog, "Result", "", expectedColor, resultAccess, threshold,
                                    tcu::COMPARE_LOG_ON_ERROR))
        return tcu::TestStatus::fail("Unexpected color buffer contents -- check log for details");

    return tcu::TestStatus::pass("Pass");
}

class CreateViewIndexFromDeviceIndexInstance : public vkt::TestInstance
{
public:
    CreateViewIndexFromDeviceIndexInstance(Context &context, const MiscTestParams &params);
    virtual ~CreateViewIndexFromDeviceIndexInstance(void) = default;

    tcu::TestStatus iterate(void) override;

protected:
    bool createDeviceGroup(void);

private:
    MiscTestParams m_testParams;

    uint32_t m_deviceGroupQueueFamilyIndex;
    CustomInstance m_deviceGroupInstance;
    vk::Move<vk::VkDevice> m_deviceGroupLogicalDevice;
    std::vector<vk::VkPhysicalDevice> m_deviceGroupPhysicalDevices;
    de::MovePtr<vk::DeviceDriver> m_deviceGroupVk;
    de::MovePtr<Allocator> m_deviceGroupAllocator;
};

CreateViewIndexFromDeviceIndexInstance::CreateViewIndexFromDeviceIndexInstance(Context &context,
                                                                               const MiscTestParams &params)
    : vkt::TestInstance(context)
    , m_testParams(params)
{
}

tcu::TestStatus CreateViewIndexFromDeviceIndexInstance::iterate()
{
    const bool useDeviceGroup = createDeviceGroup();
    const auto &vk            = useDeviceGroup ? *m_deviceGroupVk : m_context.getDeviceInterface();
    const auto device         = useDeviceGroup ? *m_deviceGroupLogicalDevice : m_context.getDevice();
    Allocator &allocator      = useDeviceGroup ? *m_deviceGroupAllocator : m_context.getDefaultAllocator();
    uint32_t queueFamilyIndex =
        useDeviceGroup ? m_deviceGroupQueueFamilyIndex : m_context.getUniversalQueueFamilyIndex();
    const auto &modeParams = m_testParams.get<ViewIndexFromDeviceIndexParams>();
    const auto deviceCount = useDeviceGroup ? m_deviceGroupPhysicalDevices.size() : 1u;
    const auto viewCount   = 3u;
    const auto imageSize   = 8u;
    const auto colorFormat = VK_FORMAT_R8G8B8A8_UINT;
    const auto extent      = makeExtent3D(imageSize, imageSize, 1u);

    VkPipelineCreateFlags basePipelineFlags = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;
    if (modeParams.useLinkTimeOptimization)
        basePipelineFlags |= VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT;

    VkPipelineCreateFlags preRasterPipelineFlags = basePipelineFlags;
    VkPipelineCreateFlags fragmentPipelineFlags  = basePipelineFlags;
    if (modeParams.pipelineStateMode != PipelineStateMode::FRAGMENT)
        preRasterPipelineFlags |= VK_PIPELINE_CREATE_VIEW_INDEX_FROM_DEVICE_INDEX_BIT;
    if (modeParams.pipelineStateMode != PipelineStateMode::PRE_RASTERIZATION)
        fragmentPipelineFlags |= VK_PIPELINE_CREATE_VIEW_INDEX_FROM_DEVICE_INDEX_BIT;

    // fill structures that are needed for pipeline creation
    const VkPipelineVertexInputStateCreateInfo vertexInputStateInfo     = initVulkanStructure();
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateInfo       = initVulkanStructure();
    inputAssemblyStateInfo.topology                                     = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    const VkPipelineRasterizationStateCreateInfo rasterizationStateInfo = initVulkanStructure();

    const auto viewport                                 = makeViewport(extent);
    const auto scissor                                  = makeRect2D(extent);
    VkPipelineViewportStateCreateInfo viewportStateInfo = initVulkanStructure();
    viewportStateInfo.viewportCount                     = 1u;
    viewportStateInfo.pViewports                        = &viewport;
    viewportStateInfo.scissorCount                      = 1u;
    viewportStateInfo.pScissors                         = &scissor;

    VkPipelineMultisampleStateCreateInfo multisampleStateInfo         = initVulkanStructure();
    multisampleStateInfo.rasterizationSamples                         = VK_SAMPLE_COUNT_1_BIT;
    const VkPipelineDepthStencilStateCreateInfo depthStencilStateInfo = initVulkanStructure();

    VkPipelineColorBlendAttachmentState colorBlendAttachmentState;
    deMemset(&colorBlendAttachmentState, 0x00, sizeof(VkPipelineColorBlendAttachmentState));
    colorBlendAttachmentState.colorWriteMask = 0xFu;
    VkPipelineColorBlendAttachmentState colorBlendAttachmentStates[viewCount];
    std::fill(colorBlendAttachmentStates, colorBlendAttachmentStates + viewCount, colorBlendAttachmentState);

    VkPipelineColorBlendStateCreateInfo colorBlendStateInfo = initVulkanStructure();
    colorBlendStateInfo.attachmentCount                     = 1;
    colorBlendStateInfo.pAttachments                        = colorBlendAttachmentStates;

    VkPipelineTessellationStateCreateInfo tessellationStateInfo = initVulkanStructure();
    tessellationStateInfo.patchControlPoints                    = 3u;

    // create color attachment with required number of layers
    const auto imageSubresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, viewCount);
    const VkImageUsageFlags imageUsage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    ImageWithBuffer imageWithBuffer(vk, device, allocator, extent, colorFormat, imageUsage, VK_IMAGE_TYPE_2D,
                                    imageSubresourceRange, viewCount);
    Move<VkImageView> imageView = makeImageView(vk, device, imageWithBuffer.getImage(), VK_IMAGE_VIEW_TYPE_2D_ARRAY,
                                                colorFormat, imageSubresourceRange);

    const auto &multiviewFeatures(m_context.getMultiviewFeatures());
    const auto srl(makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, viewCount));
    const auto copyRegion(makeBufferImageCopy(extent, srl));
    const auto beforeCopyBarrier(makeMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT));
    const auto clearValue(makeClearValueColor(tcu::Vec4(0.0f)));

    const auto pipelineLayout(makePipelineLayout(vk, device));
    auto &bc(m_context.getBinaryCollection());
    uint32_t preRasterShaderStages = 1;
    Move<VkShaderModule> preRasterModules[4];
    VkPipelineShaderStageCreateInfo preRasterShaderInfos[4];

    if (modeParams.useMeshShading)
    {
        preRasterModules[0]     = createShaderModule(vk, device, bc.get("mesh"));
        preRasterShaderInfos[0] = makePipelineShaderStageCreateInfo(VK_SHADER_STAGE_MESH_BIT_EXT, *preRasterModules[0]);
    }
    else
    {
        std::string moduleNames[4]{"vert", "tesc", "tese", "geom"};
        VkShaderStageFlagBits shaderStages[4]{VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
                                              VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
                                              VK_SHADER_STAGE_GEOMETRY_BIT};

        if (multiviewFeatures.multiviewTessellationShader)
        {
            inputAssemblyStateInfo.topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
            preRasterShaderStages += 2;
        }

        if (multiviewFeatures.multiviewGeometryShader)
        {
            moduleNames[preRasterShaderStages]  = "geom";
            shaderStages[preRasterShaderStages] = VK_SHADER_STAGE_GEOMETRY_BIT;
            ++preRasterShaderStages;
        }

        for (uint32_t i = 0u; i < preRasterShaderStages; ++i)
        {
            preRasterModules[i]     = createShaderModule(vk, device, bc.get(moduleNames[i]));
            preRasterShaderInfos[i] = makePipelineShaderStageCreateInfo(shaderStages[i], *preRasterModules[i]);
        }
    }
    const auto fragModule(createShaderModule(vk, device, bc.get("frag")));
    const auto fragShaderInfo(makePipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, *fragModule));

    // create renderpass and framebuffer
    uint32_t viewMask        = 0u;
    uint32_t correlationMask = 0u;
    for (uint32_t i = 0u; i < viewCount; ++i)
    {
        viewMask |= (1 << i);
        correlationMask |= (1 << i);
    }
    VkRenderPassMultiviewCreateInfo multiviewInfo = initVulkanStructure();
    multiviewInfo.subpassCount                    = 1u;
    multiviewInfo.pViewMasks                      = &viewMask;
    multiviewInfo.correlationMaskCount            = 1u;
    multiviewInfo.pCorrelationMasks               = &correlationMask;

    auto renderPass = makeRenderPass(
        vk, device, colorFormat, VK_FORMAT_UNDEFINED, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 0, &multiviewInfo);

    const auto framebufferCreateInfo = makeFramebufferCreateInfo(*renderPass, 1u, &*imageView, imageSize, imageSize);
    auto framebuffer                 = createFramebuffer(vk, device, &framebufferCreateInfo);

    // create pre-raster pipeline part
    VkGraphicsPipelineLibraryCreateInfoEXT preRasterLibraryInfo = initVulkanStructure();
    preRasterLibraryInfo.flags = VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT |
                                 VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT;
    VkGraphicsPipelineCreateInfo preRasterPipelineInfo = initVulkanStructure(&preRasterLibraryInfo);
    preRasterPipelineInfo.flags                        = preRasterPipelineFlags;
    preRasterPipelineInfo.layout                       = *pipelineLayout;
    preRasterPipelineInfo.renderPass                   = *renderPass;
    preRasterPipelineInfo.pVertexInputState            = &vertexInputStateInfo;
    preRasterPipelineInfo.pInputAssemblyState          = &inputAssemblyStateInfo;
    preRasterPipelineInfo.pViewportState               = &viewportStateInfo;
    preRasterPipelineInfo.pRasterizationState          = &rasterizationStateInfo;
    preRasterPipelineInfo.pTessellationState           = &tessellationStateInfo;
    preRasterPipelineInfo.stageCount                   = preRasterShaderStages;
    preRasterPipelineInfo.pStages                      = preRasterShaderInfos;
    auto preRasterPipelinePart = createGraphicsPipeline(vk, device, VK_NULL_HANDLE, &preRasterPipelineInfo);

    // create fragment pipeline part
    VkGraphicsPipelineLibraryCreateInfoEXT fragShaderLibInfo = initVulkanStructure();
    fragShaderLibInfo.flags                                  = VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT |
                              VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT;
    VkGraphicsPipelineCreateInfo fragmentPipelineInfo = initVulkanStructure(&fragShaderLibInfo);
    fragmentPipelineInfo.flags                        = fragmentPipelineFlags;
    fragmentPipelineInfo.layout                       = pipelineLayout.get();
    fragmentPipelineInfo.renderPass                   = *renderPass;
    fragmentPipelineInfo.pMultisampleState            = &multisampleStateInfo;
    fragmentPipelineInfo.pDepthStencilState           = &depthStencilStateInfo;
    fragmentPipelineInfo.pColorBlendState             = &colorBlendStateInfo;
    fragmentPipelineInfo.stageCount                   = 1u;
    fragmentPipelineInfo.pStages                      = &fragShaderInfo;
    auto fragmentPipelinePart = createGraphicsPipeline(vk, device, VK_NULL_HANDLE, &fragmentPipelineInfo);

    // merge pipelines
    const VkPipeline libraryHandles[]{
        *preRasterPipelinePart,
        *fragmentPipelinePart,
    };
    VkPipelineLibraryCreateInfoKHR linkedPipelineLibraryInfo = initVulkanStructure();
    linkedPipelineLibraryInfo.libraryCount                   = 2u;
    linkedPipelineLibraryInfo.pLibraries                     = libraryHandles;
    VkGraphicsPipelineCreateInfo linkedPipelineInfo          = initVulkanStructure(&linkedPipelineLibraryInfo);
    linkedPipelineInfo.layout                                = *pipelineLayout;
    if (modeParams.useLinkTimeOptimization)
        linkedPipelineInfo.flags = VK_PIPELINE_CREATE_LINK_TIME_OPTIMIZATION_BIT_EXT;
    const auto pipeline = createGraphicsPipeline(vk, device, VK_NULL_HANDLE, &linkedPipelineInfo);

    const auto cmdPool(
        createCommandPool(vk, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex));
    const auto cmdBuffer(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    // render triangle that covers whole color attachments
    beginCommandBuffer(vk, *cmdBuffer);

    beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, scissor, 1, &clearValue);
    vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

    if (modeParams.useMeshShading)
        vk.cmdDrawMeshTasksEXT(*cmdBuffer, 1u, 1u, 1u);
    else
        vk.cmdDraw(*cmdBuffer, 3u, 1u, 0u, 0u);

    endRenderPass(vk, *cmdBuffer);

    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 1,
                          &beforeCopyBarrier, 0, 0, 0, 0);

    vk.cmdCopyImageToBuffer(*cmdBuffer, imageWithBuffer.getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                            imageWithBuffer.getBuffer(), 1u, &copyRegion);

    endCommandBuffer(vk, *cmdBuffer);
    const VkQueue queue = getDeviceQueue(vk, device, queueFamilyIndex, 0);
    submitCommandsAndWait(vk, device, queue, *cmdBuffer);

    const auto &bufferAllocation = imageWithBuffer.getBufferAllocation();
    invalidateAlloc(vk, device, bufferAllocation);

    bool resurtIsCorrect      = true;
    const auto fragmentCount  = imageSize * imageSize;
    const auto componentCount = 4u;
    std::vector<uint8_t> allowedValueSets(deviceCount * componentCount, 0);

    for (uint8_t v = 0; v < static_cast<uint8_t>(viewCount); ++v)
    {
        // calculate allowed set of result values
        for (uint8_t d = 0; d < static_cast<uint8_t>(deviceCount); ++d)
        {
            uint8_t *allowedValuesPtr = allowedValueSets.data() + d * componentCount;
            if (preRasterPipelineFlags == fragmentPipelineFlags)
            {
                allowedValuesPtr[0] = d;
                allowedValuesPtr[1] = static_cast<uint8_t>(d + d);
                allowedValuesPtr[2] = d;
                allowedValuesPtr[3] = d;
            }
            else if (fragmentPipelineFlags & VK_PIPELINE_CREATE_VIEW_INDEX_FROM_DEVICE_INDEX_BIT)
            {
                allowedValuesPtr[0] = v;
                allowedValuesPtr[1] = static_cast<uint8_t>(v + v);
                allowedValuesPtr[2] = v;
                allowedValuesPtr[3] = d;
            }
            else
            {
                allowedValuesPtr[0] = d;
                allowedValuesPtr[1] = static_cast<uint8_t>(d + d);
                allowedValuesPtr[2] = d;
                allowedValuesPtr[3] = v;
            }

            // ignore tesselation and/or geometry stages when those features are not available
            if (!multiviewFeatures.multiviewTessellationShader || modeParams.useMeshShading)
                allowedValuesPtr[1] = 0;
            if (!multiviewFeatures.multiviewGeometryShader || modeParams.useMeshShading)
                allowedValuesPtr[2] = 0;
        }

        uint8_t *bufferPtr = static_cast<uint8_t *>(bufferAllocation.getHostPtr()) + v * fragmentCount * componentCount;
        for (uint32_t f = 0; f < fragmentCount; ++f)
        {
            uint8_t *fragmentColor = bufferPtr + f * componentCount;
            resurtIsCorrect        = false;

            // compare with all accepted values (if device group is used each device will produce different  result)
            for (uint8_t d = 0; d < static_cast<uint8_t>(deviceCount); ++d)
            {
                uint8_t *allowedValuesPtr = allowedValueSets.data() + d * componentCount;
                resurtIsCorrect           = (deMemCmp(fragmentColor, allowedValuesPtr, componentCount) == 0);

                // when fragment is corret we can skip checking other allowed values
                if (resurtIsCorrect)
                    break;
            }

            // when fragment is not corret we can skip checking other fragments
            if (!resurtIsCorrect)
                break;
        }

        // when fragment was not corret we can skip checking other views
        if (!resurtIsCorrect)
            break;
    }

    if (resurtIsCorrect)
        return tcu::TestStatus::pass("Pass");

    // log images
    auto &log = m_context.getTestContext().getLog();
    log << tcu::TestLog::ImageSet("Result", "");
    for (uint32_t v = 0; v < viewCount; ++v)
    {
        uint8_t *bufferPtr = static_cast<uint8_t *>(bufferAllocation.getHostPtr()) + v * fragmentCount * componentCount;
        tcu::PixelBufferAccess resultAccess(mapVkFormat(colorFormat), imageSize, imageSize, 1, bufferPtr);
        log << tcu::TestLog::Image(std::to_string(v), "", resultAccess);
    }
    log << tcu::TestLog::EndImageSet;

    return tcu::TestStatus::fail("Fail");
}

bool CreateViewIndexFromDeviceIndexInstance::createDeviceGroup(void)
{
    const auto &vki                 = m_context.getInstanceInterface();
    const tcu::CommandLine &cmdLine = m_context.getTestContext().getCommandLine();
    const uint32_t deviceGroupIndex = cmdLine.getVKDeviceGroupId() - 1;
    const float queuePriority       = 1.0f;

    // create vulkan instance, list all device groups and select proper one
    m_deviceGroupInstance         = createCustomInstanceWithExtension(m_context, "VK_KHR_device_group_creation");
    auto allDeviceGroupProperties = enumeratePhysicalDeviceGroups(vki, m_deviceGroupInstance);
    auto &devGroupProperties      = allDeviceGroupProperties[deviceGroupIndex];
    if (devGroupProperties.physicalDeviceCount == 1)
        return false;

    const InstanceDriver &instance(m_deviceGroupInstance.getDriver());
    VkPhysicalDeviceFeatures2 deviceFeatures2     = initVulkanStructure();
    VkDeviceGroupDeviceCreateInfo deviceGroupInfo = initVulkanStructure(&deviceFeatures2);
    deviceGroupInfo.physicalDeviceCount           = devGroupProperties.physicalDeviceCount;
    deviceGroupInfo.pPhysicalDevices              = devGroupProperties.physicalDevices;

    uint32_t physicalDeviceIndex = cmdLine.getVKDeviceId() - 1;
    if (physicalDeviceIndex >= deviceGroupInfo.physicalDeviceCount)
        physicalDeviceIndex = 0;

    const VkPhysicalDeviceFeatures deviceFeatures =
        getPhysicalDeviceFeatures(instance, deviceGroupInfo.pPhysicalDevices[physicalDeviceIndex]);
    deviceFeatures2.features = deviceFeatures;
    const std::vector<VkQueueFamilyProperties> queueProps =
        getPhysicalDeviceQueueFamilyProperties(instance, devGroupProperties.physicalDevices[physicalDeviceIndex]);

    VkPhysicalDeviceMultiviewFeatures multiviewFeatures            = m_context.getMultiviewFeatures();
    VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT gplFeatures = m_context.getGraphicsPipelineLibraryFeaturesEXT();
    VkPhysicalDeviceMeshShaderFeaturesEXT meshShaderFeatures       = m_context.getMeshShaderFeaturesEXT();

    std::vector<const char *> deviceExtensions{"VK_KHR_pipeline_library", "VK_EXT_graphics_pipeline_library",
                                               "VK_KHR_multiview"};
    if (!isCoreDeviceExtension(m_context.getUsedApiVersion(), "VK_KHR_device_group"))
        deviceExtensions.push_back("VK_KHR_device_group");

    meshShaderFeatures.pNext = nullptr;
    multiviewFeatures.pNext  = nullptr;
    gplFeatures.pNext        = &multiviewFeatures;
    if (m_testParams.mode == MiscTestMode::VIEW_INDEX_FROM_DEVICE_INDEX)
    {
        const auto &modeParams = m_testParams.get<ViewIndexFromDeviceIndexParams>();
        if (modeParams.useMeshShading)
        {
            deviceExtensions.push_back("VK_EXT_mesh_shader");
            multiviewFeatures.pNext = &meshShaderFeatures;
        }
    }
    deviceFeatures2.pNext = &gplFeatures;

    m_deviceGroupPhysicalDevices.resize(devGroupProperties.physicalDeviceCount);
    for (uint32_t pd = 0; pd < devGroupProperties.physicalDeviceCount; pd++)
        m_deviceGroupPhysicalDevices[pd] = devGroupProperties.physicalDevices[pd];

    for (size_t queueNdx = 0; queueNdx < queueProps.size(); queueNdx++)
    {
        if (queueProps[queueNdx].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            m_deviceGroupQueueFamilyIndex = (uint32_t)queueNdx;
    }

    VkDeviceQueueCreateInfo queueInfo{
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                    // const void* pNext;
        (VkDeviceQueueCreateFlags)0u,               // VkDeviceQueueCreateFlags flags;
        m_deviceGroupQueueFamilyIndex,              // uint32_t queueFamilyIndex;
        1u,                                         // uint32_t queueCount;
        &queuePriority                              // const float* pQueuePriorities;
    };

    const VkDeviceCreateInfo deviceInfo{
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, // VkStructureType sType;
        &deviceGroupInfo,                     // const void* pNext;
        (VkDeviceCreateFlags)0,               // VkDeviceCreateFlags flags;
        1u,                                   // uint32_t queueCreateInfoCount;
        &queueInfo,                           // const VkDeviceQueueCreateInfo* pQueueCreateInfos;
        0u,                                   // uint32_t enabledLayerCount;
        nullptr,                              // const char* const* ppEnabledLayerNames;
        uint32_t(deviceExtensions.size()),    // uint32_t enabledExtensionCount;
        deviceExtensions.data(),              // const char* const* ppEnabledExtensionNames;
        nullptr,                              // const VkPhysicalDeviceFeatures* pEnabledFeatures;
    };

    m_deviceGroupLogicalDevice = createCustomDevice(m_context.getTestContext().getCommandLine().isValidationEnabled(),
                                                    m_context.getPlatformInterface(), m_deviceGroupInstance, instance,
                                                    deviceGroupInfo.pPhysicalDevices[physicalDeviceIndex], &deviceInfo);

    m_deviceGroupVk = de::MovePtr<DeviceDriver>(
        new DeviceDriver(m_context.getPlatformInterface(), m_deviceGroupInstance, *m_deviceGroupLogicalDevice,
                         m_context.getUsedApiVersion(), m_context.getTestContext().getCommandLine()));

    m_deviceGroupAllocator = de::MovePtr<Allocator>(
        new SimpleAllocator(*m_deviceGroupVk, *m_deviceGroupLogicalDevice,
                            getPhysicalDeviceMemoryProperties(instance, m_deviceGroupPhysicalDevices[0])));

    return true;
}

class CreateUnusualMultisampleStatesInstance : public vkt::TestInstance
{
public:
    CreateUnusualMultisampleStatesInstance(Context &context);
    virtual ~CreateUnusualMultisampleStatesInstance(void) = default;

    tcu::TestStatus iterate(void) override;
};

CreateUnusualMultisampleStatesInstance::CreateUnusualMultisampleStatesInstance(Context &context)
    : vkt::TestInstance(context)
{
}

tcu::TestStatus CreateUnusualMultisampleStatesInstance::iterate()
{
    const auto &vk            = m_context.getDeviceInterface();
    const auto device         = m_context.getDevice();
    Allocator &allocator      = m_context.getDefaultAllocator();
    uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();

    const auto imageSize    = 8u;
    const auto colorFormat  = VK_FORMAT_R8G8B8A8_UNORM;
    const auto colorSamples = VK_SAMPLE_COUNT_4_BIT;
    const auto extent       = makeExtent3D(imageSize, imageSize, 1u);

    // fill structures that are needed for pipeline creation
    const VkPipelineVertexInputStateCreateInfo vertexInputStateInfo = initVulkanStructure();
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateInfo   = initVulkanStructure();
    inputAssemblyStateInfo.topology                                 = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineRasterizationStateCreateInfo rasterizationStateInfo   = initVulkanStructure();

    const auto viewport                                 = makeViewport(extent);
    const auto scissor                                  = makeRect2D(extent);
    VkPipelineViewportStateCreateInfo viewportStateInfo = initVulkanStructure();
    viewportStateInfo.viewportCount                     = 1u;
    viewportStateInfo.pViewports                        = &viewport;
    viewportStateInfo.scissorCount                      = 1u;
    viewportStateInfo.pScissors                         = &scissor;

    // purpose of this test is to use multisample image sampling with sample shading disabled
    VkPipelineMultisampleStateCreateInfo multisampleStateInfo         = initVulkanStructure();
    multisampleStateInfo.rasterizationSamples                         = colorSamples;
    const VkPipelineDepthStencilStateCreateInfo depthStencilStateInfo = initVulkanStructure();

    VkPipelineColorBlendAttachmentState colorBlendAttachmentState;
    deMemset(&colorBlendAttachmentState, 0x00, sizeof(VkPipelineColorBlendAttachmentState));
    colorBlendAttachmentState.colorWriteMask = 0xFu;

    VkPipelineColorBlendStateCreateInfo colorBlendStateInfo = initVulkanStructure();
    colorBlendStateInfo.attachmentCount                     = 1;
    colorBlendStateInfo.pAttachments                        = &colorBlendAttachmentState;

    // create multisampled color attachment
    const auto imageSubresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const VkImageUsageFlags imageUsage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                       VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);
    auto imageInfo    = makeColorImageCreateInfo(colorFormat, imageSize, imageSize);
    imageInfo.usage   = imageUsage;
    imageInfo.samples = colorSamples;
    ImageWithMemory imageWithMemory(vk, device, allocator, imageInfo, MemoryRequirement::Local);
    Move<VkImageView> imageView =
        makeImageView(vk, device, *imageWithMemory, VK_IMAGE_VIEW_TYPE_2D, colorFormat, imageSubresourceRange);

    // create buffer that will hold resolved multisampled attachment
    const VkBufferUsageFlags bufferUsage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    const auto bufferInfo = makeBufferCreateInfo(imageSize * imageSize * colorSamples * 4 * sizeof(float), bufferUsage);
    BufferWithMemory bufferWithMemory(vk, device, allocator, bufferInfo, MemoryRequirement::HostVisible);

    // create renderpass with two subpasses; in first subpass we change
    // specified samples of multisampled image and in second we copy data
    // of all samples to ssbo
    VkAttachmentDescription attachmentDescription{
        (VkAttachmentDescriptionFlags)0,  // VkAttachmentDescriptionFlags    flags
        colorFormat,                      // VkFormat                        format
        colorSamples,                     // VkSampleCountFlagBits           samples
        VK_ATTACHMENT_LOAD_OP_CLEAR,      // VkAttachmentLoadOp              loadOp
        VK_ATTACHMENT_STORE_OP_DONT_CARE, // VkAttachmentStoreOp             storeOp
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,  // VkAttachmentLoadOp              stencilLoadOp
        VK_ATTACHMENT_STORE_OP_DONT_CARE, // VkAttachmentStoreOp             stencilStoreOp
        VK_IMAGE_LAYOUT_UNDEFINED,        // VkImageLayout                   initialLayout
        VK_IMAGE_LAYOUT_GENERAL           // VkImageLayout                   finalLayout
    };
    const VkAttachmentReference attachmentRef{
        0u,                     // uint32_t         attachment
        VK_IMAGE_LAYOUT_GENERAL // VkImageLayout    layout
    };
    VkSubpassDescription subpassDescriptions[2];
    deMemset(&subpassDescriptions, 0x00, 2 * sizeof(VkSubpassDescription));
    subpassDescriptions[0].pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescriptions[0].colorAttachmentCount = 1;
    subpassDescriptions[0].pColorAttachments    = &attachmentRef;
    subpassDescriptions[1].pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescriptions[1].inputAttachmentCount = 1;
    subpassDescriptions[1].pInputAttachments    = &attachmentRef;

    const VkSubpassDependency subpassDependency{0,
                                                1,
                                                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                                VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
                                                VK_DEPENDENCY_BY_REGION_BIT};

    VkRenderPassCreateInfo renderPassInfo = initVulkanStructure();
    renderPassInfo.attachmentCount        = 1u;
    renderPassInfo.pAttachments           = &attachmentDescription;
    renderPassInfo.subpassCount           = 2u;
    renderPassInfo.pSubpasses             = subpassDescriptions;
    renderPassInfo.dependencyCount        = 1u;
    renderPassInfo.pDependencies          = &subpassDependency;
    auto renderPass                       = createRenderPass(vk, device, &renderPassInfo);

    // create framebuffer
    const auto framebufferCreateInfo = makeFramebufferCreateInfo(*renderPass, 1u, &*imageView, imageSize, imageSize);
    auto framebuffer                 = createFramebuffer(vk, device, &framebufferCreateInfo);

    // create descriptor for second subpass
    auto descriptorPool = DescriptorPoolBuilder()
                              .addType(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1)
                              .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1)
                              .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1);
    auto descriptorSetLayout = DescriptorSetLayoutBuilder()
                                   .addSingleBinding(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT)
                                   .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
                                   .build(vk, device);
    auto descriptorSet = makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout);
    auto dii           = makeDescriptorImageInfo(VK_NULL_HANDLE, *imageView, VK_IMAGE_LAYOUT_GENERAL);
    auto dbi           = makeDescriptorBufferInfo(*bufferWithMemory, 0, VK_WHOLE_SIZE);
    using DSUB         = DescriptorSetUpdateBuilder;
    DSUB()
        .writeSingle(*descriptorSet, DSUB::Location::binding(0), VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, &dii)
        .writeSingle(*descriptorSet, DSUB::Location::binding(1), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &dbi)
        .update(vk, device);

    const auto writePipelineLayout(makePipelineLayout(vk, device));
    const auto readPipelineLayout(makePipelineLayout(vk, device, *descriptorSetLayout));
    auto &bc(m_context.getBinaryCollection());
    auto vertModule  = createShaderModule(vk, device, bc.get("vert"));
    auto frag0Module = createShaderModule(vk, device, bc.get("frag0"));
    auto frag1Module = createShaderModule(vk, device, bc.get("frag1"));
    VkPipelineShaderStageCreateInfo shaderInfo[2]{
        makePipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, *vertModule),
        makePipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, *frag0Module)};

    // create pre-raster pipeline part
    VkGraphicsPipelineLibraryCreateInfoEXT preRasterLibraryInfo = initVulkanStructure();
    preRasterLibraryInfo.flags = VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT |
                                 VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT;
    VkGraphicsPipelineCreateInfo pipelineInfo = initVulkanStructure(&preRasterLibraryInfo);
    pipelineInfo.flags                        = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;
    pipelineInfo.layout                       = *writePipelineLayout;
    pipelineInfo.renderPass                   = *renderPass;
    pipelineInfo.pVertexInputState            = &vertexInputStateInfo;
    pipelineInfo.pInputAssemblyState          = &inputAssemblyStateInfo;
    pipelineInfo.pViewportState               = &viewportStateInfo;
    pipelineInfo.pRasterizationState          = &rasterizationStateInfo;
    pipelineInfo.stageCount                   = 1u;
    pipelineInfo.pStages                      = shaderInfo;
    auto preRasterPipelinePart                = createGraphicsPipeline(vk, device, VK_NULL_HANDLE, &pipelineInfo);

    // create fragment pipeline part
    VkGraphicsPipelineLibraryCreateInfoEXT fragShaderLibInfo = initVulkanStructure();
    fragShaderLibInfo.flags                                  = VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT |
                              VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT;
    pipelineInfo                    = initVulkanStructure(&fragShaderLibInfo);
    pipelineInfo.flags              = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;
    pipelineInfo.layout             = *writePipelineLayout;
    pipelineInfo.renderPass         = *renderPass;
    pipelineInfo.pMultisampleState  = &multisampleStateInfo;
    pipelineInfo.pDepthStencilState = &depthStencilStateInfo;
    pipelineInfo.pColorBlendState   = &colorBlendStateInfo;
    pipelineInfo.stageCount         = 1u;
    pipelineInfo.pStages            = &shaderInfo[1];
    auto fragmentPipelinePart       = createGraphicsPipeline(vk, device, VK_NULL_HANDLE, &pipelineInfo);

    // merge pipelines into writePipeline
    const VkPipeline libraryHandles[]{
        *preRasterPipelinePart,
        *fragmentPipelinePart,
    };
    VkPipelineLibraryCreateInfoKHR linkedPipelineLibraryInfo = initVulkanStructure();
    linkedPipelineLibraryInfo.libraryCount                   = 2u;
    linkedPipelineLibraryInfo.pLibraries                     = libraryHandles;
    VkGraphicsPipelineCreateInfo linkedPipelineInfo          = initVulkanStructure(&linkedPipelineLibraryInfo);
    linkedPipelineInfo.layout                                = *writePipelineLayout;
    const auto writePipeline = createGraphicsPipeline(vk, device, VK_NULL_HANDLE, &linkedPipelineInfo);

    // create readPipeline
    pipelineInfo.pNext               = nullptr;
    pipelineInfo.flags               = 0;
    pipelineInfo.layout              = *readPipelineLayout;
    shaderInfo[1].module             = *frag1Module;
    pipelineInfo.pVertexInputState   = &vertexInputStateInfo;
    pipelineInfo.pInputAssemblyState = &inputAssemblyStateInfo;
    pipelineInfo.pViewportState      = &viewportStateInfo;
    pipelineInfo.pRasterizationState = &rasterizationStateInfo;
    pipelineInfo.stageCount          = 2u;
    pipelineInfo.pStages             = shaderInfo;
    pipelineInfo.subpass             = 1;
    auto readPipeline                = createGraphicsPipeline(vk, device, VK_NULL_HANDLE, &pipelineInfo);

    VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    const auto poolCreateFlags    = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    const auto cmdPool(createCommandPool(vk, device, poolCreateFlags, queueFamilyIndex));
    const auto cmdBuffer(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));
    const auto clearValue(makeClearValueColor(tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f)));

    // render triangle that covers whole color attachment
    beginCommandBuffer(vk, *cmdBuffer);

    beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, scissor, 1, &clearValue);
    vk.cmdBindPipeline(*cmdBuffer, bindPoint, *writePipeline);
    vk.cmdDraw(*cmdBuffer, 3u, 1u, 0u, 0u);
    vk.cmdNextSubpass(*cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);
    vk.cmdBindPipeline(*cmdBuffer, bindPoint, *readPipeline);
    vk.cmdBindDescriptorSets(*cmdBuffer, bindPoint, *readPipelineLayout, 0u, 1u, &*descriptorSet, 0u, nullptr);
    vk.cmdDraw(*cmdBuffer, 3u, 1u, 0u, 0u);
    endRenderPass(vk, *cmdBuffer);

    endCommandBuffer(vk, *cmdBuffer);
    const VkQueue queue = getDeviceQueue(vk, device, queueFamilyIndex, 0);
    submitCommandsAndWait(vk, device, queue, *cmdBuffer);

    uint32_t wrongSampleCount    = 0;
    const uint32_t sampleMask    = 4321; // same sample mask is used also in the shader
    const auto &bufferAllocation = bufferWithMemory.getAllocation();
    invalidateAlloc(vk, device, bufferAllocation);
    tcu::Vec4 redColor(1.0f, 0.0f, 0.0f, 1.0f);
    tcu::Vec4 clearColor(clearValue.color.float32);
    float *bufferPtr = static_cast<float *>(bufferAllocation.getHostPtr());
    for (uint32_t f = 0; f < imageSize * imageSize; ++f)
    {
        for (uint32_t sample = 0; sample < colorSamples; ++sample)
        {
            // make sure only those samples where the sample mask bit is non-zero have the "red" pixel values
            const float *color = bufferPtr + (f * colorSamples + sample) * 4; // there are 4 color components
            tcu::Vec4 sampleColor(color[0], color[1], color[2], color[3]);
            const tcu::Vec4 &expectedColor = (sampleMask & (1 << sample)) ? redColor : clearColor;
            wrongSampleCount += (sampleColor != expectedColor);
        }
    }

    if (wrongSampleCount == 0)
        return tcu::TestStatus::pass("Pass");

    // log image
    tcu::PixelBufferAccess resultAccess(mapVkFormat(VK_FORMAT_R32G32B32A32_SFLOAT), imageSize * colorSamples, imageSize,
                                        1, bufferPtr);
    m_context.getTestContext().getLog() << tcu::LogImage("image", "", resultAccess);

    return tcu::TestStatus::fail(std::to_string(wrongSampleCount) + " wrong samples values out of " +
                                 std::to_string(imageSize * imageSize * colorSamples));
}

class PipelineLibraryMiscTestCase : public TestCase
{
public:
    PipelineLibraryMiscTestCase(tcu::TestContext &context, const char *name, const MiscTestParams data);
    ~PipelineLibraryMiscTestCase(void) = default;

    void checkSupport(Context &context) const;
    void initPrograms(SourceCollections &programCollection) const;
    TestInstance *createInstance(Context &context) const;

private:
    MiscTestParams m_testParams;
};

PipelineLibraryMiscTestCase::PipelineLibraryMiscTestCase(tcu::TestContext &context, const char *name,
                                                         const MiscTestParams params)
    : TestCase(context, name)
    , m_testParams(params)
{
}

void PipelineLibraryMiscTestCase::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality("VK_EXT_graphics_pipeline_library");

    if ((m_testParams.mode == MiscTestMode::INDEPENDENT_PIPELINE_LAYOUT_SETS_FAST_LINKED) &&
        !context.getGraphicsPipelineLibraryPropertiesEXT().graphicsPipelineLibraryFastLinking)
        TCU_THROW(NotSupportedError, "graphicsPipelineLibraryFastLinking is not supported");

    if (m_testParams.mode == MiscTestMode::SHADER_MODULE_CREATE_INFO_RT ||
        m_testParams.mode == MiscTestMode::SHADER_MODULE_CREATE_INFO_RT_LIB)
        context.requireDeviceFunctionality("VK_KHR_ray_tracing_pipeline");

    if (m_testParams.mode == MiscTestMode::SHADER_MODULE_CREATE_INFO_RT_LIB)
        context.requireDeviceFunctionality("VK_KHR_pipeline_library");

    if (m_testParams.mode == MiscTestMode::NULL_RENDERING_CREATE_INFO)
        context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");

    if (m_testParams.mode == MiscTestMode::COMMON_FRAG_LIBRARY)
    {
        context.requireDeviceFunctionality("VK_EXT_mesh_shader");

        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SHADER_CLIP_DISTANCE);
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SHADER_CULL_DISTANCE);

        // Check limits for clip and cull distances
        const VkPhysicalDeviceLimits limits =
            getPhysicalDeviceProperties(context.getInstanceInterface(), context.getPhysicalDevice()).limits;
        if ((numClipDistances > limits.maxClipDistances) || (numCullDistances > limits.maxCullDistances) ||
            ((numClipDistances + numCullDistances) > limits.maxCombinedClipAndCullDistances))
            TCU_THROW(NotSupportedError, "Specified values of clip or cull distances are not supported");
    }

    if (m_testParams.mode == MiscTestMode::VIEW_INDEX_FROM_DEVICE_INDEX)
    {
        const auto &modeParams = m_testParams.get<ViewIndexFromDeviceIndexParams>();
        if (modeParams.useMeshShading)
        {
            context.requireDeviceFunctionality("VK_KHR_multiview");
            context.requireDeviceFunctionality("VK_EXT_mesh_shader");
            const auto &meshShaderFeatures = context.getMeshShaderFeaturesEXT();
            if (!meshShaderFeatures.multiviewMeshShader)
                TCU_THROW(NotSupportedError, "multiviewMeshShader not supported");
        }
        else
            context.requireDeviceFunctionality("VK_KHR_multiview");
    }
}

void PipelineLibraryMiscTestCase::initPrograms(SourceCollections &programCollection) const
{
    if ((m_testParams.mode == MiscTestMode::BIND_NULL_DESCRIPTOR_SET) ||
        (m_testParams.mode == MiscTestMode::BIND_NULL_DESCRIPTOR_SET_IN_MONOLITHIC_PIPELINE))
    {
        std::string vertDefinition = "";
        std::string fragDefinition = "";
        std::string vertValue      = "  vec4 v = vec4(-1.0, 1.0, 2.0, -2.0);\n";
        std::string fragValue      = "  vec4 v = vec4(0.0, 0.2, 0.6, 0.75);\n";

        // define lambda that creates proper uniform buffer definition
        auto constructBufferDefinition = [](uint32_t setIndex)
        {
            return std::string("layout(set = ") + std::to_string(setIndex) +
                   ", binding = 0) uniform buf\n"
                   "{\n"
                   "  vec4 v;\n"
                   "};\n\n";
        };

        if (m_testParams.mode == MiscTestMode::BIND_NULL_DESCRIPTOR_SET_IN_MONOLITHIC_PIPELINE)
        {
            // descriptor set 0 will be nullptr, descriptor set 1 will be valid buffer with color
            fragDefinition = constructBufferDefinition(1);
            fragValue      = "";
        }
        else
        {
            const auto &modeParams = m_testParams.get<NullDescriptorSetParams>();

            if (modeParams.layoutsBits > 0u)
            {
                std::vector<uint32_t> bitsThatAreSet;
                const uint32_t maxBitsCount = 8 * sizeof(modeParams.layoutsBits);

                // find set bits
                for (uint32_t i = 0u; i < modeParams.layoutsCount; ++i)
                {
                    if (modeParams.layoutsBits & (1 << (maxBitsCount - 1 - i)))
                        bitsThatAreSet.push_back(i);
                }

                // there should be 1 or 2 bits set
                DE_ASSERT((bitsThatAreSet.size() > 0) && (bitsThatAreSet.size() < 3));

                vertDefinition = constructBufferDefinition(bitsThatAreSet[0]);
                vertValue      = "";

                if (bitsThatAreSet.size() == 2u)
                {
                    fragDefinition = constructBufferDefinition(bitsThatAreSet[1]);
                    fragValue      = "";
                }
            }
        }

        programCollection.glslSources.add("vert")
            << glu::VertexSource(std::string("#version 450\n"
                                             "precision mediump int;\nprecision highp float;\n") +
                                 vertDefinition +
                                 "out gl_PerVertex\n"
                                 "{\n"
                                 "  vec4 gl_Position;\n"
                                 "};\n\n"
                                 "void main()\n"
                                 "{\n" +
                                 vertValue +
                                 "  const float x = (v.x+v.z*((gl_VertexIndex & 2)>>1));\n"
                                 "  const float y = (v.y+v.w* (gl_VertexIndex % 2));\n"

                                 // note: there won't be full screen quad because of used scissors
                                 "  gl_Position = vec4(x, y, 0.0, 1.0);\n"
                                 "}\n");

        programCollection.glslSources.add("frag")
            << glu::FragmentSource(std::string("#version 450\n"
                                               "precision mediump int; precision highp float;"
                                               "layout(location = 0) out highp vec4 o_color;\n") +
                                   fragDefinition +
                                   "void main()\n"
                                   "{\n" +
                                   fragValue +
                                   "  o_color = v;\n"
                                   "}\n");
    }
    else if ((m_testParams.mode == MiscTestMode::INDEPENDENT_PIPELINE_LAYOUT_SETS_FAST_LINKED) ||
             (m_testParams.mode ==
              MiscTestMode::INDEPENDENT_PIPELINE_LAYOUT_SETS_WITH_LINK_TIME_OPTIMIZATION_UNION_HANDLE))
    {
        programCollection.glslSources.add("vert")
            << glu::VertexSource("#version 450\n"
                                 "precision mediump int; precision highp float;\n"
                                 "layout(set = 0, binding = 0) uniform bufA\n"
                                 "{\n"
                                 "  vec4 valueA;\n"
                                 "};\n"
                                 "layout(set = 2, binding = 0) uniform bufC\n"
                                 "{\n"
                                 "  vec4 valueC;\n"
                                 "};\n"
                                 "out gl_PerVertex\n"
                                 "{\n"
                                 "  vec4 gl_Position;\n"
                                 "};\n\n"
                                 "void main()\n"
                                 "{\n"
                                 // note: values in buffers were set to get vec4(-1, 1, 2, -2)
                                 "  const vec4  v = valueA + valueC;\n"
                                 "  const float x = (v.x+v.z*((gl_VertexIndex & 2)>>1));\n"
                                 "  const float y = (v.y+v.w* (gl_VertexIndex % 2));\n"

                                 // note: there won't be full screen quad because of used scissors
                                 "  gl_Position = vec4(x, y, 0.0, 1.0);\n"
                                 "}\n");

        programCollection.glslSources.add("frag")
            << glu::FragmentSource("#version 450\n"
                                   "precision mediump int; precision highp float;"
                                   "layout(location = 0) out highp vec4 o_color;\n"
                                   "layout(set = 0, binding = 0) uniform bufA\n"
                                   "{\n"
                                   "  vec4 valueA;\n"
                                   "};\n"
                                   "layout(set = 1, binding = 0) uniform bufB\n"
                                   "{\n"
                                   "  vec4 valueB;\n"
                                   "};\n"
                                   "void main()\n"
                                   "{\n"
                                   // note: values in buffers were set to get vec4(0.0, 0.75, 0.5, 0.2)
                                   "  o_color = valueA * valueB;\n"
                                   "}\n");
    }
    else if (m_testParams.mode == MiscTestMode::COMPARE_LINK_TIMES)
    {
        programCollection.glslSources.add("vert") << glu::VertexSource(
            "#version 450\n"
            "precision mediump int; precision highp float;"
            "layout(location = 0) in vec4 in_position;\n"
            "out gl_PerVertex\n"
            "{\n"
            "  vec4 gl_Position;\n"
            "};\n"
            "layout(constant_id = 0) const int random = 0;\n\n"
            "void main()\n"
            "{\n"
            "   gl_Position = vec4(float(1 - 2 * int(gl_VertexIndex != 1)),\n"
            "                      float(1 - 2 * int(gl_VertexIndex > 0)), 0.0, 1.0) + float(random & 1);\n"
            "}\n");

        programCollection.glslSources.add("frag")
            << glu::FragmentSource("#version 450\n"
                                   "precision mediump int; precision highp float;"
                                   "layout(location = 0) out highp vec4 o_color;\n"
                                   "layout(constant_id = 0) const int random = 0;\n\n"
                                   "void main()\n"
                                   "{\n"
                                   "  o_color = vec4(0.0, 1.0, 0.5, 1.0) + float(random & 1);\n"
                                   "}\n");
    }
    else if (m_testParams.mode == MiscTestMode::SHADER_MODULE_CREATE_INFO_COMP)
    {
        std::ostringstream comp;
        comp << "#version 450\n"
             << "layout (set=0, binding=0, std430) buffer BufferBlock {\n"
             << "    uint values[" << PipelineLibraryShaderModuleInfoInstance::kOutputBufferElements << "];\n"
             << "} outBuffer;\n"
             << "layout (local_size_x=" << PipelineLibraryShaderModuleInfoInstance::kOutputBufferElements
             << ", local_size_y=1, local_size_z=1) in;\n"
             << "void main (void)\n"
             << "{\n"
             << "    outBuffer.values[gl_LocalInvocationIndex] = gl_LocalInvocationIndex;\n"
             << "}\n";
        programCollection.glslSources.add("comp") << glu::ComputeSource(comp.str());
    }
    else if (m_testParams.mode == MiscTestMode::SHADER_MODULE_CREATE_INFO_RT ||
             m_testParams.mode == MiscTestMode::SHADER_MODULE_CREATE_INFO_RT_LIB)
    {
        const vk::ShaderBuildOptions buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);
        std::ostringstream rgen;
        rgen << "#version 460 core\n"
             << "#extension GL_EXT_ray_tracing : require\n"
             << "layout (set=0, binding=0, std430) buffer BufferBlock {\n"
             << "    uint values[" << PipelineLibraryShaderModuleInfoInstance::kOutputBufferElements << "];\n"
             << "} outBuffer;\n"
             << "void main (void)\n"
             << "{\n"
             << "    outBuffer.values[gl_LaunchIDEXT.x] = gl_LaunchIDEXT.x;\n"
             << "}\n";
        programCollection.glslSources.add("rgen") << glu::RaygenSource(rgen.str()) << buildOptions;
    }
    else if (m_testParams.mode == MiscTestMode::NULL_RENDERING_CREATE_INFO)
    {
        std::ostringstream vert;
        vert << "#version 460\n"
             << "vec2 positions[3] = vec2[](\n"
             << "    vec2(-1.0, -1.0),\n"
             << "    vec2(-1.0,  3.0),\n"
             << "    vec2( 3.0, -1.0)\n"
             << ");\n"
             << "void main() {\n"
             << "    gl_Position = vec4(positions[gl_VertexIndex % 3], 0.0, 1.0);\n"
             << "}\n";
        programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

        std::ostringstream frag;
        frag << "#version 460\n"
             << "layout (location=0) out vec4 color;\n"
             << "void main () {\n"
             << "    color = vec4(0.0, 0.0, 1.0, 1.0);\n"
             << "}\n";
        programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
    }
    else if (m_testParams.mode == MiscTestMode::COMMON_FRAG_LIBRARY)
    {
        std::ostringstream vert;
        {
            std::string perVertexBlock;
            {
                std::ostringstream str;
                str << "gl_PerVertex {\n"
                    << "    vec4  gl_Position;\n";
                str << "    float gl_ClipDistance[" << numClipDistances << "];\n";
                str << "    float gl_CullDistance[" << numCullDistances << "];\n";
                str << "}";
                perVertexBlock = str.str();
            }

            vert << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
                 << "\n"
                 << "layout(location = 0) in  vec4 v_position;\n"
                 << "layout(location = 0) out vec4 out_color;\n"
                 << "\n"
                 << "out " << perVertexBlock << ";\n"
                 << "\n"
                 << "void main (void)\n"
                 << "{\n"
                 << "    gl_Position = v_position;\n"
                 << "    out_color   = vec4(1.0, 0.5 * (v_position.x + 1.0), 0.0, 1.0);\n"
                 << "\n"
                 << "    const int barNdx = gl_VertexIndex / 6;\n"
                 << "    for (int i = 0; i < " << numClipDistances << "; ++i)\n"
                 << "      gl_ClipDistance[i] = (barNdx == i ? v_position.y : 0.0);\n"
                 << "    for (int i = 0; i < " << numCullDistances << "; ++i)\n"
                 << "    gl_CullDistance[i] = (gl_Position.y < 0) ? -0.5f : 0.5f;\n"
                 << "}\n";
        }
        programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

        const auto buildOptions =
            vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0u, true);
        std::ostringstream mesh;
        {
            mesh << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
                 << "#extension GL_EXT_mesh_shader : enable\n"
                 << "\n"
                 << "layout(local_size_x = 16) in;\n"
                 << "layout(triangles, max_vertices = 48, max_primitives = 16) out;\n"
                 << "\n"
                 << "layout (set=0, binding=0) uniform CoordsBuffer {\n"
                 << "    vec4 coords[48];\n"
                 << "} cb;\n"
                 << "\n"
                 << "layout (location = 0) out PerVertexData {\n"
                 << "    vec4 color;\n"
                 << "} v_out[];\n"
                 << "\n"

                 << "out gl_MeshPerVertexEXT {\n"
                 << "  vec4  gl_Position;\n"
                 << "  float gl_PointSize;\n"
                 << "  float gl_ClipDistance[" << numClipDistances << "];\n"
                 << "  float gl_CullDistance[" << numCullDistances << "];\n"
                 << "} gl_MeshVerticesEXT[];\n"

                 << "void main ()\n"
                 << "{\n"
                 << "  SetMeshOutputsEXT(48u, 16u);\n"
                 << "  uint idx = gl_LocalInvocationIndex * 3;\n"
                 << "  for (uint j = 0; j < 3; j++) {\n"
                 << "    uint vIdx = idx+j;\n"
                 << "    gl_MeshVerticesEXT[vIdx].gl_Position = cb.coords[vIdx];\n"
                 << "    uint barNdx = vIdx / 6;\n"

                 << "    for (int i = 0; i < " << numClipDistances << "; ++i)\n"
                 << "      gl_MeshVerticesEXT[vIdx].gl_ClipDistance[i] = ((barNdx == i) ? cb.coords[vIdx].y : 0);\n"

                 << "    for (int i = 0; i < " << numCullDistances << "; ++i)\n"
                 << "      gl_MeshVerticesEXT[vIdx].gl_CullDistance[i] = ((cb.coords[vIdx].y < 0) ? -0.5 : 0.5);\n"

                 << "    float xx = cb.coords[vIdx].x;\n"
                 << "    v_out[vIdx].color = vec4(1.0, 0.5 * (xx + 1.0), 0.0, 1.0);\n"
                 << "  }\n"
                 << "  gl_PrimitiveTriangleIndicesEXT[gl_LocalInvocationIndex] = uvec3(idx, idx+1, idx+2);\n"
                 << "}\n";
        }
        programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str()) << buildOptions;

        std::ostringstream frag;

        {
            frag << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
                 << "\n"
                 << "layout(location = 0) in flat vec4 in_color;\n"
                 << "layout(location = 0) out vec4 o_color;\n";
            frag << "in float gl_ClipDistance[" << numClipDistances << "];\n";
            frag << "in float gl_CullDistance[" << numCullDistances << "];\n";

            frag << "\n"
                 << "void main (void)\n"
                 << "{\n";

            frag << "    o_color = vec4(in_color.r, "
                 << "    gl_ClipDistance[" << (numClipDistances / 2) << "], "
                 << "    gl_CullDistance[" << (numCullDistances / 2) << "], "
                 << "    1.0);\n";
            frag << "}\n";
        }
        programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
    }
    else if (m_testParams.mode == MiscTestMode::VIEW_INDEX_FROM_DEVICE_INDEX)
    {
        const auto &modeParams = m_testParams.get<ViewIndexFromDeviceIndexParams>();
        const auto buildOptions =
            vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_4, 0, true);

        if (modeParams.useMeshShading)
        {
            std::string mesh("#version 450\n"
                             "#extension GL_EXT_mesh_shader : enable\n"
                             "#extension GL_EXT_multiview : require\n"
                             "layout(local_size_x=3) in;\n"
                             "layout(triangles) out;\n"
                             "layout(max_vertices=3, max_primitives=1) out;\n"
                             "layout(location = 0) perprimitiveEXT flat out uvec4 mViewIndex[];\n"
                             "void main() {\n"
                             "  SetMeshOutputsEXT(3u, 1u);\n"
                             "  const uint idx = gl_LocalInvocationIndex;\n"
                             "  const float x = -1.0 + 4.0 * ((idx & 2)>>1);\n"
                             "  const float y = -1.0 + 4.0 * (idx % 2);\n"
                             "  gl_MeshVerticesEXT[idx].gl_Position = vec4(x, y, 0.0, 1.0);\n"
                             "  gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0, 1, 2);\n"
                             "  mViewIndex[idx] = uvec4(0);\n"
                             "  mViewIndex[idx].x = gl_ViewIndex;\n"
                             "}\n");
            programCollection.glslSources.add("mesh") << glu::MeshSource(mesh) << buildOptions;

            std::string frag("#version 460\n"
                             "#extension GL_EXT_multiview : require\n"
                             "#extension GL_EXT_mesh_shader : enable\n"
                             "layout(location = 0) perprimitiveEXT flat in uvec4 mViewIndex;\n"
                             "layout (location=0) out uvec4 color;\n"
                             "void main () {\n"
                             "  color = mViewIndex;\n"
                             "  color.a = gl_ViewIndex;\n"
                             "}\n");
            programCollection.glslSources.add("frag") << glu::FragmentSource(frag) << buildOptions;
        }
        else
        {
            std::string vert("#version 460\n"
                             "#extension GL_EXT_multiview : require\n"
                             "layout(location = 0) flat out uvec4 vViewIndex;"
                             "void main() {\n"
                             "  const float x = -1.0 + 4.0 * ((gl_VertexIndex & 2)>>1);\n"
                             "  const float y = -1.0 + 4.0 * (gl_VertexIndex % 2);\n"
                             "  gl_Position = vec4(x, y, 0.0, 1.0);\n"
                             "  vViewIndex = uvec4(0);\n"
                             "  vViewIndex.x = gl_ViewIndex;\n"
                             "}\n");
            programCollection.glslSources.add("vert") << glu::VertexSource(vert);

            std::string tesc("#version 450\n"
                             "#extension GL_EXT_multiview : require\n"
                             "layout (vertices = 3) out;\n"
                             "layout(location = 0) flat in uvec4 vViewIndex[];\n"
                             "layout(location = 0) flat out uvec4 vtcViewIndex[];\n"
                             "void main (void)\n"
                             "{\n"
                             "  gl_TessLevelInner[0] = 1.0;\n"
                             "  gl_TessLevelInner[1] = 1.0;\n"
                             "  gl_TessLevelOuter[0] = 1.0;\n"
                             "  gl_TessLevelOuter[1] = 1.0;\n"
                             "  gl_TessLevelOuter[2] = 1.0;\n"
                             "  gl_TessLevelOuter[3] = 1.0;\n"
                             "  vtcViewIndex[gl_InvocationID] = vViewIndex[gl_InvocationID];\n"
                             "  vtcViewIndex[gl_InvocationID].y = gl_ViewIndex;\n"
                             "  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
                             "}\n");
            programCollection.glslSources.add("tesc") << glu::TessellationControlSource(tesc);

            std::string tese("#version 450\n"
                             "#extension GL_EXT_multiview : require\n"
                             "layout(triangles, fractional_odd_spacing, cw) in;\n"
                             "layout(location = 0) flat in uvec4 vtcViewIndex[];\n"
                             "layout(location = 0) flat out uvec4 vtViewIndex;\n"
                             "void main (void)\n"
                             "{\n"
                             "  gl_Position = (gl_TessCoord.x * gl_in[0].gl_Position) +\n"
                             "                (gl_TessCoord.y * gl_in[1].gl_Position) +\n"
                             "                (gl_TessCoord.z * gl_in[2].gl_Position);\n"
                             "  vtViewIndex = vtcViewIndex[0];\n"
                             "  vtViewIndex.y += gl_ViewIndex;\n"
                             "}\n");
            programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(tese);

            std::string geom("#version 450\n"
                             "#extension GL_EXT_multiview : require\n"
                             "layout (triangles) in;\n"
                             "layout (triangle_strip, max_vertices=3) out;\n"
                             "layout(location = 0) flat in uvec4 vtViewIndex[];\n"
                             "layout(location = 0) flat out uvec4 vtgViewIndex;\n"
                             "void main (void)\n"
                             "{\n"
                             "  for (int i = 0; i < 3; i++)\n"
                             "  {\n"
                             "    gl_Position = gl_in[i].gl_Position;\n"
                             "    vtgViewIndex = vtViewIndex[i];\n"
                             "    vtgViewIndex.z = gl_ViewIndex;\n"
                             "    EmitVertex();\n"
                             "  }\n"
                             "}\n");
            programCollection.glslSources.add("geom") << glu::GeometrySource(geom);

            std::string frag("#version 460\n"
                             "#extension GL_EXT_multiview : require\n"
                             "layout(location = 0) flat in uvec4 vtgViewIndex;\n"
                             "layout (location=0) out uvec4 color;\n"
                             "void main () {\n"
                             "  color = vtgViewIndex;\n"
                             "  color.a = gl_ViewIndex;\n"
                             "}\n");
            programCollection.glslSources.add("frag") << glu::FragmentSource(frag);
        }
    }
    else if (m_testParams.mode == MiscTestMode::UNUSUAL_MULTISAMPLE_STATE)
    {
        programCollection.glslSources.add("vert")
            << glu::VertexSource("#version 460\n"
                                 "void main() {\n"
                                 "  const float x = -1.0 + 4.0 * ((gl_VertexIndex & 2)>>1);\n"
                                 "  const float y = -1.0 + 4.0 * (gl_VertexIndex % 2);\n"
                                 "  gl_Position = vec4(x, y, 0.0, 1.0);\n"
                                 "}\n");
        programCollection.glslSources.add("frag0")
            << glu::FragmentSource("#version 460\n"
                                   "layout(location = 0) out highp vec4 o_color;\n"
                                   "void main()\n"
                                   "{\n"
                                   "    const int numSamples = 4;\n"
                                   "    const int sampleMask = 4321;\n"
                                   "    for (int i = 0; i < (numSamples + 31) / 32; ++i) {\n"
                                   "        gl_SampleMask[i] = sampleMask & gl_SampleMaskIn[i];\n"
                                   "    }\n"
                                   "    o_color = vec4(1, 0, 0, 1);\n"
                                   "}\n");
        programCollection.glslSources.add("frag1") << glu::FragmentSource(
            "#version 460\n"
            "layout (input_attachment_index=0, binding = 0) uniform subpassInputMS inputAttachment;\n"
            "layout (binding = 1) buffer resultBuffer { vec4 v[]; };\n"
            "void main()\n"
            "{\n"
            "    const int numSamples = 4;\n"
            "    const int imageWidth = 8;\n"
            "    const ivec2 coord = ivec2(int(gl_FragCoord.x), gl_FragCoord.y);\n"
            "    const uint fIndex = (coord.y * imageWidth + coord.x) * numSamples;\n"
            "    for (int sampleId = 0; sampleId < numSamples; ++sampleId) {\n"
            "        v[fIndex + sampleId] = subpassLoad(inputAttachment, sampleId);\n"
            "    }\n"
            "}\n");
    }
    else
    {
        DE_ASSERT(false);
    }
}

TestInstance *PipelineLibraryMiscTestCase::createInstance(Context &context) const
{
    if (m_testParams.mode == MiscTestMode::SHADER_MODULE_CREATE_INFO_COMP)
        return new PipelineLibraryShaderModuleInfoCompInstance(context);

    if (m_testParams.mode == MiscTestMode::SHADER_MODULE_CREATE_INFO_RT)
        return new PipelineLibraryShaderModuleInfoRTInstance(context, false /*withLibrary*/);

    if (m_testParams.mode == MiscTestMode::SHADER_MODULE_CREATE_INFO_RT_LIB)
        return new PipelineLibraryShaderModuleInfoRTInstance(context, true /*withLibrary*/);

    if (m_testParams.mode == MiscTestMode::NULL_RENDERING_CREATE_INFO)
        return new NullRenderingCreateInfoInstance(context);

    if (m_testParams.mode == MiscTestMode::VIEW_INDEX_FROM_DEVICE_INDEX)
        return new CreateViewIndexFromDeviceIndexInstance(context, m_testParams);

    if (m_testParams.mode == MiscTestMode::UNUSUAL_MULTISAMPLE_STATE)
        return new CreateUnusualMultisampleStatesInstance(context);

    return new PipelineLibraryMiscTestInstance(context, m_testParams);
}

void addPipelineLibraryConfigurationsTests(tcu::TestCaseGroup *group, bool optimize)
{
    const int R                                                 = -1;
    const PipelineTreeConfiguration pipelineTreeConfiguration[] = {
        {{
            {R, 4}, /*     4     */
        }},

        {{
            {R, 0}, /*     0     */
                    /*  / / \ \  */
            {0, 1},
            {0, 1},
            {0, 1},
            {0, 1} /*  1 1 1 1  */
        }},

        {{
            {R, 0}, /*     0     */
                    /*  / / \    */
            {0, 1},
            {0, 1},
            {0, 2} /*  1 1  2   */
        }},

        {{
            {R, 0}, /*     0     */
                    /*  / / \    */
            {0, 1},
            {0, 2},
            {0, 1} /* 1 2   1   */
        }},

        {{
            {R, 0}, /*     0     */
                    /*    / \    */
            {0, 2},
            {0, 2}, /*   2   2   */
        }},

        {{
            {R, 1}, /*     1     */
                    /*    / \    */
            {0, 2},
            {0, 1}, /*   2   1   */
        }},

        {{
            {R, 2}, /*     2     */
                    /*    / \    */
            {0, 1},
            {0, 1}, /*   1   1   */
        }},

        {{
            {R, 3}, /*     3     */
                    /*    /      */
            {0, 1}, /*   1       */
        }},

        {{
            {R, 1}, /*     1     */
                    /*    /      */
            {0, 3}, /*   3       */
        }},

        {{
            {R, 0}, /*     0     */
                    /*    / \    */
            {0, 0},
            {0, 0}, /*   0   0   */
                    /*  / \ / \  */
            {1, 1},
            {1, 1},
            {2, 1},
            {2, 1}, /* 1  1 1  1 */
        }},

        {{
            {R, 0}, /*     0     */
                    /*    / \    */
            {0, 0},
            {0, 1}, /*   0   1   */
                    /*  / \   \  */
            {1, 1},
            {1, 1},
            {2, 1}, /* 1   1   1 */
        }},

        {{
            {R, 1}, /*     1     */
                    /*    / \    */
            {0, 0},
            {0, 1}, /*   0   1   */
                    /*  / \      */
            {1, 1},
            {1, 1}, /* 1   1     */
        }},

        {{
            {R, 1}, /*     1     */
                    /*    /      */
            {0, 1}, /*   1       */
                    /*  / \      */
            {1, 1},
            {1, 1}, /* 1   1     */
        }},

        {{
            {R, 1}, /*        1  */
                    /*       /   */
            {0, 1}, /*      1    */
                    /*     /     */
            {1, 1}, /*    1      */
                    /*   /       */
            {2, 1}, /*  1        */
        }},
    };

    for (size_t libConfigNdx = 0; libConfigNdx < DE_LENGTH_OF_ARRAY(pipelineTreeConfiguration); ++libConfigNdx)
    {
        const bool delayedShaderCreate = (libConfigNdx != 0);
        const TestParams testParams    = {
            pipelineTreeConfiguration[libConfigNdx], //  PipelineTreeConfiguration pipelineTreeConfiguration;
            optimize,                                //  bool optimize;
            delayedShaderCreate,                     //  bool delayedShaderCreate;
            false                                    //  bool useMaintenance5;
        };
        const std::string testName = getTestName(pipelineTreeConfiguration[libConfigNdx]);

        if (optimize && testParams.pipelineTreeConfiguration.size() == 1)
            continue;

        group->addChild(new PipelineLibraryTestCase(group->getTestContext(), testName.c_str(), testParams));
    }

    // repeat first case (one that creates montolithic pipeline) to test VK_KHR_maintenance5;
    // VkShaderModule deprecation (tested with delayedShaderCreate) was added to VK_KHR_maintenance5
    if (optimize == false)
    {
        const TestParams testParams{
            pipelineTreeConfiguration[0], //  PipelineTreeConfiguration pipelineTreeConfiguration;
            false,                        //  bool optimize;
            true,                         //  bool delayedShaderCreate;
            true                          //  bool useMaintenance5;
        };

        group->addChild(new PipelineLibraryTestCase(group->getTestContext(), "maintenance5", testParams));
    }
}

} // namespace

tcu::TestCaseGroup *createPipelineLibraryTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "graphics_library"));

    // Tests graphics pipeline libraries linkage without optimization
    addTestGroup(group.get(), "fast", addPipelineLibraryConfigurationsTests, false);
    // Tests graphics pipeline libraries linkage with optimization
    addTestGroup(group.get(), "optimize", addPipelineLibraryConfigurationsTests, true);

    de::MovePtr<tcu::TestCaseGroup> miscTests(new tcu::TestCaseGroup(testCtx, "misc"));

    de::MovePtr<tcu::TestCaseGroup> independentLayoutSetsTests(
        new tcu::TestCaseGroup(testCtx, "independent_pipeline_layout_sets"));
    independentLayoutSetsTests->addChild(new PipelineLibraryMiscTestCase(
        testCtx, "fast_linked", {MiscTestMode::INDEPENDENT_PIPELINE_LAYOUT_SETS_FAST_LINKED, 0u, 0u}));
    independentLayoutSetsTests->addChild(new PipelineLibraryMiscTestCase(
        testCtx, "link_opt_union_handle",
        {MiscTestMode::INDEPENDENT_PIPELINE_LAYOUT_SETS_WITH_LINK_TIME_OPTIMIZATION_UNION_HANDLE, 0u, 0u}));
    miscTests->addChild(independentLayoutSetsTests.release());

    de::MovePtr<tcu::TestCaseGroup> bindNullDescriptorCombinationsTests(
        new tcu::TestCaseGroup(testCtx, "bind_null_descriptor_set"));
    const std::vector<const char *> bindNullDescriptorCombinations{
        // note: there will be as many descriptor sets layouts in pipeline layout as there are chcaracters in the case name;
        // '0' represents unused descriptor set layout, '1' represents used one;
        // location of first '1' represents index of layout used in vertex shader;
        // if present second '1' represents index of layout used in fragment shader
        "1",   "11", "01", "10", "101", "1010",
        "1001" // descriptor sets layouts for first pipeline part will be (&layoutA, NULL, NULL, NULL),
               //                                     for second pipeline part (NULL, NULL, NULL, &layoutB)
    };
    for (const char *name : bindNullDescriptorCombinations)
    {
        uint32_t layoutsCount = static_cast<uint32_t>(strlen(name));
        uint32_t layoutsBits  = 0u;

        // construct uint32_t with bits sets based on case name
        for (uint32_t i = 0; i < layoutsCount; ++i)
            layoutsBits |= (name[i] == '1') * (1 << (8 * sizeof(layoutsBits) - i - 1));

        bindNullDescriptorCombinationsTests->addChild(new PipelineLibraryMiscTestCase(
            testCtx, name, {MiscTestMode::BIND_NULL_DESCRIPTOR_SET, layoutsCount, layoutsBits}));
    }
    miscTests->addChild(bindNullDescriptorCombinationsTests.release());

    {
        de::MovePtr<tcu::TestCaseGroup> otherTests(new tcu::TestCaseGroup(testCtx, "other"));
        otherTests->addChild(
            new PipelineLibraryMiscTestCase(testCtx, "compare_link_times", {MiscTestMode::COMPARE_LINK_TIMES}));
        otherTests->addChild(
            new PipelineLibraryMiscTestCase(testCtx, "null_descriptor_set_in_monolithic_pipeline",
                                            {MiscTestMode::BIND_NULL_DESCRIPTOR_SET_IN_MONOLITHIC_PIPELINE}));
        otherTests->addChild(new PipelineLibraryMiscTestCase(testCtx, "null_rendering_create_info",
                                                             {MiscTestMode::NULL_RENDERING_CREATE_INFO}));
        otherTests->addChild(new PipelineLibraryMiscTestCase(testCtx, "common_frag_pipeline_library",
                                                             {MiscTestMode::COMMON_FRAG_LIBRARY}));

        std::pair<std::string, PipelineStateMode> pipelineStateModes[]{
            {"in_all_stages", PipelineStateMode::ALL_STAGES},
            {"in_pre_rasterization", PipelineStateMode::PRE_RASTERIZATION},
            {"in_fragment", PipelineStateMode::FRAGMENT}};
        std::string baseName;
        baseName.reserve(60);
        for (const auto &pipelineStateMode : pipelineStateModes)
        {
            baseName = "view_index_from_device_index_" + pipelineStateMode.first;
            for (uint32_t combination = 0; combination < 4; ++combination)
            {
                std::string name    = baseName;
                bool useMeshShading = (combination > 1);
                if (useMeshShading)
                    name += "_mesh_shading";
                bool useLinkTimeOpt = (combination % 2);
                if (useLinkTimeOpt)
                    name += "_link_time_opt";

                otherTests->addChild(
                    new PipelineLibraryMiscTestCase(testCtx, name.c_str(),
                                                    {MiscTestMode::VIEW_INDEX_FROM_DEVICE_INDEX,
                                                     pipelineStateMode.second, useMeshShading, useLinkTimeOpt}));
            }
        }

        otherTests->addChild(new PipelineLibraryMiscTestCase(testCtx, "unusual_multisample_state",
                                                             {MiscTestMode::UNUSUAL_MULTISAMPLE_STATE, 0u, 0u}));

        miscTests->addChild(otherTests.release());
    }

    {
        de::MovePtr<tcu::TestCaseGroup> nonGraphicsTests(new tcu::TestCaseGroup(testCtx, "non_graphics"));
        nonGraphicsTests->addChild(new PipelineLibraryMiscTestCase(testCtx, "shader_module_info_comp",
                                                                   {MiscTestMode::SHADER_MODULE_CREATE_INFO_COMP}));
        nonGraphicsTests->addChild(new PipelineLibraryMiscTestCase(testCtx, "shader_module_info_rt",
                                                                   {MiscTestMode::SHADER_MODULE_CREATE_INFO_RT}));
        nonGraphicsTests->addChild(new PipelineLibraryMiscTestCase(testCtx, "shader_module_info_rt_lib",
                                                                   {MiscTestMode::SHADER_MODULE_CREATE_INFO_RT_LIB}));
        miscTests->addChild(nonGraphicsTests.release());
    }

    group->addChild(miscTests.release());

    return group.release();
}

} // namespace pipeline

} // namespace vkt
