/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2020 The Khronos Group Inc.
 * Copyright (c) 2020 Valve Corporation.
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
 * \brief Tests with shaders that do not write to the Position built-in.
 *//*--------------------------------------------------------------------*/

#include "vktPipelineNoPositionTests.hpp"
#include "tcuRGBA.hpp"
#include "tcuVectorType.hpp"
#include "vkDefs.hpp"
#include "vktTestCase.hpp"
#include "vkPipelineConstructionUtil.hpp"
#include "vktCustomInstancesDevices.hpp"

#include "vkQueryUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBufferWithMemory.hpp"
#include "vktPipelineImageUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkPlatform.hpp"
#include "vkSafetyCriticalUtil.hpp"

#include "tcuVector.hpp"
#include "tcuTestLog.hpp"
#include "tcuCommandLine.hpp"

#include "deUniquePtr.hpp"

#include <cstdint>
#include <string>
#include <vector>
#include <set>
#include <sstream>
#include <array>

namespace vkt
{
namespace pipeline
{

namespace
{

using namespace vk;

enum ShaderStageBits
{
    STAGE_VERTEX          = (1 << 0),
    STAGE_TESS_CONTROL    = (1 << 1),
    STAGE_TESS_EVALUATION = (1 << 2),
    STAGE_GEOMETRY        = (1 << 3),
    STAGE_MASK_COUNT      = (1 << 4),
};

using ShaderStageFlags = uint32_t;

constexpr uint32_t kStageCount = 4u;

static_assert((1u << kStageCount) == static_cast<uint32_t>(STAGE_MASK_COUNT),
              "Total stage count does not match stage mask bits");

const uint32_t max_devgrp_phydevices = VK_MAX_DEVICE_GROUP_SIZE;

template <typename T>
inline de::SharedPtr<Unique<T>> makeSharedPtr(Move<T> move)
{
    return de::SharedPtr<Unique<T>>(new Unique<T>(move));
}

struct TestParams
{
    vk::PipelineConstructionType pipelineConstructionType; // The way pipeline is constructed
    ShaderStageFlags selectedStages;                       // Stages that will be present in the pipeline.
    ShaderStageFlags writeStages;   // Subset of selectedStages that will write to the Position built-in.
    uint32_t numViews;              // Number of views for multiview.
    bool explicitDeclarations;      // Explicitly declare the input and output blocks or not.
    bool useSSBO;                   // Write to an SSBO from the selected stages.
    bool useViewIndexAsDeviceIndex; // Treat gl_ViewIndex shader input variable like gl_DeviceIndex.

    // Commonly used checks.
    bool tessellation(void) const
    {
        return (selectedStages & (STAGE_TESS_CONTROL | STAGE_TESS_EVALUATION));
    }
    bool geometry(void) const
    {
        return (selectedStages & STAGE_GEOMETRY);
    }
};

// Generates the combinations list of stage flags for writeStages when a given subset of stages are selected.
std::vector<ShaderStageFlags> getWriteSubCases(ShaderStageFlags selectedStages)
{
    std::set<ShaderStageFlags> uniqueCases;
    for (ShaderStageFlags stages = 0; stages < STAGE_MASK_COUNT; ++stages)
        uniqueCases.insert(stages & selectedStages);
    return std::vector<ShaderStageFlags>(begin(uniqueCases), end(uniqueCases));
}

class NoPositionCase : public vkt::TestCase
{
public:
    NoPositionCase(tcu::TestContext &testCtx, const std::string &name, const TestParams &params);
    virtual ~NoPositionCase(void)
    {
    }

    virtual void initPrograms(vk::SourceCollections &programCollection) const;
    virtual TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;

    static tcu::Vec4 getBackGroundColor(void)
    {
        return tcu::RGBA::blue().toVec();
    }
    static VkFormat getImageFormat(void)
    {
        return VK_FORMAT_R8G8B8A8_UNORM;
    }
    static VkExtent3D getImageExtent(void)
    {
        return makeExtent3D(64u, 64u, 1u);
    }

private:
    TestParams m_params;
};

class NoPositionInstance : public vkt::TestInstance
{
public:
    NoPositionInstance(Context &context, const TestParams &params);
    virtual ~NoPositionInstance(void)
    {
    }

    virtual tcu::TestStatus iterate(void);

    void createDeviceGroup(void);

    const vk::DeviceInterface &getDeviceInterface(void)
    {
        return *m_deviceDriver;
    }
    vk::VkInstance getInstance(void)
    {
        return m_deviceGroupInstance;
    }
    vk::VkDevice getDevice(void)
    {
        return *m_logicalDevice;
    }
    vk::VkPhysicalDevice getPhysicalDevice(uint32_t i = 0)
    {
        return m_physicalDevices[i];
    }

private:
    uint32_t m_numPhysDevices;
    uint32_t m_numViews;
    uint32_t m_queueFamilyIndex;
    CustomInstance m_deviceGroupInstance;
    vk::Move<vk::VkDevice> m_logicalDevice;
    std::vector<vk::VkPhysicalDevice> m_physicalDevices;
#ifndef CTS_USES_VULKANSC
    de::MovePtr<vk::DeviceDriver> m_deviceDriver;
#else
    de::MovePtr<vk::DeviceDriverSC, vk::DeinitDeviceDeleter> m_deviceDriver;
#endif // CTS_USES_VULKANSC
    de::MovePtr<Allocator> m_allocator;

    TestParams m_params;
};

NoPositionCase::NoPositionCase(tcu::TestContext &testCtx, const std::string &name, const TestParams &params)
    : vkt::TestCase(testCtx, name)
    , m_params(params)
{
}

void NoPositionCase::initPrograms(vk::SourceCollections &programCollection) const
{
    // Add shaders for the selected stages and write to gl_Position in the subset of stages marked for writing.

    // Optional writes, extensions and declarations.
    std::string ssboDecl;
    std::string extensions;
    std::string vertSSBOWrite;
    std::string tescSSBOWrite;
    std::string teseSSBOWrite;
    std::string geomSSBOWrite;

    const bool multiview = (m_params.numViews > 1u);

    if (multiview || m_params.useViewIndexAsDeviceIndex)
        extensions = "#extension GL_EXT_multiview : require\n";

    if (m_params.useSSBO)
    {
        const uint32_t numCountersPerStage =
            m_params.useViewIndexAsDeviceIndex ? max_devgrp_phydevices : m_params.numViews;
        const auto ssboElementCount = kStageCount * numCountersPerStage;
        ssboDecl                    = "layout (set=0, binding=0, std430) buffer StorageBlock { uint counters[" +
                   de::toString(ssboElementCount) + "]; } ssbo;\n";

        const std::array<std::string *, kStageCount> writeStrings = {
            {&vertSSBOWrite, &tescSSBOWrite, &teseSSBOWrite, &geomSSBOWrite}};
        for (size_t stageNum = 0; stageNum < writeStrings.size(); ++stageNum)
        {
            std::ostringstream s;
            s << "    atomicAdd(ssbo.counters[" << stageNum;
            if (multiview || m_params.useViewIndexAsDeviceIndex)
            {
                s << " * " << numCountersPerStage << " + ";
                s << "gl_ViewIndex";
            }
            s << "], 1);\n";
            s.flush();
            *writeStrings[stageNum] = s.str();
        }
    }

    if (m_params.selectedStages & STAGE_VERTEX)
    {
        std::ostringstream vert;
        vert << "#version 450\n"
             << extensions << ssboDecl << "layout (location=0) in vec4 in_pos;\n"
             << (m_params.explicitDeclarations ? "out gl_PerVertex\n"
                                                 "{\n"
                                                 "    vec4 gl_Position;\n"
                                                 "    float gl_PointSize;\n"
                                                 "    float gl_ClipDistance[];\n"
                                                 "    float gl_CullDistance[];\n"
                                                 "};\n" :
                                                 "")
             << "void main (void)\n"
             << "{\n"
             << ((m_params.writeStages & STAGE_VERTEX) ? "    gl_Position = in_pos;\n" : "") << vertSSBOWrite << "}\n";

        programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());
    }

    if (m_params.selectedStages & STAGE_TESS_CONTROL)
    {
        std::ostringstream tesc;
        tesc << "#version 450\n"
             << extensions << ssboDecl << "layout (vertices = 3) out;\n"
             << (m_params.explicitDeclarations ? "in gl_PerVertex\n"
                                                 "{\n"
                                                 "    vec4 gl_Position;\n"
                                                 "    float gl_PointSize;\n"
                                                 "    float gl_ClipDistance[];\n"
                                                 "    float gl_CullDistance[];\n"
                                                 "} gl_in[gl_MaxPatchVertices];\n"
                                                 "out gl_PerVertex\n"
                                                 "{\n"
                                                 "    vec4 gl_Position;\n"
                                                 "    float gl_PointSize;\n"
                                                 "    float gl_ClipDistance[];\n"
                                                 "    float gl_CullDistance[];\n"
                                                 "} gl_out[];\n" :
                                                 "")
             << "void main (void)\n"
             << "{\n"
             << "    gl_TessLevelInner[0] = 1.0;\n"
             << "    gl_TessLevelInner[1] = 1.0;\n"
             << "    gl_TessLevelOuter[0] = 1.0;\n"
             << "    gl_TessLevelOuter[1] = 1.0;\n"
             << "    gl_TessLevelOuter[2] = 1.0;\n"
             << "    gl_TessLevelOuter[3] = 1.0;\n"
             << "\n"
             << ((m_params.writeStages & STAGE_TESS_CONTROL) ?
                     "    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n" :
                     "")
             << tescSSBOWrite << "}\n";

        programCollection.glslSources.add("tesc") << glu::TessellationControlSource(tesc.str());
    }

    if (m_params.selectedStages & STAGE_TESS_EVALUATION)
    {
        std::ostringstream tese;
        tese << "#version 450\n"
             << extensions << ssboDecl << "layout (triangles, fractional_odd_spacing, cw) in;\n"
             << (m_params.explicitDeclarations ? "in gl_PerVertex\n"
                                                 "{\n"
                                                 "    vec4 gl_Position;\n"
                                                 "    float gl_PointSize;\n"
                                                 "    float gl_ClipDistance[];\n"
                                                 "    float gl_CullDistance[];\n"
                                                 "} gl_in[gl_MaxPatchVertices];\n"
                                                 "out gl_PerVertex\n"
                                                 "{\n"
                                                 "    vec4 gl_Position;\n"
                                                 "    float gl_PointSize;\n"
                                                 "    float gl_ClipDistance[];\n"
                                                 "    float gl_CullDistance[];\n"
                                                 "};\n" :
                                                 "")
             << "void main (void)\n"
             << "{\n"
             << ((m_params.writeStages & STAGE_TESS_EVALUATION) ?
                     "    gl_Position = (gl_TessCoord.x * gl_in[0].gl_Position) +\n"
                     "                  (gl_TessCoord.y * gl_in[1].gl_Position) +\n"
                     "                  (gl_TessCoord.z * gl_in[2].gl_Position);\n" :
                     "")
             << teseSSBOWrite << "}\n";

        programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(tese.str());
    }

    if (m_params.selectedStages & STAGE_GEOMETRY)
    {
        std::ostringstream geom;
        geom << "#version 450\n"
             << extensions << ssboDecl << "layout (triangles) in;\n"
             << "layout (triangle_strip, max_vertices=3) out;\n"
             << (m_params.explicitDeclarations ? "in gl_PerVertex\n"
                                                 "{\n"
                                                 "    vec4 gl_Position;\n"
                                                 "    float gl_PointSize;\n"
                                                 "    float gl_ClipDistance[];\n"
                                                 "    float gl_CullDistance[];\n"
                                                 "} gl_in[3];\n"
                                                 "out gl_PerVertex\n"
                                                 "{\n"
                                                 "    vec4 gl_Position;\n"
                                                 "    float gl_PointSize;\n"
                                                 "    float gl_ClipDistance[];\n"
                                                 "    float gl_CullDistance[];\n"
                                                 "};\n" :
                                                 "")
             << "void main (void)\n"
             << "{\n"
             << "    for (int i = 0; i < 3; i++)\n"
             << "    {\n"
             << ((m_params.writeStages & STAGE_GEOMETRY) ? "        gl_Position = gl_in[i].gl_Position;\n" : "")
             << "        EmitVertex();\n"
             << "    }\n"
             << geomSSBOWrite << "}\n";

        programCollection.glslSources.add("geom") << glu::GeometrySource(geom.str());
    }

    {
        const auto backgroundColor = getBackGroundColor();

        std::ostringstream colorStr;
        colorStr << "vec4(" << backgroundColor.x() << ", " << backgroundColor.y() << ", " << backgroundColor.z() << ", "
                 << backgroundColor.w() << ")";

        std::ostringstream frag;
        frag << "#version 450\n"
             << "layout (location=0) out vec4 out_color;\n"
             << "void main (void)\n"
             << "{\n"
             << "    out_color = " << colorStr.str() << ";\n"
             << "}\n";

        programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
    }
}

TestInstance *NoPositionCase::createInstance(Context &context) const
{
    return new NoPositionInstance(context, m_params);
}

void NoPositionCase::checkSupport(Context &context) const
{
    const auto features = getPhysicalDeviceFeatures(context.getInstanceInterface(), context.getPhysicalDevice());
    const bool hasTess  = m_params.tessellation();
    const bool hasGeom  = m_params.geometry();

    if (hasTess && !features.tessellationShader)
        TCU_THROW(NotSupportedError, "Tessellation shaders not supported");

    if (hasGeom && !features.geometryShader)
        TCU_THROW(NotSupportedError, "Geometry shaders not supported");

    if ((m_params.numViews > 1u) || (m_params.useViewIndexAsDeviceIndex))
    {
        context.requireDeviceFunctionality("VK_KHR_multiview");
        const auto &multiviewFeatures = context.getMultiviewFeatures();

        if (!multiviewFeatures.multiview)
            TCU_THROW(NotSupportedError, "Multiview not supported");

        if (hasTess && !multiviewFeatures.multiviewTessellationShader)
            TCU_THROW(NotSupportedError, "Multiview not supported with tessellation shaders");

        if (hasGeom && !multiviewFeatures.multiviewGeometryShader)
            TCU_THROW(NotSupportedError, "Multiview not supported with geometry shaders");

        if (m_params.numViews > context.getMultiviewProperties().maxMultiviewViewCount)
            TCU_THROW(NotSupportedError, "Not enough views supported");
    }

    if (m_params.useSSBO)
    {
        if (!features.vertexPipelineStoresAndAtomics)
            TCU_THROW(NotSupportedError, "Vertex pipeline stores and atomics not supported");
    }

    if (m_params.useViewIndexAsDeviceIndex)
    {
        context.requireInstanceFunctionality("VK_KHR_device_group_creation");
        context.requireDeviceFunctionality("VK_KHR_device_group");
    }

    checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                          m_params.pipelineConstructionType);
}

NoPositionInstance::NoPositionInstance(Context &context, const TestParams &params)
    : vkt::TestInstance(context)
    , m_numPhysDevices(1)
    , m_queueFamilyIndex(0)
    , m_params(params)
{
    if (m_params.useViewIndexAsDeviceIndex)
        createDeviceGroup();

    m_numViews = m_params.useViewIndexAsDeviceIndex ? m_numPhysDevices : m_params.numViews;
    if (m_numViews > context.getMultiviewProperties().maxMultiviewViewCount)
        TCU_THROW(NotSupportedError, "Not enough views supported");
}

void NoPositionInstance::createDeviceGroup(void)
{
    const tcu::CommandLine &cmdLine = m_context.getTestContext().getCommandLine();
    const uint32_t devGroupIdx      = cmdLine.getVKDeviceGroupId() - 1;
    uint32_t physDeviceIdx          = cmdLine.getVKDeviceId() - 1;
    const float queuePriority       = 1.0f;
    const auto &vki                 = m_context.getInstanceInterface();

    m_deviceGroupInstance = createCustomInstanceWithExtension(m_context, "VK_KHR_device_group_creation");
    const InstanceDriver &instance(m_deviceGroupInstance.getDriver());

    std::vector<VkPhysicalDeviceGroupProperties> devGroupsProperties =
        enumeratePhysicalDeviceGroups(vki, m_deviceGroupInstance);
    m_numPhysDevices         = devGroupsProperties[devGroupIdx].physicalDeviceCount;
    auto &devGroupProperties = devGroupsProperties[devGroupIdx];

    if (physDeviceIdx >= devGroupProperties.physicalDeviceCount)
        physDeviceIdx = 0;

    // Enable device features
    VkPhysicalDeviceFeatures2 deviceFeatures2     = initVulkanStructure();
    VkDeviceGroupDeviceCreateInfo deviceGroupInfo = initVulkanStructure(&deviceFeatures2);
    deviceGroupInfo.physicalDeviceCount           = devGroupProperties.physicalDeviceCount;
    deviceGroupInfo.pPhysicalDevices              = devGroupProperties.physicalDevices;
    const VkPhysicalDeviceFeatures deviceFeatures =
        getPhysicalDeviceFeatures(instance, deviceGroupInfo.pPhysicalDevices[physDeviceIdx]);
    deviceFeatures2.features = deviceFeatures;

    m_physicalDevices.resize(m_numPhysDevices);
    for (uint32_t physDevIdx = 0; physDevIdx < m_numPhysDevices; physDevIdx++)
        m_physicalDevices[physDevIdx] = devGroupProperties.physicalDevices[physDevIdx];

    // Prepare queue info
    const std::vector<VkQueueFamilyProperties> queueProps =
        getPhysicalDeviceQueueFamilyProperties(instance, devGroupProperties.physicalDevices[physDeviceIdx]);
    for (size_t queueNdx = 0; queueNdx < queueProps.size(); queueNdx++)
    {
        if (queueProps[queueNdx].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            m_queueFamilyIndex = (uint32_t)queueNdx;
    }

    VkDeviceQueueCreateInfo queueInfo = {
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                    // const void* pNext;
        (VkDeviceQueueCreateFlags)0u,               // VkDeviceQueueCreateFlags flags;
        m_queueFamilyIndex,                         // uint32_t queueFamilyIndex;
        1u,                                         // uint32_t queueCount;
        &queuePriority                              // const float* pQueuePriorities;
    };

    // Enable extensions
    const auto &contextMultiviewFeatures                = m_context.getMultiviewFeatures();
    const bool multiViewSupport                         = contextMultiviewFeatures.multiview;
    VkPhysicalDeviceMultiviewFeatures multiviewFeatures = vk::initVulkanStructure();
#ifndef CTS_USES_VULKANSC
    const auto &contextGpl                                         = m_context.getGraphicsPipelineLibraryFeaturesEXT();
    const bool gplSupport                                          = contextGpl.graphicsPipelineLibrary;
    VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT gplFeatures = vk::initVulkanStructure();
#endif
    const auto addFeatures = vk::makeStructChainAdder(&deviceFeatures2);
    if (multiViewSupport)
        addFeatures(&multiviewFeatures);
#ifndef CTS_USES_VULKANSC
    if (isConstructionTypeLibrary(m_params.pipelineConstructionType) && gplSupport)
        addFeatures(&gplFeatures);
#endif
    vki.getPhysicalDeviceFeatures2(deviceGroupInfo.pPhysicalDevices[physDeviceIdx], &deviceFeatures2);
    // Enable extensions
    std::vector<const char *> deviceExtensions;
    if (!isCoreDeviceExtension(m_context.getUsedApiVersion(), "VK_KHR_device_group"))
        deviceExtensions.push_back("VK_KHR_device_group");

    if (multiViewSupport)
        deviceExtensions.push_back("VK_KHR_multiview");

#ifndef CTS_USES_VULKANSC
    if (isConstructionTypeLibrary(m_params.pipelineConstructionType) && gplSupport)
    {
        deviceExtensions.push_back("VK_KHR_pipeline_library");
        deviceExtensions.push_back("VK_EXT_graphics_pipeline_library");
    }
#endif

    void *pNext = &deviceGroupInfo;

#ifdef CTS_USES_VULKANSC
    VkDeviceObjectReservationCreateInfo memReservationInfo = cmdLine.isSubProcess() ?
                                                                 m_context.getResourceInterface()->getStatMax() :
                                                                 resetDeviceObjectReservationCreateInfo();
    memReservationInfo.pNext                               = pNext;
    pNext                                                  = &memReservationInfo;

    VkPhysicalDeviceVulkanSC10Features sc10Features = createDefaultSC10Features();
    sc10Features.pNext                              = pNext;
    pNext                                           = &sc10Features;
    VkPipelineCacheCreateInfo pcCI;
    std::vector<VkPipelinePoolSize> poolSizes;
    if (m_context.getTestContext().getCommandLine().isSubProcess())
    {
        if (m_context.getResourceInterface()->getCacheDataSize() > 0)
        {
            pcCI = {
                VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, // VkStructureType sType;
                nullptr,                                      // const void* pNext;
                VK_PIPELINE_CACHE_CREATE_READ_ONLY_BIT |
                    VK_PIPELINE_CACHE_CREATE_USE_APPLICATION_STORAGE_BIT, // VkPipelineCacheCreateFlags flags;
                m_context.getResourceInterface()->getCacheDataSize(),     // uintptr_t initialDataSize;
                m_context.getResourceInterface()->getCacheData()          // const void* pInitialData;
            };
            memReservationInfo.pipelineCacheCreateInfoCount = 1;
            memReservationInfo.pPipelineCacheCreateInfos    = &pcCI;
        }

        poolSizes = m_context.getResourceInterface()->getPipelinePoolSizes();
        if (!poolSizes.empty())
        {
            memReservationInfo.pipelinePoolSizeCount = uint32_t(poolSizes.size());
            memReservationInfo.pPipelinePoolSizes    = poolSizes.data();
        }
    }
#endif // CTS_USES_VULKANSC

    const VkDeviceCreateInfo deviceCreateInfo = {
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, // VkStructureType sType;
        pNext,                                // const void* pNext;
        (VkDeviceCreateFlags)0,               // VkDeviceCreateFlags flags;
        1u,                                   // uint32_t queueCreateInfoCount;
        &queueInfo,                           // const VkDeviceQueueCreateInfo* pQueueCreateInfos;
        0u,                                   // uint32_t enabledLayerCount;
        nullptr,                              // const char* const* ppEnabledLayerNames;
        de::sizeU32(deviceExtensions),        // uint32_t enabledExtensionCount;
        de::dataOrNull(deviceExtensions),     // const char* const* ppEnabledExtensionNames;
        deviceFeatures2.pNext == nullptr ? &deviceFeatures :
                                           nullptr, // const VkPhysicalDeviceFeatures* pEnabledFeatures;
    };

    m_logicalDevice = createCustomDevice(m_context.getTestContext().getCommandLine().isValidationEnabled(),
                                         m_context.getPlatformInterface(), m_deviceGroupInstance, instance,
                                         deviceGroupInfo.pPhysicalDevices[physDeviceIdx], &deviceCreateInfo);

#ifndef CTS_USES_VULKANSC
    m_deviceDriver = de::MovePtr<DeviceDriver>(new DeviceDriver(m_context.getPlatformInterface(), m_deviceGroupInstance,
                                                                *m_logicalDevice, m_context.getUsedApiVersion()));
#else
    m_deviceDriver = de::MovePtr<DeviceDriverSC, DeinitDeviceDeleter>(
        new DeviceDriverSC(m_context.getPlatformInterface(), m_context.getInstance(), *m_logicalDevice,
                           m_context.getTestContext().getCommandLine(), m_context.getResourceInterface(),
                           m_context.getDeviceVulkanSC10Properties(), m_context.getDeviceProperties(),
                           m_context.getUsedApiVersion()),
        vk::DeinitDeviceDeleter(m_context.getResourceInterface().get(), *m_logicalDevice));
#endif // CTS_USES_VULKANSC

    m_allocator = de::MovePtr<Allocator>(new SimpleAllocator(
        *m_deviceDriver, *m_logicalDevice, getPhysicalDeviceMemoryProperties(instance, m_physicalDevices[0])));
}

// Make a render pass with one subpass per color attachment
RenderPassWrapper makeRenderPass(const DeviceInterface &vk, const VkDevice device,
                                 const PipelineConstructionType pipelineConstructionType, const VkFormat colorFormat,
                                 const uint32_t numAttachments,
                                 de::MovePtr<VkRenderPassMultiviewCreateInfo> multiviewCreateInfo,
                                 const VkImageLayout initialColorImageLayout = VK_IMAGE_LAYOUT_UNDEFINED)
{
    const VkAttachmentDescription colorAttachmentDescription = {
        (VkAttachmentDescriptionFlags)0,          // VkAttachmentDescriptionFlags flags;
        colorFormat,                              // VkFormat format;
        VK_SAMPLE_COUNT_1_BIT,                    // VkSampleCountFlagBits samples;
        VK_ATTACHMENT_LOAD_OP_CLEAR,              // VkAttachmentLoadOp loadOp;
        VK_ATTACHMENT_STORE_OP_STORE,             // VkAttachmentStoreOp storeOp;
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,          // VkAttachmentLoadOp stencilLoadOp;
        VK_ATTACHMENT_STORE_OP_DONT_CARE,         // VkAttachmentStoreOp stencilStoreOp;
        initialColorImageLayout,                  // VkImageLayout initialLayout;
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout finalLayout;
    };
    std::vector<VkAttachmentDescription> attachmentDescriptions(numAttachments, colorAttachmentDescription);

    // Create a subpass for each attachment (each attachement is a layer of an arrayed image).
    std::vector<VkAttachmentReference> colorAttachmentReferences(numAttachments);
    std::vector<VkSubpassDescription> subpasses;

    // Ordering here must match the framebuffer attachments
    for (uint32_t i = 0; i < numAttachments; ++i)
    {
        const VkAttachmentReference attachmentRef = {
            i,                                       // uint32_t attachment;
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL // VkImageLayout layout;
        };

        colorAttachmentReferences[i] = attachmentRef;

        const VkSubpassDescription subpassDescription = {
            (VkSubpassDescriptionFlags)0,    // VkSubpassDescriptionFlags flags;
            VK_PIPELINE_BIND_POINT_GRAPHICS, // VkPipelineBindPoint pipelineBindPoint;
            0u,                              // uint32_t inputAttachmentCount;
            nullptr,                         // const VkAttachmentReference* pInputAttachments;
            1u,                              // uint32_t colorAttachmentCount;
            &colorAttachmentReferences[i],   // const VkAttachmentReference* pColorAttachments;
            nullptr,                         // const VkAttachmentReference* pResolveAttachments;
            nullptr,                         // const VkAttachmentReference* pDepthStencilAttachment;
            0u,                              // uint32_t preserveAttachmentCount;
            nullptr                          // const uint32_t* pPreserveAttachments;
        };
        subpasses.push_back(subpassDescription);
    }

    const VkRenderPassCreateInfo renderPassInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,            // VkStructureType sType;
        multiviewCreateInfo.get(),                            // const void* pNext;
        (VkRenderPassCreateFlags)0,                           // VkRenderPassCreateFlags flags;
        static_cast<uint32_t>(attachmentDescriptions.size()), // uint32_t attachmentCount;
        &attachmentDescriptions[0],                           // const VkAttachmentDescription* pAttachments;
        static_cast<uint32_t>(subpasses.size()),              // uint32_t subpassCount;
        &subpasses[0],                                        // const VkSubpassDescription* pSubpasses;
        0u,                                                   // uint32_t dependencyCount;
        nullptr                                               // const VkSubpassDependency* pDependencies;
    };

    return RenderPassWrapper(pipelineConstructionType, vk, device, &renderPassInfo);
}

inline VkImageSubresourceRange makeColorSubresourceRange(const int baseArrayLayer, const int layerCount)
{
    return makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, static_cast<uint32_t>(baseArrayLayer),
                                     static_cast<uint32_t>(layerCount));
}

tcu::TestStatus NoPositionInstance::iterate(void)
{
    const bool useDeviceGroup = m_params.useViewIndexAsDeviceIndex;
    const auto &vki           = m_context.getInstanceInterface();
    const auto &vkd           = useDeviceGroup ? getDeviceInterface() : m_context.getDeviceInterface();
    const auto physicalDevice = useDeviceGroup ? getPhysicalDevice() : m_context.getPhysicalDevice();
    const auto device         = useDeviceGroup ? getDevice() : m_context.getDevice();
    const auto qIndex         = useDeviceGroup ? m_queueFamilyIndex : m_context.getUniversalQueueFamilyIndex();
    const auto queue          = useDeviceGroup ? getDeviceQueue(vkd, device, qIndex, 0) : m_context.getUniversalQueue();
    auto &alloc               = useDeviceGroup ? *m_allocator : m_context.getDefaultAllocator();
    const auto format         = NoPositionCase::getImageFormat();
    const auto extent         = NoPositionCase::getImageExtent();
    const auto bgColor        = NoPositionCase::getBackGroundColor();
    const VkImageUsageFlags usage = (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    const auto viewType           = (m_numViews > 1u ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D);
    const bool tess               = m_params.tessellation();
    VkShaderStageFlags stageFlags = 0u;

    // Shader modules.
    ShaderWrapper vert;
    ShaderWrapper tesc;
    ShaderWrapper tese;
    ShaderWrapper geom;
    ShaderWrapper frag;

    if (m_params.selectedStages & STAGE_VERTEX)
    {
        vert = ShaderWrapper(vkd, device, m_context.getBinaryCollection().get("vert"), 0u);
        stageFlags |= VK_SHADER_STAGE_VERTEX_BIT;
    }
    if (m_params.selectedStages & STAGE_TESS_CONTROL)
    {
        tesc = ShaderWrapper(vkd, device, m_context.getBinaryCollection().get("tesc"), 0u);
        stageFlags |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    }
    if (m_params.selectedStages & STAGE_TESS_EVALUATION)
    {
        tese = ShaderWrapper(vkd, device, m_context.getBinaryCollection().get("tese"), 0u);
        stageFlags |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    }
    if (m_params.selectedStages & STAGE_GEOMETRY)
    {
        geom = ShaderWrapper(vkd, device, m_context.getBinaryCollection().get("geom"), 0u);
        stageFlags |= VK_SHADER_STAGE_GEOMETRY_BIT;
    }

    frag = ShaderWrapper(vkd, device, m_context.getBinaryCollection().get("frag"), 0u);
    stageFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;

    const uint32_t layers = m_numViews;

    // Color attachment.
    const VkImageCreateInfo colorImageInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        0u,                                  // VkImageCreateFlags flags;
        vk::VK_IMAGE_TYPE_2D,                // VkImageType imageType;
        format,                              // VkFormat format;
        extent,                              // VkExtent3D extent;
        1u,                                  // uint32_t mipLevels;
        layers,                              // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        usage,                               // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        0u,                                  // uint32_t queueFamilyIndexCount;
        nullptr,                             // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout initialLayout;
    };
    ImageWithMemory colorImage(vkd, device, alloc, colorImageInfo, MemoryRequirement::Any);

    const auto subresourceRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, layers);

    // Vertices and vertex buffer.
    const uint32_t numVertices            = 3;
    const tcu::Vec4 vertices[numVertices] = {
        tcu::Vec4(0.0f, -0.5f, 0.0f, 1.0f),
        tcu::Vec4(0.5f, 0.5f, 0.0f, 1.0f),
        tcu::Vec4(-0.5f, 0.5f, 0.0f, 1.0f),
    };

    const auto vertexBufferSize   = static_cast<VkDeviceSize>(numVertices * sizeof(vertices[0]));
    const auto vertexBufferInfo   = makeBufferCreateInfo(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    const auto vertexBufferOffset = static_cast<VkDeviceSize>(0);
    BufferWithMemory vertexBuffer(vkd, device, alloc, vertexBufferInfo, MemoryRequirement::HostVisible);

    auto &vertexBufferAlloc = vertexBuffer.getAllocation();
    void *vertexBufferPtr   = vertexBufferAlloc.getHostPtr();
    deMemcpy(vertexBufferPtr, &vertices[0], static_cast<size_t>(vertexBufferSize));
    flushAlloc(vkd, device, vertexBufferAlloc);

    de::MovePtr<VkRenderPassMultiviewCreateInfo> multiviewInfo;
    std::vector<uint32_t> viewMasks;
    std::vector<uint32_t> correlationMasks;

    uint32_t subpassCount = 1;

    if ((m_numViews > 1u) || (m_params.useViewIndexAsDeviceIndex))
    {
        if (m_params.useViewIndexAsDeviceIndex)
        {
            // In case of useViewIndexAsDeviceIndex,
            // each view has its own view mask
            viewMasks.resize(m_numViews);
            correlationMasks.resize(m_numViews);

            for (uint32_t viewIdx = 0u; viewIdx < m_numViews; ++viewIdx)
            {
                viewMasks[viewIdx] |= (1 << viewIdx);
                correlationMasks[viewIdx] |= (1 << viewIdx);
            }

            subpassCount = de::sizeU32(viewMasks);
        }
        else
        {
            viewMasks.resize(1);
            correlationMasks.resize(1);

            for (uint32_t viewIdx = 0u; viewIdx < m_numViews; ++viewIdx)
            {
                viewMasks[0] |= (1 << viewIdx);
                correlationMasks[0] |= (1 << viewIdx);
            }
        }

        multiviewInfo = de::MovePtr<VkRenderPassMultiviewCreateInfo>(new VkRenderPassMultiviewCreateInfo{
            VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO, // VkStructureType sType;
            nullptr,                                             // const void* pNext;
            de::sizeU32(viewMasks),                              // uint32_t subpassCount;
            de::dataOrNull(viewMasks),                           // const uint32_t* pViewMasks;
            0u,                                                  // uint32_t dependencyCount;
            nullptr,                                             // const int32_t* pViewOffsets;
            de::sizeU32(correlationMasks),                       // uint32_t correlationMaskCount;
            de::dataOrNull(correlationMasks),                    // const uint32_t* pCorrelationMasks;
        });
    }

    RenderPassWrapper renderPass(makeRenderPass(vkd, device, m_params.pipelineConstructionType, format, subpassCount,
                                                multiviewInfo, VK_IMAGE_LAYOUT_UNDEFINED));

    // Descriptor set layout and pipeline layout.
    DescriptorSetLayoutBuilder layoutBuilder;
    if (m_params.useSSBO)
    {
        layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, stageFlags);
    }
    const auto descriptorSetLayout = layoutBuilder.build(vkd, device);
    const PipelineLayoutWrapper pipelineLayout(m_params.pipelineConstructionType, vkd, device,
                                               descriptorSetLayout.get());

    // Pipeline.
    const std::vector<VkViewport> viewports{makeViewport(extent)};
    const std::vector<VkRect2D> scissors{makeRect2D(extent)};

    const auto primitiveTopology(tess ? VK_PRIMITIVE_TOPOLOGY_PATCH_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    const VkPipelineCreateFlags createFlags = m_params.useViewIndexAsDeviceIndex ?
                                                  VK_PIPELINE_CREATE_VIEW_INDEX_FROM_DEVICE_INDEX_BIT :
                                                  VkPipelineCreateFlagBits(0u);

    std::vector<GraphicsPipelineWrapper> pipelines;
    pipelines.reserve(subpassCount);

    std::vector<de::SharedPtr<Unique<VkImageView>>> colorAttachments;
    std::vector<VkImage> images;
    std::vector<VkImageView> attachmentHandles;

    VkPipeline basePipeline = VK_NULL_HANDLE;

    for (uint32_t subpassNdx = 0; subpassNdx < subpassCount; ++subpassNdx)
    {
        colorAttachments.push_back(makeSharedPtr(makeImageView(
            vkd, device, *colorImage, viewType, format,
            makeColorSubresourceRange(0, m_params.useViewIndexAsDeviceIndex ? subpassCount : m_numViews))));
        images.push_back(*colorImage);
        attachmentHandles.push_back(**colorAttachments.back());

#ifndef CTS_USES_VULKANSC // Pipeline derivatives are forbidden in Vulkan SC
        pipelines.emplace_back(
            vki, vkd, physicalDevice, device, m_context.getDeviceExtensions(), m_params.pipelineConstructionType,
            createFlags | (basePipeline == VK_NULL_HANDLE ? VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT :
                                                            VK_PIPELINE_CREATE_DERIVATIVE_BIT));
#else
        pipelines.emplace_back(vki, vkd, physicalDevice, device, m_context.getDeviceExtensions(),
                               m_params.pipelineConstructionType, createFlags);
#endif // CTS_USES_VULKANSC

        pipelines.back()
            .setDefaultTopology(primitiveTopology)
            .setDefaultRasterizationState()
            .setDefaultMultisampleState()
            .setDefaultDepthStencilState()
            .setDefaultColorBlendState()
            .setupVertexInputState()
            .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, *renderPass, subpassNdx, vert,
                                              nullptr, tesc, tese, geom)
            .setupFragmentShaderState(pipelineLayout, *renderPass, 0u, frag)
            .setupFragmentOutputState(*renderPass)
            .setMonolithicPipelineLayout(pipelineLayout)
            .buildPipeline(VK_NULL_HANDLE, basePipeline, -1);

        if (pipelines.front().wasBuild())
            basePipeline = pipelines.front().getPipeline();
    }

    renderPass.createFramebuffer(vkd, device, static_cast<uint32_t>(attachmentHandles.size()), &images[0],
                                 &attachmentHandles[0], extent.width, extent.height);

    // Descriptor set and output SSBO if needed.
    Move<VkDescriptorPool> descriptorPool;
    Move<VkDescriptorSet> descriptorSet;
    de::MovePtr<BufferWithMemory> ssboBuffer;
    const uint32_t numCountersPerStage = m_params.useViewIndexAsDeviceIndex ? max_devgrp_phydevices : m_params.numViews;
    const auto ssboElementCount        = kStageCount * numCountersPerStage;
    const auto ssboBufferSize          = static_cast<VkDeviceSize>(ssboElementCount * sizeof(uint32_t));

    if (m_params.useSSBO)
    {
        // Output SSBO.
        const auto ssboBufferInfo = makeBufferCreateInfo(ssboBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        ssboBuffer                = de::MovePtr<BufferWithMemory>(
            new BufferWithMemory(vkd, device, alloc, ssboBufferInfo, MemoryRequirement::HostVisible));
        auto &ssboBufferAlloc = ssboBuffer->getAllocation();

        deMemset(ssboBufferAlloc.getHostPtr(), 0, static_cast<size_t>(ssboBufferSize));
        flushAlloc(vkd, device, ssboBufferAlloc);

        // Descriptor pool.
        DescriptorPoolBuilder poolBuilder;
        poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        descriptorPool = poolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

        // Descriptor set.
        descriptorSet            = makeDescriptorSet(vkd, device, descriptorPool.get(), descriptorSetLayout.get());
        const auto ssboWriteInfo = makeDescriptorBufferInfo(ssboBuffer->get(), 0ull, ssboBufferSize);
        DescriptorSetUpdateBuilder updateBuilder;
        updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(0u),
                                  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &ssboWriteInfo);
        updateBuilder.update(vkd, device);
    }

    // Command pool and buffer.
    const auto cmdPool      = makeCommandPool(vkd, device, qIndex);
    const auto cmdBufferPtr = allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto cmdBuffer    = cmdBufferPtr.get();

    const std::vector<VkClearValue> colors(subpassCount, makeClearValueColorVec4(bgColor));

    // Render triangle.
    beginCommandBuffer(vkd, cmdBuffer);
    renderPass.begin(vkd, cmdBuffer, scissors.front(), subpassCount, &colors[0]);

    // Draw
    for (uint32_t subpassNdx = 0; subpassNdx < subpassCount; ++subpassNdx)
    {
        if (subpassNdx != 0)
            renderPass.nextSubpass(vkd, cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);

        pipelines[subpassNdx].bind(cmdBuffer);
        if (m_params.useSSBO)
            vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), 0u, 1u,
                                      &descriptorSet.get(), 0u, nullptr);
        vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
        vkd.cmdDraw(cmdBuffer, numVertices, 1u, 0u, 0u);
    }

    renderPass.end(vkd, cmdBuffer);

    // Output verification buffer.
    const auto tcuFormat   = mapVkFormat(format);
    const auto pixelSize   = static_cast<uint32_t>(tcu::getPixelSize(tcuFormat));
    const auto layerPixels = extent.width * extent.height;
    const auto layerBytes  = layerPixels * pixelSize;
    const auto totalBytes  = layerBytes * m_numViews;

    const auto verificationBufferInfo = makeBufferCreateInfo(totalBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    BufferWithMemory verificationBuffer(vkd, device, alloc, verificationBufferInfo, MemoryRequirement::HostVisible);

    // Copy output image to verification buffer.
    const auto preTransferBarrier = makeImageMemoryBarrier(
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, colorImage.get(), subresourceRange);
    vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
                           0u, nullptr, 0u, nullptr, 1u, &preTransferBarrier);

    const auto subresourceLayers       = makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, m_numViews);
    const VkBufferImageCopy copyRegion = {
        0ull,                  // VkDeviceSize bufferOffset;
        0u,                    // uint32_t bufferRowLength;
        0u,                    // uint32_t bufferImageHeight;
        subresourceLayers,     // VkImageSubresourceLayers imageSubresource;
        makeOffset3D(0, 0, 0), // VkOffset3D imageOffset;
        extent,                // VkExtent3D imageExtent;
    };
    vkd.cmdCopyImageToBuffer(cmdBuffer, colorImage.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                             verificationBuffer.get(), 1u, &copyRegion);

    const auto postTransferBarrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
    vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u,
                           &postTransferBarrier, 0u, nullptr, 0u, nullptr);

    // Output SSBO to host barrier.
    if (m_params.useSSBO)
    {
        const auto ssboBarrier = makeMemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u,
                               &ssboBarrier, 0u, nullptr, 0u, nullptr);
    }

    // Submit commands.
    endCommandBuffer(vkd, cmdBuffer);
    const uint32_t deviceMask = (1 << m_numPhysDevices) - 1;
    submitCommandsAndWait(vkd, device, queue, cmdBuffer, useDeviceGroup, deviceMask);

    // Verify the image has the background color.
    auto &verificationBufferAlloc = verificationBuffer.getAllocation();
    auto verificationBufferPtr    = reinterpret_cast<const char *>(verificationBufferAlloc.getHostPtr());
    invalidateAlloc(vkd, device, verificationBufferAlloc);

    const auto iWidth  = static_cast<int>(extent.width);
    const auto iHeight = static_cast<int>(extent.height);
    const auto iDepth  = static_cast<int>(extent.depth);

    for (uint32_t layer = 0u; layer < m_params.numViews; ++layer)
    {
        const auto pixels =
            tcu::ConstPixelBufferAccess(tcuFormat, iWidth, iHeight, iDepth,
                                        reinterpret_cast<const void *>(verificationBufferPtr + layer * layerBytes));

        for (int y = 0; y < iHeight; ++y)
            for (int x = 0; x < iWidth; ++x)
            {
                const auto pixel = pixels.getPixel(x, y);
                if (pixel != bgColor)
                {
                    std::ostringstream msg;
                    msg << "Unexpected color found at pixel (" << x << ", " << y << ") in layer " << layer;

                    auto &log = m_context.getTestContext().getLog();
                    log << tcu::TestLog::Message << msg.str() << tcu::TestLog::EndMessage;
                    log << tcu::TestLog::Image("Result", "Result Image", pixels);
                    TCU_FAIL(msg.str());
                }
            }
    }

    // Verify SSBO if used.
    if (m_params.useSSBO)
    {
        // Get stored counters.
        const auto ssboBufferSizeSz = static_cast<size_t>(ssboBufferSize);
        auto &ssboAlloc             = ssboBuffer->getAllocation();
        invalidateAlloc(vkd, device, ssboAlloc);

        std::vector<uint32_t> ssboCounters(ssboElementCount);
        DE_ASSERT(ssboBufferSizeSz == ssboCounters.size() * sizeof(decltype(ssboCounters)::value_type));
        deMemcpy(ssboCounters.data(), ssboAlloc.getHostPtr(), ssboBufferSizeSz);

        // Minimum accepted counter values.
        // Vertex, Tessellation Control, Tesellation Evaluation, Geometry.
        uint32_t numActualCountersPerStage     = m_numViews;
        uint32_t expectedCounters[kStageCount] = {3u, 3u, 3u, 1u};

        // Verify.
        for (uint32_t stageIdx = 0u; stageIdx < kStageCount; ++stageIdx)
            for (uint32_t counterIdx = 0u; counterIdx < numActualCountersPerStage; ++counterIdx)
            {
                // If the stage is not selected, the expected value is exactly zero. Otherwise, it must be at least as expectedCounters.
                uint32_t expectedVal     = expectedCounters[stageIdx];
                uint32_t minVal          = ((m_params.selectedStages & (1u << stageIdx)) ? expectedVal : 0u);
                const uint32_t storedVal = ssboCounters[stageIdx * numCountersPerStage + counterIdx];

                bool ok = false;
                if (minVal != 0u)
                {
                    if (storedVal != 0)
                        ok = (storedVal == minVal) ?
                                 true
                                 // All shaders must process at least gl_ViewIndex|gl_DeviceIndex times.
                                 :
                                 ((storedVal % minVal) == 0u);
                    else
                        ok = false;
                }
                else
                    ok = true; /* continue */

                if (!ok)
                {
                    const char *stageNames[kStageCount] = {
                        "vertex",
                        "tessellation control",
                        "tessellation evaluation",
                        "geometry",
                    };

                    std::ostringstream msg;
                    msg << "Unexpected SSBO counter value in view " << counterIdx << " for the " << stageNames[stageIdx]
                        << " shader:"
                        << " got " << storedVal << " but expected " << minVal;
                    TCU_FAIL(msg.str());
                }
            }
    }

    return tcu::TestStatus::pass("Pass");
}

} // namespace

tcu::TestCaseGroup *createNoPositionTests(tcu::TestContext &testCtx,
                                          vk::PipelineConstructionType pipelineConstructionType)
{
    // Tests with shaders that do not write to the Position built-in
    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "no_position"));

    for (int aux = 0; aux < 2; ++aux)
    {
        const bool explicitDeclarations = (aux == 1);
        const std::string declGroupName(explicitDeclarations ? "explicit_declarations" : "implicit_declarations");
        de::MovePtr<tcu::TestCaseGroup> declGroup(new tcu::TestCaseGroup(testCtx, declGroupName.c_str()));

        for (int aux2 = 0; aux2 < 2; ++aux2)
        {
            const bool useSSBO = (aux2 == 1);
            const std::string ssboGroupName(useSSBO ? "ssbo_writes" : "basic");
            de::MovePtr<tcu::TestCaseGroup> ssboGroup(new tcu::TestCaseGroup(testCtx, ssboGroupName.c_str()));

            const uint32_t maxTestedViewCount = useSSBO ? 3u : 2u;
            for (uint32_t viewCount = 1u; viewCount <= maxTestedViewCount; ++viewCount)
            {
                auto makeViewGroupName = [&]() -> std::string
                {
                    switch (viewCount)
                    {
                    case 1u:
                        return "single_view";
                    case 2u:
                        return "multiview";
                    case 3u:
                        return "device_index_as_view_index";
                    }
                    DE_ASSERT(false);
                    return std::string();
                };

                const std::string viewGroupName      = makeViewGroupName();
                const bool useDeviceIndexAsViewIndex = (3u == viewCount);

                // Shader objects do not support multiview
                if (viewCount != 1 && vk::isConstructionTypeShaderObject(pipelineConstructionType))
                    continue;
                de::MovePtr<tcu::TestCaseGroup> viewGroup(new tcu::TestCaseGroup(testCtx, viewGroupName.c_str()));

                for (ShaderStageFlags stages = 0u; stages < STAGE_MASK_COUNT; ++stages)
                {
                    // Vertex must always be present.
                    if (!(stages & STAGE_VERTEX))
                        continue;

                    // Tessellation stages must both be present or none must be.
                    if (static_cast<bool>(stages & STAGE_TESS_CONTROL) !=
                        static_cast<bool>(stages & STAGE_TESS_EVALUATION))
                        continue;

                    const auto writeMaskCases = getWriteSubCases(stages);
                    for (const auto writeMask : writeMaskCases)
                    {
                        std::string testName;
                        if (stages & STAGE_VERTEX)
                            testName += (testName.empty() ? "" : "_") + std::string("v") +
                                        ((writeMask & STAGE_VERTEX) ? "1" : "0");
                        if (stages & STAGE_TESS_CONTROL)
                            testName += (testName.empty() ? "" : "_") + std::string("c") +
                                        ((writeMask & STAGE_TESS_CONTROL) ? "1" : "0");
                        if (stages & STAGE_TESS_EVALUATION)
                            testName += (testName.empty() ? "" : "_") + std::string("e") +
                                        ((writeMask & STAGE_TESS_EVALUATION) ? "1" : "0");
                        if (stages & STAGE_GEOMETRY)
                            testName += (testName.empty() ? "" : "_") + std::string("g") +
                                        ((writeMask & STAGE_GEOMETRY) ? "1" : "0");

                        TestParams params{};
                        params.pipelineConstructionType = pipelineConstructionType;
                        params.selectedStages           = stages;
                        params.writeStages              = writeMask;
                        // In case of useDeviceIndexAsViewIndex,
                        // number of physical devices in the group will decide the number of views
                        params.numViews                  = useDeviceIndexAsViewIndex ? 0u : viewCount;
                        params.explicitDeclarations      = explicitDeclarations;
                        params.useSSBO                   = useSSBO;
                        params.useViewIndexAsDeviceIndex = useDeviceIndexAsViewIndex;

                        viewGroup->addChild(new NoPositionCase(testCtx, testName, params));
                    }
                }

                ssboGroup->addChild(viewGroup.release());
            }

            declGroup->addChild(ssboGroup.release());
        }

        group->addChild(declGroup.release());
    }

    return group.release();
}

} // namespace pipeline
} // namespace vkt
