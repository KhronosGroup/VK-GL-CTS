/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Imagination Technologies Ltd.
 * Copyright (c) 2017 Google Inc.
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
 * \brief Multisample Tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineMultisampleTests.hpp"
#include "vktPipelineMultisampleImageTests.hpp"
#include "vktPipelineMultisampleSampleLocationsExtTests.hpp"
#include "vktPipelineMultisampleMixedAttachmentSamplesTests.hpp"
#include "vktPipelineMultisampleResolveRenderAreaTests.hpp"
#include "vktPipelineMultisampleShaderFragmentMaskTests.hpp"
#include "vktPipelineMultisampledRenderToSingleSampledTests.hpp"
#include "vktPipelineMultisampleResolveMaint10Tests.hpp"
#include "vktPipelineClearUtil.hpp"
#include "vktPipelineImageUtil.hpp"
#include "vktPipelineVertexUtil.hpp"
#include "vktPipelineReferenceRenderer.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTestLog.hpp"
#include "deUniquePtr.hpp"
#include "deSharedPtr.hpp"
#include "deStringUtil.hpp"
#include "deMemory.h"

#include <sstream>
#include <vector>
#include <map>
#include <memory>
#include <algorithm>
#include <set>
#include <array>
#include <utility>
#include <cmath>

namespace vkt
{
namespace pipeline
{

using namespace vk;

namespace
{
enum GeometryType
{
    GEOMETRY_TYPE_OPAQUE_TRIANGLE,
    GEOMETRY_TYPE_OPAQUE_LINE,
    GEOMETRY_TYPE_OPAQUE_POINT,
    GEOMETRY_TYPE_OPAQUE_QUAD,
    GEOMETRY_TYPE_OPAQUE_QUAD_NONZERO_DEPTH, //!< placed at z = 0.5
    GEOMETRY_TYPE_TRANSLUCENT_QUAD,
    GEOMETRY_TYPE_INVISIBLE_TRIANGLE,
    GEOMETRY_TYPE_INVISIBLE_QUAD,
    GEOMETRY_TYPE_GRADIENT_QUAD
};

enum TestModeBits
{
    TEST_MODE_DEPTH_BIT   = 1u,
    TEST_MODE_STENCIL_BIT = 2u,
};
typedef uint32_t TestModeFlags;

enum RenderType
{
    // resolve multisample rendering to single sampled image
    RENDER_TYPE_RESOLVE = 0u,

    // copy samples to an array of single sampled images
    RENDER_TYPE_COPY_SAMPLES = 1u,

    // render first with only depth/stencil and then with color + depth/stencil
    RENDER_TYPE_DEPTHSTENCIL_ONLY = 2u,

    // render using color attachment at location 1 and location 0 set as unused
    RENDER_TYPE_UNUSED_ATTACHMENT = 3u,

    // render using color attachment with single sample, required by alpha_to_one tests.
    RENDER_TYPE_SINGLE_SAMPLE = 4u
};

enum ImageBackingMode
{
    IMAGE_BACKING_MODE_REGULAR = 0u,
    IMAGE_BACKING_MODE_SPARSE
};

struct MultisampleTestParams
{
    PipelineConstructionType pipelineConstructionType;
    GeometryType geometryType;
    float pointSize;
    ImageBackingMode backingMode;
    bool useFragmentShadingRate;
};

void initMultisamplePrograms(SourceCollections &sources, MultisampleTestParams params);
bool isSupportedSampleCount(const InstanceInterface &instanceInterface, VkPhysicalDevice physicalDevice,
                            VkSampleCountFlagBits rasterizationSamples);
bool isSupportedDepthStencilFormat(const InstanceInterface &vki, const VkPhysicalDevice physDevice,
                                   const VkFormat format);
VkPipelineColorBlendAttachmentState getDefaultColorBlendAttachmentState(void);
VkPipelineColorBlendAttachmentState getAlphaToCoverageBlendState(bool blendEnable);
uint32_t getUniqueColorsCount(const tcu::ConstPixelBufferAccess &image);
VkImageAspectFlags getImageAspectFlags(const VkFormat format);
VkPrimitiveTopology getPrimitiveTopology(const GeometryType geometryType);
std::vector<Vertex4RGBA> generateVertices(const GeometryType geometryType);
VkFormat findSupportedDepthStencilFormat(Context &context, const bool useDepth, const bool useStencil);

class MultisampleTest : public vkt::TestCase
{
public:
    MultisampleTest(tcu::TestContext &testContext, const std::string &name,
                    PipelineConstructionType pipelineConstructionType,
                    const VkPipelineMultisampleStateCreateInfo &multisampleStateParams,
                    const VkPipelineColorBlendAttachmentState &blendState, GeometryType geometryType, float pointSize,
                    ImageBackingMode backingMode, const bool useFragmentShadingRate);
    virtual ~MultisampleTest(void)
    {
    }

    virtual void initPrograms(SourceCollections &programCollection) const;
    virtual TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;

protected:
    virtual TestInstance *createMultisampleTestInstance(
        Context &context, VkPrimitiveTopology topology, float pointSize, const std::vector<Vertex4RGBA> &vertices,
        const VkPipelineMultisampleStateCreateInfo &multisampleStateParams,
        const VkPipelineColorBlendAttachmentState &colorBlendState) const = 0;

    const PipelineConstructionType m_pipelineConstructionType;
    VkPipelineMultisampleStateCreateInfo m_multisampleStateParams;
    const VkPipelineColorBlendAttachmentState m_colorBlendState;
    const GeometryType m_geometryType;
    const float m_pointSize;
    const ImageBackingMode m_backingMode;
    std::vector<VkSampleMask> m_sampleMask;
    bool m_useFragmentShadingRate;
};

class RasterizationSamplesTest : public MultisampleTest
{
public:
    RasterizationSamplesTest(tcu::TestContext &testContext, const std::string &name,
                             PipelineConstructionType pipelineConstructionType,
                             VkSampleCountFlagBits rasterizationSamples, GeometryType geometryType, float pointSize,
                             ImageBackingMode backingMode, TestModeFlags modeFlags, const bool useFragmentShadingRate);
    virtual ~RasterizationSamplesTest(void)
    {
    }

protected:
    virtual TestInstance *createMultisampleTestInstance(
        Context &context, VkPrimitiveTopology topology, float pointSize, const std::vector<Vertex4RGBA> &vertices,
        const VkPipelineMultisampleStateCreateInfo &multisampleStateParams,
        const VkPipelineColorBlendAttachmentState &colorBlendState) const;

    static VkPipelineMultisampleStateCreateInfo getRasterizationSamplesStateParams(
        VkSampleCountFlagBits rasterizationSamples);

    const ImageBackingMode m_backingMode;
    const TestModeFlags m_modeFlags;
};

class MinSampleShadingTest : public MultisampleTest
{
public:
    MinSampleShadingTest(tcu::TestContext &testContext, const std::string &name,
                         const PipelineConstructionType pipelineConstructionType,
                         VkSampleCountFlagBits rasterizationSamples, float minSampleShading, GeometryType geometryType,
                         float pointSize, ImageBackingMode backingMode, const bool minSampleShadingEnabled,
                         const bool useFragmentShadingRate);
    virtual ~MinSampleShadingTest(void)
    {
    }

protected:
    virtual void initPrograms(SourceCollections &programCollection) const;
    virtual void checkSupport(Context &context) const;
    virtual TestInstance *createMultisampleTestInstance(
        Context &context, VkPrimitiveTopology topology, float pointSize, const std::vector<Vertex4RGBA> &vertices,
        const VkPipelineMultisampleStateCreateInfo &multisampleStateParams,
        const VkPipelineColorBlendAttachmentState &colorBlendState) const;

    static VkPipelineMultisampleStateCreateInfo getMinSampleShadingStateParams(
        VkSampleCountFlagBits rasterizationSamples, float minSampleShading, bool minSampleShadingEnabled);

    const float m_pointSize;
    const ImageBackingMode m_backingMode;
    const bool m_minSampleShadingEnabled;
};

class SampleMaskTest : public MultisampleTest
{
public:
    SampleMaskTest(tcu::TestContext &testContext, const std::string &name,
                   const PipelineConstructionType pipelineConstructionType, VkSampleCountFlagBits rasterizationSamples,
                   const std::vector<VkSampleMask> &sampleMask, GeometryType geometryType, float pointSize,
                   ImageBackingMode backingMode, const bool useFragmentShadingRate);

    virtual ~SampleMaskTest(void)
    {
    }

protected:
    virtual TestInstance *createMultisampleTestInstance(
        Context &context, VkPrimitiveTopology topology, float pointSize, const std::vector<Vertex4RGBA> &vertices,
        const VkPipelineMultisampleStateCreateInfo &multisampleStateParams,
        const VkPipelineColorBlendAttachmentState &colorBlendState) const;

    static VkPipelineMultisampleStateCreateInfo getSampleMaskStateParams(VkSampleCountFlagBits rasterizationSamples,
                                                                         const std::vector<VkSampleMask> &sampleMask);

    const ImageBackingMode m_backingMode;
};

class AlphaToOneTest : public MultisampleTest
{
public:
    AlphaToOneTest(tcu::TestContext &testContext, const std::string &name,
                   const PipelineConstructionType pipelineConstructionType, VkSampleCountFlagBits rasterizationSamples,
                   ImageBackingMode backingMode, const bool useFragmentShadingRate);

    virtual ~AlphaToOneTest(void)
    {
    }

protected:
    virtual void checkSupport(Context &context) const;
    virtual TestInstance *createMultisampleTestInstance(
        Context &context, VkPrimitiveTopology topology, float pointSize, const std::vector<Vertex4RGBA> &vertices,
        const VkPipelineMultisampleStateCreateInfo &multisampleStateParams,
        const VkPipelineColorBlendAttachmentState &colorBlendState) const;

    static VkPipelineMultisampleStateCreateInfo getAlphaToOneStateParams(VkSampleCountFlagBits rasterizationSamples);
    static VkPipelineColorBlendAttachmentState getAlphaToOneBlendState(void);

    const ImageBackingMode m_backingMode;
};

class AlphaToCoverageTest : public MultisampleTest
{
public:
    AlphaToCoverageTest(tcu::TestContext &testContext, const std::string &name,
                        const PipelineConstructionType pipelineConstructionType,
                        VkSampleCountFlagBits rasterizationSamples, GeometryType geometryType,
                        ImageBackingMode backingMode, const bool useFragmentShadingRate, const bool checkDepthBuffer);

    virtual ~AlphaToCoverageTest(void)
    {
    }
    void initPrograms(SourceCollections &programCollection) const override;

protected:
    TestInstance *createMultisampleTestInstance(
        Context &context, VkPrimitiveTopology topology, float pointSize, const std::vector<Vertex4RGBA> &vertices,
        const VkPipelineMultisampleStateCreateInfo &multisampleStateParams,
        const VkPipelineColorBlendAttachmentState &colorBlendState) const override;

    static VkPipelineMultisampleStateCreateInfo getAlphaToCoverageStateParams(
        VkSampleCountFlagBits rasterizationSamples);

    GeometryType m_geometryType;
    const ImageBackingMode m_backingMode;
    const bool m_checkDepthBuffer;
};

class AlphaToCoverageNoColorAttachmentTest : public MultisampleTest
{
public:
    AlphaToCoverageNoColorAttachmentTest(tcu::TestContext &testContext, const std::string &name,
                                         const PipelineConstructionType pipelineConstructionType,
                                         VkSampleCountFlagBits rasterizationSamples, GeometryType geometryType,
                                         ImageBackingMode backingMode, const bool useFragmentShadingRate);

    virtual ~AlphaToCoverageNoColorAttachmentTest(void)
    {
    }

protected:
    virtual TestInstance *createMultisampleTestInstance(
        Context &context, VkPrimitiveTopology topology, float pointSize, const std::vector<Vertex4RGBA> &vertices,
        const VkPipelineMultisampleStateCreateInfo &multisampleStateParams,
        const VkPipelineColorBlendAttachmentState &colorBlendState) const;

    static VkPipelineMultisampleStateCreateInfo getStateParams(VkSampleCountFlagBits rasterizationSamples);

    GeometryType m_geometryType;
    const ImageBackingMode m_backingMode;
};

class AlphaToCoverageColorUnusedAttachmentTest : public MultisampleTest
{
public:
    AlphaToCoverageColorUnusedAttachmentTest(tcu::TestContext &testContext, const std::string &name,
                                             const PipelineConstructionType pipelineConstructionType,
                                             VkSampleCountFlagBits rasterizationSamples, GeometryType geometryType,
                                             ImageBackingMode backingMode, const bool useFragmentShadingRate);

    virtual ~AlphaToCoverageColorUnusedAttachmentTest(void)
    {
    }

protected:
    virtual void initPrograms(SourceCollections &programCollection) const;

    virtual TestInstance *createMultisampleTestInstance(
        Context &context, VkPrimitiveTopology topology, float pointSize, const std::vector<Vertex4RGBA> &vertices,
        const VkPipelineMultisampleStateCreateInfo &multisampleStateParams,
        const VkPipelineColorBlendAttachmentState &colorBlendState) const;

    static VkPipelineMultisampleStateCreateInfo getStateParams(VkSampleCountFlagBits rasterizationSamples);

    GeometryType m_geometryType;
    const ImageBackingMode m_backingMode;
};

class SampleMaskWithConservativeTest : public vkt::TestCase
{
public:
    SampleMaskWithConservativeTest(tcu::TestContext &testContext, const std::string &name,
                                   const PipelineConstructionType pipelineConstructionType,
                                   const VkSampleCountFlagBits rasterizationSamples,
                                   const VkConservativeRasterizationModeEXT conservativeRasterizationMode,
                                   const bool enableMinSampleShading, const float minSampleShading,
                                   const bool enableSampleMask, const VkSampleMask sampleMask,
                                   const bool enablePostDepthCoverage, const bool useFragmentShadingRate);

    ~SampleMaskWithConservativeTest(void)
    {
    }

    void initPrograms(SourceCollections &programCollection) const;
    TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;

private:
    const PipelineConstructionType m_pipelineConstructionType;
    const VkSampleCountFlagBits m_rasterizationSamples;
    const bool m_enableMinSampleShading;
    float m_minSampleShading;
    const bool m_enableSampleMask;
    const VkSampleMask m_sampleMask;
    const VkConservativeRasterizationModeEXT m_conservativeRasterizationMode;
    const bool m_enablePostDepthCoverage;
    const RenderType m_renderType;
    const bool m_useFragmentShadingRate;
};
#ifndef CTS_USES_VULKANSC
class SampleMaskWithDepthTestTest : public vkt::TestCase
{
public:
    SampleMaskWithDepthTestTest(tcu::TestContext &testContext, const std::string &name,
                                const PipelineConstructionType pipelineConstructionType,
                                const VkSampleCountFlagBits rasterizationSamples, const bool enablePostDepthCoverage,
                                const bool useFragmentShadingRate);

    ~SampleMaskWithDepthTestTest(void)
    {
    }

    void initPrograms(SourceCollections &programCollection) const;
    TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;

private:
    const PipelineConstructionType m_pipelineConstructionType;
    const VkSampleCountFlagBits m_rasterizationSamples;
    const bool m_enablePostDepthCoverage;
    const bool m_useFragmentShadingRate;
};
#endif // CTS_USES_VULKANSC

class CompatibleRenderPassTest : public vkt::TestCase
{
public:
    CompatibleRenderPassTest(tcu::TestContext &testContext, const std::string &name,
                             const PipelineConstructionType pipelineConstructionType, bool dynamic);

    ~CompatibleRenderPassTest(void)
    {
    }

    void initPrograms(SourceCollections &programCollection) const;
    TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;

private:
    const PipelineConstructionType m_pipelineConstructionType;
    const bool m_dynamic;
};

class MultisampleRenderer
{
public:
    MultisampleRenderer(Context &context, PipelineConstructionType pipelineConstructionType, const VkFormat colorFormat,
                        const tcu::IVec2 &renderSize, const VkPrimitiveTopology topology,
                        const std::vector<Vertex4RGBA> &vertices,
                        const VkPipelineMultisampleStateCreateInfo &multisampleStateParams,
                        const VkPipelineColorBlendAttachmentState &blendState, const RenderType renderType,
                        const ImageBackingMode backingMode, const bool useFragmentShadingRate);

    MultisampleRenderer(Context &context, PipelineConstructionType pipelineConstructionType, const VkFormat colorFormat,
                        const VkFormat depthStencilFormat, const tcu::IVec2 &renderSize, const bool useDepth,
                        const bool useStencil, const uint32_t numTopologies, const VkPrimitiveTopology *pTopology,
                        const std::vector<Vertex4RGBA> *pVertices,
                        const VkPipelineMultisampleStateCreateInfo &multisampleStateParams,
                        const VkPipelineColorBlendAttachmentState &blendState, const RenderType renderType,
                        const ImageBackingMode backingMode, const bool useFragmentShadingRate,
                        const float depthClearValue = 1.0f);

    MultisampleRenderer(Context &context, PipelineConstructionType pipelineConstructionType, const VkFormat colorFormat,
                        const VkFormat depthStencilFormat, const tcu::IVec2 &renderSize, const bool useDepth,
                        const bool useStencil, const bool useConservative, const bool useFragmentShadingRate,
                        const uint32_t numTopologies, const VkPrimitiveTopology *pTopology,
                        const std::vector<Vertex4RGBA> *pVertices,
                        const VkPipelineMultisampleStateCreateInfo &multisampleStateParams,
                        const VkPipelineColorBlendAttachmentState &blendState,
                        const VkPipelineRasterizationConservativeStateCreateInfoEXT &conservativeStateCreateInfo,
                        const RenderType renderType, const ImageBackingMode backingMode,
                        const float depthClearValue = 1.0f);

    virtual ~MultisampleRenderer(void);

    de::MovePtr<tcu::TextureLevel> render(void);
    de::MovePtr<tcu::TextureLevel> getSingleSampledImage(uint32_t sampleId);
    de::MovePtr<tcu::TextureLevel> renderReusingDepth();

protected:
    void initialize(Context &context, const uint32_t numTopologies, const VkPrimitiveTopology *pTopology,
                    const std::vector<Vertex4RGBA> *pVertices);

    Context &m_context;
    const PipelineConstructionType m_pipelineConstructionType;

    const Unique<VkSemaphore> m_bindSemaphore;

    const VkFormat m_colorFormat;
    const VkFormat m_depthStencilFormat;
    tcu::IVec2 m_renderSize;
    const bool m_useDepth;
    const bool m_useStencil;
    const bool m_useConservative;

    const VkPipelineMultisampleStateCreateInfo m_multisampleStateParams;
    const VkPipelineColorBlendAttachmentState m_colorBlendState;
    const VkPipelineRasterizationConservativeStateCreateInfoEXT m_rasterizationConservativeStateCreateInfo;

    const RenderType m_renderType;

    Move<VkImage> m_colorImage;
    de::MovePtr<Allocation> m_colorImageAlloc;
    Move<VkImageView> m_colorAttachmentView;

    Move<VkImage> m_resolveImage;
    de::MovePtr<Allocation> m_resolveImageAlloc;
    Move<VkImageView> m_resolveAttachmentView;

    struct PerSampleImage
    {
        Move<VkImage> m_image;
        de::MovePtr<Allocation> m_imageAlloc;
        Move<VkImageView> m_attachmentView;
    };
    std::vector<de::SharedPtr<PerSampleImage>> m_perSampleImages;

    Move<VkImage> m_depthStencilImage;
    de::MovePtr<Allocation> m_depthStencilImageAlloc;
    Move<VkImageView> m_depthStencilAttachmentView;

    RenderPassWrapper m_renderPass;

    ShaderWrapper m_vertexShaderModule;
    ShaderWrapper m_fragmentShaderModule;

    ShaderWrapper m_copySampleVertexShaderModule;
    ShaderWrapper m_copySampleFragmentShaderModule;

    Move<VkBuffer> m_vertexBuffer;
    de::MovePtr<Allocation> m_vertexBufferAlloc;

    PipelineLayoutWrapper m_pipelineLayout;
    std::vector<GraphicsPipelineWrapper> m_graphicsPipelines;

    Move<VkDescriptorSetLayout> m_copySampleDesciptorLayout;
    Move<VkDescriptorPool> m_copySampleDesciptorPool;
    Move<VkDescriptorSet> m_copySampleDesciptorSet;

    PipelineLayoutWrapper m_copySamplePipelineLayout;
    std::vector<GraphicsPipelineWrapper> m_copySamplePipelines;

    Move<VkCommandPool> m_cmdPool;
    Move<VkCommandBuffer> m_cmdBuffer;

    std::vector<de::SharedPtr<Allocation>> m_allocations;

    ImageBackingMode m_backingMode;
    const float m_depthClearValue;
    const bool m_useFragmentShadingRate;
};

class RasterizationSamplesInstance : public vkt::TestInstance
{
public:
    RasterizationSamplesInstance(Context &context, const PipelineConstructionType pipelineConstructionType,
                                 VkPrimitiveTopology topology, float pointSize,
                                 const std::vector<Vertex4RGBA> &vertices,
                                 const VkPipelineMultisampleStateCreateInfo &multisampleStateParams,
                                 const VkPipelineColorBlendAttachmentState &blendState, const TestModeFlags modeFlags,
                                 ImageBackingMode backingMode, const bool useFragmentShadingRate);
    virtual ~RasterizationSamplesInstance(void)
    {
    }

    virtual tcu::TestStatus iterate(void);

protected:
    virtual tcu::TestStatus verifyImage(const tcu::ConstPixelBufferAccess &result);

    const VkFormat m_colorFormat;
    const tcu::IVec2 m_renderSize;
    const VkPrimitiveTopology m_primitiveTopology;
    const float m_pointSize;
    const std::vector<Vertex4RGBA> m_vertices;
    const std::vector<Vertex4RGBA> m_fullQuadVertices; //!< used by depth/stencil case
    const TestModeFlags m_modeFlags;
    de::MovePtr<MultisampleRenderer> m_multisampleRenderer;
    const bool m_useFragmentShadingRate;
};

class MinSampleShadingInstance : public vkt::TestInstance
{
public:
    MinSampleShadingInstance(Context &context, const PipelineConstructionType pipelineConstructionType,
                             VkPrimitiveTopology topology, float pointSize, const std::vector<Vertex4RGBA> &vertices,
                             const VkPipelineMultisampleStateCreateInfo &multisampleStateParams,
                             const VkPipelineColorBlendAttachmentState &blendState, ImageBackingMode backingMode,
                             const bool useFragmentShadingRate);
    virtual ~MinSampleShadingInstance(void)
    {
    }

    virtual tcu::TestStatus iterate(void);

protected:
    virtual tcu::TestStatus verifySampleShadedImage(const std::vector<tcu::TextureLevel> &testShadingImages,
                                                    const tcu::ConstPixelBufferAccess &noSampleshadingImage);

    const PipelineConstructionType m_pipelineConstructionType;
    const VkFormat m_colorFormat;
    const tcu::IVec2 m_renderSize;
    const VkPrimitiveTopology m_primitiveTopology;
    const std::vector<Vertex4RGBA> m_vertices;
    const VkPipelineMultisampleStateCreateInfo m_multisampleStateParams;
    const VkPipelineColorBlendAttachmentState m_colorBlendState;
    const ImageBackingMode m_backingMode;
    const bool m_useFragmentShadingRate;
};

class MinSampleShadingDisabledInstance : public MinSampleShadingInstance
{
public:
    MinSampleShadingDisabledInstance(Context &context, const PipelineConstructionType pipelineConstructionType,
                                     VkPrimitiveTopology topology, float pointSize,
                                     const std::vector<Vertex4RGBA> &vertices,
                                     const VkPipelineMultisampleStateCreateInfo &multisampleStateParams,
                                     const VkPipelineColorBlendAttachmentState &blendState,
                                     ImageBackingMode backingMode, const bool useFragmentShadingRate);
    virtual ~MinSampleShadingDisabledInstance(void)
    {
    }

protected:
    virtual tcu::TestStatus verifySampleShadedImage(const std::vector<tcu::TextureLevel> &sampleShadedImages,
                                                    const tcu::ConstPixelBufferAccess &noSampleshadingImage);
};

class SampleMaskInstance : public vkt::TestInstance
{
public:
    SampleMaskInstance(Context &context, const PipelineConstructionType pipelineConstructionType,
                       VkPrimitiveTopology topology, float pointSize, const std::vector<Vertex4RGBA> &vertices,
                       const VkPipelineMultisampleStateCreateInfo &multisampleStateParams,
                       const VkPipelineColorBlendAttachmentState &blendState, ImageBackingMode backingMode,
                       const bool useFragmentShadingRate);
    virtual ~SampleMaskInstance(void)
    {
    }

    virtual tcu::TestStatus iterate(void);

protected:
    virtual tcu::TestStatus verifyImage(const tcu::ConstPixelBufferAccess &testShadingImage,
                                        const tcu::ConstPixelBufferAccess &minShadingImage,
                                        const tcu::ConstPixelBufferAccess &maxShadingImage);
    const PipelineConstructionType m_pipelineConstructionType;
    const VkFormat m_colorFormat;
    const tcu::IVec2 m_renderSize;
    const VkPrimitiveTopology m_primitiveTopology;
    const std::vector<Vertex4RGBA> m_vertices;
    const VkPipelineMultisampleStateCreateInfo m_multisampleStateParams;
    const VkPipelineColorBlendAttachmentState m_colorBlendState;
    const ImageBackingMode m_backingMode;
    const bool m_useFragmentShadingRate;
};

class AlphaToOneInstance : public vkt::TestInstance
{
public:
    AlphaToOneInstance(Context &context, const PipelineConstructionType pipelineConstructionType,
                       VkPrimitiveTopology topology, const std::vector<Vertex4RGBA> &vertices,
                       const VkPipelineMultisampleStateCreateInfo &multisampleStateParams,
                       const VkPipelineColorBlendAttachmentState &blendState, ImageBackingMode backingMode,
                       const bool useFragmentShadingRate);
    virtual ~AlphaToOneInstance(void)
    {
    }

    virtual tcu::TestStatus iterate(void);

protected:
    virtual tcu::TestStatus verifyImage(const tcu::ConstPixelBufferAccess &alphaOneImage,
                                        const tcu::ConstPixelBufferAccess &noAlphaOneImage);
    const PipelineConstructionType m_pipelineConstructionType;
    const VkFormat m_colorFormat;
    const tcu::IVec2 m_renderSize;
    const VkPrimitiveTopology m_primitiveTopology;
    const std::vector<Vertex4RGBA> m_vertices;
    const VkPipelineMultisampleStateCreateInfo m_multisampleStateParams;
    const VkPipelineColorBlendAttachmentState m_colorBlendState;
    const ImageBackingMode m_backingMode;
    const bool m_useFragmentShadingRate;
};

class AlphaToCoverageInstance : public vkt::TestInstance
{
public:
    AlphaToCoverageInstance(Context &context, const PipelineConstructionType pipelineConstructionType,
                            VkPrimitiveTopology topology, const std::vector<Vertex4RGBA> &vertices,
                            const VkPipelineMultisampleStateCreateInfo &multisampleStateParams,
                            const VkPipelineColorBlendAttachmentState &blendState, GeometryType geometryType,
                            ImageBackingMode backingMode, const bool useFragmentShadingRate,
                            const bool checkDepthBuffer);
    virtual ~AlphaToCoverageInstance(void)
    {
    }

    virtual tcu::TestStatus iterate(void);

protected:
    virtual tcu::TestStatus verifyImage(const tcu::ConstPixelBufferAccess &result);
    virtual tcu::TestStatus verifyDepthBufferCheck(const tcu::ConstPixelBufferAccess &result);

    const PipelineConstructionType m_pipelineConstructionType;
    const VkFormat m_colorFormat;
    const VkFormat m_depthStencilFormat;
    const tcu::IVec2 m_renderSize;
    const VkPrimitiveTopology m_primitiveTopology;
    const std::vector<Vertex4RGBA> m_vertices;
    const VkPipelineMultisampleStateCreateInfo m_multisampleStateParams;
    const VkPipelineColorBlendAttachmentState m_colorBlendState;
    const GeometryType m_geometryType;
    const ImageBackingMode m_backingMode;
    const bool m_useFragmentShadingRate;
    const bool m_checkDepthBuffer;
};

class AlphaToCoverageNoColorAttachmentInstance : public vkt::TestInstance
{
public:
    AlphaToCoverageNoColorAttachmentInstance(Context &context, const PipelineConstructionType pipelineConstructionType,
                                             VkPrimitiveTopology topology, const std::vector<Vertex4RGBA> &vertices,
                                             const VkPipelineMultisampleStateCreateInfo &multisampleStateParams,
                                             const VkPipelineColorBlendAttachmentState &blendState,
                                             GeometryType geometryType, ImageBackingMode backingMode,
                                             const bool useFragmentShadingRate);
    virtual ~AlphaToCoverageNoColorAttachmentInstance(void)
    {
    }

    virtual tcu::TestStatus iterate(void);

protected:
    virtual tcu::TestStatus verifyImage(const tcu::ConstPixelBufferAccess &result);

    const PipelineConstructionType m_pipelineConstructionType;
    const VkFormat m_colorFormat;
    const VkFormat m_depthStencilFormat;
    const tcu::IVec2 m_renderSize;
    const VkPrimitiveTopology m_primitiveTopology;
    const std::vector<Vertex4RGBA> m_vertices;
    const VkPipelineMultisampleStateCreateInfo m_multisampleStateParams;
    const VkPipelineColorBlendAttachmentState m_colorBlendState;
    const GeometryType m_geometryType;
    const ImageBackingMode m_backingMode;
    const bool m_useFragmentShadingRate;
};

class AlphaToCoverageColorUnusedAttachmentInstance : public vkt::TestInstance
{
public:
    AlphaToCoverageColorUnusedAttachmentInstance(Context &context,
                                                 const PipelineConstructionType pipelineConstructionType,
                                                 VkPrimitiveTopology topology, const std::vector<Vertex4RGBA> &vertices,
                                                 const VkPipelineMultisampleStateCreateInfo &multisampleStateParams,
                                                 const VkPipelineColorBlendAttachmentState &blendState,
                                                 GeometryType geometryType, ImageBackingMode backingMode,
                                                 const bool useFragmentShadingRate);
    virtual ~AlphaToCoverageColorUnusedAttachmentInstance(void)
    {
    }

    virtual tcu::TestStatus iterate(void);

protected:
    virtual tcu::TestStatus verifyImage(const tcu::ConstPixelBufferAccess &result);

    const PipelineConstructionType m_pipelineConstructionType;
    const VkFormat m_colorFormat;
    const tcu::IVec2 m_renderSize;
    const VkPrimitiveTopology m_primitiveTopology;
    const std::vector<Vertex4RGBA> m_vertices;
    const VkPipelineMultisampleStateCreateInfo m_multisampleStateParams;
    const VkPipelineColorBlendAttachmentState m_colorBlendState;
    const GeometryType m_geometryType;
    const ImageBackingMode m_backingMode;
    const bool m_useFragmentShadingRate;
};

class SampleMaskWithConservativeInstance : public vkt::TestInstance
{
public:
    SampleMaskWithConservativeInstance(Context &context, const PipelineConstructionType pipelineConstructionType,
                                       const VkSampleCountFlagBits rasterizationSamples,
                                       const bool enableMinSampleShading, const float minSampleShading,
                                       const bool enableSampleMask, const VkSampleMask sampleMask,
                                       const VkConservativeRasterizationModeEXT conservativeRasterizationMode,
                                       const bool enablePostDepthCoverage, const bool enableFullyCoveredEXT,
                                       const RenderType renderType, const bool useFragmentShadingRate);
    ~SampleMaskWithConservativeInstance(void)
    {
    }

    tcu::TestStatus iterate(void);

protected:
    VkPipelineMultisampleStateCreateInfo getMultisampleState(const VkSampleCountFlagBits rasterizationSamples,
                                                             const bool enableMinSampleShading,
                                                             const float minSampleShading, const bool enableSampleMask);
    VkPipelineRasterizationConservativeStateCreateInfoEXT getRasterizationConservativeStateCreateInfo(
        const VkConservativeRasterizationModeEXT conservativeRasterizationMode);
    std::vector<Vertex4RGBA> generateVertices(void);
    tcu::TestStatus verifyImage(const std::vector<tcu::TextureLevel> &sampleShadedImages,
                                const tcu::ConstPixelBufferAccess &result);

    const PipelineConstructionType m_pipelineConstructionType;
    const VkSampleCountFlagBits m_rasterizationSamples;
    const bool m_enablePostDepthCoverage;
    const bool m_enableFullyCoveredEXT;
    const VkFormat m_colorFormat;
    const VkFormat m_depthStencilFormat;
    const tcu::IVec2 m_renderSize;
    const bool m_useDepth;
    const bool m_useStencil;
    const bool m_useConservative;
    const bool m_useFragmentShadingRate;
    const VkConservativeRasterizationModeEXT m_conservativeRasterizationMode;
    const VkPrimitiveTopology m_topology;
    const tcu::Vec4 m_renderColor;
    const float m_depthClearValue;
    const std::vector<Vertex4RGBA> m_vertices;
    const bool m_enableSampleMask;
    const std::vector<VkSampleMask> m_sampleMask;
    const bool m_enableMinSampleShading;
    const float m_minSampleShading;
    const VkPipelineMultisampleStateCreateInfo m_multisampleStateParams;
    const VkPipelineRasterizationConservativeStateCreateInfoEXT m_rasterizationConservativeStateCreateInfo;
    const VkPipelineColorBlendAttachmentState m_blendState;
    const RenderType m_renderType;
    const ImageBackingMode m_imageBackingMode;
};

#ifndef CTS_USES_VULKANSC
class SampleMaskWithDepthTestInstance : public vkt::TestInstance
{
public:
    SampleMaskWithDepthTestInstance(Context &context, const PipelineConstructionType pipelineConstructionType,
                                    const VkSampleCountFlagBits rasterizationSamples,
                                    const bool enablePostDepthCoverage, const bool useFragmentShadingRate);
    ~SampleMaskWithDepthTestInstance(void)
    {
    }

    tcu::TestStatus iterate(void);

protected:
    VkPipelineMultisampleStateCreateInfo getMultisampleState(const VkSampleCountFlagBits rasterizationSamples);
    std::vector<Vertex4RGBA> generateVertices(void);
    tcu::TestStatus verifyImage(const tcu::ConstPixelBufferAccess &result);

    struct SampleCoverage
    {
        SampleCoverage()
        {
        }
        SampleCoverage(uint32_t min_, uint32_t max_) : min(min_), max(max_)
        {
        }

        uint32_t min;
        uint32_t max;
    };

    const PipelineConstructionType m_pipelineConstructionType;
    const VkSampleCountFlagBits m_rasterizationSamples;
    const bool m_enablePostDepthCoverage;
    const VkFormat m_colorFormat;
    const VkFormat m_depthStencilFormat;
    const tcu::IVec2 m_renderSize;
    const bool m_useDepth;
    const bool m_useStencil;
    const VkPrimitiveTopology m_topology;
    const tcu::Vec4 m_renderColor;
    const std::vector<Vertex4RGBA> m_vertices;
    const VkPipelineMultisampleStateCreateInfo m_multisampleStateParams;
    const VkPipelineColorBlendAttachmentState m_blendState;
    const RenderType m_renderType;
    const ImageBackingMode m_imageBackingMode;
    const float m_depthClearValue;
    std::map<VkSampleCountFlagBits, SampleCoverage> m_refCoverageAfterDepthTest;
    const bool m_useFragmentShadingRate;
};

// Helper functions

void checkSupport(Context &context, MultisampleTestParams params)
{
    checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                          params.pipelineConstructionType);
}
#endif // CTS_USES_VULKANSC

class CompatibleRenderPassTestInstance : public vkt::TestInstance
{
public:
    CompatibleRenderPassTestInstance(Context &context, const PipelineConstructionType pipelineConstructionType,
                                     bool dynamic);
    ~CompatibleRenderPassTestInstance(void)
    {
    }

    tcu::TestStatus iterate(void);

private:
    const PipelineConstructionType m_pipelineConstructionType;
    const bool m_dynamic;
};

void initMultisamplePrograms(SourceCollections &sources, MultisampleTestParams params)
{
    const std::string pointSize = params.geometryType == GEOMETRY_TYPE_OPAQUE_POINT ?
                                      (std::string("    gl_PointSize = ") + de::toString(params.pointSize) + ".0f;\n") :
                                      std::string("");
    std::ostringstream vertexSource;

    vertexSource << "#version 310 es\n"
                    "layout(location = 0) in vec4 position;\n"
                    "layout(location = 1) in vec4 color;\n"
                    "layout(location = 0) out highp vec4 vtxColor;\n"
                    "void main (void)\n"
                    "{\n"
                    "    gl_Position = position;\n"
                    "    vtxColor = color;\n"
                 << pointSize << "}\n";

    static const char *fragmentSource = "#version 310 es\n"
                                        "layout(location = 0) in highp vec4 vtxColor;\n"
                                        "layout(location = 0) out highp vec4 fragColor;\n"
                                        "void main (void)\n"
                                        "{\n"
                                        "    fragColor = vtxColor;\n"
                                        "}\n";

    sources.glslSources.add("color_vert") << glu::VertexSource(vertexSource.str());
    sources.glslSources.add("color_frag") << glu::FragmentSource(fragmentSource);
}

void initSampleShadingPrograms(SourceCollections &sources, MultisampleTestParams params, bool minSampleShadingEnabled)
{
    {
        const std::string pointSize =
            params.geometryType == GEOMETRY_TYPE_OPAQUE_POINT ?
                (std::string("    gl_PointSize = ") + de::toString(params.pointSize) + ".0f;\n") :
                std::string("");
        std::ostringstream vertexSource;
        std::ostringstream fragmentSource;

        vertexSource << "#version 440\n"
                        "layout(location = 0) in vec4 position;\n"
                        "layout(location = 1) in vec4 color;\n"
                        "void main (void)\n"
                        "{\n"
                        "    gl_Position = position;\n"
                     << pointSize << "}\n";

        fragmentSource << "#version 440\n"
                          "layout(location = 0) out highp vec4 fragColor;\n"
                          "void main (void)\n"
                          "{\n";
        if (minSampleShadingEnabled)
        {
            fragmentSource
                << "    uint sampleId = gl_SampleID;\n"; // Enable sample shading for shader objects by reading gl_SampleID
        }
        fragmentSource << "    fragColor = vec4(fract(gl_FragCoord.xy), 0.0, 1.0);\n"
                          "}\n";

        sources.glslSources.add("color_vert") << glu::VertexSource(vertexSource.str());
        sources.glslSources.add("color_frag") << glu::FragmentSource(fragmentSource.str());
    }

    {
        static const char *vertexSource = "#version 440\n"
                                          "void main (void)\n"
                                          "{\n"
                                          "    const vec4 positions[4] = vec4[4](\n"
                                          "        vec4(-1.0, -1.0, 0.0, 1.0),\n"
                                          "        vec4(-1.0,  1.0, 0.0, 1.0),\n"
                                          "        vec4( 1.0, -1.0, 0.0, 1.0),\n"
                                          "        vec4( 1.0,  1.0, 0.0, 1.0)\n"
                                          "    );\n"
                                          "    gl_Position = positions[gl_VertexIndex];\n"
                                          "}\n";

        static const char *fragmentSource =
            "#version 440\n"
            "precision highp float;\n"
            "layout(location = 0) out highp vec4 fragColor;\n"
            "layout(set = 0, binding = 0, input_attachment_index = 0) uniform subpassInputMS imageMS;\n"
            "layout(push_constant) uniform PushConstantsBlock\n"
            "{\n"
            "    int sampleId;\n"
            "} pushConstants;\n"
            "void main (void)\n"
            "{\n"
            "    fragColor = subpassLoad(imageMS, pushConstants.sampleId);\n"
            "}\n";

        sources.glslSources.add("quad_vert") << glu::VertexSource(vertexSource);
        sources.glslSources.add("copy_sample_frag") << glu::FragmentSource(fragmentSource);
    }
}

void initAlphaToCoverageColorUnusedAttachmentPrograms(SourceCollections &sources)
{
    std::ostringstream vertexSource;

    vertexSource << "#version 310 es\n"
                    "layout(location = 0) in vec4 position;\n"
                    "layout(location = 1) in vec4 color;\n"
                    "layout(location = 0) out highp vec4 vtxColor;\n"
                    "void main (void)\n"
                    "{\n"
                    "    gl_Position = position;\n"
                    "    vtxColor = color;\n"
                    "}\n";

    // Location 0 is unused, but the alpha for coverage is written there. Location 1 has no alpha channel.
    static const char *fragmentSource = "#version 310 es\n"
                                        "layout(location = 0) in highp vec4 vtxColor;\n"
                                        "layout(location = 0) out highp vec4 fragColor0;\n"
                                        "layout(location = 1) out highp vec3 fragColor1;\n"
                                        "void main (void)\n"
                                        "{\n"
                                        "    fragColor0 = vtxColor;\n"
                                        "    fragColor1 = vtxColor.rgb;\n"
                                        "}\n";

    sources.glslSources.add("color_vert") << glu::VertexSource(vertexSource.str());
    sources.glslSources.add("color_frag") << glu::FragmentSource(fragmentSource);
}

bool isSupportedSampleCount(const InstanceInterface &instanceInterface, VkPhysicalDevice physicalDevice,
                            VkSampleCountFlagBits rasterizationSamples)
{
    VkPhysicalDeviceProperties deviceProperties;

    instanceInterface.getPhysicalDeviceProperties(physicalDevice, &deviceProperties);

    return !!(deviceProperties.limits.framebufferColorSampleCounts & rasterizationSamples);
}

bool checkFragmentShadingRateRequirements(Context &context, uint32_t sampleCount)
{
    const auto &vki           = context.getInstanceInterface();
    const auto physicalDevice = context.getPhysicalDevice();

    context.requireDeviceFunctionality("VK_KHR_fragment_shading_rate");

    if (!context.getFragmentShadingRateFeatures().pipelineFragmentShadingRate)
        TCU_THROW(NotSupportedError, "pipelineFragmentShadingRate not supported");

    // Fetch information about supported fragment shading rates
    uint32_t supportedFragmentShadingRateCount = 0;
    vki.getPhysicalDeviceFragmentShadingRatesKHR(physicalDevice, &supportedFragmentShadingRateCount, nullptr);

    std::vector<vk::VkPhysicalDeviceFragmentShadingRateKHR> supportedFragmentShadingRates(
        supportedFragmentShadingRateCount,
        {vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_KHR, nullptr, vk::VK_SAMPLE_COUNT_1_BIT, {1, 1}});
    vki.getPhysicalDeviceFragmentShadingRatesKHR(physicalDevice, &supportedFragmentShadingRateCount,
                                                 supportedFragmentShadingRates.data());

    for (const auto &rate : supportedFragmentShadingRates)
    {
        if ((rate.fragmentSize.width == 2u) && (rate.fragmentSize.height == 2u) && (rate.sampleCounts & sampleCount))
            return true;
    }

    return false;
}

VkPipelineColorBlendAttachmentState getDefaultColorBlendAttachmentState()
{
    const VkPipelineColorBlendAttachmentState colorBlendState = {
        false,                                                // VkBool32 blendEnable;
        VK_BLEND_FACTOR_ONE,                                  // VkBlendFactor srcColorBlendFactor;
        VK_BLEND_FACTOR_ZERO,                                 // VkBlendFactor dstColorBlendFactor;
        VK_BLEND_OP_ADD,                                      // VkBlendOp colorBlendOp;
        VK_BLEND_FACTOR_ONE,                                  // VkBlendFactor srcAlphaBlendFactor;
        VK_BLEND_FACTOR_ZERO,                                 // VkBlendFactor dstAlphaBlendFactor;
        VK_BLEND_OP_ADD,                                      // VkBlendOp alphaBlendOp;
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | // VkColorComponentFlags colorWriteMask;
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};

    return colorBlendState;
}

VkPipelineColorBlendAttachmentState getAlphaToCoverageBlendState(bool blendEnable)
{
    const VkPipelineColorBlendAttachmentState colorBlendState = {
        blendEnable,                                          // VkBool32 blendEnable;
        VK_BLEND_FACTOR_SRC_ALPHA,                            // VkBlendFactor srcColorBlendFactor;
        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,                  // VkBlendFactor dstColorBlendFactor;
        VK_BLEND_OP_ADD,                                      // VkBlendOp colorBlendOp;
        VK_BLEND_FACTOR_ZERO,                                 // VkBlendFactor srcAlphaBlendFactor;
        VK_BLEND_FACTOR_ONE,                                  // VkBlendFactor dstAlphaBlendFactor;
        VK_BLEND_OP_ADD,                                      // VkBlendOp alphaBlendOp;
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | // VkColorComponentFlags colorWriteMask;
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};

    return colorBlendState;
}

uint32_t getUniqueColorsCount(const tcu::ConstPixelBufferAccess &image)
{
    DE_ASSERT(image.getFormat().getPixelSize() == 4);

    std::map<uint32_t, uint32_t> histogram; // map<pixel value, number of occurrences>
    const uint32_t pixelCount = image.getWidth() * image.getHeight() * image.getDepth();

    for (uint32_t pixelNdx = 0; pixelNdx < pixelCount; pixelNdx++)
    {
        const uint32_t pixelValue = *((const uint32_t *)image.getDataPtr() + pixelNdx);

        if (histogram.find(pixelValue) != histogram.end())
            histogram[pixelValue]++;
        else
            histogram[pixelValue] = 1;
    }

    return (uint32_t)histogram.size();
}

VkImageAspectFlags getImageAspectFlags(const VkFormat format)
{
    const tcu::TextureFormat tcuFormat = mapVkFormat(format);

    if (tcuFormat.order == tcu::TextureFormat::DS)
        return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    else if (tcuFormat.order == tcu::TextureFormat::D)
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    else if (tcuFormat.order == tcu::TextureFormat::S)
        return VK_IMAGE_ASPECT_STENCIL_BIT;

    DE_ASSERT(false);
    return 0u;
}

std::vector<Vertex4RGBA> generateVertices(const GeometryType geometryType)
{
    std::vector<Vertex4RGBA> vertices;

    switch (geometryType)
    {
    case GEOMETRY_TYPE_OPAQUE_TRIANGLE:
    case GEOMETRY_TYPE_INVISIBLE_TRIANGLE:
    {
        Vertex4RGBA vertexData[3] = {{tcu::Vec4(-0.75f, 0.0f, 0.0f, 1.0f), tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f)},
                                     {tcu::Vec4(0.75f, 0.125f, 0.0f, 1.0f), tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f)},
                                     {tcu::Vec4(0.75f, -0.125f, 0.0f, 1.0f), tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f)}};

        if (geometryType == GEOMETRY_TYPE_INVISIBLE_TRIANGLE)
        {
            for (int i = 0; i < 3; i++)
                vertexData[i].color = tcu::Vec4();
        }

        vertices = std::vector<Vertex4RGBA>(vertexData, vertexData + 3);
        break;
    }

    case GEOMETRY_TYPE_OPAQUE_LINE:
    {
        const Vertex4RGBA vertexData[2] = {{tcu::Vec4(-0.75f, 0.25f, 0.0f, 1.0f), tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f)},
                                           {tcu::Vec4(0.75f, -0.25f, 0.0f, 1.0f), tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f)}};

        vertices = std::vector<Vertex4RGBA>(vertexData, vertexData + 2);
        break;
    }

    case GEOMETRY_TYPE_OPAQUE_POINT:
    {
        const Vertex4RGBA vertex = {tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f), tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f)};

        vertices = std::vector<Vertex4RGBA>(1, vertex);
        break;
    }

    case GEOMETRY_TYPE_OPAQUE_QUAD:
    case GEOMETRY_TYPE_OPAQUE_QUAD_NONZERO_DEPTH:
    case GEOMETRY_TYPE_TRANSLUCENT_QUAD:
    case GEOMETRY_TYPE_INVISIBLE_QUAD:
    case GEOMETRY_TYPE_GRADIENT_QUAD:
    {
        Vertex4RGBA vertexData[4] = {{tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f), tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f)},
                                     {tcu::Vec4(1.0f, -1.0f, 0.0f, 1.0f), tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f)},
                                     {tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f), tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f)},
                                     {tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f), tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f)}};

        if (geometryType == GEOMETRY_TYPE_TRANSLUCENT_QUAD)
        {
            for (int i = 0; i < 4; i++)
                vertexData[i].color.w() = 0.25f;
        }
        else if (geometryType == GEOMETRY_TYPE_INVISIBLE_QUAD)
        {
            for (int i = 0; i < 4; i++)
                vertexData[i].color.w() = 0.0f;
        }
        else if (geometryType == GEOMETRY_TYPE_GRADIENT_QUAD)
        {
            vertexData[0].color.w() = 0.0f;
            vertexData[2].color.w() = 0.0f;
        }
        else if (geometryType == GEOMETRY_TYPE_OPAQUE_QUAD_NONZERO_DEPTH)
        {
            for (int i = 0; i < 4; i++)
                vertexData[i].position.z() = 0.5f;
        }

        vertices = std::vector<Vertex4RGBA>(vertexData, vertexData + de::arrayLength(vertexData));
        break;
    }

    default:
        DE_ASSERT(false);
    }
    return vertices;
}

VkPrimitiveTopology getPrimitiveTopology(const GeometryType geometryType)
{
    switch (geometryType)
    {
    case GEOMETRY_TYPE_OPAQUE_TRIANGLE:
    case GEOMETRY_TYPE_INVISIBLE_TRIANGLE:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    case GEOMETRY_TYPE_OPAQUE_LINE:
        return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    case GEOMETRY_TYPE_OPAQUE_POINT:
        return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

    case GEOMETRY_TYPE_OPAQUE_QUAD:
    case GEOMETRY_TYPE_OPAQUE_QUAD_NONZERO_DEPTH:
    case GEOMETRY_TYPE_TRANSLUCENT_QUAD:
    case GEOMETRY_TYPE_INVISIBLE_QUAD:
    case GEOMETRY_TYPE_GRADIENT_QUAD:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    default:
        DE_ASSERT(false);
        return VK_PRIMITIVE_TOPOLOGY_LAST;
    }
}

bool isSupportedDepthStencilFormat(const InstanceInterface &vki, const VkPhysicalDevice physDevice,
                                   const VkFormat format)
{
    VkFormatProperties formatProps;
    vki.getPhysicalDeviceFormatProperties(physDevice, format, &formatProps);
    return (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0;
}

VkFormat findSupportedDepthStencilFormat(Context &context, const bool useDepth, const bool useStencil)
{
    if (useDepth && !useStencil)
        return VK_FORMAT_D16_UNORM; // must be supported

    const InstanceInterface &vki      = context.getInstanceInterface();
    const VkPhysicalDevice physDevice = context.getPhysicalDevice();

    // One of these formats must be supported.

    if (isSupportedDepthStencilFormat(vki, physDevice, VK_FORMAT_D24_UNORM_S8_UINT))
        return VK_FORMAT_D24_UNORM_S8_UINT;

    if (isSupportedDepthStencilFormat(vki, physDevice, VK_FORMAT_D32_SFLOAT_S8_UINT))
        return VK_FORMAT_D32_SFLOAT_S8_UINT;

    return VK_FORMAT_UNDEFINED;
}

// MultisampleTest

MultisampleTest::MultisampleTest(tcu::TestContext &testContext, const std::string &name,
                                 const PipelineConstructionType pipelineConstructionType,
                                 const VkPipelineMultisampleStateCreateInfo &multisampleStateParams,
                                 const VkPipelineColorBlendAttachmentState &blendState, GeometryType geometryType,
                                 float pointSize, ImageBackingMode backingMode, const bool useFragmentShadingRate)
    : vkt::TestCase(testContext, name)
    , m_pipelineConstructionType(pipelineConstructionType)
    , m_multisampleStateParams(multisampleStateParams)
    , m_colorBlendState(blendState)
    , m_geometryType(geometryType)
    , m_pointSize(pointSize)
    , m_backingMode(backingMode)
    , m_useFragmentShadingRate(useFragmentShadingRate)
{
    if (m_multisampleStateParams.pSampleMask)
    {
        // Copy pSampleMask to avoid dependencies with other classes

        const uint32_t maskCount = deCeilFloatToInt32(float(m_multisampleStateParams.rasterizationSamples) / 32);

        for (uint32_t maskNdx = 0; maskNdx < maskCount; maskNdx++)
            m_sampleMask.push_back(m_multisampleStateParams.pSampleMask[maskNdx]);

        m_multisampleStateParams.pSampleMask = m_sampleMask.data();
    }
}

void MultisampleTest::initPrograms(SourceCollections &programCollection) const
{
    MultisampleTestParams params = {m_pipelineConstructionType, m_geometryType, m_pointSize, m_backingMode,
                                    m_useFragmentShadingRate};
    initMultisamplePrograms(programCollection, params);
}

TestInstance *MultisampleTest::createInstance(Context &context) const
{
    return createMultisampleTestInstance(context, getPrimitiveTopology(m_geometryType), m_pointSize,
                                         generateVertices(m_geometryType), m_multisampleStateParams, m_colorBlendState);
}

void MultisampleTest::checkSupport(Context &context) const
{
    if (m_geometryType == GEOMETRY_TYPE_OPAQUE_POINT && m_pointSize > 1.0f)
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_LARGE_POINTS);

    if (m_useFragmentShadingRate &&
        !checkFragmentShadingRateRequirements(context, m_multisampleStateParams.rasterizationSamples))
        TCU_THROW(NotSupportedError, "Required FragmentShadingRate not supported");

    checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                          m_pipelineConstructionType);
}

// RasterizationSamplesTest

RasterizationSamplesTest::RasterizationSamplesTest(tcu::TestContext &testContext, const std::string &name,
                                                   PipelineConstructionType pipelineConstructionType,
                                                   VkSampleCountFlagBits rasterizationSamples,
                                                   GeometryType geometryType, float pointSize,
                                                   ImageBackingMode backingMode, TestModeFlags modeFlags,
                                                   const bool useFragmentShadingRate)
    : MultisampleTest(testContext, name, pipelineConstructionType,
                      getRasterizationSamplesStateParams(rasterizationSamples), getDefaultColorBlendAttachmentState(),
                      geometryType, pointSize, backingMode, useFragmentShadingRate)
    , m_backingMode(backingMode)
    , m_modeFlags(modeFlags)
{
}

VkPipelineMultisampleStateCreateInfo RasterizationSamplesTest::getRasterizationSamplesStateParams(
    VkSampleCountFlagBits rasterizationSamples)
{
    const VkPipelineMultisampleStateCreateInfo multisampleStateParams = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                  // const void* pNext;
        0u,                                                       // VkPipelineMultisampleStateCreateFlags flags;
        rasterizationSamples,                                     // VkSampleCountFlagBits rasterizationSamples;
        false,                                                    // VkBool32 sampleShadingEnable;
        0.0f,                                                     // float minSampleShading;
        nullptr,                                                  // const VkSampleMask* pSampleMask;
        false,                                                    // VkBool32 alphaToCoverageEnable;
        false                                                     // VkBool32 alphaToOneEnable;
    };

    return multisampleStateParams;
}

TestInstance *RasterizationSamplesTest::createMultisampleTestInstance(
    Context &context, VkPrimitiveTopology topology, float pointSize, const std::vector<Vertex4RGBA> &vertices,
    const VkPipelineMultisampleStateCreateInfo &multisampleStateParams,
    const VkPipelineColorBlendAttachmentState &colorBlendState) const
{
    return new RasterizationSamplesInstance(context, m_pipelineConstructionType, topology, pointSize, vertices,
                                            multisampleStateParams, colorBlendState, m_modeFlags, m_backingMode,
                                            m_useFragmentShadingRate);
}

// MinSampleShadingTest

MinSampleShadingTest::MinSampleShadingTest(tcu::TestContext &testContext, const std::string &name,
                                           const PipelineConstructionType pipelineConstructionType,
                                           VkSampleCountFlagBits rasterizationSamples, float minSampleShading,
                                           GeometryType geometryType, float pointSize, ImageBackingMode backingMode,
                                           const bool minSampleShadingEnabled, const bool useFragmentShadingRate)
    : MultisampleTest(testContext, name, pipelineConstructionType,
                      getMinSampleShadingStateParams(rasterizationSamples, minSampleShading, minSampleShadingEnabled),
                      getDefaultColorBlendAttachmentState(), geometryType, pointSize, backingMode,
                      useFragmentShadingRate)
    , m_pointSize(pointSize)
    , m_backingMode(backingMode)
    , m_minSampleShadingEnabled(minSampleShadingEnabled)
{
}

void MinSampleShadingTest::checkSupport(Context &context) const
{
    MultisampleTest::checkSupport(context);

    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SAMPLE_RATE_SHADING);
}

void MinSampleShadingTest::initPrograms(SourceCollections &programCollection) const
{
    MultisampleTestParams params = {m_pipelineConstructionType, m_geometryType, m_pointSize, m_backingMode,
                                    m_useFragmentShadingRate};
    initSampleShadingPrograms(programCollection, params, m_minSampleShadingEnabled);
}

TestInstance *MinSampleShadingTest::createMultisampleTestInstance(
    Context &context, VkPrimitiveTopology topology, float pointSize, const std::vector<Vertex4RGBA> &vertices,
    const VkPipelineMultisampleStateCreateInfo &multisampleStateParams,
    const VkPipelineColorBlendAttachmentState &colorBlendState) const
{
    if (m_minSampleShadingEnabled)
        return new MinSampleShadingInstance(context, m_pipelineConstructionType, topology, pointSize, vertices,
                                            multisampleStateParams, colorBlendState, m_backingMode,
                                            m_useFragmentShadingRate);
    else
        return new MinSampleShadingDisabledInstance(context, m_pipelineConstructionType, topology, pointSize, vertices,
                                                    multisampleStateParams, colorBlendState, m_backingMode,
                                                    m_useFragmentShadingRate);
}

VkPipelineMultisampleStateCreateInfo MinSampleShadingTest::getMinSampleShadingStateParams(
    VkSampleCountFlagBits rasterizationSamples, float minSampleShading, bool minSampleShadingEnabled)
{
    const VkPipelineMultisampleStateCreateInfo multisampleStateParams = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                  // const void* pNext;
        0u,                                                       // VkPipelineMultisampleStateCreateFlags flags;
        rasterizationSamples,                                     // VkSampleCountFlagBits rasterizationSamples;
        minSampleShadingEnabled ? VK_TRUE : VK_FALSE,             // VkBool32 sampleShadingEnable;
        minSampleShading,                                         // float minSampleShading;
        nullptr,                                                  // const VkSampleMask* pSampleMask;
        false,                                                    //  VkBool32 alphaToCoverageEnable;
        false                                                     //  VkBool32 alphaToOneEnable;
    };

    return multisampleStateParams;
}

// SampleMaskTest

SampleMaskTest::SampleMaskTest(tcu::TestContext &testContext, const std::string &name,
                               const PipelineConstructionType pipelineConstructionType,
                               VkSampleCountFlagBits rasterizationSamples, const std::vector<VkSampleMask> &sampleMask,
                               GeometryType geometryType, float pointSize, ImageBackingMode backingMode,
                               const bool useFragmentShadingRate)
    : MultisampleTest(testContext, name, pipelineConstructionType,
                      getSampleMaskStateParams(rasterizationSamples, sampleMask), getDefaultColorBlendAttachmentState(),
                      geometryType, pointSize, backingMode, useFragmentShadingRate)
    , m_backingMode(backingMode)
{
}

TestInstance *SampleMaskTest::createMultisampleTestInstance(
    Context &context, VkPrimitiveTopology topology, float pointSize, const std::vector<Vertex4RGBA> &vertices,
    const VkPipelineMultisampleStateCreateInfo &multisampleStateParams,
    const VkPipelineColorBlendAttachmentState &colorBlendState) const
{
    DE_UNREF(pointSize);
    return new SampleMaskInstance(context, m_pipelineConstructionType, topology, pointSize, vertices,
                                  multisampleStateParams, colorBlendState, m_backingMode, m_useFragmentShadingRate);
}

VkPipelineMultisampleStateCreateInfo SampleMaskTest::getSampleMaskStateParams(
    VkSampleCountFlagBits rasterizationSamples, const std::vector<VkSampleMask> &sampleMask)
{
    const VkPipelineMultisampleStateCreateInfo multisampleStateParams = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                  // const void* pNext;
        0u,                                                       // VkPipelineMultisampleStateCreateFlags flags;
        rasterizationSamples,                                     // VkSampleCountFlagBits rasterizationSamples;
        false,                                                    // VkBool32 sampleShadingEnable;
        0.0f,                                                     // float minSampleShading;
        sampleMask.data(),                                        // const VkSampleMask* pSampleMask;
        false,                                                    // VkBool32 alphaToCoverageEnable;
        false                                                     // VkBool32 alphaToOneEnable;
    };

    return multisampleStateParams;
}

// AlphaToOneTest

AlphaToOneTest::AlphaToOneTest(tcu::TestContext &testContext, const std::string &name,
                               const PipelineConstructionType pipelineConstructionType,
                               VkSampleCountFlagBits rasterizationSamples, ImageBackingMode backingMode,
                               const bool useFragmentShadingRate)
    : MultisampleTest(testContext, name, pipelineConstructionType, getAlphaToOneStateParams(rasterizationSamples),
                      getAlphaToOneBlendState(), GEOMETRY_TYPE_GRADIENT_QUAD, 1.0f, backingMode, useFragmentShadingRate)
    , m_backingMode(backingMode)
{
}

void AlphaToOneTest::checkSupport(Context &context) const
{
    MultisampleTest::checkSupport(context);

    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_ALPHA_TO_ONE);
}

TestInstance *AlphaToOneTest::createMultisampleTestInstance(
    Context &context, VkPrimitiveTopology topology, float pointSize, const std::vector<Vertex4RGBA> &vertices,
    const VkPipelineMultisampleStateCreateInfo &multisampleStateParams,
    const VkPipelineColorBlendAttachmentState &colorBlendState) const
{
    DE_UNREF(pointSize);
    return new AlphaToOneInstance(context, m_pipelineConstructionType, topology, vertices, multisampleStateParams,
                                  colorBlendState, m_backingMode, m_useFragmentShadingRate);
}

VkPipelineMultisampleStateCreateInfo AlphaToOneTest::getAlphaToOneStateParams(
    VkSampleCountFlagBits rasterizationSamples)
{
    const VkPipelineMultisampleStateCreateInfo multisampleStateParams = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                  // const void* pNext;
        0u,                                                       // VkPipelineMultisampleStateCreateFlags flags;
        rasterizationSamples,                                     // VkSampleCountFlagBits rasterizationSamples;
        false,                                                    // VkBool32 sampleShadingEnable;
        0.0f,                                                     // float minSampleShading;
        nullptr,                                                  // const VkSampleMask* pSampleMask;
        false,                                                    // VkBool32 alphaToCoverageEnable;
        true                                                      // VkBool32 alphaToOneEnable;
    };

    return multisampleStateParams;
}

VkPipelineColorBlendAttachmentState AlphaToOneTest::getAlphaToOneBlendState(void)
{
    const VkPipelineColorBlendAttachmentState colorBlendState = {
        true,                                                 // VkBool32 blendEnable;
        VK_BLEND_FACTOR_SRC_ALPHA,                            // VkBlendFactor srcColorBlendFactor;
        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,                  // VkBlendFactor dstColorBlendFactor;
        VK_BLEND_OP_ADD,                                      // VkBlendOp colorBlendOp;
        VK_BLEND_FACTOR_SRC_ALPHA,                            // VkBlendFactor srcAlphaBlendFactor;
        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,                  // VkBlendFactor dstAlphaBlendFactor;
        VK_BLEND_OP_ADD,                                      // VkBlendOp alphaBlendOp;
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | // VkColorComponentFlags colorWriteMask;
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};

    return colorBlendState;
}

// AlphaToCoverageTest

AlphaToCoverageTest::AlphaToCoverageTest(tcu::TestContext &testContext, const std::string &name,
                                         const PipelineConstructionType pipelineConstructionType,
                                         VkSampleCountFlagBits rasterizationSamples, GeometryType geometryType,
                                         ImageBackingMode backingMode, const bool useFragmentShadingRate,
                                         const bool checkDepthBuffer)
    : MultisampleTest(testContext, name, pipelineConstructionType, getAlphaToCoverageStateParams(rasterizationSamples),
                      getAlphaToCoverageBlendState(checkDepthBuffer), geometryType, 1.0f, backingMode,
                      useFragmentShadingRate)
    , m_geometryType(geometryType)
    , m_backingMode(backingMode)
    , m_checkDepthBuffer(checkDepthBuffer)
{
    if (checkDepthBuffer)
        DE_ASSERT(geometryType == GEOMETRY_TYPE_INVISIBLE_QUAD);
}

void AlphaToCoverageTest::initPrograms(SourceCollections &programCollection) const
{
    MultisampleTest::initPrograms(programCollection);

    if (m_checkDepthBuffer)
    {
        std::ostringstream vert;
        vert << "#version 460\n"
             << "layout (push_constant, std430) uniform PushConstantBlock { float depth; } pc;\n"
             << "layout (location=0) out vec4 vtxColor;\n"
             << "vec2 positions[3] = vec2[](\n"
             << "    vec2(-1.0, -1.0),\n"
             << "    vec2(-1.0, 3.0),\n"
             << "    vec2(3.0, -1.0)\n"
             << ");\n"
             << "void main (void) {\n"
             << "    gl_Position = vec4(positions[gl_VertexIndex % 3], pc.depth, 1.0);\n"
             << "    vtxColor = vec4(0.0, 0.0, 1.0, 1.0);\n"
             << "}\n";
        programCollection.glslSources.add("checkDepth-vert") << glu::VertexSource(vert.str());
    }
}

TestInstance *AlphaToCoverageTest::createMultisampleTestInstance(
    Context &context, VkPrimitiveTopology topology, float pointSize, const std::vector<Vertex4RGBA> &vertices,
    const VkPipelineMultisampleStateCreateInfo &multisampleStateParams,
    const VkPipelineColorBlendAttachmentState &colorBlendState) const
{
    DE_UNREF(pointSize);
    return new AlphaToCoverageInstance(context, m_pipelineConstructionType, topology, vertices, multisampleStateParams,
                                       colorBlendState, m_geometryType, m_backingMode, m_useFragmentShadingRate,
                                       m_checkDepthBuffer);
}

VkPipelineMultisampleStateCreateInfo AlphaToCoverageTest::getAlphaToCoverageStateParams(
    VkSampleCountFlagBits rasterizationSamples)
{
    const VkPipelineMultisampleStateCreateInfo multisampleStateParams = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                  // const void* pNext;
        0u,                                                       // VkPipelineMultisampleStateCreateFlags flags;
        rasterizationSamples,                                     // VkSampleCountFlagBits rasterizationSamples;
        false,                                                    // VkBool32 sampleShadingEnable;
        0.0f,                                                     // float minSampleShading;
        nullptr,                                                  // const VkSampleMask* pSampleMask;
        true,                                                     // VkBool32 alphaToCoverageEnable;
        false                                                     // VkBool32 alphaToOneEnable;
    };

    return multisampleStateParams;
}

// AlphaToCoverageNoColorAttachmentTest

AlphaToCoverageNoColorAttachmentTest::AlphaToCoverageNoColorAttachmentTest(
    tcu::TestContext &testContext, const std::string &name, const PipelineConstructionType pipelineConstructionType,
    VkSampleCountFlagBits rasterizationSamples, GeometryType geometryType, ImageBackingMode backingMode,
    const bool useFragmentShadingRate)
    : MultisampleTest(testContext, name, pipelineConstructionType, getStateParams(rasterizationSamples),
                      getDefaultColorBlendAttachmentState(), geometryType, 1.0f, backingMode, useFragmentShadingRate)
    , m_geometryType(geometryType)
    , m_backingMode(backingMode)
{
}

TestInstance *AlphaToCoverageNoColorAttachmentTest::createMultisampleTestInstance(
    Context &context, VkPrimitiveTopology topology, float pointSize, const std::vector<Vertex4RGBA> &vertices,
    const VkPipelineMultisampleStateCreateInfo &multisampleStateParams,
    const VkPipelineColorBlendAttachmentState &colorBlendState) const
{
    DE_UNREF(pointSize);
    return new AlphaToCoverageNoColorAttachmentInstance(context, m_pipelineConstructionType, topology, vertices,
                                                        multisampleStateParams, colorBlendState, m_geometryType,
                                                        m_backingMode, m_useFragmentShadingRate);
}

VkPipelineMultisampleStateCreateInfo AlphaToCoverageNoColorAttachmentTest::getStateParams(
    VkSampleCountFlagBits rasterizationSamples)
{
    const VkPipelineMultisampleStateCreateInfo multisampleStateParams = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                  // const void* pNext;
        0u,                                                       // VkPipelineMultisampleStateCreateFlags flags;
        rasterizationSamples,                                     // VkSampleCountFlagBits rasterizationSamples;
        false,                                                    // VkBool32 sampleShadingEnable;
        0.0f,                                                     // float minSampleShading;
        nullptr,                                                  // const VkSampleMask* pSampleMask;
        true,                                                     // VkBool32 alphaToCoverageEnable;
        false                                                     // VkBool32 alphaToOneEnable;
    };

    return multisampleStateParams;
}

// AlphaToCoverageColorUnusedAttachmentTest

AlphaToCoverageColorUnusedAttachmentTest::AlphaToCoverageColorUnusedAttachmentTest(
    tcu::TestContext &testContext, const std::string &name, const PipelineConstructionType pipelineConstructionType,
    VkSampleCountFlagBits rasterizationSamples, GeometryType geometryType, ImageBackingMode backingMode,
    const bool useFragmentShadingRate)
    : MultisampleTest(testContext, name, pipelineConstructionType, getStateParams(rasterizationSamples),
                      getDefaultColorBlendAttachmentState(), geometryType, 1.0f, backingMode, useFragmentShadingRate)
    , m_geometryType(geometryType)
    , m_backingMode(backingMode)
{
}

void AlphaToCoverageColorUnusedAttachmentTest::initPrograms(SourceCollections &programCollection) const
{
    initAlphaToCoverageColorUnusedAttachmentPrograms(programCollection);
}

TestInstance *AlphaToCoverageColorUnusedAttachmentTest::createMultisampleTestInstance(
    Context &context, VkPrimitiveTopology topology, float pointSize, const std::vector<Vertex4RGBA> &vertices,
    const VkPipelineMultisampleStateCreateInfo &multisampleStateParams,
    const VkPipelineColorBlendAttachmentState &colorBlendState) const
{
    DE_UNREF(pointSize);
    return new AlphaToCoverageColorUnusedAttachmentInstance(context, m_pipelineConstructionType, topology, vertices,
                                                            multisampleStateParams, colorBlendState, m_geometryType,
                                                            m_backingMode, m_useFragmentShadingRate);
}

VkPipelineMultisampleStateCreateInfo AlphaToCoverageColorUnusedAttachmentTest::getStateParams(
    VkSampleCountFlagBits rasterizationSamples)
{
    const VkPipelineMultisampleStateCreateInfo multisampleStateParams = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                  // const void* pNext;
        0u,                                                       // VkPipelineMultisampleStateCreateFlags flags;
        rasterizationSamples,                                     // VkSampleCountFlagBits rasterizationSamples;
        false,                                                    // VkBool32 sampleShadingEnable;
        0.0f,                                                     // float minSampleShading;
        nullptr,                                                  // const VkSampleMask* pSampleMask;
        true,                                                     // VkBool32 alphaToCoverageEnable;
        false                                                     // VkBool32 alphaToOneEnable;
    };

    return multisampleStateParams;
}

// Alpha to one with alpha to coverage. If an implementation doesn't do the operations in the right order, it will fail
// a trivial test: color all samples with a value that has alpha 0.0, and alpha to one will replace the alpha with 1.0.
// Then, alpha to coverage will result in the samples being covered. When using the right order, no samples should have
// coverage because the alpha to coverage tests should happen first, and there will be no samples to modify to set the
// alpha to 1.0.
struct A2CplusA2OneParams
{
    PipelineConstructionType constructionType;
    bool dynamicA2C;
    bool dynamicA2One;
    bool exportFragDepth;
    bool sampleShadingEnable;
};

void A2CplusA2OneSupport(Context &context, A2CplusA2OneParams params)
{
    const auto ctx = context.getContextCommonData();
    checkPipelineConstructionRequirements(ctx.vki, ctx.physicalDevice, params.constructionType);
    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_ALPHA_TO_ONE);

    if (params.sampleShadingEnable)
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SAMPLE_RATE_SHADING);

#ifndef CTS_USES_VULKANSC
    if (params.dynamicA2C || params.dynamicA2One)
    {
        context.requireDeviceFunctionality("VK_EXT_extended_dynamic_state3");
        const auto &eds3Features = context.getExtendedDynamicState3FeaturesEXT();

        if (params.dynamicA2C && !eds3Features.extendedDynamicState3AlphaToCoverageEnable)
            TCU_THROW(NotSupportedError, "extendedDynamicState3AlphaToCoverageEnable not supported");

        if (params.dynamicA2One && !eds3Features.extendedDynamicState3AlphaToOneEnable)
            TCU_THROW(NotSupportedError, "extendedDynamicState3AlphaToOneEnable not supported");
    }
#endif
}

void A2CplusA2OnePrograms(SourceCollections &programCollection, A2CplusA2OneParams params)
{
    std::ostringstream vert;
    vert << "#version 460\n"
         << "vec2 positions[3] = vec2[](\n"
         << "    vec2(-1.0, -1.0),\n"
         << "    vec2(-1.0, 3.0),\n"
         << "    vec2(3.0, -1.0)\n"
         << ");\n"
         << "void main (void) {\n"
         << "    gl_Position = vec4(positions[gl_VertexIndex % 3], 0.0, 1.0);\n"
         << "}\n";
    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

    // Using push constants so the pixel color is not a constant.
    std::ostringstream frag;
    frag << "#version 460\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "layout (push_constant, std430) uniform PCBlock { vec4 color; float depth; } pc;\n"
         << "void main(void) {\n"
         << "    outColor = pc.color;\n"
         << (params.exportFragDepth ? "    gl_FragDepth = pc.depth;\n" : "") << "}\n";
    programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

tcu::TestStatus A2CplusA2OneRun(Context &context, A2CplusA2OneParams params)
{
    const auto &ctx = context.getContextCommonData();
    const tcu::IVec3 fbExtent(1, 1, 1);
    const auto vkExtent     = makeExtent3D(fbExtent);
    const auto colorFormat  = VK_FORMAT_R8G8B8A8_UNORM;
    const auto depthFormat  = VK_FORMAT_D16_UNORM;
    const auto tcuFormat    = mapVkFormat(colorFormat);
    const auto colorUsage   = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    const auto depthUsage   = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    const auto resolveUsage = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);
    const tcu::Vec4 geomColor(1.0f, 1.0f, 1.0f, 0.0f); // Note geometry color has alpha 0.0.
    const float clearDepth = 1.0f;
    const float geomDepth  = 0.0f;
    const tcu::Vec4 threshold(0.0f, 0.0f, 0.0f, 0.0f); // When using 0 and 1 only, we expect exact results.
    const auto bindPoint     = VK_PIPELINE_BIND_POINT_GRAPHICS;
    const auto pcStages      = VK_SHADER_STAGE_FRAGMENT_BIT;
    const auto sampleCount   = VK_SAMPLE_COUNT_4_BIT;
    const auto imageType     = VK_IMAGE_TYPE_2D;
    const auto imageViewType = VK_IMAGE_VIEW_TYPE_2D;
    const auto colorSRR      = makeDefaultImageSubresourceRange();
    const auto depthSRR      = makeImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, 1u);

    // Color buffer and resolve attachment with verification buffer.
    const VkImageCreateInfo colorBufferInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        nullptr,
        0u,
        imageType,
        colorFormat,
        vkExtent,
        1u,
        1u,
        sampleCount,
        VK_IMAGE_TILING_OPTIMAL,
        colorUsage,
        VK_SHARING_MODE_EXCLUSIVE,
        0u,
        nullptr,
        VK_IMAGE_LAYOUT_UNDEFINED,
    };
    ImageWithMemory colorBuffer(ctx.vkd, ctx.device, ctx.allocator, colorBufferInfo, MemoryRequirement::Any);
    const auto colorView = makeImageView(ctx.vkd, ctx.device, *colorBuffer, imageViewType, colorFormat, colorSRR);

    std::unique_ptr<ImageWithMemory> depthBuffer;
    Move<VkImageView> depthView;
    if (params.exportFragDepth)
    {
        const VkImageCreateInfo depthBufferInfo = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            nullptr,
            0u,
            imageType,
            depthFormat,
            vkExtent,
            1u,
            1u,
            sampleCount,
            VK_IMAGE_TILING_OPTIMAL,
            depthUsage,
            VK_SHARING_MODE_EXCLUSIVE,
            0u,
            nullptr,
            VK_IMAGE_LAYOUT_UNDEFINED,
        };
        depthBuffer.reset(
            new ImageWithMemory(ctx.vkd, ctx.device, ctx.allocator, depthBufferInfo, MemoryRequirement::Any));
        depthView = makeImageView(ctx.vkd, ctx.device, depthBuffer->get(), imageViewType, depthFormat, depthSRR);
    }

    // Resolve buffer, single sample.
    ImageWithBuffer resolveBuffer(ctx.vkd, ctx.device, ctx.allocator, vkExtent, colorFormat, resolveUsage, imageType);

    // Push constants.
    struct PushConstantData
    {
        // This structure has to match the shader push constant declaration.
        tcu::Vec4 color;
        float depth;
    };
    const PushConstantData pcData{geomColor, geomDepth};

    const auto pcSize  = static_cast<uint32_t>(sizeof(pcData));
    const auto pcRange = makePushConstantRange(pcStages, 0u, pcSize);

    PipelineLayoutWrapper pipelineLayout(params.constructionType, ctx.vkd, ctx.device, VK_NULL_HANDLE, &pcRange);

    std::vector<VkAttachmentDescription> attDescs{
        makeAttachmentDescription(0u, colorFormat, sampleCount, VK_ATTACHMENT_LOAD_OP_CLEAR,
                                  VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                  VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED,
                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
        makeAttachmentDescription(0u, colorFormat, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                  VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                  VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED,
                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
    };

    std::vector<VkAttachmentReference> attRefs{
        makeAttachmentReference(0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
        makeAttachmentReference(1u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
    };

    if (params.exportFragDepth)
    {
        attDescs.push_back(makeAttachmentDescription(0u, depthFormat, sampleCount, VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                     VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                     VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED,
                                                     VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL));
        attRefs.push_back(makeAttachmentReference(2u, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL));
    }

    const auto subpass = makeSubpassDescription(0u, bindPoint, 0u, nullptr, 1u, &attRefs.at(0u), &attRefs.at(1u),
                                                (params.exportFragDepth ? &attRefs.at(2u) : nullptr), 0u, nullptr);

    const VkRenderPassCreateInfo rpInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        nullptr,
        0u,
        de::sizeU32(attDescs),
        de::dataOrNull(attDescs),
        1u,
        &subpass,
        0u,
        nullptr,
    };
    RenderPassWrapper renderPass(params.constructionType, ctx.vkd, ctx.device, &rpInfo);

    std::vector<VkImage> fbImages{*colorBuffer, resolveBuffer.getImage()};
    std::vector<VkImageView> fbViews{*colorView, resolveBuffer.getImageView()};
    if (params.exportFragDepth)
    {
        fbImages.push_back(depthBuffer->get());
        fbViews.push_back(*depthView);
    }
    DE_ASSERT(fbImages.size() == fbViews.size());
    renderPass.createFramebuffer(ctx.vkd, ctx.device, de::sizeU32(fbImages), de::dataOrNull(fbImages),
                                 de::dataOrNull(fbViews), vkExtent.width, vkExtent.height);

    // Modules.
    const auto &binaries = context.getBinaryCollection();
    const ShaderWrapper vertModule(ctx.vkd, ctx.device, binaries.get("vert"));
    const ShaderWrapper fragModule(ctx.vkd, ctx.device, binaries.get("frag"));

    const std::vector<VkViewport> viewports(1u, makeViewport(vkExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(vkExtent));

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = initVulkanStructureConst();

    const auto staticA2C   = (params.dynamicA2C ? VK_FALSE : VK_TRUE);
    const auto staticA2One = (params.dynamicA2One ? VK_FALSE : VK_TRUE);
    const auto staticSRSE  = (params.sampleShadingEnable ? VK_TRUE : VK_FALSE);

    const VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        nullptr,
        0u,
        sampleCount,
        staticSRSE,
        1.0f,
        nullptr,
        staticA2C,   // Alpha to coverage.
        staticA2One, // Alpha to one.
    };

    std::vector<VkDynamicState> dynamicStates;
#ifndef CTS_USES_VULKANSC
    if (params.dynamicA2C)
        dynamicStates.push_back(VK_DYNAMIC_STATE_ALPHA_TO_COVERAGE_ENABLE_EXT);
    if (params.dynamicA2One)
        dynamicStates.push_back(VK_DYNAMIC_STATE_ALPHA_TO_ONE_ENABLE_EXT);
#endif

    const VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        nullptr,
        0u,
        de::sizeU32(dynamicStates),
        de::dataOrNull(dynamicStates),
    };

    const auto stencilState =
        makeStencilOpState(VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_NEVER, 0u, 0u, 0u);

    const VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        nullptr,
        0u,
        VK_TRUE,
        VK_TRUE,
        VK_COMPARE_OP_LESS,
        VK_FALSE,
        VK_FALSE,
        stencilState,
        stencilState,
        0.0f,
        0.0f,
    };

    const auto dsPtr = (params.exportFragDepth ? &depthStencilStateCreateInfo : nullptr);

    GraphicsPipelineWrapper pipeline(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device, context.getDeviceExtensions(),
                                     params.constructionType);
    pipeline.setDynamicState(&dynamicStateCreateInfo)
        .setDefaultTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
        .setDefaultRasterizationState()
        .setDefaultDepthStencilState()
        .setDefaultColorBlendState()
        .setupVertexInputState(&vertexInputStateCreateInfo)
        .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, renderPass.get(), 0u, vertModule)
        .setupFragmentShaderState(pipelineLayout, renderPass.get(), 0u, fragModule, dsPtr, &multisampleStateCreateInfo)
        .setupFragmentOutputState(renderPass.get(), 0u, nullptr, &multisampleStateCreateInfo)
        .buildPipeline();

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    std::vector<VkClearValue> clearValues{makeClearValueColor(clearColor)};
    if (params.exportFragDepth)
    {
        // We need this extra clear color value because clear values are indexed by attachment index.
        clearValues.push_back(makeClearValueColor(clearColor));
        clearValues.push_back(makeClearValueDepthStencil(clearDepth, 0u));
    }
    renderPass.begin(ctx.vkd, cmdBuffer, scissors.front(), de::sizeU32(clearValues), de::dataOrNull(clearValues));
    pipeline.bind(cmdBuffer);
    ctx.vkd.cmdPushConstants(cmdBuffer, *pipelineLayout, pcStages, 0u, pcSize, &pcData);
#ifndef CTS_USES_VULKANSC
    if (params.dynamicA2C)
        ctx.vkd.cmdSetAlphaToCoverageEnableEXT(cmdBuffer, VK_TRUE);
    if (params.dynamicA2One)
        ctx.vkd.cmdSetAlphaToOneEnableEXT(cmdBuffer, VK_TRUE);
#endif
    ctx.vkd.cmdDraw(cmdBuffer, 3u, 1u, 0u, 0u);
    renderPass.end(ctx.vkd, cmdBuffer);
    copyImageToBuffer(ctx.vkd, cmdBuffer, resolveBuffer.getImage(), resolveBuffer.getBuffer(), fbExtent.swizzle(0, 1),
                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1u,
                      VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_ASPECT_COLOR_BIT,
                      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    // Verify color output.
    invalidateAlloc(ctx.vkd, ctx.device, resolveBuffer.getBufferAllocation());
    tcu::PixelBufferAccess resultAccess(tcuFormat, fbExtent, resolveBuffer.getBufferAllocation().getHostPtr());

    tcu::TextureLevel referenceLevel(tcuFormat, fbExtent.x(), fbExtent.y());
    auto referenceAccess = referenceLevel.getAccess();
    tcu::clear(referenceAccess, clearColor);

    auto &log = context.getTestContext().getLog();
    if (!tcu::floatThresholdCompare(log, "Result", "", referenceAccess, resultAccess, threshold,
                                    tcu::COMPARE_LOG_ON_ERROR))
        TCU_FAIL("Unexpected color in result buffer; check log for details");

    return tcu::TestStatus::pass("Pass");
}

// SampleMaskWithConservativeTest
SampleMaskWithConservativeTest::SampleMaskWithConservativeTest(
    tcu::TestContext &testContext, const std::string &name, const PipelineConstructionType pipelineConstructionType,
    const VkSampleCountFlagBits rasterizationSamples,
    const VkConservativeRasterizationModeEXT conservativeRasterizationMode, const bool enableMinSampleShading,
    const float minSampleShading, const bool enableSampleMask, const VkSampleMask sampleMask,
    const bool enablePostDepthCoverage, const bool useFragmentShadingRate)
    : vkt::TestCase(testContext, name)
    , m_pipelineConstructionType(pipelineConstructionType)
    , m_rasterizationSamples(rasterizationSamples)
    , m_enableMinSampleShading(enableMinSampleShading)
    , m_minSampleShading(minSampleShading)
    , m_enableSampleMask(enableSampleMask)
    , m_sampleMask(sampleMask)
    , m_conservativeRasterizationMode(conservativeRasterizationMode)
    , m_enablePostDepthCoverage(enablePostDepthCoverage)
    , m_renderType(RENDER_TYPE_RESOLVE)
    , m_useFragmentShadingRate(useFragmentShadingRate)
{
}

void SampleMaskWithConservativeTest::checkSupport(Context &context) const
{
    if (!context.getDeviceProperties().limits.standardSampleLocations)
        TCU_THROW(NotSupportedError, "standardSampleLocations required");

    if (m_useFragmentShadingRate && !checkFragmentShadingRateRequirements(context, m_rasterizationSamples))
        TCU_THROW(NotSupportedError, "Required FragmentShadingRate not supported");

    if (m_useFragmentShadingRate &&
        !context.getFragmentShadingRateProperties().fragmentShadingRateWithConservativeRasterization)
        TCU_THROW(NotSupportedError,
                  "fragmentShadingRateWithConservativeRasterization not supported with conservative rasterization");

    context.requireDeviceFunctionality("VK_EXT_conservative_rasterization");

    const auto &conservativeRasterizationProperties = context.getConservativeRasterizationPropertiesEXT();
    const uint32_t subPixelPrecisionBits            = context.getDeviceProperties().limits.subPixelPrecisionBits;
    const uint32_t subPixelPrecision                = (1 << subPixelPrecisionBits);
    const float primitiveOverestimationSizeMult =
        float(subPixelPrecision) * conservativeRasterizationProperties.primitiveOverestimationSize;

    DE_ASSERT(subPixelPrecisionBits < sizeof(uint32_t) * 8);

    if (m_enablePostDepthCoverage)
    {
        context.requireDeviceFunctionality("VK_EXT_post_depth_coverage");
        if (!conservativeRasterizationProperties.conservativeRasterizationPostDepthCoverage)
            TCU_THROW(NotSupportedError, "conservativeRasterizationPostDepthCoverage not supported");
    }

    context.getTestContext().getLog() << tcu::TestLog::Message << "maxExtraPrimitiveOverestimationSize="
                                      << conservativeRasterizationProperties.maxExtraPrimitiveOverestimationSize << '\n'
                                      << "extraPrimitiveOverestimationSizeGranularity="
                                      << conservativeRasterizationProperties.extraPrimitiveOverestimationSizeGranularity
                                      << '\n'
                                      << "degenerateTrianglesRasterized="
                                      << conservativeRasterizationProperties.degenerateTrianglesRasterized << '\n'
                                      << "primitiveOverestimationSize="
                                      << conservativeRasterizationProperties.primitiveOverestimationSize
                                      << " (==" << primitiveOverestimationSizeMult << '/' << subPixelPrecision << ")\n"
                                      << tcu::TestLog::EndMessage;

    if (m_conservativeRasterizationMode == VK_CONSERVATIVE_RASTERIZATION_MODE_OVERESTIMATE_EXT)
    {
        if (conservativeRasterizationProperties.extraPrimitiveOverestimationSizeGranularity >
            conservativeRasterizationProperties.maxExtraPrimitiveOverestimationSize)
            TCU_FAIL("Granularity cannot be greater than maximum extra size");
    }
    else if (m_conservativeRasterizationMode == VK_CONSERVATIVE_RASTERIZATION_MODE_UNDERESTIMATE_EXT)
    {
        if (conservativeRasterizationProperties.primitiveUnderestimation == false)
            TCU_THROW(NotSupportedError, "Underestimation is not supported");
    }
    else
        TCU_THROW(InternalError, "Non-conservative mode tests are not supported by this class");

    if (!conservativeRasterizationProperties.fullyCoveredFragmentShaderInputVariable)
    {
        TCU_THROW(NotSupportedError, "FullyCoveredEXT input variable is not supported");
    }

    checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                          m_pipelineConstructionType);
}

void SampleMaskWithConservativeTest::initPrograms(SourceCollections &programCollection) const
{
    {
        DE_ASSERT((int)m_rasterizationSamples <= 32);

        static const char *vertexSource = "#version 440\n"
                                          "layout(location = 0) in vec4 position;\n"
                                          "layout(location = 1) in vec4 color;\n"
                                          "layout(location = 0) out vec4 vtxColor;\n"
                                          "out gl_PerVertex\n"
                                          "{\n"
                                          "    vec4 gl_Position;\n"
                                          "};\n"
                                          "\n"
                                          "void main (void)\n"
                                          "{\n"
                                          "    gl_Position = position;\n"
                                          "    vtxColor = color;\n"
                                          "}\n";

        std::ostringstream fragmentSource;
        fragmentSource << "#version 440\n"
                       << (m_enablePostDepthCoverage ? "#extension GL_ARB_post_depth_coverage : require\n" : "")
                       << (m_conservativeRasterizationMode == VK_CONSERVATIVE_RASTERIZATION_MODE_UNDERESTIMATE_EXT ?
                               "#extension GL_NV_conservative_raster_underestimation : enable\n" :
                               "")
                       << "layout(early_fragment_tests) in;\n"
                       << (m_enablePostDepthCoverage ? "layout(post_depth_coverage) in;\n" : "")
                       << "layout(location = 0) in vec4 vtxColor;\n"
                          "layout(location = 0) out vec4 fragColor;\n"
                          "void main (void)\n"
                          "{\n";
        if (m_enableMinSampleShading)
        {
            fragmentSource << "    const int coveredSamples = bitCount(gl_SampleMaskIn[0]);\n"
                              "    fragColor = vtxColor * (1.0 / "
                           << (int32_t)m_rasterizationSamples << " * coveredSamples);\n";
        }
        else if (m_conservativeRasterizationMode == VK_CONSERVATIVE_RASTERIZATION_MODE_UNDERESTIMATE_EXT)
        {
            fragmentSource << "    fragColor = gl_FragFullyCoveredNV ? vtxColor : vec4(0.0f);\n";
        }
        else
        {
            fragmentSource << "    fragColor = vtxColor;\n";
        }
        fragmentSource << "}\n";

        programCollection.glslSources.add("color_vert") << glu::VertexSource(vertexSource);
        programCollection.glslSources.add("color_frag") << glu::FragmentSource(fragmentSource.str());
    }

    {
        static const char *vertexSource = "#version 440\n"
                                          "void main (void)\n"
                                          "{\n"
                                          "    const vec4 positions[4] = vec4[4](\n"
                                          "        vec4(-1.0, -1.0, 0.0, 1.0),\n"
                                          "        vec4(-1.0,  1.0, 0.0, 1.0),\n"
                                          "        vec4( 1.0, -1.0, 0.0, 1.0),\n"
                                          "        vec4( 1.0,  1.0, 0.0, 1.0)\n"
                                          "    );\n"
                                          "    gl_Position = positions[gl_VertexIndex];\n"
                                          "}\n";

        static const char *fragmentSource =
            "#version 440\n"
            "precision highp float;\n"
            "layout(location = 0) out highp vec4 fragColor;\n"
            "layout(set = 0, binding = 0, input_attachment_index = 0) uniform subpassInputMS imageMS;\n"
            "layout(push_constant) uniform PushConstantsBlock\n"
            "{\n"
            "    int sampleId;\n"
            "} pushConstants;\n"
            "void main (void)\n"
            "{\n"
            "    fragColor = subpassLoad(imageMS, pushConstants.sampleId);\n"
            "}\n";

        programCollection.glslSources.add("quad_vert") << glu::VertexSource(vertexSource);
        programCollection.glslSources.add("copy_sample_frag") << glu::FragmentSource(fragmentSource);
    }
}

TestInstance *SampleMaskWithConservativeTest::createInstance(Context &context) const
{
    return new SampleMaskWithConservativeInstance(
        context, m_pipelineConstructionType, m_rasterizationSamples, m_enableMinSampleShading, m_minSampleShading,
        m_enableSampleMask, m_sampleMask, m_conservativeRasterizationMode, m_enablePostDepthCoverage, true,
        m_renderType, m_useFragmentShadingRate);
}

// SampleMaskWithDepthTestTest
#ifndef CTS_USES_VULKANSC
SampleMaskWithDepthTestTest::SampleMaskWithDepthTestTest(tcu::TestContext &testContext, const std::string &name,
                                                         const PipelineConstructionType pipelineConstructionType,
                                                         const VkSampleCountFlagBits rasterizationSamples,
                                                         const bool enablePostDepthCoverage,
                                                         const bool useFragmentShadingRate)
    : vkt::TestCase(testContext, name)
    , m_pipelineConstructionType(pipelineConstructionType)
    , m_rasterizationSamples(rasterizationSamples)
    , m_enablePostDepthCoverage(enablePostDepthCoverage)
    , m_useFragmentShadingRate(useFragmentShadingRate)
{
}

void SampleMaskWithDepthTestTest::checkSupport(Context &context) const
{
    if (!context.getDeviceProperties().limits.standardSampleLocations)
        TCU_THROW(NotSupportedError, "standardSampleLocations required");

    context.requireDeviceFunctionality("VK_EXT_post_depth_coverage");

    checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                          m_pipelineConstructionType);

    if (m_useFragmentShadingRate)
    {
        if (!context.getFragmentShadingRateProperties().fragmentShadingRateWithShaderSampleMask)
            TCU_THROW(NotSupportedError, "fragmentShadingRateWithShaderSampleMask not supported");

        if (!checkFragmentShadingRateRequirements(context, m_rasterizationSamples))
            TCU_THROW(NotSupportedError, "Required FragmentShadingRate not supported");
    }
}

void SampleMaskWithDepthTestTest::initPrograms(SourceCollections &programCollection) const
{
    DE_ASSERT((int)m_rasterizationSamples <= 32);

    static const char *vertexSource = "#version 440\n"
                                      "layout(location = 0) in vec4 position;\n"
                                      "layout(location = 1) in vec4 color;\n"
                                      "layout(location = 0) out vec4 vtxColor;\n"
                                      "out gl_PerVertex\n"
                                      "{\n"
                                      "    vec4 gl_Position;\n"
                                      "};\n"
                                      "\n"
                                      "void main (void)\n"
                                      "{\n"
                                      "    gl_Position = position;\n"
                                      "    vtxColor = color;\n"
                                      "}\n";

    uint32_t samplesPerFragment = m_rasterizationSamples;
    if (m_useFragmentShadingRate)
    {
        // When FSR coverage is enabled the tests uses a pipeline FSR rate of {2,2},
        // which means each fragment shader invocation covers 4 pixels.
        samplesPerFragment *= 4;

        if (!m_enablePostDepthCoverage)
            // For the 4 specific pixels this tests verifies, the primitive
            // drawn by the test fully covers 3 of those pixels and
            // partially covers 1 of them. When the fragment shader executes
            // for those 4 pixels the non-PostDepthCoverage sample mask
            // (the sample mask before the depth test) will only have
            // 7/8 of the samples set since the last 1/8 is not even covered
            // by the primitive.
            samplesPerFragment -= m_rasterizationSamples / 2;
    }

    std::ostringstream fragmentSource;
    fragmentSource << "#version 440\n"
                   << (m_enablePostDepthCoverage ? "#extension GL_ARB_post_depth_coverage : require\n" : "")
                   << "layout(early_fragment_tests) in;\n"
                   << (m_enablePostDepthCoverage ? "layout(post_depth_coverage) in;\n" : "")
                   << "layout(location = 0) in vec4 vtxColor;\n"
                      "layout(location = 0) out vec4 fragColor;\n"
                      "void main (void)\n"
                      "{\n"
                      "    const int coveredSamples = bitCount(gl_SampleMaskIn[0]);\n"
                      "    fragColor = vtxColor * (1.0 / "
                   << samplesPerFragment
                   << " * coveredSamples);\n"
                      "}\n";

    programCollection.glslSources.add("color_vert") << glu::VertexSource(vertexSource);
    programCollection.glslSources.add("color_frag") << glu::FragmentSource(fragmentSource.str());
}

TestInstance *SampleMaskWithDepthTestTest::createInstance(Context &context) const
{
    return new SampleMaskWithDepthTestInstance(context, m_pipelineConstructionType, m_rasterizationSamples,
                                               m_enablePostDepthCoverage, m_useFragmentShadingRate);
}
#endif // CTS_USES_VULKANSC

CompatibleRenderPassTest::CompatibleRenderPassTest(tcu::TestContext &testContext, const std::string &name,
                                                   const PipelineConstructionType pipelineConstructionType,
                                                   bool dynamic)
    : vkt::TestCase(testContext, name)
    , m_pipelineConstructionType(pipelineConstructionType)
    , m_dynamic(dynamic)
{
}
void CompatibleRenderPassTest::initPrograms(SourceCollections &programCollection) const
{
    std::stringstream vert;
    std::stringstream frag;

    vert << "#version 450\n"
         << "void main() {\n"
         << "    vec2 pos = vec2(float(gl_VertexIndex & 1), float((gl_VertexIndex >> 1) & 1));\n"
         << "    gl_Position = vec4(pos * 2.0f - 1.0f, 0.0f, 1.0f);\n"
         << "}\n";

    frag << "#version 450\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "void main() {\n"
         << "    outColor = vec4(1.0f);\n"
         << "}\n";

    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());
    programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
}

TestInstance *CompatibleRenderPassTest::createInstance(Context &context) const
{
    return new CompatibleRenderPassTestInstance(context, m_pipelineConstructionType, m_dynamic);
}

void CompatibleRenderPassTest::checkSupport(Context &context) const
{
    checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                          m_pipelineConstructionType);
}

// RasterizationSamplesInstance

RasterizationSamplesInstance::RasterizationSamplesInstance(
    Context &context, PipelineConstructionType pipelineConstructionType, VkPrimitiveTopology topology, float pointSize,
    const std::vector<Vertex4RGBA> &vertices, const VkPipelineMultisampleStateCreateInfo &multisampleStateParams,
    const VkPipelineColorBlendAttachmentState &blendState, const TestModeFlags modeFlags, ImageBackingMode backingMode,
    const bool useFragmentShadingRate)
    : vkt::TestInstance(context)
    , m_colorFormat(VK_FORMAT_R8G8B8A8_UNORM)
    , m_renderSize(32, 32)
    , m_primitiveTopology(topology)
    , m_pointSize(pointSize)
    , m_vertices(vertices)
    , m_fullQuadVertices(generateVertices(GEOMETRY_TYPE_OPAQUE_QUAD_NONZERO_DEPTH))
    , m_modeFlags(modeFlags)
    , m_useFragmentShadingRate(useFragmentShadingRate)
{
    if (m_modeFlags != 0)
    {
        const bool useDepth               = (m_modeFlags & TEST_MODE_DEPTH_BIT) != 0;
        const bool useStencil             = (m_modeFlags & TEST_MODE_STENCIL_BIT) != 0;
        const VkFormat depthStencilFormat = findSupportedDepthStencilFormat(context, useDepth, useStencil);

        if (depthStencilFormat == VK_FORMAT_UNDEFINED)
            TCU_THROW(NotSupportedError, "Required depth/stencil format is not supported");

        const VkPrimitiveTopology pTopology[2]      = {m_primitiveTopology, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP};
        const std::vector<Vertex4RGBA> pVertices[2] = {m_vertices, m_fullQuadVertices};

        m_multisampleRenderer = de::MovePtr<MultisampleRenderer>(
            new MultisampleRenderer(context, pipelineConstructionType, m_colorFormat, depthStencilFormat, m_renderSize,
                                    useDepth, useStencil, 2u, pTopology, pVertices, multisampleStateParams, blendState,
                                    RENDER_TYPE_RESOLVE, backingMode, m_useFragmentShadingRate));
    }
    else
    {
        m_multisampleRenderer = de::MovePtr<MultisampleRenderer>(new MultisampleRenderer(
            context, pipelineConstructionType, m_colorFormat, m_renderSize, topology, vertices, multisampleStateParams,
            blendState, RENDER_TYPE_RESOLVE, backingMode, m_useFragmentShadingRate));
    }
}

tcu::TestStatus RasterizationSamplesInstance::iterate(void)
{
    de::MovePtr<tcu::TextureLevel> level(m_multisampleRenderer->render());
    return verifyImage(level->getAccess());
}

tcu::TestStatus RasterizationSamplesInstance::verifyImage(const tcu::ConstPixelBufferAccess &result)
{
    // Verify range of unique pixels
    {
        const uint32_t numUniqueColors = getUniqueColorsCount(result);
        const uint32_t minUniqueColors =
            (m_primitiveTopology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST && m_pointSize == 1.0f) ? 2 : 3;

        tcu::TestLog &log = m_context.getTestContext().getLog();

        log << tcu::TestLog::Message << "\nMin. unique colors expected: " << minUniqueColors << "\n"
            << "Unique colors found: " << numUniqueColors << "\n"
            << tcu::TestLog::EndMessage;

        if (numUniqueColors < minUniqueColors)
            return tcu::TestStatus::fail("Unique colors out of expected bounds");
    }

    // Verify shape of the rendered primitive (fuzzy-compare)
    {
        const tcu::TextureFormat tcuColorFormat = mapVkFormat(m_colorFormat);
        const tcu::TextureFormat tcuDepthFormat = tcu::TextureFormat();
        const ColorVertexShader vertexShader;
        const ColorFragmentShader fragmentShader(tcuColorFormat, tcuDepthFormat);
        const rr::Program program(&vertexShader, &fragmentShader);
        ReferenceRenderer refRenderer(m_renderSize.x(), m_renderSize.y(), 1, tcuColorFormat, tcuDepthFormat, &program);
        rr::RenderState renderState(refRenderer.getViewportState(),
                                    m_context.getDeviceProperties().limits.subPixelPrecisionBits);

        if (m_primitiveTopology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST)
        {
            VkPhysicalDeviceProperties deviceProperties;

            m_context.getInstanceInterface().getPhysicalDeviceProperties(m_context.getPhysicalDevice(),
                                                                         &deviceProperties);

            // gl_PointSize is clamped to pointSizeRange
            renderState.point.pointSize = deFloatMin(m_pointSize, deviceProperties.limits.pointSizeRange[1]);
        }

        if (m_modeFlags == 0)
        {
            refRenderer.colorClear(tcu::Vec4(0.0f));
            refRenderer.draw(renderState, mapVkPrimitiveTopology(m_primitiveTopology), m_vertices);
        }
        else
        {
            // For depth/stencil case the primitive is invisible and the surroundings are filled red.
            refRenderer.colorClear(tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f));
            refRenderer.draw(renderState, mapVkPrimitiveTopology(m_primitiveTopology), m_vertices);
        }

        if (!tcu::fuzzyCompare(m_context.getTestContext().getLog(), "FuzzyImageCompare", "Image comparison",
                               refRenderer.getAccess(), result, 0.05f, tcu::COMPARE_LOG_RESULT))
            return tcu::TestStatus::fail("Primitive has unexpected shape");
    }

    return tcu::TestStatus::pass("Primitive rendered, unique colors within expected bounds");
}

// MinSampleShadingInstance

MinSampleShadingInstance::MinSampleShadingInstance(Context &context,
                                                   const PipelineConstructionType pipelineConstructionType,
                                                   VkPrimitiveTopology topology, float pointSize,
                                                   const std::vector<Vertex4RGBA> &vertices,
                                                   const VkPipelineMultisampleStateCreateInfo &multisampleStateParams,
                                                   const VkPipelineColorBlendAttachmentState &colorBlendState,
                                                   ImageBackingMode backingMode, const bool useFragmentShadingRate)
    : vkt::TestInstance(context)
    , m_pipelineConstructionType(pipelineConstructionType)
    , m_colorFormat(VK_FORMAT_R8G8B8A8_UNORM)
    , m_renderSize(32, 32)
    , m_primitiveTopology(topology)
    , m_vertices(vertices)
    , m_multisampleStateParams(multisampleStateParams)
    , m_colorBlendState(colorBlendState)
    , m_backingMode(backingMode)
    , m_useFragmentShadingRate(useFragmentShadingRate)
{
    DE_UNREF(pointSize);
}

tcu::TestStatus MinSampleShadingInstance::iterate(void)
{
    de::MovePtr<tcu::TextureLevel> noSampleshadingImage;
    std::vector<tcu::TextureLevel> sampleShadedImages;

    // Render and resolve without sample shading
    {
        VkPipelineMultisampleStateCreateInfo multisampleStateParms = m_multisampleStateParams;
        multisampleStateParms.sampleShadingEnable                  = VK_FALSE;
        multisampleStateParms.minSampleShading                     = 0.0;

        MultisampleRenderer renderer(m_context, m_pipelineConstructionType, m_colorFormat, m_renderSize,
                                     m_primitiveTopology, m_vertices, multisampleStateParms, m_colorBlendState,
                                     RENDER_TYPE_RESOLVE, m_backingMode, m_useFragmentShadingRate);
        noSampleshadingImage = renderer.render();
    }

    // Render with test minSampleShading and collect per-sample images
    {
        MultisampleRenderer renderer(m_context, m_pipelineConstructionType, m_colorFormat, m_renderSize,
                                     m_primitiveTopology, m_vertices, m_multisampleStateParams, m_colorBlendState,
                                     RENDER_TYPE_COPY_SAMPLES, m_backingMode, m_useFragmentShadingRate);
        renderer.render();

        sampleShadedImages.resize(m_multisampleStateParams.rasterizationSamples);
        for (uint32_t sampleId = 0; sampleId < sampleShadedImages.size(); sampleId++)
        {
            sampleShadedImages[sampleId] = *renderer.getSingleSampledImage(sampleId);
        }
    }

    // Log images
    {
        tcu::TestLog &testLog = m_context.getTestContext().getLog();

        testLog << tcu::TestLog::ImageSet("Images", "Images")
                << tcu::TestLog::Image("noSampleshadingImage", "Image rendered without sample shading",
                                       noSampleshadingImage->getAccess());

        for (uint32_t sampleId = 0; sampleId < sampleShadedImages.size(); sampleId++)
        {
            testLog << tcu::TestLog::Image("sampleShadedImage", "One sample of sample shaded image",
                                           sampleShadedImages[sampleId].getAccess());
        }
        testLog << tcu::TestLog::EndImageSet;
    }

    return verifySampleShadedImage(sampleShadedImages, noSampleshadingImage->getAccess());
}

tcu::TestStatus MinSampleShadingInstance::verifySampleShadedImage(
    const std::vector<tcu::TextureLevel> &sampleShadedImages, const tcu::ConstPixelBufferAccess &noSampleshadingImage)
{
    const uint32_t pixelCount =
        noSampleshadingImage.getWidth() * noSampleshadingImage.getHeight() * noSampleshadingImage.getDepth();

    bool anyPixelCovered = false;

    for (uint32_t pixelNdx = 0; pixelNdx < pixelCount; pixelNdx++)
    {
        const uint32_t noSampleShadingValue = *((const uint32_t *)noSampleshadingImage.getDataPtr() + pixelNdx);

        if (noSampleShadingValue == 0)
        {
            // non-covered pixel, continue
            continue;
        }
        else
        {
            anyPixelCovered = true;
        }

        int numNotCoveredSamples = 0;

        std::map<uint32_t, uint32_t> histogram; // map<pixel value, number of occurrences>

        // Collect histogram of occurrences or each pixel across all samples
        for (size_t i = 0; i < sampleShadedImages.size(); ++i)
        {
            const uint32_t sampleShadedValue =
                *((const uint32_t *)sampleShadedImages[i].getAccess().getDataPtr() + pixelNdx);

            if (sampleShadedValue == 0)
            {
                numNotCoveredSamples++;
                continue;
            }

            if (histogram.find(sampleShadedValue) != histogram.end())
                histogram[sampleShadedValue]++;
            else
                histogram[sampleShadedValue] = 1;
        }

        if (numNotCoveredSamples == static_cast<int>(sampleShadedImages.size()))
        {
            return tcu::TestStatus::fail("Got uncovered pixel, where covered samples were expected");
        }

        const int uniqueColorsCount          = (int)histogram.size();
        const int expectedUniqueSamplesCount = static_cast<int>(
            m_multisampleStateParams.minSampleShading * static_cast<float>(sampleShadedImages.size()) + 0.5f);

        if (uniqueColorsCount + numNotCoveredSamples < expectedUniqueSamplesCount)
        {
            return tcu::TestStatus::fail("Got less unique colors than requested through minSampleShading");
        }
    }

    if (!anyPixelCovered)
    {
        return tcu::TestStatus::fail("Did not get any covered pixel, cannot test minSampleShading");
    }

    return tcu::TestStatus::pass("Got proper count of unique colors");
}

MinSampleShadingDisabledInstance::MinSampleShadingDisabledInstance(
    Context &context, const PipelineConstructionType pipelineConstructionType, VkPrimitiveTopology topology,
    float pointSize, const std::vector<Vertex4RGBA> &vertices,
    const VkPipelineMultisampleStateCreateInfo &multisampleStateParams,
    const VkPipelineColorBlendAttachmentState &blendState, ImageBackingMode backingMode,
    const bool useFragmentShadingRate)
    : MinSampleShadingInstance(context, pipelineConstructionType, topology, pointSize, vertices, multisampleStateParams,
                               blendState, backingMode, useFragmentShadingRate)
{
}

tcu::TestStatus MinSampleShadingDisabledInstance::verifySampleShadedImage(
    const std::vector<tcu::TextureLevel> &sampleShadedImages, const tcu::ConstPixelBufferAccess &noSampleshadingImage)
{
    const uint32_t samplesCount = (int)sampleShadedImages.size();
    const uint32_t width        = noSampleshadingImage.getWidth();
    const uint32_t height       = noSampleshadingImage.getHeight();
    const uint32_t depth        = noSampleshadingImage.getDepth();
    const tcu::UVec4 zeroPixel  = tcu::UVec4();
    bool anyPixelCovered        = false;

    DE_ASSERT(depth == 1);
    DE_UNREF(depth);

    for (uint32_t y = 0; y < height; ++y)
        for (uint32_t x = 0; x < width; ++x)
        {
            const tcu::UVec4 noSampleShadingValue = noSampleshadingImage.getPixelUint(x, y);

            if (noSampleShadingValue == zeroPixel)
                continue;

            anyPixelCovered               = true;
            tcu::UVec4 sampleShadingValue = tcu::UVec4();

            // Collect histogram of occurrences or each pixel across all samples
            for (size_t i = 0; i < samplesCount; ++i)
            {
                const tcu::UVec4 sampleShadedValue = sampleShadedImages[i].getAccess().getPixelUint(x, y);

                sampleShadingValue += sampleShadedValue;
            }

            sampleShadingValue = sampleShadingValue / samplesCount;

            if (sampleShadingValue.w() != 255)
            {
                return tcu::TestStatus::fail("Invalid Alpha channel value");
            }

            if (sampleShadingValue != noSampleShadingValue)
            {
                return tcu::TestStatus::fail("Invalid color");
            }
        }

    if (!anyPixelCovered)
    {
        return tcu::TestStatus::fail("Did not get any covered pixel, cannot test minSampleShadingDisabled");
    }

    return tcu::TestStatus::pass("Got proper count of unique colors");
}

SampleMaskInstance::SampleMaskInstance(Context &context, const PipelineConstructionType pipelineConstructionType,
                                       VkPrimitiveTopology topology, float pointSize,
                                       const std::vector<Vertex4RGBA> &vertices,
                                       const VkPipelineMultisampleStateCreateInfo &multisampleStateParams,
                                       const VkPipelineColorBlendAttachmentState &blendState,
                                       ImageBackingMode backingMode, const bool useFragmentShadingRate)
    : vkt::TestInstance(context)
    , m_pipelineConstructionType(pipelineConstructionType)
    , m_colorFormat(VK_FORMAT_R8G8B8A8_UNORM)
    , m_renderSize(32, 32)
    , m_primitiveTopology(topology)
    , m_vertices(vertices)
    , m_multisampleStateParams(multisampleStateParams)
    , m_colorBlendState(blendState)
    , m_backingMode(backingMode)
    , m_useFragmentShadingRate(useFragmentShadingRate)
{
    DE_UNREF(pointSize);
}

tcu::TestStatus SampleMaskInstance::iterate(void)
{
    de::MovePtr<tcu::TextureLevel> testSampleMaskImage;
    de::MovePtr<tcu::TextureLevel> minSampleMaskImage;
    de::MovePtr<tcu::TextureLevel> maxSampleMaskImage;

    // Render with test flags
    {
        MultisampleRenderer renderer(m_context, m_pipelineConstructionType, m_colorFormat, m_renderSize,
                                     m_primitiveTopology, m_vertices, m_multisampleStateParams, m_colorBlendState,
                                     RENDER_TYPE_RESOLVE, m_backingMode, m_useFragmentShadingRate);
        testSampleMaskImage = renderer.render();
    }

    // Render with all flags off
    {
        VkPipelineMultisampleStateCreateInfo multisampleParams = m_multisampleStateParams;
        const std::vector<VkSampleMask> sampleMask(multisampleParams.rasterizationSamples / 32, (VkSampleMask)0);

        multisampleParams.pSampleMask = sampleMask.data();

        MultisampleRenderer renderer(m_context, m_pipelineConstructionType, m_colorFormat, m_renderSize,
                                     m_primitiveTopology, m_vertices, multisampleParams, m_colorBlendState,
                                     RENDER_TYPE_RESOLVE, m_backingMode, m_useFragmentShadingRate);
        minSampleMaskImage = renderer.render();
    }

    // Render with all flags on
    {
        VkPipelineMultisampleStateCreateInfo multisampleParams = m_multisampleStateParams;
        const std::vector<VkSampleMask> sampleMask(multisampleParams.rasterizationSamples / 32, ~((VkSampleMask)0));

        multisampleParams.pSampleMask = sampleMask.data();

        MultisampleRenderer renderer(m_context, m_pipelineConstructionType, m_colorFormat, m_renderSize,
                                     m_primitiveTopology, m_vertices, multisampleParams, m_colorBlendState,
                                     RENDER_TYPE_RESOLVE, m_backingMode, m_useFragmentShadingRate);
        maxSampleMaskImage = renderer.render();
    }

    return verifyImage(testSampleMaskImage->getAccess(), minSampleMaskImage->getAccess(),
                       maxSampleMaskImage->getAccess());
}

tcu::TestStatus SampleMaskInstance::verifyImage(const tcu::ConstPixelBufferAccess &testSampleMaskImage,
                                                const tcu::ConstPixelBufferAccess &minSampleMaskImage,
                                                const tcu::ConstPixelBufferAccess &maxSampleMaskImage)
{
    const uint32_t testColorCount = getUniqueColorsCount(testSampleMaskImage);
    const uint32_t minColorCount  = getUniqueColorsCount(minSampleMaskImage);
    const uint32_t maxColorCount  = getUniqueColorsCount(maxSampleMaskImage);

    tcu::TestLog &log = m_context.getTestContext().getLog();

    log << tcu::TestLog::Message << "\nColors found: " << testColorCount << "\n"
        << "Min. colors expected: " << minColorCount << "\n"
        << "Max. colors expected: " << maxColorCount << "\n"
        << tcu::TestLog::EndMessage;

    if (minColorCount > testColorCount || testColorCount > maxColorCount)
        return tcu::TestStatus::fail("Unique colors out of expected bounds");
    else
        return tcu::TestStatus::pass("Unique colors within expected bounds");
}
#ifndef CTS_USES_VULKANSC
tcu::TestStatus testRasterSamplesConsistency(Context &context, MultisampleTestParams params)
{
    const VkSampleCountFlagBits samples[] = {VK_SAMPLE_COUNT_1_BIT, VK_SAMPLE_COUNT_2_BIT,  VK_SAMPLE_COUNT_4_BIT,
                                             VK_SAMPLE_COUNT_8_BIT, VK_SAMPLE_COUNT_16_BIT, VK_SAMPLE_COUNT_32_BIT,
                                             VK_SAMPLE_COUNT_64_BIT};

    const Vertex4RGBA vertexData[3] = {{tcu::Vec4(-0.75f, 0.0f, 0.0f, 1.0f), tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f)},
                                       {tcu::Vec4(0.75f, 0.125f, 0.0f, 1.0f), tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f)},
                                       {tcu::Vec4(0.75f, -0.125f, 0.0f, 1.0f), tcu::Vec4(1.0f, 0.0f, 0.0f, 1.0f)}};

    const std::vector<Vertex4RGBA> vertices(vertexData, vertexData + 3);
    uint32_t prevUniqueColors = 2;
    int renderCount           = 0;

    // Do not render with 1 sample (start with samplesNdx = 1).
    for (int samplesNdx = 1; samplesNdx < DE_LENGTH_OF_ARRAY(samples); samplesNdx++)
    {
        if (!isSupportedSampleCount(context.getInstanceInterface(), context.getPhysicalDevice(), samples[samplesNdx]))
            continue;

        if (params.useFragmentShadingRate && !checkFragmentShadingRateRequirements(context, samples[samplesNdx]))
            continue;

        const VkPipelineMultisampleStateCreateInfo multisampleStateParams{
            VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                  // const void* pNext;
            0u,                                                       // VkPipelineMultisampleStateCreateFlags flags;
            samples[samplesNdx],                                      // VkSampleCountFlagBits rasterizationSamples;
            false,                                                    // VkBool32 sampleShadingEnable;
            0.0f,                                                     // float minSampleShading;
            nullptr,                                                  // const VkSampleMask* pSampleMask;
            false,                                                    // VkBool32 alphaToCoverageEnable;
            false                                                     // VkBool32 alphaToOneEnable;
        };

        MultisampleRenderer renderer(context, params.pipelineConstructionType, VK_FORMAT_R8G8B8A8_UNORM,
                                     tcu::IVec2(32, 32), VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, vertices,
                                     multisampleStateParams, getDefaultColorBlendAttachmentState(), RENDER_TYPE_RESOLVE,
                                     params.backingMode, params.useFragmentShadingRate);
        de::MovePtr<tcu::TextureLevel> result = renderer.render();
        const uint32_t uniqueColors           = getUniqueColorsCount(result->getAccess());

        renderCount++;

        if (prevUniqueColors > uniqueColors)
        {
            std::ostringstream message;

            message << "More unique colors generated with " << samples[samplesNdx - 1] << " than with "
                    << samples[samplesNdx];
            return tcu::TestStatus::fail(message.str());
        }

        prevUniqueColors = uniqueColors;
    }

    if (renderCount == 0)
    {
        if (params.useFragmentShadingRate && !context.getFragmentShadingRateFeatures().pipelineFragmentShadingRate)
            TCU_THROW(NotSupportedError, "pipelineFragmentShadingRate is unsupported");
        TCU_THROW(NotSupportedError, "Multisampling is unsupported");
    }

    return tcu::TestStatus::pass("Number of unique colors increases as the sample count increases");
}
#endif // CTS_USES_VULKANSC

// AlphaToOneInstance

AlphaToOneInstance::AlphaToOneInstance(Context &context, const PipelineConstructionType pipelineConstructionType,
                                       VkPrimitiveTopology topology, const std::vector<Vertex4RGBA> &vertices,
                                       const VkPipelineMultisampleStateCreateInfo &multisampleStateParams,
                                       const VkPipelineColorBlendAttachmentState &blendState,
                                       ImageBackingMode backingMode, const bool useFragmentShadingRate)
    : vkt::TestInstance(context)
    , m_pipelineConstructionType(pipelineConstructionType)
    , m_colorFormat(VK_FORMAT_R8G8B8A8_UNORM)
    , m_renderSize(32, 32)
    , m_primitiveTopology(topology)
    , m_vertices(vertices)
    , m_multisampleStateParams(multisampleStateParams)
    , m_colorBlendState(blendState)
    , m_backingMode(backingMode)
    , m_useFragmentShadingRate(useFragmentShadingRate)
{
}

tcu::TestStatus AlphaToOneInstance::iterate(void)
{
    DE_ASSERT(m_multisampleStateParams.alphaToOneEnable);
    DE_ASSERT(m_colorBlendState.blendEnable);

    de::MovePtr<tcu::TextureLevel> alphaOneImage;
    de::MovePtr<tcu::TextureLevel> noAlphaOneImage;

    RenderType renderType = m_multisampleStateParams.rasterizationSamples == vk::VK_SAMPLE_COUNT_1_BIT ?
                                RENDER_TYPE_SINGLE_SAMPLE :
                                RENDER_TYPE_RESOLVE;

    // Render with blend enabled and alpha to one on
    {
        MultisampleRenderer renderer(m_context, m_pipelineConstructionType, m_colorFormat, m_renderSize,
                                     m_primitiveTopology, m_vertices, m_multisampleStateParams, m_colorBlendState,
                                     renderType, m_backingMode, m_useFragmentShadingRate);
        alphaOneImage = renderer.render();
    }

    // Render with blend enabled and alpha to one off
    {
        VkPipelineMultisampleStateCreateInfo multisampleParams = m_multisampleStateParams;
        multisampleParams.alphaToOneEnable                     = false;

        MultisampleRenderer renderer(m_context, m_pipelineConstructionType, m_colorFormat, m_renderSize,
                                     m_primitiveTopology, m_vertices, multisampleParams, m_colorBlendState, renderType,
                                     m_backingMode, m_useFragmentShadingRate);
        noAlphaOneImage = renderer.render();
    }

    return verifyImage(alphaOneImage->getAccess(), noAlphaOneImage->getAccess());
}

tcu::TestStatus AlphaToOneInstance::verifyImage(const tcu::ConstPixelBufferAccess &alphaOneImage,
                                                const tcu::ConstPixelBufferAccess &noAlphaOneImage)
{
    for (int y = 0; y < m_renderSize.y(); y++)
    {
        for (int x = 0; x < m_renderSize.x(); x++)
        {
            if (alphaOneImage.getPixel(x, y).w() != 1.0)
            {
                std::ostringstream message;
                message << "Unsatisfied condition: " << alphaOneImage.getPixel(x, y) << " doesn't have alpha set to 1";
                return tcu::TestStatus::fail(message.str());
            }

            if (!tcu::boolAll(tcu::greaterThanEqual(alphaOneImage.getPixel(x, y), noAlphaOneImage.getPixel(x, y))))
            {
                std::ostringstream message;
                message << "Unsatisfied condition: " << alphaOneImage.getPixel(x, y)
                        << " >= " << noAlphaOneImage.getPixel(x, y);
                return tcu::TestStatus::fail(message.str());
            }
        }
    }

    return tcu::TestStatus::pass(
        "Image rendered with alpha-to-one contains pixels of image rendered with no alpha-to-one");
}

// AlphaToCoverageInstance

AlphaToCoverageInstance::AlphaToCoverageInstance(Context &context,
                                                 const PipelineConstructionType pipelineConstructionType,
                                                 VkPrimitiveTopology topology, const std::vector<Vertex4RGBA> &vertices,
                                                 const VkPipelineMultisampleStateCreateInfo &multisampleStateParams,
                                                 const VkPipelineColorBlendAttachmentState &blendState,
                                                 GeometryType geometryType, ImageBackingMode backingMode,
                                                 const bool useFragmentShadingRate, const bool checkDepthBuffer)
    : vkt::TestInstance(context)
    , m_pipelineConstructionType(pipelineConstructionType)
    , m_colorFormat(VK_FORMAT_R8G8B8A8_UNORM)
    , m_depthStencilFormat(VK_FORMAT_D16_UNORM)
    , m_renderSize(32, 32)
    , m_primitiveTopology(topology)
    , m_vertices(vertices)
    , m_multisampleStateParams(multisampleStateParams)
    , m_colorBlendState(blendState)
    , m_geometryType(geometryType)
    , m_backingMode(backingMode)
    , m_useFragmentShadingRate(useFragmentShadingRate)
    , m_checkDepthBuffer(checkDepthBuffer)
{
}

tcu::TestStatus AlphaToCoverageInstance::iterate(void)
{
    DE_ASSERT(m_multisampleStateParams.alphaToCoverageEnable);

    de::MovePtr<tcu::TextureLevel> result;
    MultisampleRenderer renderer(m_context, m_pipelineConstructionType, m_colorFormat, m_depthStencilFormat,
                                 m_renderSize, m_checkDepthBuffer, false, 1u, &m_primitiveTopology, &m_vertices,
                                 m_multisampleStateParams, m_colorBlendState, RENDER_TYPE_RESOLVE, m_backingMode,
                                 m_useFragmentShadingRate);

    result = renderer.render();

    const auto colorStatus = verifyImage(result->getAccess());
    auto depthStatus       = tcu::TestStatus::pass("Pass");

    if (m_checkDepthBuffer)
    {
        const auto redrawResult = renderer.renderReusingDepth();
        depthStatus             = verifyDepthBufferCheck(redrawResult->getAccess());
    }

    if (colorStatus.getCode() == QP_TEST_RESULT_FAIL)
        return colorStatus;

    if (depthStatus.getCode() == QP_TEST_RESULT_FAIL)
        return depthStatus;

    return colorStatus;
}

tcu::TestStatus AlphaToCoverageInstance::verifyDepthBufferCheck(const tcu::ConstPixelBufferAccess &result)
{
    const tcu::Vec4 refColor(0.0f, 0.0f, 1.0f, 1.0f); // Must match "checkDepth-vert".
    const tcu::Vec4 threshold(0.0f, 0.0f, 0.0f, 0.0f);

    if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "BlueColor", "", refColor, result, threshold,
                                    tcu::COMPARE_LOG_ON_ERROR))
        return tcu::TestStatus::fail("Depth buffer verification failed: depth buffer was not clear");
    return tcu::TestStatus::pass("Pass");
}

tcu::TestStatus AlphaToCoverageInstance::verifyImage(const tcu::ConstPixelBufferAccess &result)
{
    float maxColorValue;
    float minColorValue;

    switch (m_geometryType)
    {
    case GEOMETRY_TYPE_OPAQUE_QUAD:
        maxColorValue = 1.01f;
        minColorValue = 0.99f;
        break;

    case GEOMETRY_TYPE_TRANSLUCENT_QUAD:
        maxColorValue = 0.52f;
        minColorValue = 0.0f;
        break;

    case GEOMETRY_TYPE_INVISIBLE_QUAD:
        maxColorValue = 0.01f;
        minColorValue = 0.0f;
        break;

    default:
        maxColorValue = 0.0f;
        minColorValue = 0.0f;
        DE_ASSERT(false);
    }

    auto &log = m_context.getTestContext().getLog();
    log << tcu::TestLog::Image("Result", "Result Image", result);

    for (int y = 0; y < m_renderSize.y(); y++)
    {
        for (int x = 0; x < m_renderSize.x(); x++)
        {
            const auto pixel = result.getPixel(x, y);
            const auto red   = pixel.x();

            if (red > maxColorValue || red < minColorValue)
            {
                std::ostringstream message;
                message << "Pixel is not in the expected range: " << red << " not in [" << minColorValue << ", "
                        << maxColorValue << "]";
                return tcu::TestStatus::fail(message.str());
            }
        }
    }

    return tcu::TestStatus::pass("Image matches reference value");
}

// AlphaToCoverageNoColorAttachmentInstance

AlphaToCoverageNoColorAttachmentInstance::AlphaToCoverageNoColorAttachmentInstance(
    Context &context, const PipelineConstructionType pipelineConstructionType, VkPrimitiveTopology topology,
    const std::vector<Vertex4RGBA> &vertices, const VkPipelineMultisampleStateCreateInfo &multisampleStateParams,
    const VkPipelineColorBlendAttachmentState &blendState, GeometryType geometryType, ImageBackingMode backingMode,
    const bool useFragmentShadingRate)
    : vkt::TestInstance(context)
    , m_pipelineConstructionType(pipelineConstructionType)
    , m_colorFormat(VK_FORMAT_R8G8B8A8_UNORM)
    , m_depthStencilFormat(VK_FORMAT_D16_UNORM)
    , m_renderSize(32, 32)
    , m_primitiveTopology(topology)
    , m_vertices(vertices)
    , m_multisampleStateParams(multisampleStateParams)
    , m_colorBlendState(blendState)
    , m_geometryType(geometryType)
    , m_backingMode(backingMode)
    , m_useFragmentShadingRate(useFragmentShadingRate)
{
}

tcu::TestStatus AlphaToCoverageNoColorAttachmentInstance::iterate(void)
{
    DE_ASSERT(m_multisampleStateParams.alphaToCoverageEnable);

    de::MovePtr<tcu::TextureLevel> result;
    MultisampleRenderer renderer(m_context, m_pipelineConstructionType, m_colorFormat, m_depthStencilFormat,
                                 m_renderSize, true, false, 1u, &m_primitiveTopology, &m_vertices,
                                 m_multisampleStateParams, m_colorBlendState, RENDER_TYPE_DEPTHSTENCIL_ONLY,
                                 m_backingMode, m_useFragmentShadingRate, 1.0f);

    result = renderer.render();

    return verifyImage(result->getAccess());
}

tcu::TestStatus AlphaToCoverageNoColorAttachmentInstance::verifyImage(const tcu::ConstPixelBufferAccess &result)
{
    for (int y = 0; y < m_renderSize.y(); y++)
    {
        for (int x = 0; x < m_renderSize.x(); x++)
        {
            // Expect full red for each pixel. Fail if clear color is showing.
            if (result.getPixel(x, y).x() < 1.0f)
            {
                // Log result image when failing.
                m_context.getTestContext().getLog()
                    << tcu::TestLog::ImageSet("Result", "Result image")
                    << tcu::TestLog::Image("Rendered", "Rendered image", result) << tcu::TestLog::EndImageSet;

                return tcu::TestStatus::fail("Fail");
            }
        }
    }

    return tcu::TestStatus::pass("Pass");
}

// AlphaToCoverageColorUnusedAttachmentInstance

AlphaToCoverageColorUnusedAttachmentInstance::AlphaToCoverageColorUnusedAttachmentInstance(
    Context &context, const PipelineConstructionType pipelineConstructionType, VkPrimitiveTopology topology,
    const std::vector<Vertex4RGBA> &vertices, const VkPipelineMultisampleStateCreateInfo &multisampleStateParams,
    const VkPipelineColorBlendAttachmentState &blendState, GeometryType geometryType, ImageBackingMode backingMode,
    const bool useFragmentShadingRate)
    : vkt::TestInstance(context)
    , m_pipelineConstructionType(pipelineConstructionType)
    , m_colorFormat(VK_FORMAT_R5G6B5_UNORM_PACK16)
    , m_renderSize(32, 32)
    , m_primitiveTopology(topology)
    , m_vertices(vertices)
    , m_multisampleStateParams(multisampleStateParams)
    , m_colorBlendState(blendState)
    , m_geometryType(geometryType)
    , m_backingMode(backingMode)
    , m_useFragmentShadingRate(useFragmentShadingRate)
{
}

tcu::TestStatus AlphaToCoverageColorUnusedAttachmentInstance::iterate(void)
{
    DE_ASSERT(m_multisampleStateParams.alphaToCoverageEnable);

    de::MovePtr<tcu::TextureLevel> result;
    MultisampleRenderer renderer(m_context, m_pipelineConstructionType, m_colorFormat, m_renderSize,
                                 m_primitiveTopology, m_vertices, m_multisampleStateParams, m_colorBlendState,
                                 RENDER_TYPE_UNUSED_ATTACHMENT, m_backingMode, m_useFragmentShadingRate);

    result = renderer.render();

    return verifyImage(result->getAccess());
}

tcu::TestStatus AlphaToCoverageColorUnusedAttachmentInstance::verifyImage(const tcu::ConstPixelBufferAccess &result)
{
    for (int y = 0; y < m_renderSize.y(); y++)
    {
        for (int x = 0; x < m_renderSize.x(); x++)
        {
            // Quad color gets written to color buffer at location 1, and the alpha value to location 0 which is unused.
            // The coverage should still be affected by the alpha written to location 0.
            if ((m_geometryType == GEOMETRY_TYPE_OPAQUE_QUAD && result.getPixel(x, y).x() < 1.0f) ||
                (m_geometryType == GEOMETRY_TYPE_INVISIBLE_QUAD && result.getPixel(x, y).x() > 0.0f))
            {
                // Log result image when failing.
                m_context.getTestContext().getLog()
                    << tcu::TestLog::ImageSet("Result", "Result image")
                    << tcu::TestLog::Image("Rendered", "Rendered image", result) << tcu::TestLog::EndImageSet;

                return tcu::TestStatus::fail("Fail");
            }
        }
    }

    return tcu::TestStatus::pass("Pass");
}

// SampleMaskWithConservativeInstance

SampleMaskWithConservativeInstance::SampleMaskWithConservativeInstance(
    Context &context, const PipelineConstructionType pipelineConstructionType,
    const VkSampleCountFlagBits rasterizationSamples, const bool enableMinSampleShading, const float minSampleShading,
    const bool enableSampleMask, const VkSampleMask sampleMask,
    const VkConservativeRasterizationModeEXT conservativeRasterizationMode, const bool enablePostDepthCoverage,
    const bool enableFullyCoveredEXT, const RenderType renderType, const bool useFragmentShadingRate)
    : vkt::TestInstance(context)
    , m_pipelineConstructionType(pipelineConstructionType)
    , m_rasterizationSamples(rasterizationSamples)
    , m_enablePostDepthCoverage(enablePostDepthCoverage)
    , m_enableFullyCoveredEXT(enableFullyCoveredEXT)
    , m_colorFormat(VK_FORMAT_R8G8B8A8_UNORM)
    , m_depthStencilFormat(VK_FORMAT_D16_UNORM)
    , m_renderSize(tcu::IVec2(10, 10))
    , m_useDepth(true)
    , m_useStencil(false)
    , m_useConservative(true)
    , m_useFragmentShadingRate(useFragmentShadingRate)
    , m_conservativeRasterizationMode(conservativeRasterizationMode)
    , m_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
    , m_renderColor(tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f))
    , m_depthClearValue(0.5f)
    , m_vertices(generateVertices())
    , m_enableSampleMask(enableSampleMask)
    , m_sampleMask(std::vector<VkSampleMask>{sampleMask})
    , m_enableMinSampleShading(enableMinSampleShading)
    , m_minSampleShading(minSampleShading)
    , m_multisampleStateParams(
          getMultisampleState(rasterizationSamples, enableMinSampleShading, minSampleShading, enableSampleMask))
    , m_rasterizationConservativeStateCreateInfo(
          getRasterizationConservativeStateCreateInfo(conservativeRasterizationMode))
    , m_blendState(getDefaultColorBlendAttachmentState())
    , m_renderType(renderType)
    , m_imageBackingMode(IMAGE_BACKING_MODE_REGULAR)
{
}

tcu::TestStatus SampleMaskWithConservativeInstance::iterate(void)
{

    de::MovePtr<tcu::TextureLevel> noSampleshadingImage;
    std::vector<tcu::TextureLevel> sampleShadedImages;

    {
        MultisampleRenderer renderer(m_context, m_pipelineConstructionType, m_colorFormat, m_depthStencilFormat,
                                     m_renderSize, m_useDepth, m_useStencil, m_useConservative,
                                     m_useFragmentShadingRate, 1u, &m_topology, &m_vertices, m_multisampleStateParams,
                                     m_blendState, m_rasterizationConservativeStateCreateInfo, RENDER_TYPE_RESOLVE,
                                     m_imageBackingMode, m_depthClearValue);
        noSampleshadingImage = renderer.render();
    }

    {
        const VkPipelineColorBlendAttachmentState colorBlendState = {
            false,                                                // VkBool32 blendEnable;
            VK_BLEND_FACTOR_ONE,                                  // VkBlendFactor srcColorBlendFactor;
            VK_BLEND_FACTOR_ZERO,                                 // VkBlendFactor dstColorBlendFactor;
            VK_BLEND_OP_ADD,                                      // VkBlendOp colorBlendOp;
            VK_BLEND_FACTOR_ONE,                                  // VkBlendFactor srcAlphaBlendFactor;
            VK_BLEND_FACTOR_ZERO,                                 // VkBlendFactor dstAlphaBlendFactor;
            VK_BLEND_OP_ADD,                                      // VkBlendOp alphaBlendOp;
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | // VkColorComponentFlags colorWriteMask;
                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};

        MultisampleRenderer mRenderer(m_context, m_pipelineConstructionType, m_colorFormat, m_renderSize,
                                      VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, m_vertices, m_multisampleStateParams,
                                      colorBlendState, RENDER_TYPE_COPY_SAMPLES, IMAGE_BACKING_MODE_REGULAR,
                                      m_useFragmentShadingRate);
        mRenderer.render();

        sampleShadedImages.resize(m_multisampleStateParams.rasterizationSamples);
        for (uint32_t sampleId = 0; sampleId < sampleShadedImages.size(); sampleId++)
        {
            sampleShadedImages[sampleId] = *mRenderer.getSingleSampledImage(sampleId);
        }
    }

    return verifyImage(sampleShadedImages, noSampleshadingImage->getAccess());
}

VkPipelineMultisampleStateCreateInfo SampleMaskWithConservativeInstance::getMultisampleState(
    const VkSampleCountFlagBits rasterizationSamples, const bool enableMinSampleShading, const float minSampleShading,
    const bool enableSampleMask)
{
    const VkPipelineMultisampleStateCreateInfo multisampleStateParams = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                  // const void* pNext;
        0u,                                                       // VkPipelineMultisampleStateCreateFlags flags;
        rasterizationSamples,                                     // VkSampleCountFlagBits rasterizationSamples;
        enableMinSampleShading ? VK_TRUE : VK_FALSE,              // VkBool32 sampleShadingEnable;
        enableMinSampleShading ? minSampleShading : 0.0f,         // float minSampleShading;
        enableSampleMask ? m_sampleMask.data() : nullptr,         // const VkSampleMask* pSampleMask;
        false,                                                    // VkBool32 alphaToCoverageEnable;
        false                                                     // VkBool32 alphaToOneEnable;
    };

    return multisampleStateParams;
}

VkPipelineRasterizationConservativeStateCreateInfoEXT SampleMaskWithConservativeInstance::
    getRasterizationConservativeStateCreateInfo(const VkConservativeRasterizationModeEXT conservativeRasterizationMode)
{
    const VkPipelineRasterizationConservativeStateCreateInfoEXT rasterizationConservativeStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT, //  VkStructureType sType;
        nullptr,                                                                     //  const void* pNext;
        (VkPipelineRasterizationConservativeStateCreateFlagsEXT)0, //  VkPipelineRasterizationConservativeStateCreateFlagsEXT flags;
        conservativeRasterizationMode, //  VkConservativeRasterizationModeEXT conservativeRasterizationMode;
        0.0f                           //  float extraPrimitiveOverestimationSize;
    };

    return rasterizationConservativeStateCreateInfo;
}

std::vector<Vertex4RGBA> SampleMaskWithConservativeInstance::generateVertices(void)
{
    std::vector<Vertex4RGBA> vertices;

    {
        const Vertex4RGBA vertexInput = {tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f), m_renderColor};
        vertices.push_back(vertexInput);
    }
    {
        const Vertex4RGBA vertexInput = {tcu::Vec4(1.0f, -1.0f, 1.0f, 1.0f), m_renderColor};
        vertices.push_back(vertexInput);
    }
    {
        const Vertex4RGBA vertexInput = {tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f), m_renderColor};
        vertices.push_back(vertexInput);
    }

    return vertices;
}

tcu::TestStatus SampleMaskWithConservativeInstance::verifyImage(
    const std::vector<tcu::TextureLevel> &sampleShadedImages, const tcu::ConstPixelBufferAccess &result)
{
    bool pass         = true;
    const int width   = result.getWidth();
    const int height  = result.getHeight();
    tcu::TestLog &log = m_context.getTestContext().getLog();

    const uint32_t samplesCount = (int)sampleShadedImages.size();

    for (size_t i = 0; i < samplesCount; ++i)
    {
        const tcu::ConstPixelBufferAccess &s = sampleShadedImages[i].getAccess();

        log << tcu::TestLog::ImageSet("Per sample image", "Per sampe image") << tcu::TestLog::Image("Layer", "Layer", s)
            << tcu::TestLog::EndImageSet;
    }

    // Leave sample count intact (return 1) if multiplication by minSampleShading won't exceed base 2
    // otherwise round up to the nearest power of 2
    auto sampleCountDivider = [](float x)
    {
        float power = 1.0;
        while (power < x)
        {
            power *= 2;
        }
        return power;
    };

    DE_ASSERT(width == 10);
    DE_ASSERT(height == 10);

    const tcu::Vec4 clearColor = tcu::Vec4(0.0f);
    std::vector<std::pair<int, int>> fullyCoveredPixelsCoordinateSet;

    // Generating set of pixel coordinate values covered by the triangle
    if (m_conservativeRasterizationMode == VK_CONSERVATIVE_RASTERIZATION_MODE_OVERESTIMATE_EXT)
    {
        for (int i = 0; i < width; i++)
        {
            for (int j = 0; j < height; j++)
            {
                // Rasterization will cover half of the triangle plus 1 pixel edge due to the overeestimation
                if (i < 5 && i + j < 11)
                    fullyCoveredPixelsCoordinateSet.push_back(std::make_pair(i, j));
            }
        }
    }
    else
    {
        if (m_useFragmentShadingRate && !m_enableMinSampleShading)
        {
            // When m_enableMinSampleShading is not enabled shader uses gl_FragFullyCoveredNV.
            // Additionaly when FSR coverage is enabled the tests uses a pipeline FSR rate of { 2,2 }
            // and as a result rasterization will cover only four pixels due to the underestimation.
            for (int i = 2; i < 4; i++)
                for (int j = 2; j < 4; j++)
                    fullyCoveredPixelsCoordinateSet.push_back(std::make_pair(i, j));
        }
        else
        {
            for (int i = 1; i < width; i++)
            {
                for (int j = 1; j < height; j++)
                {
                    // Rasterization will cover half of the triangle minus 1 pixel edge due to the underestimation
                    if (i < 5 && i + j < 8)
                        fullyCoveredPixelsCoordinateSet.push_back(std::make_pair(i, j));
                }
            }
        }
    }

    for (int x = 0; x < width; ++x)
        for (int y = 0; y < height; ++y)
        {
            const tcu::Vec4 resultPixel = result.getPixel(x, y);

            if (std::find(fullyCoveredPixelsCoordinateSet.begin(), fullyCoveredPixelsCoordinateSet.end(),
                          std::make_pair(x, y)) != fullyCoveredPixelsCoordinateSet.end())
            {
                if (m_enableMinSampleShading)
                {
                    tcu::UVec4 sampleShadingValue = tcu::UVec4();
                    for (size_t i = 0; i < samplesCount; ++i)
                    {
                        const tcu::UVec4 sampleShadedValue = sampleShadedImages[i].getAccess().getPixelUint(x, y);

                        sampleShadingValue += sampleShadedValue;
                    }

                    //Calculate coverage of a single sample Image based on accumulated value from the whole set
                    int sampleCoverageValue = sampleShadingValue.w() / samplesCount;
                    //Calculates an estimated coverage value based on the number of samples and the minimumSampleShading
                    int expectedCovergaveValue =
                        (int)(255.0 / sampleCountDivider((float)m_rasterizationSamples * m_minSampleShading)) + 1;

                    //The specification allows for larger sample count than minimum value, however resulted coverage should never be lower than minimum
                    if (sampleCoverageValue > expectedCovergaveValue)
                    {
                        log << tcu::TestLog::Message << "Coverage value " << sampleCoverageValue
                            << " greather than expected: " << expectedCovergaveValue << tcu::TestLog::EndMessage;

                        pass = false;
                    }
                }
                else if (m_enableSampleMask)
                {
                    // Sample mask with all bits on will not affect fragment coverage
                    if (m_sampleMask[0] == 0xFFFFFFFF)
                    {
                        if (resultPixel != m_renderColor)
                        {
                            log << tcu::TestLog::Message << "x: " << x << " y: " << y << " Result: " << resultPixel
                                << " Reference: " << m_renderColor << tcu::TestLog::EndMessage;

                            pass = false;
                        }
                    }
                    // Sample mask with half bits off will reduce sample coverage by half
                    else if (m_sampleMask[0] == 0xAAAAAAAA)
                    {

                        const tcu::Vec4 renderColorHalfOpacity(0.0f, 0.5f, 0.0f, 0.5f);
                        const float threshold = 0.02f;

                        for (uint32_t componentNdx = 0u; componentNdx < m_renderColor.SIZE; ++componentNdx)
                        {
                            if ((renderColorHalfOpacity[componentNdx] != 0.0f &&
                                 resultPixel[componentNdx] <= (renderColorHalfOpacity[componentNdx] - threshold)) ||
                                resultPixel[componentNdx] >= (renderColorHalfOpacity[componentNdx] + threshold))
                            {
                                log << tcu::TestLog::Message << "x: " << x << " y: " << y << " Result: " << resultPixel
                                    << " Reference: " << renderColorHalfOpacity << " +/- " << threshold
                                    << tcu::TestLog::EndMessage;

                                pass = false;
                            }
                        }
                    }
                    // Sample mask with all bits off will cause all fragment to failed opacity test
                    else if (m_sampleMask[0] == 0x00000000)
                    {
                        if (resultPixel != clearColor)
                        {
                            log << tcu::TestLog::Message << "x: " << x << " y: " << y << " Result: " << resultPixel
                                << " Reference: " << clearColor << tcu::TestLog::EndMessage;

                            pass = false;
                        }
                    }
                    else
                    {
                        log << tcu::TestLog::Message << "Unexpected sample mask value" << tcu::TestLog::EndMessage;

                        pass = false;
                    }
                }
                else
                {
                    if (resultPixel != m_renderColor)
                    {
                        log << tcu::TestLog::Message << "x: " << x << " y: " << y << " Result: " << resultPixel
                            << " Reference: " << m_renderColor << tcu::TestLog::EndMessage;

                        pass = false;
                    }
                }
            }
            else
            {
                if (resultPixel != clearColor)
                {
                    log << tcu::TestLog::Message << "x: " << x << " y: " << y << " Result: " << resultPixel
                        << " Reference: " << clearColor << tcu::TestLog::EndMessage;

                    pass = false;
                }
            }
        }

    if (pass)
        return tcu::TestStatus::pass("Passed");
    else
    {
        log << tcu::TestLog::ImageSet("LayerContent", "Layer content") << tcu::TestLog::Image("Layer", "Layer", result)
            << tcu::TestLog::EndImageSet;

        return tcu::TestStatus::fail("Failed");
    }
}

// SampleMaskWithDepthTestInstance
#ifndef CTS_USES_VULKANSC
SampleMaskWithDepthTestInstance::SampleMaskWithDepthTestInstance(
    Context &context, const PipelineConstructionType pipelineConstructionType,
    const VkSampleCountFlagBits rasterizationSamples, const bool enablePostDepthCoverage,
    const bool useFragmentShadingRate)
    : vkt::TestInstance(context)
    , m_pipelineConstructionType(pipelineConstructionType)
    , m_rasterizationSamples(rasterizationSamples)
    , m_enablePostDepthCoverage(enablePostDepthCoverage)
    , m_colorFormat(VK_FORMAT_R8G8B8A8_UNORM)
    , m_depthStencilFormat(VK_FORMAT_D16_UNORM)
    , m_renderSize(tcu::IVec2(3, 3))
    , m_useDepth(true)
    , m_useStencil(false)
    , m_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
    , m_renderColor(tcu::Vec4(0.0f, 1.0f, 0.0f, 1.0f))
    , m_vertices(generateVertices())
    , m_multisampleStateParams(getMultisampleState(rasterizationSamples))
    , m_blendState(getDefaultColorBlendAttachmentState())
    , m_renderType(RENDER_TYPE_RESOLVE)
    , m_imageBackingMode(IMAGE_BACKING_MODE_REGULAR)
    , m_depthClearValue(0.667f)
    , m_useFragmentShadingRate(useFragmentShadingRate)
{
    m_refCoverageAfterDepthTest[VK_SAMPLE_COUNT_2_BIT] =
        SampleCoverage(1u, 1u); // !< Sample coverage of the diagonally halved pixel,
    m_refCoverageAfterDepthTest[VK_SAMPLE_COUNT_4_BIT] =
        SampleCoverage(2u, 2u); // !< with max possible subPixelPrecisionBits threshold
    m_refCoverageAfterDepthTest[VK_SAMPLE_COUNT_8_BIT]  = SampleCoverage(2u, 6u);  // !<
    m_refCoverageAfterDepthTest[VK_SAMPLE_COUNT_16_BIT] = SampleCoverage(6u, 11u); // !<
}

tcu::TestStatus SampleMaskWithDepthTestInstance::iterate(void)
{
    de::MovePtr<tcu::TextureLevel> result;

    MultisampleRenderer renderer(m_context, m_pipelineConstructionType, m_colorFormat, m_depthStencilFormat,
                                 m_renderSize, m_useDepth, m_useStencil, 1u, &m_topology, &m_vertices,
                                 m_multisampleStateParams, m_blendState, m_renderType, m_imageBackingMode,
                                 m_useFragmentShadingRate, m_depthClearValue);
    result = renderer.render();

    return verifyImage(result->getAccess());
}

VkPipelineMultisampleStateCreateInfo SampleMaskWithDepthTestInstance::getMultisampleState(
    const VkSampleCountFlagBits rasterizationSamples)
{
    const VkPipelineMultisampleStateCreateInfo multisampleStateParams = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                  // const void* pNext;
        0u,                                                       // VkPipelineMultisampleStateCreateFlags flags;
        rasterizationSamples,                                     // VkSampleCountFlagBits rasterizationSamples;
        false,                                                    // VkBool32 sampleShadingEnable;
        0.0f,                                                     // float minSampleShading;
        nullptr,                                                  // const VkSampleMask* pSampleMask;
        false,                                                    // VkBool32 alphaToCoverageEnable;
        false                                                     // VkBool32 alphaToOneEnable;
    };

    return multisampleStateParams;
}

std::vector<Vertex4RGBA> SampleMaskWithDepthTestInstance::generateVertices(void)
{
    std::vector<Vertex4RGBA> vertices;

    {
        const Vertex4RGBA vertexInput = {tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f), m_renderColor};
        vertices.push_back(vertexInput);
    }
    {
        const Vertex4RGBA vertexInput = {tcu::Vec4(1.0f, -1.0f, 1.0f, 1.0f), m_renderColor};
        vertices.push_back(vertexInput);
    }
    {
        const Vertex4RGBA vertexInput = {tcu::Vec4(-1.0f, 1.0f, 1.0f, 1.0f), m_renderColor};
        vertices.push_back(vertexInput);
    }

    return vertices;
}

tcu::TestStatus SampleMaskWithDepthTestInstance::verifyImage(const tcu::ConstPixelBufferAccess &result)
{
    bool pass         = true;
    const int width   = result.getWidth();
    const int height  = result.getHeight();
    tcu::TestLog &log = m_context.getTestContext().getLog();

    DE_ASSERT(width == 3);
    DE_ASSERT(height == 3);

    const tcu::Vec4 clearColor = tcu::Vec4(0.0f);

    for (int x = 0; x < width; ++x)
        for (int y = 0; y < height; ++y)
        {
            const tcu::Vec4 resultPixel = result.getPixel(x, y);

            if (x + y == 0)
            {
                const float threshold   = 0.02f;
                tcu::Vec4 expectedPixel = m_renderColor;

                if (m_useFragmentShadingRate && m_enablePostDepthCoverage)
                {
                    // The fragment shader for this test outputs a fragment value that
                    // is based off gl_SampleMaskIn. For the FSR case that sample mask
                    // applies to 4 pixels, rather than the usual 1 pixel per fragment
                    // shader invocation. Those 4 pixels represent:
                    //   a) The fully covered pixel (this "x + y == 0" case)
                    //   b) The two partially covered pixels (the "x + y == 1" case below)
                    //   c) The non-covered pixel (the "else" case below)
                    //
                    // For the PostDepthCoverage case, the gl_SampleMaskIn represents
                    // coverage after the depth test, so it has roughly 50% of the bits
                    // set. This means that the expected result for this case (a)
                    // will not be the "m_renderColor" but ~50% of the m_renderColor.
                    expectedPixel = expectedPixel * tcu::Vec4(0.5f);
                }

                bool localPass = true;
                for (uint32_t componentNdx = 0u; componentNdx < m_renderColor.SIZE; ++componentNdx)
                {
                    if (m_renderColor[componentNdx] != 0.0f &&
                        (resultPixel[componentNdx] <= expectedPixel[componentNdx] * (1.0f - threshold) ||
                         resultPixel[componentNdx] >= expectedPixel[componentNdx] * (1.0f + threshold)))
                        localPass = false;
                }

                if (!localPass)
                {
                    log << tcu::TestLog::Message << "x: " << x << " y: " << y << " Result: " << resultPixel
                        << " Reference range ( " << expectedPixel * (1.0f - threshold) << " ; "
                        << expectedPixel * (1.0f + threshold) << " )" << tcu::TestLog::EndMessage;
                    pass = false;
                }
            }
            else if (x + y == 1)
            {
                const float threshold = 0.02f;
                float minCoverage =
                    (float)m_refCoverageAfterDepthTest[m_rasterizationSamples].min / (float)m_rasterizationSamples;
                float maxCoverage =
                    (float)m_refCoverageAfterDepthTest[m_rasterizationSamples].max / (float)m_rasterizationSamples;

                // default: m_rasterizationSamples bits set in FS's gl_SampleMaskIn[0] (before depth test)
                // post_depth_coverage: m_refCoverageAfterDepthTest[m_rasterizationSamples] bits set in FS's gl_SampleMaskIn[0] (after depth test)

                if (m_enablePostDepthCoverage)
                {
                    minCoverage *= minCoverage;
                    maxCoverage *= maxCoverage;
                }

                bool localPass = true;
                for (uint32_t componentNdx = 0u; componentNdx < m_renderColor.SIZE; ++componentNdx)
                {
                    if (m_renderColor[componentNdx] != 0.0f &&
                        (resultPixel[componentNdx] <= m_renderColor[componentNdx] * (minCoverage - threshold) ||
                         resultPixel[componentNdx] >= m_renderColor[componentNdx] * (maxCoverage + threshold)))
                        localPass = false;
                }

                if (!localPass)
                {
                    log << tcu::TestLog::Message << "x: " << x << " y: " << y << " Result: " << resultPixel
                        << " Reference range ( " << m_renderColor * (minCoverage - threshold) << " ; "
                        << m_renderColor * (maxCoverage + threshold) << " )" << tcu::TestLog::EndMessage;
                    pass = false;
                }
            }
            else
            {
                if (resultPixel != clearColor)
                {
                    log << tcu::TestLog::Message << "x: " << x << " y: " << y << " Result: " << resultPixel
                        << " Reference: " << clearColor << tcu::TestLog::EndMessage;
                    pass = false;
                }
            }
        }

    if (pass)
        return tcu::TestStatus::pass("Passed");
    else
        return tcu::TestStatus::fail("Failed");
}
#endif // CTS_USES_VULKANSC

CompatibleRenderPassTestInstance::CompatibleRenderPassTestInstance(
    Context &context, const PipelineConstructionType pipelineConstructionType, bool dynamic)
    : vkt::TestInstance(context)
    , m_pipelineConstructionType(pipelineConstructionType)
    , m_dynamic(dynamic)
{
}

tcu::TestStatus CompatibleRenderPassTestInstance::iterate(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice vkDevice   = m_context.getDevice();
    Allocator &memAlloc       = m_context.getDefaultAllocator();
    const VkQueue queue       = m_context.getUniversalQueue();
    const auto queueIndex     = m_context.getUniversalQueueFamilyIndex();
    auto &log                 = m_context.getTestContext().getLog();

    vk::VkImageCreateInfo colorImageParams = {
        vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                                       // VkStructureType sType;
        nullptr,                                                                       // const void* pNext;
        (vk::VkImageCreateFlags)0u,                                                    // VkImageCreateFlags flags;
        vk::VK_IMAGE_TYPE_2D,                                                          // VkImageType imageType;
        vk::VK_FORMAT_R8G8B8A8_UNORM,                                                  // VkFormat format;
        {32u, 32u, 1u},                                                                // VkExtent3D extent;
        1u,                                                                            // uint32_t mipLevels;
        1u,                                                                            // uint32_t arrayLayers;
        vk::VK_SAMPLE_COUNT_4_BIT,                                                     // VkSampleCountFlagBits samples;
        vk::VK_IMAGE_TILING_OPTIMAL,                                                   // VkImageTiling tiling;
        vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | vk::VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // VkImageUsageFlags usage;
        vk::VK_SHARING_MODE_EXCLUSIVE,                                                 // VkSharingMode sharingMode;
        1u,                        // uint32_t queueFamilyIndexCount;
        nullptr,                   // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED, // VkImageLayout initialLayout;
    };

    vk::ImageWithMemory colorAttachment(vk, vkDevice, memAlloc, colorImageParams, MemoryRequirement::Any);
    colorImageParams.samples = vk::VK_SAMPLE_COUNT_1_BIT;
    vk::ImageWithMemory resolveAttachment(vk, vkDevice, memAlloc, colorImageParams, MemoryRequirement::Any);

    vk::VkImageViewCreateInfo colorAttachmentViewParams = {
        vk::VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType sType;
        nullptr,                                      // const void* pNext;
        0u,                                           // VkImageViewCreateFlags flags;
        *colorAttachment,                             // VkImage image;
        vk::VK_IMAGE_VIEW_TYPE_2D,                    // VkImageViewType viewType;
        vk::VK_FORMAT_R8G8B8A8_UNORM,                 // VkFormat format;
        {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B,
         VK_COMPONENT_SWIZZLE_A},                   // VkComponentMapping components;
        {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u} // VkImageSubresourceRange subresourceRange;
    };

    auto colorImageView             = createImageView(vk, vkDevice, &colorAttachmentViewParams);
    colorAttachmentViewParams.image = *resolveAttachment;
    auto resolveImageView           = createImageView(vk, vkDevice, &colorAttachmentViewParams);

    std::vector<VkAttachmentDescription> attachmentDescriptions;
    const VkAttachmentDescription colorAttachmentDescription = {
        0u,                                      // VkAttachmentDescriptionFlags flags;
        vk::VK_FORMAT_R8G8B8A8_UNORM,            // VkFormat format;
        vk::VK_SAMPLE_COUNT_4_BIT,               // VkSampleCountFlagBits samples;
        VK_ATTACHMENT_LOAD_OP_CLEAR,             // VkAttachmentLoadOp loadOp;
        VK_ATTACHMENT_STORE_OP_STORE,            // VkAttachmentStoreOp storeOp;
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,         // VkAttachmentLoadOp stencilLoadOp;
        VK_ATTACHMENT_STORE_OP_DONT_CARE,        // VkAttachmentStoreOp stencilStoreOp;
        VK_IMAGE_LAYOUT_UNDEFINED,               // VkImageLayout initialLayout;
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL // VkImageLayout finalLayout;
    };

    const VkAttachmentDescription resolveAttachmentDescription = {
        0u,                                      // VkAttachmentDescriptionFlags flags;
        vk::VK_FORMAT_R8G8B8A8_UNORM,            // VkFormat format;
        VK_SAMPLE_COUNT_1_BIT,                   // VkSampleCountFlagBits samples;
        VK_ATTACHMENT_LOAD_OP_CLEAR,             // VkAttachmentLoadOp loadOp;
        VK_ATTACHMENT_STORE_OP_STORE,            // VkAttachmentStoreOp storeOp;
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,         // VkAttachmentLoadOp stencilLoadOp;
        VK_ATTACHMENT_STORE_OP_DONT_CARE,        // VkAttachmentStoreOp stencilStoreOp;
        VK_IMAGE_LAYOUT_UNDEFINED,               // VkImageLayout initialLayout;
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL // VkImageLayout finalLayout;
    };

    attachmentDescriptions.push_back(colorAttachmentDescription);
    attachmentDescriptions.push_back(resolveAttachmentDescription);

    const VkAttachmentReference colorAttachmentReference = {
        0u,                                      // uint32_t attachment;
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL // VkImageLayout layout;
    };

    const VkAttachmentReference resolveAttachmentReference = {
        1u,                                      // uint32_t attachment;
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL // VkImageLayout layout;
    };

    vk::VkSubpassDescription subpassDescription = {
        0u,                                  // VkSubpassDescriptionFlags       flags
        vk::VK_PIPELINE_BIND_POINT_GRAPHICS, // VkPipelineBindPoint             pipelineBindPoint
        0u,                                  // uint32_t                        inputAttachmentCount
        nullptr,                             // const VkAttachmentReference*    pInputAttachments
        1u,                                  // uint32_t                        colorAttachmentCount
        &colorAttachmentReference,           // const VkAttachmentReference*    pColorAttachments
        &resolveAttachmentReference,         // const VkAttachmentReference*    pResolveAttachments
        nullptr,                             // const VkAttachmentReference*    pDepthStencilAttachment
        0u,                                  // uint32_t                        preserveAttachmentCount
        nullptr                              // const VkAttachmentReference*    pPreserveAttachments
    };

    vk::VkRenderPassCreateInfo renderPassParams = {
        vk::VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, // VkStructureType sType;
        nullptr,                                       // const void* pNext;
        0u,                                            // VkRenderPassCreateFlags flags;
        2u,                                            // uint32_t attachmentCount;
        attachmentDescriptions.data(),                 // const VkAttachmentDescription* pAttachments;
        1u,                                            // uint32_t subpassCount;
        &subpassDescription,                           // const VkSubpassDescription* pSubpasses;
        0u,                                            // uint32_t dependencyCount;
        nullptr                                        // const VkSubpassDependency* pDependencies;
    };

    auto renderPass = RenderPassWrapper(m_pipelineConstructionType, vk, vkDevice, &renderPassParams);

    std::vector<vk::VkImageView> framebufferAttachments{*colorImageView, *resolveImageView};

    const VkFramebufferCreateInfo framebufferParams = {
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // VkStructureType sType;
        nullptr,                                   // const void* pNext;
        0u,                                        // VkFramebufferCreateFlags flags;
        *renderPass,                               // VkRenderPass renderPass;
        2u,                                        // uint32_t attachmentCount;
        framebufferAttachments.data(),             // const VkImageView* pAttachments;
        32u,                                       // uint32_t width;
        32u,                                       // uint32_t height;
        1u                                         // uint32_t layers;
    };

    renderPass.createFramebuffer(vk, vkDevice, &framebufferParams, *colorAttachment);

    renderPassParams.attachmentCount       = 1;
    subpassDescription.pResolveAttachments = nullptr;
    auto compatibleRenderPass = RenderPassWrapper(m_pipelineConstructionType, vk, vkDevice, &renderPassParams);

    const std::vector<VkViewport> viewports{vk::makeViewport(32u, 32u)};
    const std::vector<VkRect2D> scissors{vk::makeRect2D(32u, 32u)};

    const VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                  // const void* pNext;
        0u,                                                       // VkPipelineMultisampleStateCreateFlags flags;
        vk::VK_SAMPLE_COUNT_4_BIT,                                // VkSampleCountFlagBits rasterizationSamples;
        VK_FALSE,                                                 // VkBool32 sampleShadingEnable;
        1.0f,                                                     // float minSampleShading;
        nullptr,                                                  // const VkSampleMask* pSampleMask;
        VK_FALSE,                                                 // VkBool32 alphaToCoverageEnable;
        VK_FALSE,                                                 // VkBool32 alphaToOneEnable;
    };

    const VkPipelineLayoutCreateInfo pipelineLayoutParams = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType sType;
        nullptr,                                       // const void* pNext;
        0u,                                            // VkPipelineLayoutCreateFlags flags;
        0u,                                            // uint32_t setLayoutCount;
        nullptr,                                       // const VkDescriptorSetLayout* pSetLayouts;
        0u,                                            // uint32_t pushConstantRangeCount;
        nullptr                                        // const VkPushConstantRange* pPushConstantRanges;
    };

    const auto &binaries  = m_context.getBinaryCollection();
    const auto vertModule = createShaderModule(vk, vkDevice, binaries.get("vert"));
    const auto fragModule = createShaderModule(vk, vkDevice, binaries.get("frag"));
    auto pipelineLayout   = PipelineLayoutWrapper(m_pipelineConstructionType, vk, vkDevice, &pipelineLayoutParams);

    const VkPipelineVertexInputStateCreateInfo vertexInputStateParams = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                   // const void* pNext;
        0u,                                                        // VkPipelineVertexInputStateCreateFlags flags;
        0u,                                                        // uint32_t vertexBindingDescriptionCount;
        nullptr, // const VkVertexInputBindingDescription* pVertexBindingDescriptions;
        0u,      // uint32_t vertexAttributeDescriptionCount;
        nullptr  // const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
    };

    const auto &edsFeatures = m_context.getExtendedDynamicStateFeaturesEXT();
    std::vector<vk::VkDynamicState> dynamicStates;

    if (m_dynamic)
    {
        dynamicStates.push_back(vk::VK_DYNAMIC_STATE_VIEWPORT);
        dynamicStates.push_back(vk::VK_DYNAMIC_STATE_SCISSOR);
        dynamicStates.push_back(vk::VK_DYNAMIC_STATE_DEPTH_BIAS);
        dynamicStates.push_back(vk::VK_DYNAMIC_STATE_BLEND_CONSTANTS);
        dynamicStates.push_back(vk::VK_DYNAMIC_STATE_DEPTH_BOUNDS);
        dynamicStates.push_back(vk::VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK);
        dynamicStates.push_back(vk::VK_DYNAMIC_STATE_STENCIL_WRITE_MASK);
        dynamicStates.push_back(vk::VK_DYNAMIC_STATE_STENCIL_REFERENCE);
#ifndef CTS_USES_VULKANSC
        if (edsFeatures.extendedDynamicState)
        {
            dynamicStates.push_back(vk::VK_DYNAMIC_STATE_CULL_MODE);
            dynamicStates.push_back(vk::VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE);
            dynamicStates.push_back(vk::VK_DYNAMIC_STATE_DEPTH_COMPARE_OP);
            dynamicStates.push_back(vk::VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE);
            dynamicStates.push_back(vk::VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE);
            dynamicStates.push_back(vk::VK_DYNAMIC_STATE_FRONT_FACE);
            dynamicStates.push_back(vk::VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY);
            dynamicStates.push_back(vk::VK_DYNAMIC_STATE_STENCIL_OP);
            dynamicStates.push_back(vk::VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE);
        }
#endif
    }

    const VkPipelineDynamicStateCreateInfo dynamicStateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                              // const void* pNext;
        0u,                                                   // VkPipelineDynamicStateCreateFlags flags;
        de::sizeU32(dynamicStates),                           // uint32_t dynamicStateCount;
        de::dataOrNull(dynamicStates),                        // const VkDynamicState* pDynamicStates;
    };

    std::vector<VkViewport> staticViewports = viewports;
    std::vector<VkRect2D> staticScissors    = scissors;
    if (m_dynamic && edsFeatures.extendedDynamicState)
    {
        staticViewports.clear();
        staticScissors.clear();
    }

    const auto pipeline =
        makeGraphicsPipeline(vk, vkDevice, pipelineLayout.get(), vertModule.get(), VK_NULL_HANDLE, VK_NULL_HANDLE,
                             VK_NULL_HANDLE, fragModule.get(), compatibleRenderPass.get(), staticViewports,
                             staticScissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 0u, 0u, &vertexInputStateParams,
                             nullptr, &multisampleStateCreateInfo, nullptr, nullptr, &dynamicStateInfo);

    auto cmdPool   = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueIndex);
    auto cmdBuffer = allocateCommandBuffer(vk, vkDevice, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    VkClearValue colorClearValue;
    colorClearValue.color.float32[0] = 0.25f;
    colorClearValue.color.float32[1] = 0.25f;
    colorClearValue.color.float32[2] = 0.25f;
    colorClearValue.color.float32[3] = 1.0f;

    std::vector<VkClearValue> clearValues;
    clearValues.push_back(colorClearValue);
    colorClearValue.color.float32[0] = 0.5f;
    colorClearValue.color.float32[1] = 0.5f;
    colorClearValue.color.float32[2] = 0.5f;
    clearValues.push_back(colorClearValue);

    beginCommandBuffer(vk, *cmdBuffer, 0u);

    renderPass.begin(vk, *cmdBuffer, makeRect2D(0, 0, 32u, 32u), (uint32_t)clearValues.size(), clearValues.data());

    vk.cmdBindPipeline(*cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());

    if (m_dynamic)
    {
        vk.cmdSetViewport(*cmdBuffer, 0u, 1u, viewports.data());
        vk.cmdSetScissor(*cmdBuffer, 0u, 1u, scissors.data());
        vk.cmdSetDepthBias(*cmdBuffer, 0.0f, 1.0f, 1.0f);
        float blendConstants[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        vk.cmdSetBlendConstants(*cmdBuffer, blendConstants);
        vk.cmdSetDepthBounds(*cmdBuffer, 0.0f, 1.0f);
        vk.cmdSetStencilCompareMask(*cmdBuffer, vk::VK_STENCIL_FACE_FRONT_AND_BACK, 0xff);
        vk.cmdSetStencilWriteMask(*cmdBuffer, vk::VK_STENCIL_FACE_FRONT_AND_BACK, 0xff);
        vk.cmdSetStencilReference(*cmdBuffer, vk::VK_STENCIL_FACE_FRONT_AND_BACK, 0xff);
#ifndef CTS_USES_VULKANSC
        if (edsFeatures.extendedDynamicState)
        {
            vk.cmdSetCullMode(*cmdBuffer, vk::VK_CULL_MODE_NONE);
            vk.cmdSetDepthBoundsTestEnable(*cmdBuffer, VK_FALSE);
            vk.cmdSetDepthCompareOp(*cmdBuffer, vk::VK_COMPARE_OP_ALWAYS);
            vk.cmdSetDepthTestEnable(*cmdBuffer, VK_FALSE);
            vk.cmdSetDepthWriteEnable(*cmdBuffer, VK_FALSE);
            vk.cmdSetFrontFace(*cmdBuffer, vk::VK_FRONT_FACE_COUNTER_CLOCKWISE);
            vk.cmdSetPrimitiveTopology(*cmdBuffer, vk::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
            vk.cmdSetStencilOp(*cmdBuffer, vk::VK_STENCIL_FACE_FRONT_AND_BACK, vk::VK_STENCIL_OP_KEEP,
                               vk::VK_STENCIL_OP_KEEP, vk::VK_STENCIL_OP_KEEP, vk::VK_COMPARE_OP_ALWAYS);
            vk.cmdSetStencilTestEnable(*cmdBuffer, VK_FALSE);
        }
#endif
    }

    vk.cmdDraw(*cmdBuffer, 4u, 1u, 0u, 0u);

    renderPass.end(vk, *cmdBuffer);

    endCommandBuffer(vk, *cmdBuffer);
    submitCommandsAndWait(vk, vkDevice, queue, *cmdBuffer);

    const auto colorLevel  = readColorAttachment(vk, vkDevice, queue, queueIndex, memAlloc, *resolveAttachment,
                                                 vk::VK_FORMAT_R8G8B8A8_UNORM, tcu::UVec2(32u, 32u));
    const auto colorAccess = colorLevel->getAccess();

    const tcu::IVec3 iExtent(32, 32, 1);
    tcu::TextureLevel refColor(mapVkFormat(vk::VK_FORMAT_R8G8B8A8_UNORM), iExtent.x(), iExtent.y());
    const tcu::Vec4 clearColor(1.0f, 1.0f, 1.0f, 1.0f);
    tcu::clear(refColor, clearColor);
    auto refColorAccess = refColor.getAccess();
    tcu::Vec4 colorThreshold(0.0f, 0.0f, 0.0f, 0.0f);
    const auto colorOK = tcu::floatThresholdCompare(log, "Color", "Color Result", refColorAccess, colorAccess,
                                                    colorThreshold, tcu::COMPARE_LOG_ON_ERROR);

    if (!colorOK)
    {
        return tcu::TestStatus::fail("Fail");
    }

    return tcu::TestStatus::pass("Passed");
}

// MultisampleRenderer

MultisampleRenderer::MultisampleRenderer(Context &context, const PipelineConstructionType pipelineConstructionType,
                                         const VkFormat colorFormat, const tcu::IVec2 &renderSize,
                                         const VkPrimitiveTopology topology, const std::vector<Vertex4RGBA> &vertices,
                                         const VkPipelineMultisampleStateCreateInfo &multisampleStateParams,
                                         const VkPipelineColorBlendAttachmentState &blendState,
                                         const RenderType renderType, const ImageBackingMode backingMode,
                                         const bool useFragmentShadingRate)
    : m_context(context)
    , m_pipelineConstructionType(pipelineConstructionType)
    , m_bindSemaphore(createSemaphore(context.getDeviceInterface(), context.getDevice()))
    , m_colorFormat(colorFormat)
    , m_depthStencilFormat(VK_FORMAT_UNDEFINED)
    , m_renderSize(renderSize)
    , m_useDepth(false)
    , m_useStencil(false)
    , m_useConservative(false)
    , m_multisampleStateParams(multisampleStateParams)
    , m_colorBlendState(blendState)
    , m_rasterizationConservativeStateCreateInfo()
    , m_renderType(renderType)
    , m_backingMode(backingMode)
    , m_depthClearValue(1.0f)
    , m_useFragmentShadingRate(useFragmentShadingRate)
{
    initialize(context, 1u, &topology, &vertices);
}

MultisampleRenderer::MultisampleRenderer(Context &context, const PipelineConstructionType pipelineConstructionType,
                                         const VkFormat colorFormat, const VkFormat depthStencilFormat,
                                         const tcu::IVec2 &renderSize, const bool useDepth, const bool useStencil,
                                         const uint32_t numTopologies, const VkPrimitiveTopology *pTopology,
                                         const std::vector<Vertex4RGBA> *pVertices,
                                         const VkPipelineMultisampleStateCreateInfo &multisampleStateParams,
                                         const VkPipelineColorBlendAttachmentState &blendState,
                                         const RenderType renderType, const ImageBackingMode backingMode,
                                         const bool useFragmentShadingRate, const float depthClearValue)
    : m_context(context)
    , m_pipelineConstructionType(pipelineConstructionType)
    , m_bindSemaphore(createSemaphore(context.getDeviceInterface(), context.getDevice()))
    , m_colorFormat(colorFormat)
    , m_depthStencilFormat(depthStencilFormat)
    , m_renderSize(renderSize)
    , m_useDepth(useDepth)
    , m_useStencil(useStencil)
    , m_useConservative(false)
    , m_multisampleStateParams(multisampleStateParams)
    , m_colorBlendState(blendState)
    , m_rasterizationConservativeStateCreateInfo()
    , m_renderType(renderType)
    , m_backingMode(backingMode)
    , m_depthClearValue(depthClearValue)
    , m_useFragmentShadingRate(useFragmentShadingRate)
{
    initialize(context, numTopologies, pTopology, pVertices);
}

MultisampleRenderer::MultisampleRenderer(
    Context &context, const PipelineConstructionType pipelineConstructionType, const VkFormat colorFormat,
    const VkFormat depthStencilFormat, const tcu::IVec2 &renderSize, const bool useDepth, const bool useStencil,
    const bool useConservative, const bool useFragmentShadingRate, const uint32_t numTopologies,
    const VkPrimitiveTopology *pTopology, const std::vector<Vertex4RGBA> *pVertices,
    const VkPipelineMultisampleStateCreateInfo &multisampleStateParams,
    const VkPipelineColorBlendAttachmentState &blendState,
    const VkPipelineRasterizationConservativeStateCreateInfoEXT &conservativeStateCreateInfo,
    const RenderType renderType, const ImageBackingMode backingMode, const float depthClearValue)
    : m_context(context)
    , m_pipelineConstructionType(pipelineConstructionType)
    , m_bindSemaphore(createSemaphore(context.getDeviceInterface(), context.getDevice()))
    , m_colorFormat(colorFormat)
    , m_depthStencilFormat(depthStencilFormat)
    , m_renderSize(renderSize)
    , m_useDepth(useDepth)
    , m_useStencil(useStencil)
    , m_useConservative(useConservative)
    , m_multisampleStateParams(multisampleStateParams)
    , m_colorBlendState(blendState)
    , m_rasterizationConservativeStateCreateInfo(conservativeStateCreateInfo)
    , m_renderType(renderType)
    , m_backingMode(backingMode)
    , m_depthClearValue(depthClearValue)
    , m_useFragmentShadingRate(useFragmentShadingRate)
{
    initialize(context, numTopologies, pTopology, pVertices);
}

void MultisampleRenderer::initialize(Context &context, const uint32_t numTopologies,
                                     const VkPrimitiveTopology *pTopology, const std::vector<Vertex4RGBA> *pVertices)
{
    if (!isSupportedSampleCount(context.getInstanceInterface(), context.getPhysicalDevice(),
                                m_multisampleStateParams.rasterizationSamples))
        throw tcu::NotSupportedError("Unsupported number of rasterization samples");

    const InstanceInterface &vki            = context.getInstanceInterface();
    const DeviceInterface &vk               = context.getDeviceInterface();
    const VkPhysicalDevice physicalDevice   = context.getPhysicalDevice();
    const VkDevice vkDevice                 = context.getDevice();
    const VkPhysicalDeviceFeatures features = context.getDeviceFeatures();
    const uint32_t queueFamilyIndices[] = {context.getUniversalQueueFamilyIndex(), context.getSparseQueueFamilyIndex()};
    const bool sparse                   = m_backingMode == IMAGE_BACKING_MODE_SPARSE;
    const VkComponentMapping componentMappingRGBA = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
                                                     VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
    const VkImageCreateFlags imageCreateFlags =
        sparse ? (VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT) : 0u;
    const VkSharingMode sharingMode =
        (sparse && context.getUniversalQueueFamilyIndex() != context.getSparseQueueFamilyIndex()) ?
            VK_SHARING_MODE_CONCURRENT :
            VK_SHARING_MODE_EXCLUSIVE;
    Allocator &memAlloc         = m_context.getDefaultAllocator();
    const bool usesResolveImage = m_renderType == RENDER_TYPE_RESOLVE ||
                                  m_renderType == RENDER_TYPE_DEPTHSTENCIL_ONLY ||
                                  m_renderType == RENDER_TYPE_UNUSED_ATTACHMENT;

    if (sparse)
    {
        bool sparseSamplesSupported = false;
        switch (m_multisampleStateParams.rasterizationSamples)
        {
        case VK_SAMPLE_COUNT_1_BIT:
            sparseSamplesSupported = features.sparseResidencyImage2D;
            break;
        case VK_SAMPLE_COUNT_2_BIT:
            sparseSamplesSupported = features.sparseResidency2Samples;
            break;
        case VK_SAMPLE_COUNT_4_BIT:
            sparseSamplesSupported = features.sparseResidency4Samples;
            break;
        case VK_SAMPLE_COUNT_8_BIT:
            sparseSamplesSupported = features.sparseResidency8Samples;
            break;
        case VK_SAMPLE_COUNT_16_BIT:
            sparseSamplesSupported = features.sparseResidency16Samples;
            break;
        default:
            break;
        }

        if (!sparseSamplesSupported)
            throw tcu::NotSupportedError("Unsupported number of rasterization samples for sparse residency");
    }

    if (sparse && !context.getDeviceFeatures().sparseBinding)
        throw tcu::NotSupportedError("No sparseBinding support");

    // Create color image
    {
        const VkImageUsageFlags imageUsageFlags =
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
            (m_renderType == RENDER_TYPE_COPY_SAMPLES ? VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT : (VkImageUsageFlagBits)0u);

        const VkImageCreateInfo colorImageParams = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                          // VkStructureType sType;
            nullptr,                                                      // const void* pNext;
            imageCreateFlags,                                             // VkImageCreateFlags flags;
            VK_IMAGE_TYPE_2D,                                             // VkImageType imageType;
            m_colorFormat,                                                // VkFormat format;
            {(uint32_t)m_renderSize.x(), (uint32_t)m_renderSize.y(), 1u}, // VkExtent3D extent;
            1u,                                                           // uint32_t mipLevels;
            1u,                                                           // uint32_t arrayLayers;
            m_multisampleStateParams.rasterizationSamples,                // VkSampleCountFlagBits samples;
            VK_IMAGE_TILING_OPTIMAL,                                      // VkImageTiling tiling;
            imageUsageFlags,                                              // VkImageUsageFlags usage;
            sharingMode,                                                  // VkSharingMode sharingMode;
            sharingMode == VK_SHARING_MODE_CONCURRENT ? 2u : 1u,          // uint32_t queueFamilyIndexCount;
            queueFamilyIndices,                                           // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED,                                    // VkImageLayout initialLayout;
        };

#ifndef CTS_USES_VULKANSC
        if (sparse && !checkSparseImageFormatSupport(context.getPhysicalDevice(), context.getInstanceInterface(),
                                                     colorImageParams))
            TCU_THROW(NotSupportedError, "The image format does not support sparse operations.");
#endif // CTS_USES_VULKANSC

        m_colorImage = createImage(vk, vkDevice, &colorImageParams);

        // Allocate and bind color image memory
        if (sparse)
        {
#ifndef CTS_USES_VULKANSC
            allocateAndBindSparseImage(vk, vkDevice, context.getPhysicalDevice(), context.getInstanceInterface(),
                                       colorImageParams, *m_bindSemaphore, context.getSparseQueue(), memAlloc,
                                       m_allocations, mapVkFormat(m_colorFormat), *m_colorImage);
#endif // CTS_USES_VULKANSC
        }
        else
        {
            m_colorImageAlloc =
                memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_colorImage), MemoryRequirement::Any);
            VK_CHECK(vk.bindImageMemory(vkDevice, *m_colorImage, m_colorImageAlloc->getMemory(),
                                        m_colorImageAlloc->getOffset()));
        }
    }

    // Create resolve image
    if (usesResolveImage)
    {
        const VkImageCreateInfo resolveImageParams = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                                    // VkStructureType sType;
            nullptr,                                                                // const void* pNext;
            0u,                                                                     // VkImageCreateFlags flags;
            VK_IMAGE_TYPE_2D,                                                       // VkImageType imageType;
            m_colorFormat,                                                          // VkFormat format;
            {(uint32_t)m_renderSize.x(), (uint32_t)m_renderSize.y(), 1u},           // VkExtent3D extent;
            1u,                                                                     // uint32_t mipLevels;
            1u,                                                                     // uint32_t arrayLayers;
            VK_SAMPLE_COUNT_1_BIT,                                                  // VkSampleCountFlagBits samples;
            VK_IMAGE_TILING_OPTIMAL,                                                // VkImageTiling tiling;
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | // VkImageUsageFlags usage;
                VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            VK_SHARING_MODE_EXCLUSIVE, // VkSharingMode sharingMode;
            1u,                        // uint32_t queueFamilyIndexCount;
            queueFamilyIndices,        // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED  // VkImageLayout initialLayout;
        };

        m_resolveImage = createImage(vk, vkDevice, &resolveImageParams);

        // Allocate and bind resolve image memory
        m_resolveImageAlloc =
            memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_resolveImage), MemoryRequirement::Any);
        VK_CHECK(vk.bindImageMemory(vkDevice, *m_resolveImage, m_resolveImageAlloc->getMemory(),
                                    m_resolveImageAlloc->getOffset()));

        // Create resolve attachment view
        {
            const VkImageViewCreateInfo resolveAttachmentViewParams = {
                VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,   // VkStructureType sType;
                nullptr,                                    // const void* pNext;
                0u,                                         // VkImageViewCreateFlags flags;
                *m_resolveImage,                            // VkImage image;
                VK_IMAGE_VIEW_TYPE_2D,                      // VkImageViewType viewType;
                m_colorFormat,                              // VkFormat format;
                componentMappingRGBA,                       // VkComponentMapping components;
                {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u} // VkImageSubresourceRange subresourceRange;
            };

            m_resolveAttachmentView = createImageView(vk, vkDevice, &resolveAttachmentViewParams);
        }
    }

    // Create per-sample output images
    if (m_renderType == RENDER_TYPE_COPY_SAMPLES)
    {
        const VkImageCreateInfo perSampleImageParams = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                                    // VkStructureType sType;
            nullptr,                                                                // const void* pNext;
            0u,                                                                     // VkImageCreateFlags flags;
            VK_IMAGE_TYPE_2D,                                                       // VkImageType imageType;
            m_colorFormat,                                                          // VkFormat format;
            {(uint32_t)m_renderSize.x(), (uint32_t)m_renderSize.y(), 1u},           // VkExtent3D extent;
            1u,                                                                     // uint32_t mipLevels;
            1u,                                                                     // uint32_t arrayLayers;
            VK_SAMPLE_COUNT_1_BIT,                                                  // VkSampleCountFlagBits samples;
            VK_IMAGE_TILING_OPTIMAL,                                                // VkImageTiling tiling;
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | // VkImageUsageFlags usage;
                VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            VK_SHARING_MODE_EXCLUSIVE, // VkSharingMode sharingMode;
            1u,                        // uint32_t queueFamilyIndexCount;
            queueFamilyIndices,        // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED  // VkImageLayout initialLayout;
        };

        m_perSampleImages.resize(static_cast<size_t>(m_multisampleStateParams.rasterizationSamples));

        for (size_t i = 0; i < m_perSampleImages.size(); ++i)
        {
            m_perSampleImages[i]  = de::SharedPtr<PerSampleImage>(new PerSampleImage);
            PerSampleImage &image = *m_perSampleImages[i];

            image.m_image = createImage(vk, vkDevice, &perSampleImageParams);

            // Allocate and bind image memory
            image.m_imageAlloc =
                memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *image.m_image), MemoryRequirement::Any);
            VK_CHECK(vk.bindImageMemory(vkDevice, *image.m_image, image.m_imageAlloc->getMemory(),
                                        image.m_imageAlloc->getOffset()));

            // Create per-sample attachment view
            {
                const VkImageViewCreateInfo perSampleAttachmentViewParams = {
                    VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,   // VkStructureType sType;
                    nullptr,                                    // const void* pNext;
                    0u,                                         // VkImageViewCreateFlags flags;
                    *image.m_image,                             // VkImage image;
                    VK_IMAGE_VIEW_TYPE_2D,                      // VkImageViewType viewType;
                    m_colorFormat,                              // VkFormat format;
                    componentMappingRGBA,                       // VkComponentMapping components;
                    {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u} // VkImageSubresourceRange subresourceRange;
                };

                image.m_attachmentView = createImageView(vk, vkDevice, &perSampleAttachmentViewParams);
            }
        }
    }

    // Create a depth/stencil image
    if (m_useDepth || m_useStencil)
    {
        const VkImageCreateInfo depthStencilImageParams = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                          // VkStructureType sType;
            nullptr,                                                      // const void* pNext;
            0u,                                                           // VkImageCreateFlags flags;
            VK_IMAGE_TYPE_2D,                                             // VkImageType imageType;
            m_depthStencilFormat,                                         // VkFormat format;
            {(uint32_t)m_renderSize.x(), (uint32_t)m_renderSize.y(), 1u}, // VkExtent3D extent;
            1u,                                                           // uint32_t mipLevels;
            1u,                                                           // uint32_t arrayLayers;
            m_multisampleStateParams.rasterizationSamples,                // VkSampleCountFlagBits samples;
            VK_IMAGE_TILING_OPTIMAL,                                      // VkImageTiling tiling;
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,                  // VkImageUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,                                    // VkSharingMode sharingMode;
            1u,                                                           // uint32_t queueFamilyIndexCount;
            queueFamilyIndices,                                           // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED                                     // VkImageLayout initialLayout;
        };

        m_depthStencilImage = createImage(vk, vkDevice, &depthStencilImageParams);

        // Allocate and bind depth/stencil image memory
        m_depthStencilImageAlloc =
            memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, *m_depthStencilImage), MemoryRequirement::Any);
        VK_CHECK(vk.bindImageMemory(vkDevice, *m_depthStencilImage, m_depthStencilImageAlloc->getMemory(),
                                    m_depthStencilImageAlloc->getOffset()));
    }

    // Create color attachment view
    {
        const VkImageViewCreateInfo colorAttachmentViewParams = {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,   // VkStructureType sType;
            nullptr,                                    // const void* pNext;
            0u,                                         // VkImageViewCreateFlags flags;
            *m_colorImage,                              // VkImage image;
            VK_IMAGE_VIEW_TYPE_2D,                      // VkImageViewType viewType;
            m_colorFormat,                              // VkFormat format;
            componentMappingRGBA,                       // VkComponentMapping components;
            {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u} // VkImageSubresourceRange subresourceRange;
        };

        m_colorAttachmentView = createImageView(vk, vkDevice, &colorAttachmentViewParams);
    }

    VkImageAspectFlags depthStencilAttachmentAspect = (VkImageAspectFlagBits)0;

    // Create depth/stencil attachment view
    if (m_useDepth || m_useStencil)
    {
        depthStencilAttachmentAspect = getImageAspectFlags(m_depthStencilFormat);

        const VkImageViewCreateInfo depthStencilAttachmentViewParams = {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,      // VkStructureType sType;
            nullptr,                                       // const void* pNext;
            0u,                                            // VkImageViewCreateFlags flags;
            *m_depthStencilImage,                          // VkImage image;
            VK_IMAGE_VIEW_TYPE_2D,                         // VkImageViewType viewType;
            m_depthStencilFormat,                          // VkFormat format;
            componentMappingRGBA,                          // VkComponentMapping components;
            {depthStencilAttachmentAspect, 0u, 1u, 0u, 1u} // VkImageSubresourceRange subresourceRange;
        };

        m_depthStencilAttachmentView = createImageView(vk, vkDevice, &depthStencilAttachmentViewParams);
    }

    // Create render pass
    {
        std::vector<VkAttachmentDescription> attachmentDescriptions;
        {
            const VkAttachmentDescription colorAttachmentDescription = {
                0u,                                            // VkAttachmentDescriptionFlags flags;
                m_colorFormat,                                 // VkFormat format;
                m_multisampleStateParams.rasterizationSamples, // VkSampleCountFlagBits samples;
                VK_ATTACHMENT_LOAD_OP_CLEAR,                   // VkAttachmentLoadOp loadOp;
                VK_ATTACHMENT_STORE_OP_STORE,                  // VkAttachmentStoreOp storeOp;
                VK_ATTACHMENT_LOAD_OP_DONT_CARE,               // VkAttachmentLoadOp stencilLoadOp;
                VK_ATTACHMENT_STORE_OP_DONT_CARE,              // VkAttachmentStoreOp stencilStoreOp;
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,      // VkImageLayout initialLayout;
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL       // VkImageLayout finalLayout;
            };
            attachmentDescriptions.push_back(colorAttachmentDescription);
        }

        uint32_t resolveAttachmentIndex = VK_ATTACHMENT_UNUSED;

        if (usesResolveImage)
        {
            resolveAttachmentIndex = static_cast<uint32_t>(attachmentDescriptions.size());

            const VkAttachmentDescription resolveAttachmentDescription = {
                0u,                                       // VkAttachmentDescriptionFlags flags;
                m_colorFormat,                            // VkFormat format;
                VK_SAMPLE_COUNT_1_BIT,                    // VkSampleCountFlagBits samples;
                VK_ATTACHMENT_LOAD_OP_CLEAR,              // VkAttachmentLoadOp loadOp;
                VK_ATTACHMENT_STORE_OP_STORE,             // VkAttachmentStoreOp storeOp;
                VK_ATTACHMENT_LOAD_OP_DONT_CARE,          // VkAttachmentLoadOp stencilLoadOp;
                VK_ATTACHMENT_STORE_OP_DONT_CARE,         // VkAttachmentStoreOp stencilStoreOp;
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout initialLayout;
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL  // VkImageLayout finalLayout;
            };
            attachmentDescriptions.push_back(resolveAttachmentDescription);
        }

        uint32_t perSampleAttachmentIndex = VK_ATTACHMENT_UNUSED;

        if (m_renderType == RENDER_TYPE_COPY_SAMPLES)
        {
            perSampleAttachmentIndex = static_cast<uint32_t>(attachmentDescriptions.size());

            const VkAttachmentDescription perSampleAttachmentDescription = {
                0u,                                       // VkAttachmentDescriptionFlags flags;
                m_colorFormat,                            // VkFormat format;
                VK_SAMPLE_COUNT_1_BIT,                    // VkSampleCountFlagBits samples;
                VK_ATTACHMENT_LOAD_OP_CLEAR,              // VkAttachmentLoadOp loadOp;
                VK_ATTACHMENT_STORE_OP_STORE,             // VkAttachmentStoreOp storeOp;
                VK_ATTACHMENT_LOAD_OP_DONT_CARE,          // VkAttachmentLoadOp stencilLoadOp;
                VK_ATTACHMENT_STORE_OP_DONT_CARE,         // VkAttachmentStoreOp stencilStoreOp;
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout initialLayout;
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL  // VkImageLayout finalLayout;
            };

            for (size_t i = 0; i < m_perSampleImages.size(); ++i)
            {
                attachmentDescriptions.push_back(perSampleAttachmentDescription);
            }
        }

        uint32_t depthStencilAttachmentIndex = VK_ATTACHMENT_UNUSED;

        if (m_useDepth || m_useStencil)
        {
            depthStencilAttachmentIndex = static_cast<uint32_t>(attachmentDescriptions.size());

            const VkAttachmentDescription depthStencilAttachmentDescription = {
                0u,                                            // VkAttachmentDescriptionFlags flags;
                m_depthStencilFormat,                          // VkFormat format;
                m_multisampleStateParams.rasterizationSamples, // VkSampleCountFlagBits samples;
                (m_useDepth ? VK_ATTACHMENT_LOAD_OP_CLEAR :
                              VK_ATTACHMENT_LOAD_OP_DONT_CARE), // VkAttachmentLoadOp loadOp;
                (m_useDepth ? VK_ATTACHMENT_STORE_OP_STORE :
                              VK_ATTACHMENT_STORE_OP_DONT_CARE), // VkAttachmentStoreOp storeOp;
                (m_useStencil ? VK_ATTACHMENT_LOAD_OP_CLEAR :
                                VK_ATTACHMENT_LOAD_OP_DONT_CARE), // VkAttachmentStoreOp stencilLoadOp;
                (m_useStencil ? VK_ATTACHMENT_STORE_OP_STORE :
                                VK_ATTACHMENT_STORE_OP_DONT_CARE), // VkAttachmentStoreOp stencilStoreOp;
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,  // VkImageLayout initialLayout;
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL   // VkImageLayout finalLayout;
            };
            attachmentDescriptions.push_back(depthStencilAttachmentDescription);
        }

        const VkAttachmentReference colorAttachmentReference = {
            0u,                                      // uint32_t attachment;
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL // VkImageLayout layout;
        };

        const VkAttachmentReference inputAttachmentReference = {
            0u,                                      // uint32_t attachment;
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL // VkImageLayout layout;
        };

        const VkAttachmentReference resolveAttachmentReference = {
            resolveAttachmentIndex,                  // uint32_t attachment;
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL // VkImageLayout layout;
        };

        const VkAttachmentReference colorAttachmentReferencesUnusedAttachment[] = {
            {
                VK_ATTACHMENT_UNUSED,     // uint32_t            attachment
                VK_IMAGE_LAYOUT_UNDEFINED // VkImageLayout    layout
            },
            {
                0u,                                      // uint32_t            attachment
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL // VkImageLayout    layout
            }};

        const VkAttachmentReference resolveAttachmentReferencesUnusedAttachment[] = {
            {
                VK_ATTACHMENT_UNUSED,     // uint32_t            attachment
                VK_IMAGE_LAYOUT_UNDEFINED // VkImageLayout    layout
            },
            {
                resolveAttachmentIndex,                  // uint32_t            attachment
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL // VkImageLayout    layout
            }};

        std::vector<VkAttachmentReference> perSampleAttachmentReferences(m_perSampleImages.size());
        if (m_renderType == RENDER_TYPE_COPY_SAMPLES)
        {
            for (size_t i = 0; i < m_perSampleImages.size(); ++i)
            {
                const VkAttachmentReference perSampleAttachmentReference = {
                    perSampleAttachmentIndex + static_cast<uint32_t>(i), // uint32_t attachment;
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL             // VkImageLayout layout;
                };
                perSampleAttachmentReferences[i] = perSampleAttachmentReference;
            }
        }

        const VkAttachmentReference depthStencilAttachmentReference = {
            depthStencilAttachmentIndex,                     // uint32_t attachment;
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL // VkImageLayout layout;
        };

        std::vector<VkSubpassDescription> subpassDescriptions;
        std::vector<VkSubpassDependency> subpassDependencies;

        if (m_renderType == RENDER_TYPE_DEPTHSTENCIL_ONLY)
        {
            const VkSubpassDescription subpassDescription0 = {
                0u,                               // VkSubpassDescriptionFlags    flags
                VK_PIPELINE_BIND_POINT_GRAPHICS,  // VkPipelineBindPoint            pipelineBindPoint
                0u,                               // uint32_t                        inputAttachmentCount
                nullptr,                          // const VkAttachmentReference*    pInputAttachments
                0u,                               // uint32_t                        colorAttachmentCount
                nullptr,                          // const VkAttachmentReference*    pColorAttachments
                nullptr,                          // const VkAttachmentReference*    pResolveAttachments
                &depthStencilAttachmentReference, // const VkAttachmentReference*    pDepthStencilAttachment
                0u,                               // uint32_t                        preserveAttachmentCount
                nullptr                           // const VkAttachmentReference*    pPreserveAttachments
            };

            const VkSubpassDescription subpassDescription1 = {
                0u,                               // VkSubpassDescriptionFlags    flags
                VK_PIPELINE_BIND_POINT_GRAPHICS,  // VkPipelineBindPoint            pipelineBindPoint
                0u,                               // uint32_t                        inputAttachmentCount
                nullptr,                          // const VkAttachmentReference*    pInputAttachments
                1u,                               // uint32_t                        colorAttachmentCount
                &colorAttachmentReference,        // const VkAttachmentReference*    pColorAttachments
                &resolveAttachmentReference,      // const VkAttachmentReference*    pResolveAttachments
                &depthStencilAttachmentReference, // const VkAttachmentReference*    pDepthStencilAttachment
                0u,                               // uint32_t                        preserveAttachmentCount
                nullptr                           // const VkAttachmentReference*    pPreserveAttachments
            };

            const VkSubpassDependency subpassDependency = {
                0u,                                           // uint32_t                srcSubpass
                1u,                                           // uint32_t                dstSubpass
                VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,    // VkPipelineStageFlags    srcStageMask
                VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,    // VkPipelineStageFlags    dstStageMask
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, // VkAccessFlags        srcAccessMask
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,  // VkAccessFlags        dstAccessMask
                0u                                            // VkDependencyFlags    dependencyFlags
            };

            subpassDescriptions.push_back(subpassDescription0);
            subpassDescriptions.push_back(subpassDescription1);
            subpassDependencies.push_back(subpassDependency);
        }
        else if (m_renderType == RENDER_TYPE_UNUSED_ATTACHMENT)
        {
            const VkSubpassDescription renderSubpassDescription = {
                0u,                                          // VkSubpassDescriptionFlags    flags
                VK_PIPELINE_BIND_POINT_GRAPHICS,             // VkPipelineBindPoint            pipelineBindPoint
                0u,                                          // uint32_t                        inputAttachmentCount
                nullptr,                                     // const VkAttachmentReference*    pInputAttachments
                2u,                                          // uint32_t                        colorAttachmentCount
                colorAttachmentReferencesUnusedAttachment,   // const VkAttachmentReference*    pColorAttachments
                resolveAttachmentReferencesUnusedAttachment, // const VkAttachmentReference*    pResolveAttachments
                nullptr,                                     // const VkAttachmentReference*    pDepthStencilAttachment
                0u,                                          // uint32_t                        preserveAttachmentCount
                nullptr                                      // const VkAttachmentReference*    pPreserveAttachments
            };

            subpassDescriptions.push_back(renderSubpassDescription);
        }
        else
        {
            {
                const VkSubpassDescription renderSubpassDescription = {
                    0u,                              // VkSubpassDescriptionFlags flags;
                    VK_PIPELINE_BIND_POINT_GRAPHICS, // VkPipelineBindPoint pipelineBindPoint;
                    0u,                              // uint32_t inputAttachmentCount;
                    nullptr,                         // const VkAttachmentReference* pInputAttachments;
                    1u,                              // uint32_t colorAttachmentCount;
                    &colorAttachmentReference,       // const VkAttachmentReference* pColorAttachments;
                    usesResolveImage ? &resolveAttachmentReference :
                                       nullptr, // const VkAttachmentReference* pResolveAttachments;
                    (m_useDepth || m_useStencil ? &depthStencilAttachmentReference :
                                                  nullptr), // const VkAttachmentReference* pDepthStencilAttachment;
                    0u,                                     // uint32_t preserveAttachmentCount;
                    nullptr                                 // const VkAttachmentReference* pPreserveAttachments;
                };
                subpassDescriptions.push_back(renderSubpassDescription);
            }

            if (m_renderType == RENDER_TYPE_COPY_SAMPLES)
            {

                for (size_t i = 0; i < m_perSampleImages.size(); ++i)
                {
                    const VkSubpassDescription copySampleSubpassDescription = {
                        0u,                                // VkSubpassDescriptionFlags flags;
                        VK_PIPELINE_BIND_POINT_GRAPHICS,   // VkPipelineBindPoint pipelineBindPoint;
                        1u,                                // uint32_t inputAttachmentCount;
                        &inputAttachmentReference,         // const VkAttachmentReference* pInputAttachments;
                        1u,                                // uint32_t colorAttachmentCount;
                        &perSampleAttachmentReferences[i], // const VkAttachmentReference* pColorAttachments;
                        nullptr,                           // const VkAttachmentReference* pResolveAttachments;
                        nullptr,                           // const VkAttachmentReference* pDepthStencilAttachment;
                        0u,                                // uint32_t preserveAttachmentCount;
                        nullptr                            // const VkAttachmentReference* pPreserveAttachments;
                    };
                    subpassDescriptions.push_back(copySampleSubpassDescription);

                    const VkSubpassDependency copySampleSubpassDependency = {
                        0u,                                            // uint32_t                            srcSubpass
                        1u + static_cast<uint32_t>(i),                 // uint32_t                            dstSubpass
                        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // VkPipelineStageFlags                srcStageMask
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, // VkPipelineStageFlags                dstStageMask
                        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,  // VkAccessFlags                    srcAccessMask
                        VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,   // VkAccessFlags                    dstAccessMask
                        0u,                                    // VkDependencyFlags                dependencyFlags
                    };
                    subpassDependencies.push_back(copySampleSubpassDependency);
                }
                // the very last sample pass must synchronize with all prior subpasses
                for (size_t i = 0; i < (m_perSampleImages.size() - 1); ++i)
                {
                    const VkSubpassDependency storeSubpassDependency = {
                        1u + static_cast<uint32_t>(i), // uint32_t                            srcSubpass
                        static_cast<uint32_t>(
                            m_perSampleImages.size()),                 // uint32_t                            dstSubpass
                        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // VkPipelineStageFlags                srcStageMask
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, // VkPipelineStageFlags                dstStageMask
                        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,  // VkAccessFlags                    srcAccessMask
                        VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,   // VkAccessFlags                    dstAccessMask
                        0u,                                    // VkDependencyFlags                dependencyFlags
                    };
                    subpassDependencies.push_back(storeSubpassDependency);
                }
            }
        }

        const VkRenderPassCreateInfo renderPassParams = {
            VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, // VkStructureType sType;
            nullptr,                                   // const void* pNext;
            0u,                                        // VkRenderPassCreateFlags flags;
            (uint32_t)attachmentDescriptions.size(),   // uint32_t attachmentCount;
            &attachmentDescriptions[0],                // const VkAttachmentDescription* pAttachments;
            (uint32_t)subpassDescriptions.size(),      // uint32_t subpassCount;
            &subpassDescriptions[0],                   // const VkSubpassDescription* pSubpasses;
            (uint32_t)subpassDependencies.size(),      // uint32_t dependencyCount;
            subpassDependencies.size() != 0 ? &subpassDependencies[0] : nullptr};

        m_renderPass = RenderPassWrapper(m_pipelineConstructionType, vk, vkDevice, &renderPassParams);
    }

    // Create framebuffer
    {
        std::vector<VkImage> images;
        std::vector<VkImageView> attachments;
        images.push_back(*m_colorImage);
        attachments.push_back(*m_colorAttachmentView);
        if (usesResolveImage)
        {
            images.push_back(*m_resolveImage);
            attachments.push_back(*m_resolveAttachmentView);
        }
        if (m_renderType == RENDER_TYPE_COPY_SAMPLES)
        {
            for (size_t i = 0; i < m_perSampleImages.size(); ++i)
            {
                images.push_back(*m_perSampleImages[i]->m_image);
                attachments.push_back(*m_perSampleImages[i]->m_attachmentView);
            }
        }

        if (m_useDepth || m_useStencil)
        {
            images.push_back(*m_depthStencilImage);
            attachments.push_back(*m_depthStencilAttachmentView);
        }

        const VkFramebufferCreateInfo framebufferParams = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // VkStructureType sType;
            nullptr,                                   // const void* pNext;
            0u,                                        // VkFramebufferCreateFlags flags;
            *m_renderPass,                             // VkRenderPass renderPass;
            (uint32_t)attachments.size(),              // uint32_t attachmentCount;
            &attachments[0],                           // const VkImageView* pAttachments;
            (uint32_t)m_renderSize.x(),                // uint32_t width;
            (uint32_t)m_renderSize.y(),                // uint32_t height;
            1u                                         // uint32_t layers;
        };

        m_renderPass.createFramebuffer(vk, vkDevice, &framebufferParams, images);
    }

    // Create pipeline layout
    {
        const VkPipelineLayoutCreateInfo pipelineLayoutParams = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType sType;
            nullptr,                                       // const void* pNext;
            0u,                                            // VkPipelineLayoutCreateFlags flags;
            0u,                                            // uint32_t setLayoutCount;
            nullptr,                                       // const VkDescriptorSetLayout* pSetLayouts;
            0u,                                            // uint32_t pushConstantRangeCount;
            nullptr                                        // const VkPushConstantRange* pPushConstantRanges;
        };

        m_pipelineLayout = PipelineLayoutWrapper(m_pipelineConstructionType, vk, vkDevice, &pipelineLayoutParams);

        if (m_renderType == RENDER_TYPE_COPY_SAMPLES)
        {

            // Create descriptor set layout
            const VkDescriptorSetLayoutBinding layoutBinding = {
                0u,                                  // uint32_t binding;
                VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, // VkDescriptorType descriptorType;
                1u,                                  // uint32_t descriptorCount;
                VK_SHADER_STAGE_FRAGMENT_BIT,        // VkShaderStageFlags stageFlags;
                nullptr,                             // const VkSampler* pImmutableSamplers;
            };

            const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutParams = {
                VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, // VkStructureType                        sType
                nullptr,                                             // const void*                            pNext
                0u,                                                  // VkDescriptorSetLayoutCreateFlags        flags
                1u,            // uint32_t                                bindingCount
                &layoutBinding // const VkDescriptorSetLayoutBinding*    pBindings
            };
            m_copySampleDesciptorLayout = createDescriptorSetLayout(vk, vkDevice, &descriptorSetLayoutParams);

            // Create pipeline layout

            const VkPushConstantRange pushConstantRange = {
                VK_SHADER_STAGE_FRAGMENT_BIT, // VkShaderStageFlags stageFlags;
                0u,                           // uint32_t offset;
                sizeof(int32_t)               // uint32_t size;
            };
            const VkPipelineLayoutCreateInfo copySamplePipelineLayoutParams = {
                VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType sType;
                nullptr,                                       // const void* pNext;
                0u,                                            // VkPipelineLayoutCreateFlags flags;
                1u,                                            // uint32_t setLayoutCount;
                &m_copySampleDesciptorLayout.get(),            // const VkDescriptorSetLayout* pSetLayouts;
                1u,                                            // uint32_t pushConstantRangeCount;
                &pushConstantRange                             // const VkPushConstantRange* pPushConstantRanges;
            };
            m_copySamplePipelineLayout =
                PipelineLayoutWrapper(m_pipelineConstructionType, vk, vkDevice, &copySamplePipelineLayoutParams);
        }
    }

    m_vertexShaderModule   = ShaderWrapper(vk, vkDevice, m_context.getBinaryCollection().get("color_vert"), 0);
    m_fragmentShaderModule = ShaderWrapper(vk, vkDevice, m_context.getBinaryCollection().get("color_frag"), 0);

    if (m_renderType == RENDER_TYPE_COPY_SAMPLES)
    {
        m_copySampleVertexShaderModule =
            ShaderWrapper(vk, vkDevice, m_context.getBinaryCollection().get("quad_vert"), 0);
        m_copySampleFragmentShaderModule =
            ShaderWrapper(vk, vkDevice, m_context.getBinaryCollection().get("copy_sample_frag"), 0);
    }

    // Create pipeline
    {
        const VkVertexInputBindingDescription vertexInputBindingDescription = {
            0u,                         // uint32_t binding;
            sizeof(Vertex4RGBA),        // uint32_t stride;
            VK_VERTEX_INPUT_RATE_VERTEX // VkVertexInputRate inputRate;
        };

        const VkVertexInputAttributeDescription vertexInputAttributeDescriptions[2] = {
            {
                0u,                            // uint32_t location;
                0u,                            // uint32_t binding;
                VK_FORMAT_R32G32B32A32_SFLOAT, // VkFormat format;
                0u                             // uint32_t offset;
            },
            {
                1u,                            // uint32_t location;
                0u,                            // uint32_t binding;
                VK_FORMAT_R32G32B32A32_SFLOAT, // VkFormat format;
                offsetof(Vertex4RGBA, color),  // uint32_t offset;
            }};

        const VkPipelineVertexInputStateCreateInfo vertexInputStateParams = {
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                   // const void* pNext;
            0u,                                                        // VkPipelineVertexInputStateCreateFlags flags;
            1u,                                                        // uint32_t vertexBindingDescriptionCount;
            &vertexInputBindingDescription,  // const VkVertexInputBindingDescription* pVertexBindingDescriptions;
            2u,                              // uint32_t vertexAttributeDescriptionCount;
            vertexInputAttributeDescriptions // const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
        };

        const std::vector<VkViewport> viewports{makeViewport(m_renderSize)};
        const std::vector<VkRect2D> scissors{makeRect2D(m_renderSize)};

        const uint32_t attachmentCount = m_renderType == RENDER_TYPE_UNUSED_ATTACHMENT ? 2u : 1u;

        std::vector<VkPipelineColorBlendAttachmentState> attachments;

        for (uint32_t attachmentIdx = 0; attachmentIdx < attachmentCount; attachmentIdx++)
            attachments.push_back(m_colorBlendState);

        VkPipelineColorBlendStateCreateInfo colorBlendStateParams = {
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                  // const void* pNext;
            0u,                                                       // VkPipelineColorBlendStateCreateFlags flags;
            false,                                                    // VkBool32 logicOpEnable;
            VK_LOGIC_OP_COPY,                                         // VkLogicOp logicOp;
            attachmentCount,                                          // uint32_t attachmentCount;
            attachments.data(),      // const VkPipelineColorBlendAttachmentState* pAttachments;
            {0.0f, 0.0f, 0.0f, 0.0f} // float blendConstants[4];
        };

        const VkStencilOpState stencilOpState = {
            VK_STENCIL_OP_KEEP,    // VkStencilOp failOp;
            VK_STENCIL_OP_REPLACE, // VkStencilOp passOp;
            VK_STENCIL_OP_KEEP,    // VkStencilOp depthFailOp;
            VK_COMPARE_OP_GREATER, // VkCompareOp compareOp;
            1u,                    // uint32_t compareMask;
            1u,                    // uint32_t writeMask;
            1u,                    // uint32_t reference;
        };

        const VkPipelineDepthStencilStateCreateInfo depthStencilStateParams = {
            VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                    // const void* pNext;
            0u,                                                         // VkPipelineDepthStencilStateCreateFlags flags;
            m_useDepth,                                                 // VkBool32 depthTestEnable;
            m_useDepth,                                                 // VkBool32 depthWriteEnable;
            VK_COMPARE_OP_LESS,                                         // VkCompareOp depthCompareOp;
            false,                                                      // VkBool32 depthBoundsTestEnable;
            m_useStencil,                                               // VkBool32 stencilTestEnable;
            stencilOpState,                                             // VkStencilOpState front;
            stencilOpState,                                             // VkStencilOpState back;
            0.0f,                                                       // float minDepthBounds;
            1.0f,                                                       // float maxDepthBounds;
        };

        const VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo{
            VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, // VkStructureType                                sType
            m_useConservative ? &m_rasterizationConservativeStateCreateInfo :
                                nullptr,     // const void*                                    pNext
            0u,                              // VkPipelineRasterizationStateCreateFlags        flags
            VK_FALSE,                        // VkBool32                                        depthClampEnable
            VK_FALSE,                        // VkBool32                                        rasterizerDiscardEnable
            VK_POLYGON_MODE_FILL,            // VkPolygonMode                                polygonMode
            VK_CULL_MODE_NONE,               // VkCullModeFlags                                cullMode
            VK_FRONT_FACE_COUNTER_CLOCKWISE, // VkFrontFace                                    frontFace
            VK_FALSE,                        // VkBool32                                        depthBiasEnable
            0.0f,                            // float                                        depthBiasConstantFactor
            0.0f,                            // float                                        depthBiasClamp
            0.0f,                            // float                                        depthBiasSlopeFactor
            1.0f                             // float                                        lineWidth
        };

        VkPipelineFragmentShadingRateStateCreateInfoKHR shadingRateStateCreateInfo{
            VK_STRUCTURE_TYPE_PIPELINE_FRAGMENT_SHADING_RATE_STATE_CREATE_INFO_KHR, // VkStructureType sType;
            nullptr,                                                                // const void* pNext;
            {2, 2},                                                                 // VkExtent2D fragmentSize;
            {VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR,
             VK_FRAGMENT_SHADING_RATE_COMBINER_OP_KEEP_KHR}, // VkFragmentShadingRateCombinerOpKHR combinerOps[2];
        };

        const uint32_t numSubpasses = m_renderType == RENDER_TYPE_DEPTHSTENCIL_ONLY ? 2u : 1u;

        m_graphicsPipelines.reserve(numSubpasses * numTopologies);
        for (uint32_t subpassIdx = 0; subpassIdx < numSubpasses; subpassIdx++)
        {
            if (m_renderType == RENDER_TYPE_DEPTHSTENCIL_ONLY)
            {
                if (subpassIdx == 0)
                {
                    colorBlendStateParams.attachmentCount = 0;
                }
                else
                {
                    colorBlendStateParams.attachmentCount = 1;
                }
            }
            for (uint32_t i = 0u; i < numTopologies; ++i)
            {
                m_graphicsPipelines.emplace_back(vki, vk, physicalDevice, vkDevice, context.getDeviceExtensions(),
                                                 m_pipelineConstructionType);
                m_graphicsPipelines.back()
                    .setDefaultTopology(pTopology[i])
                    .setupVertexInputState(&vertexInputStateParams)
                    .setupPreRasterizationShaderState(
                        viewports, scissors, m_pipelineLayout, *m_renderPass, subpassIdx, m_vertexShaderModule,
                        &rasterizationStateCreateInfo, ShaderWrapper(), ShaderWrapper(), ShaderWrapper(), nullptr,
                        (m_useFragmentShadingRate ? &shadingRateStateCreateInfo : nullptr))
                    .setupFragmentShaderState(m_pipelineLayout, *m_renderPass, subpassIdx, m_fragmentShaderModule,
                                              &depthStencilStateParams, &m_multisampleStateParams)
                    .setupFragmentOutputState(*m_renderPass, subpassIdx, &colorBlendStateParams,
                                              &m_multisampleStateParams)
                    .setMonolithicPipelineLayout(m_pipelineLayout)
                    .buildPipeline();
            }
        }
    }

    if (m_renderType == RENDER_TYPE_COPY_SAMPLES)
    {
        // Create pipelines for copying samples to single sampled images
        {
            const VkPipelineVertexInputStateCreateInfo vertexInputStateParams{
                VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType sType;
                nullptr,                                                   // const void* pNext;
                0u,      // VkPipelineVertexInputStateCreateFlags flags;
                0u,      // uint32_t vertexBindingDescriptionCount;
                nullptr, // const VkVertexInputBindingDescription* pVertexBindingDescriptions;
                0u,      // uint32_t vertexAttributeDescriptionCount;
                nullptr  // const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
            };

            const std::vector<VkViewport> viewports{makeViewport(m_renderSize)};
            const std::vector<VkRect2D> scissors{makeRect2D(m_renderSize)};

            const VkPipelineColorBlendStateCreateInfo colorBlendStateParams{
                VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, // VkStructureType sType;
                nullptr,                                                  // const void* pNext;
                0u,                                                       // VkPipelineColorBlendStateCreateFlags flags;
                false,                                                    // VkBool32 logicOpEnable;
                VK_LOGIC_OP_COPY,                                         // VkLogicOp logicOp;
                1u,                                                       // uint32_t attachmentCount;
                &m_colorBlendState,      // const VkPipelineColorBlendAttachmentState* pAttachments;
                {0.0f, 0.0f, 0.0f, 0.0f} // float blendConstants[4];
            };

            m_copySamplePipelines.reserve(m_perSampleImages.size());
            for (size_t i = 0; i < m_perSampleImages.size(); ++i)
            {
                // Pipeline is to be used in subpasses subsequent to sample-shading subpass

                const uint32_t subpassIdx = 1u + (uint32_t)i;
                m_copySamplePipelines.emplace_back(vki, vk, physicalDevice, vkDevice, m_context.getDeviceExtensions(),
                                                   m_pipelineConstructionType);
                m_copySamplePipelines.back()
                    .setDefaultTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
                    .setDefaultRasterizationState()
                    .setDefaultMultisampleState()
                    .setDefaultDepthStencilState()
                    .setupVertexInputState(&vertexInputStateParams)
                    .setupPreRasterizationShaderState(viewports, scissors, m_copySamplePipelineLayout, *m_renderPass,
                                                      subpassIdx, m_copySampleVertexShaderModule)
                    .setupFragmentShaderState(m_copySamplePipelineLayout, *m_renderPass, subpassIdx,
                                              m_copySampleFragmentShaderModule)
                    .setupFragmentOutputState(*m_renderPass, subpassIdx, &colorBlendStateParams)
                    .setMonolithicPipelineLayout(m_copySamplePipelineLayout)
                    .buildPipeline();
            }
        }

        const VkDescriptorPoolSize descriptorPoolSize{
            VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, // VkDescriptorType type;
            1u                                   // uint32_t descriptorCount;
        };

        const VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{
            VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,     // VkStructureType                    sType
            nullptr,                                           // const void*                        pNext
            VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, // VkDescriptorPoolCreateFlags        flags
            1u,                                                // uint32_t                            maxSets
            1u,                                                // uint32_t                            poolSizeCount
            &descriptorPoolSize                                // const VkDescriptorPoolSize*        pPoolSizes
        };

        m_copySampleDesciptorPool = createDescriptorPool(vk, vkDevice, &descriptorPoolCreateInfo);

        const VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, // VkStructureType                    sType
            nullptr,                                        // const void*                        pNext
            *m_copySampleDesciptorPool,                     // VkDescriptorPool                    descriptorPool
            1u,                                             // uint32_t                            descriptorSetCount
            &m_copySampleDesciptorLayout.get(),             // const VkDescriptorSetLayout*        pSetLayouts
        };

        m_copySampleDesciptorSet = allocateDescriptorSet(vk, vkDevice, &descriptorSetAllocateInfo);

        const VkDescriptorImageInfo imageInfo{VK_NULL_HANDLE, *m_colorAttachmentView,
                                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        const VkWriteDescriptorSet descriptorWrite{
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, // VkStructureType sType;
            nullptr,                                // const void* pNext;
            *m_copySampleDesciptorSet,              // VkDescriptorSet dstSet;
            0u,                                     // uint32_t dstBinding;
            0u,                                     // uint32_t dstArrayElement;
            1u,                                     // uint32_t descriptorCount;
            VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,    // VkDescriptorType descriptorType;
            &imageInfo,                             // const VkDescriptorImageInfo* pImageInfo;
            nullptr,                                // const VkDescriptorBufferInfo* pBufferInfo;
            nullptr,                                // const VkBufferView* pTexelBufferView;
        };
        vk.updateDescriptorSets(vkDevice, 1u, &descriptorWrite, 0u, nullptr);
    }

    // Create vertex buffer
    {
        const VkBufferCreateInfo vertexBufferParams{
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
            nullptr,                              // const void* pNext;
            0u,                                   // VkBufferCreateFlags flags;
            1024u,                                // VkDeviceSize size;
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,    // VkBufferUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
            1u,                                   // uint32_t queueFamilyIndexCount;
            &queueFamilyIndices[0]                // const uint32_t* pQueueFamilyIndices;
        };

        m_vertexBuffer      = createBuffer(vk, vkDevice, &vertexBufferParams);
        m_vertexBufferAlloc = memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_vertexBuffer),
                                                MemoryRequirement::HostVisible);

        VK_CHECK(vk.bindBufferMemory(vkDevice, *m_vertexBuffer, m_vertexBufferAlloc->getMemory(),
                                     m_vertexBufferAlloc->getOffset()));

        // Load vertices into vertex buffer
        {
            Vertex4RGBA *pDst = static_cast<Vertex4RGBA *>(m_vertexBufferAlloc->getHostPtr());

            if (m_renderType == RENDER_TYPE_DEPTHSTENCIL_ONLY)
            {
                DE_ASSERT(numTopologies == 1);

                std::vector<Vertex4RGBA> vertices = pVertices[0];

                // Set alpha to zero for the first draw. This should prevent depth writes because of zero coverage.
                for (size_t i = 0; i < vertices.size(); i++)
                    vertices[i].color.w() = 0.0f;

                deMemcpy(pDst, &vertices[0], vertices.size() * sizeof(Vertex4RGBA));

                pDst += vertices.size();

                // The second draw uses original vertices which are pure red.
                deMemcpy(pDst, &pVertices[0][0], pVertices[0].size() * sizeof(Vertex4RGBA));
            }
            else
            {
                for (uint32_t i = 0u; i < numTopologies; ++i)
                {
                    deMemcpy(pDst, &pVertices[i][0], pVertices[i].size() * sizeof(Vertex4RGBA));
                    pDst += pVertices[i].size();
                }
            }
        }
        flushAlloc(vk, vkDevice, *m_vertexBufferAlloc);
    }

    // Create command pool
    m_cmdPool = createCommandPool(vk, vkDevice, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndices[0]);

    // Create command buffer
    {
        VkClearValue colorClearValue;
        if (m_renderType == RENDER_TYPE_DEPTHSTENCIL_ONLY)
        {
            colorClearValue.color.float32[0] = 0.25;
            colorClearValue.color.float32[1] = 0.25;
            colorClearValue.color.float32[2] = 0.25;
            colorClearValue.color.float32[3] = 1.0f;
        }
        else
        {
            colorClearValue.color.float32[0] = 0.0f;
            colorClearValue.color.float32[1] = 0.0f;
            colorClearValue.color.float32[2] = 0.0f;
            colorClearValue.color.float32[3] = 0.0f;
        }

        VkClearValue depthStencilClearValue;
        depthStencilClearValue.depthStencil.depth   = m_depthClearValue;
        depthStencilClearValue.depthStencil.stencil = 0u;

        std::vector<VkClearValue> clearValues;
        clearValues.push_back(colorClearValue);
        if (usesResolveImage)
        {
            clearValues.push_back(colorClearValue);
        }
        if (m_renderType == RENDER_TYPE_COPY_SAMPLES)
        {
            for (size_t i = 0; i < m_perSampleImages.size(); ++i)
            {
                clearValues.push_back(colorClearValue);
            }
        }
        if (m_useDepth || m_useStencil)
        {
            clearValues.push_back(depthStencilClearValue);
        }

        vk::VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        std::vector<VkImageMemoryBarrier> imageLayoutBarriers;

        {
            const VkImageMemoryBarrier colorImageBarrier =
                // color attachment image
                {
                    VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,      // VkStructureType sType;
                    nullptr,                                     // const void* pNext;
                    0u,                                          // VkAccessFlags srcAccessMask;
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,        // VkAccessFlags dstAccessMask;
                    VK_IMAGE_LAYOUT_UNDEFINED,                   // VkImageLayout oldLayout;
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,    // VkImageLayout newLayout;
                    VK_QUEUE_FAMILY_IGNORED,                     // uint32_t srcQueueFamilyIndex;
                    VK_QUEUE_FAMILY_IGNORED,                     // uint32_t dstQueueFamilyIndex;
                    *m_colorImage,                               // VkImage image;
                    {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u}, // VkImageSubresourceRange subresourceRange;
                };
            imageLayoutBarriers.push_back(colorImageBarrier);
        }
        if (usesResolveImage)
        {
            const VkImageMemoryBarrier resolveImageBarrier =
                // resolve attachment image
                {
                    VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,      // VkStructureType sType;
                    nullptr,                                     // const void* pNext;
                    0u,                                          // VkAccessFlags srcAccessMask;
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,        // VkAccessFlags dstAccessMask;
                    VK_IMAGE_LAYOUT_UNDEFINED,                   // VkImageLayout oldLayout;
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,    // VkImageLayout newLayout;
                    VK_QUEUE_FAMILY_IGNORED,                     // uint32_t srcQueueFamilyIndex;
                    VK_QUEUE_FAMILY_IGNORED,                     // uint32_t dstQueueFamilyIndex;
                    *m_resolveImage,                             // VkImage image;
                    {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u}, // VkImageSubresourceRange subresourceRange;
                };
            imageLayoutBarriers.push_back(resolveImageBarrier);
        }
        if (m_renderType == RENDER_TYPE_COPY_SAMPLES)
        {
            for (size_t i = 0; i < m_perSampleImages.size(); ++i)
            {
                const VkImageMemoryBarrier perSampleImageBarrier =
                    // resolve attachment image
                    {
                        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,      // VkStructureType sType;
                        nullptr,                                     // const void* pNext;
                        0u,                                          // VkAccessFlags srcAccessMask;
                        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,        // VkAccessFlags dstAccessMask;
                        VK_IMAGE_LAYOUT_UNDEFINED,                   // VkImageLayout oldLayout;
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,    // VkImageLayout newLayout;
                        VK_QUEUE_FAMILY_IGNORED,                     // uint32_t srcQueueFamilyIndex;
                        VK_QUEUE_FAMILY_IGNORED,                     // uint32_t dstQueueFamilyIndex;
                        *m_perSampleImages[i]->m_image,              // VkImage image;
                        {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u}, // VkImageSubresourceRange subresourceRange;
                    };
                imageLayoutBarriers.push_back(perSampleImageBarrier);
            }
        }
        if (m_useDepth || m_useStencil)
        {
            const VkImageMemoryBarrier depthStencilImageBarrier =
                // depth/stencil attachment image
                {
                    VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,           // VkStructureType sType;
                    nullptr,                                          // const void* pNext;
                    0u,                                               // VkAccessFlags srcAccessMask;
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,     // VkAccessFlags dstAccessMask;
                    VK_IMAGE_LAYOUT_UNDEFINED,                        // VkImageLayout oldLayout;
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, // VkImageLayout newLayout;
                    VK_QUEUE_FAMILY_IGNORED,                          // uint32_t srcQueueFamilyIndex;
                    VK_QUEUE_FAMILY_IGNORED,                          // uint32_t dstQueueFamilyIndex;
                    *m_depthStencilImage,                             // VkImage image;
                    {depthStencilAttachmentAspect, 0u, 1u, 0u, 1u},   // VkImageSubresourceRange subresourceRange;
                };
            imageLayoutBarriers.push_back(depthStencilImageBarrier);
            dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        }

        m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        beginCommandBuffer(vk, *m_cmdBuffer, 0u);

        vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, dstStageMask, (VkDependencyFlags)0, 0u,
                              nullptr, 0u, nullptr, (uint32_t)imageLayoutBarriers.size(), &imageLayoutBarriers[0]);

        m_renderPass.begin(vk, *m_cmdBuffer, makeRect2D(0, 0, m_renderSize.x(), m_renderSize.y()),
                           (uint32_t)clearValues.size(), &clearValues[0]);

        VkDeviceSize vertexBufferOffset = 0u;

        for (uint32_t i = 0u; i < numTopologies; ++i)
        {
            m_graphicsPipelines[i].bind(*m_cmdBuffer);
            vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);
            vk.cmdDraw(*m_cmdBuffer, (uint32_t)pVertices[i].size(), 1, 0, 0);

            vertexBufferOffset += static_cast<VkDeviceSize>(pVertices[i].size() * sizeof(Vertex4RGBA));
        }

        if (m_renderType == RENDER_TYPE_DEPTHSTENCIL_ONLY)
        {
            // The first draw was without color buffer and zero coverage. The depth buffer is expected to still have the clear value.
            m_renderPass.nextSubpass(vk, *m_cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);
            m_graphicsPipelines[1].bind(*m_cmdBuffer);
            vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);
            // The depth test should pass as the first draw didn't touch the depth buffer.
            vk.cmdDraw(*m_cmdBuffer, (uint32_t)pVertices[0].size(), 1, 0, 0);
        }
        else if (m_renderType == RENDER_TYPE_COPY_SAMPLES)
        {
            // Copy each sample id to single sampled image
            for (int32_t sampleId = 0; sampleId < (int32_t)m_perSampleImages.size(); ++sampleId)
            {
                m_renderPass.nextSubpass(vk, *m_cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);
                m_copySamplePipelines[sampleId].bind(*m_cmdBuffer);
                vk.cmdBindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_copySamplePipelineLayout, 0u,
                                         1u, &m_copySampleDesciptorSet.get(), 0u, nullptr);
                vk.cmdPushConstants(*m_cmdBuffer, *m_copySamplePipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                    sizeof(int32_t), &sampleId);
                vk.cmdDraw(*m_cmdBuffer, 4, 1, 0, 0);
            }
        }

        m_renderPass.end(vk, *m_cmdBuffer);

        endCommandBuffer(vk, *m_cmdBuffer);
    }
}

MultisampleRenderer::~MultisampleRenderer(void)
{
}

de::MovePtr<tcu::TextureLevel> MultisampleRenderer::render(void)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice vkDevice         = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();

    if (m_backingMode == IMAGE_BACKING_MODE_SPARSE)
    {
        const VkPipelineStageFlags stageBits[] = {VK_PIPELINE_STAGE_TRANSFER_BIT};
        submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get(), false, 1u, 1u, &m_bindSemaphore.get(), stageBits);
    }
    else
    {
        submitCommandsAndWait(vk, vkDevice, queue, m_cmdBuffer.get());
    }

    if (m_renderType == RENDER_TYPE_RESOLVE || m_renderType == RENDER_TYPE_DEPTHSTENCIL_ONLY ||
        m_renderType == RENDER_TYPE_UNUSED_ATTACHMENT)
    {
        return readColorAttachment(vk, vkDevice, queue, queueFamilyIndex, m_context.getDefaultAllocator(),
                                   *m_resolveImage, m_colorFormat, m_renderSize.cast<uint32_t>());
    }
    else if (m_renderType == RENDER_TYPE_SINGLE_SAMPLE)
    {
        return readColorAttachment(vk, vkDevice, queue, queueFamilyIndex, m_context.getDefaultAllocator(),
                                   *m_colorImage, m_colorFormat, m_renderSize.cast<uint32_t>());
    }
    else
    {
        return de::MovePtr<tcu::TextureLevel>();
    }
}

de::MovePtr<tcu::TextureLevel> MultisampleRenderer::getSingleSampledImage(uint32_t sampleId)
{
    return readColorAttachment(m_context.getDeviceInterface(), m_context.getDevice(), m_context.getUniversalQueue(),
                               m_context.getUniversalQueueFamilyIndex(), m_context.getDefaultAllocator(),
                               *m_perSampleImages[sampleId]->m_image, m_colorFormat, m_renderSize.cast<uint32_t>());
}

de::MovePtr<tcu::TextureLevel> MultisampleRenderer::renderReusingDepth()
{
    const auto ctx          = m_context.getContextCommonData();
    const auto renderSize   = m_renderSize.cast<uint32_t>();
    const auto scissor      = makeRect2D(renderSize);
    const auto fbExtent     = makeExtent3D(scissor.extent.width, scissor.extent.height, 1u);
    const auto colorUsage   = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const auto sampleCount  = m_multisampleStateParams.rasterizationSamples;
    const auto singleSample = VK_SAMPLE_COUNT_1_BIT;
    const auto bindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;

    ImageWithBuffer secondColorBuffer(ctx.vkd, ctx.device, ctx.allocator, fbExtent, m_colorFormat, colorUsage,
                                      VK_IMAGE_TYPE_2D, makeDefaultImageSubresourceRange(), 1u, sampleCount);
    ImageWithBuffer secondResolveBuffer(ctx.vkd, ctx.device, ctx.allocator, fbExtent, m_colorFormat, colorUsage,
                                        VK_IMAGE_TYPE_2D, makeDefaultImageSubresourceRange(), 1u, singleSample);

    const auto pcSize         = static_cast<uint32_t>(sizeof(float));
    const auto pcStages       = VK_SHADER_STAGE_VERTEX_BIT;
    const auto pcRange        = makePushConstantRange(pcStages, 0u, pcSize);
    const auto pipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, VK_NULL_HANDLE, &pcRange);

    const std::vector<VkAttachmentDescription> attachmentDescriptions{
        {
            // Color attachment.
            0u,                                       // VkAttachmentDescriptionFlags flags;
            m_colorFormat,                            // VkFormat format;
            sampleCount,                              // VkSampleCountFlagBits samples;
            VK_ATTACHMENT_LOAD_OP_CLEAR,              // VkAttachmentLoadOp loadOp;
            VK_ATTACHMENT_STORE_OP_DONT_CARE,         // VkAttachmentStoreOp storeOp;
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,          // VkAttachmentLoadOp stencilLoadOp;
            VK_ATTACHMENT_STORE_OP_DONT_CARE,         // VkAttachmentStoreOp stencilStoreOp;
            VK_IMAGE_LAYOUT_UNDEFINED,                // VkImageLayout initialLayout;
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout finalLayout;
        },
        {
            // Depth/stencil attachment.
            0u,                                               // VkAttachmentDescriptionFlags flags;
            m_depthStencilFormat,                             // VkFormat format;
            sampleCount,                                      // VkSampleCountFlagBits samples;
            VK_ATTACHMENT_LOAD_OP_LOAD,                       // VkAttachmentLoadOp loadOp;
            VK_ATTACHMENT_STORE_OP_DONT_CARE,                 // VkAttachmentStoreOp storeOp;
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,                  // VkAttachmentLoadOp stencilLoadOp;
            VK_ATTACHMENT_STORE_OP_DONT_CARE,                 // VkAttachmentStoreOp stencilStoreOp;
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, // VkImageLayout initialLayout;
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, // VkImageLayout finalLayout;
        },
        {
            // Resolve attachment.
            0u,                                       // VkAttachmentDescriptionFlags flags;
            m_colorFormat,                            // VkFormat format;
            singleSample,                             // VkSampleCountFlagBits samples;
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,          // VkAttachmentLoadOp loadOp;
            VK_ATTACHMENT_STORE_OP_STORE,             // VkAttachmentStoreOp storeOp;
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,          // VkAttachmentLoadOp stencilLoadOp;
            VK_ATTACHMENT_STORE_OP_DONT_CARE,         // VkAttachmentStoreOp stencilStoreOp;
            VK_IMAGE_LAYOUT_UNDEFINED,                // VkImageLayout initialLayout;
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout finalLayout;
        },
    };

    const auto colorAttachmentReference = makeAttachmentReference(0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    const auto dsAttachmentReference    = makeAttachmentReference(1u, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    const auto resolveAttachmentReference = makeAttachmentReference(2u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    const VkSubpassDescription subpassDescription = {
        0u,                          // VkSubpassDescriptionFlags flags;
        bindPoint,                   // VkPipelineBindPoint pipelineBindPoint;
        0u,                          // uint32_t inputAttachmentCount;
        nullptr,                     // const VkAttachmentReference* pInputAttachments;
        1u,                          // uint32_t colorAttachmentCount;
        &colorAttachmentReference,   // const VkAttachmentReference* pColorAttachments;
        &resolveAttachmentReference, // const VkAttachmentReference* pResolveAttachments;
        &dsAttachmentReference,      // const VkAttachmentReference* pDepthStencilAttachment;
        0u,                          // uint32_t preserveAttachmentCount;
        nullptr,                     // const uint32_t* pPreserveAttachments;
    };

    const VkRenderPassCreateInfo rpCreateInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, // VkStructureType sType;
        nullptr,                                   // const void* pNext;
        0u,                                        // VkRenderPassCreateFlags flags;
        de::sizeU32(attachmentDescriptions),       // uint32_t attachmentCount;
        de::dataOrNull(attachmentDescriptions),    // const VkAttachmentDescription* pAttachments;
        1u,                                        // uint32_t subpassCount;
        &subpassDescription,                       // const VkSubpassDescription* pSubpasses;
        0u,                                        // uint32_t dependencyCount;
        nullptr,                                   // const VkSubpassDependency* pDependencies;
    };
    const auto renderPass = createRenderPass(ctx.vkd, ctx.device, &rpCreateInfo);

    const std::vector<VkImageView> fbImageViews{
        secondColorBuffer.getImageView(),
        *m_depthStencilAttachmentView,
        secondResolveBuffer.getImageView(),
    };
    const auto framebuffer = makeFramebuffer(ctx.vkd, ctx.device, renderPass.get(), de::sizeU32(fbImageViews),
                                             de::dataOrNull(fbImageViews), fbExtent.width, fbExtent.height);

    const std::vector<VkViewport> viewports(1u, makeViewport(fbExtent));
    const std::vector<VkRect2D> scissors(1u, scissor);
    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = initVulkanStructure();
    const auto stencilOpState =
        makeStencilOpState(VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_NEVER, 0u, 0u, 0u);

    // This is the key to test the depth buffer contains the clear value and has not been written to:
    // The comparison op is EQUAL, so we will only draw if the depth buffer contains the expected value.
    const VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                    // const void* pNext;
        0u,                                                         // VkPipelineDepthStencilStateCreateFlags flags;
        VK_TRUE,                                                    // VkBool32 depthTestEnable;
        VK_FALSE,                                                   // VkBool32 depthWriteEnable;
        VK_COMPARE_OP_EQUAL,                                        // VkCompareOp depthCompareOp;
        VK_FALSE,                                                   // VkBool32 depthBoundsTestEnable;
        VK_FALSE,                                                   // VkBool32 stencilTestEnable;
        stencilOpState,                                             // VkStencilOpState front;
        stencilOpState,                                             // VkStencilOpState back;
        0.0f,                                                       // float minDepthBounds;
        1.0f,                                                       // float maxDepthBounds;
    };

    const VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                  // const void* pNext;
        0u,                                                       // VkPipelineMultisampleStateCreateFlags flags;
        sampleCount,                                              // VkSampleCountFlagBits rasterizationSamples;
        VK_FALSE,                                                 // VkBool32 sampleShadingEnable;
        1.0f,                                                     // float minSampleShading;
        nullptr,                                                  // const VkSampleMask* pSampleMask;
        VK_FALSE,                                                 // VkBool32 alphaToCoverageEnable;
        VK_FALSE,                                                 // VkBool32 alphaToOneEnable;
    };

    const auto &binaries  = m_context.getBinaryCollection();
    const auto vertModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("checkDepth-vert"));
    const auto fragModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("color_frag"));
    const auto pipeline   = makeGraphicsPipeline(
        ctx.vkd, ctx.device, pipelineLayout.get(), vertModule.get(), VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
        fragModule.get(), renderPass.get(), viewports, scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0u, 0u,
        &vertexInputStateCreateInfo, nullptr, &multisampleStateCreateInfo, &depthStencilStateCreateInfo);

    const CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = cmd.cmdBuffer.get();
    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    {
        // Make sure the previous depth buffer writes have completed already.
        const auto depthBarrier = makeMemoryBarrier(
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT));
        const auto depthStages =
            (VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
        cmdPipelineMemoryBarrier(ctx.vkd, cmdBuffer, depthStages, depthStages, &depthBarrier);
    }
    beginRenderPass(ctx.vkd, cmdBuffer, renderPass.get(), framebuffer.get(), scissor, clearColor);
    ctx.vkd.cmdBindPipeline(cmdBuffer, bindPoint, pipeline.get());
    ctx.vkd.cmdPushConstants(cmdBuffer, pipelineLayout.get(), pcStages, 0u, pcSize, &m_depthClearValue);
    ctx.vkd.cmdDraw(cmdBuffer, 3u, 1u, 0u, 0u);
    endRenderPass(ctx.vkd, cmdBuffer);
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    return readColorAttachment(ctx.vkd, ctx.device, ctx.queue, ctx.qfIndex, ctx.allocator,
                               secondResolveBuffer.getImage(), m_colorFormat, renderSize);
}

// Multisample tests with subpasses using no attachments.
class VariableRateTestCase : public vkt::TestCase
{
public:
    using SampleCounts = std::vector<vk::VkSampleCountFlagBits>;

    struct PushConstants
    {
        int width;
        int height;
        int samples;
    };

    struct TestParams
    {
        PipelineConstructionType pipelineConstructionType; // The way pipeline is constructed.
        bool nonEmptyFramebuffer;                          // Empty framebuffer or not.
        vk::VkSampleCountFlagBits fbCount;                 // If not empty, framebuffer sample count.
        bool unusedAttachment;                             // If not empty, create unused attachment or not.
        SampleCounts subpassCounts;                        // Counts for the different subpasses.
        bool useFragmentShadingRate;                       // Use pipeline fragment shading rate.
    };

    static const int32_t kWidth  = 256u;
    static const int32_t kHeight = 256u;

    VariableRateTestCase(tcu::TestContext &testCtx, const std::string &name, const TestParams &params);
    virtual ~VariableRateTestCase(void)
    {
    }

    virtual void initPrograms(vk::SourceCollections &programCollection) const;
    virtual TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;

    static constexpr vk::VkFormat kColorFormat = vk::VK_FORMAT_R8G8B8A8_UNORM;

private:
    TestParams m_params;
};

class VariableRateTestInstance : public vkt::TestInstance
{
public:
    using TestParams = VariableRateTestCase::TestParams;

    VariableRateTestInstance(Context &context, const TestParams &counts);
    virtual ~VariableRateTestInstance(void)
    {
    }

    virtual tcu::TestStatus iterate(void);

private:
    TestParams m_params;
};

VariableRateTestCase::VariableRateTestCase(tcu::TestContext &testCtx, const std::string &name, const TestParams &params)
    : vkt::TestCase(testCtx, name)
    , m_params(params)
{
}

void VariableRateTestCase::initPrograms(vk::SourceCollections &programCollection) const
{
    std::stringstream vertSrc;

    vertSrc << "#version 450\n"
            << "\n"
            << "layout(location=0) in vec2 inPos;\n"
            << "\n"
            << "void main() {\n"
            << "    gl_Position = vec4(inPos, 0.0, 1.0);\n"
            << "}\n";

    std::stringstream fragSrc;

    fragSrc
        << "#version 450\n"
        << "\n"
        << "layout(set=0, binding=0, std430) buffer OutBuffer {\n"
        << "    int coverage[];\n"
        << "} out_buffer;\n"
        << "\n"
        << "layout(push_constant) uniform PushConstants {\n"
        << "    int width;\n"
        << "    int height;\n"
        << "    int samples;\n"
        << "} push_constants;\n"
        << "\n"
        << "void main() {\n"
        << "   ivec2 coord = ivec2(floor(gl_FragCoord.xy));\n"
        << "   int pos = ((coord.y * push_constants.width) + coord.x) * push_constants.samples + int(gl_SampleID);\n"
        << "   out_buffer.coverage[pos] = 1;\n"
        << "}\n";

    programCollection.glslSources.add("vert") << glu::VertexSource(vertSrc.str());
    programCollection.glslSources.add("frag") << glu::FragmentSource(fragSrc.str());
}

TestInstance *VariableRateTestCase::createInstance(Context &context) const
{
    return new VariableRateTestInstance(context, m_params);
}

void VariableRateTestCase::checkSupport(Context &context) const
{
    const auto &vki           = context.getInstanceInterface();
    const auto physicalDevice = context.getPhysicalDevice();

    // When using multiple subpasses, require variableMultisampleRate.
    if (m_params.subpassCounts.size() > 1)
    {
        if (!vk::getPhysicalDeviceFeatures(vki, physicalDevice).variableMultisampleRate)
            TCU_THROW(NotSupportedError, "Variable multisample rate not supported");
    }

    // Check if sampleRateShading is supported.
    if (!vk::getPhysicalDeviceFeatures(vki, physicalDevice).sampleRateShading)
        TCU_THROW(NotSupportedError, "Sample rate shading is not supported");

    // Make sure all subpass sample counts are supported.
    const auto properties       = vk::getPhysicalDeviceProperties(vki, physicalDevice);
    const auto &supportedCounts = properties.limits.framebufferNoAttachmentsSampleCounts;

    for (const auto count : m_params.subpassCounts)
    {
        if ((supportedCounts & count) == 0u)
            TCU_THROW(NotSupportedError, "Sample count combination not supported");
    }

    if (m_params.nonEmptyFramebuffer)
    {
        // Check the framebuffer sample count is supported.
        const auto formatProperties = vk::getPhysicalDeviceImageFormatProperties(
            vki, physicalDevice, kColorFormat, vk::VK_IMAGE_TYPE_2D, vk::VK_IMAGE_TILING_OPTIMAL,
            vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 0u);
        if ((formatProperties.sampleCounts & m_params.fbCount) == 0u)
            TCU_THROW(NotSupportedError,
                      "Sample count of " + de::toString(m_params.fbCount) + " not supported for color attachment");
    }

    if (m_params.useFragmentShadingRate && !checkFragmentShadingRateRequirements(context, m_params.fbCount))
        TCU_THROW(NotSupportedError, "Required FragmentShadingRate not supported");

    checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                          m_params.pipelineConstructionType);
}

void zeroOutAndFlush(const vk::DeviceInterface &vkd, vk::VkDevice device, vk::BufferWithMemory &buffer,
                     vk::VkDeviceSize size)
{
    auto &alloc = buffer.getAllocation();
    deMemset(alloc.getHostPtr(), 0, static_cast<size_t>(size));
    vk::flushAlloc(vkd, device, alloc);
}

VariableRateTestInstance::VariableRateTestInstance(Context &context, const TestParams &params)
    : vkt::TestInstance(context)
    , m_params(params)
{
}

tcu::TestStatus VariableRateTestInstance::iterate(void)
{
    using PushConstants = VariableRateTestCase::PushConstants;

    const auto &vki       = m_context.getInstanceInterface();
    const auto &vkd       = m_context.getDeviceInterface();
    const auto physDevice = m_context.getPhysicalDevice();
    const auto device     = m_context.getDevice();
    auto &allocator       = m_context.getDefaultAllocator();
    const auto &queue     = m_context.getUniversalQueue();
    const auto queueIndex = m_context.getUniversalQueueFamilyIndex();

    const vk::VkDeviceSize kWidth  = static_cast<vk::VkDeviceSize>(VariableRateTestCase::kWidth);
    const vk::VkDeviceSize kHeight = static_cast<vk::VkDeviceSize>(VariableRateTestCase::kHeight);
    constexpr auto kColorFormat    = VariableRateTestCase::kColorFormat;

    const auto kWidth32  = static_cast<uint32_t>(kWidth);
    const auto kHeight32 = static_cast<uint32_t>(kHeight);

    std::vector<std::unique_ptr<vk::BufferWithMemory>> referenceBuffers;
    std::vector<std::unique_ptr<vk::BufferWithMemory>> outputBuffers;
    std::vector<size_t> bufferNumElements;
    std::vector<vk::VkDeviceSize> bufferSizes;

    // Create reference and output buffers.
    for (const auto count : m_params.subpassCounts)
    {
        bufferNumElements.push_back(static_cast<size_t>(kWidth * kHeight * count));
        bufferSizes.push_back(bufferNumElements.back() * sizeof(int32_t));
        const auto bufferCreateInfo =
            vk::makeBufferCreateInfo(bufferSizes.back(), vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

        referenceBuffers.emplace_back(
            new vk::BufferWithMemory{vkd, device, allocator, bufferCreateInfo, MemoryRequirement::HostVisible});
        outputBuffers.emplace_back(
            new vk::BufferWithMemory{vkd, device, allocator, bufferCreateInfo, MemoryRequirement::HostVisible});
    }

    // Descriptor set layout.
    vk::DescriptorSetLayoutBuilder builder;
    builder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_FRAGMENT_BIT);
    const auto descriptorSetLayout = builder.build(vkd, device);

    // Pipeline layout.
    const vk::VkPushConstantRange pushConstantRange = {
        vk::VK_SHADER_STAGE_FRAGMENT_BIT,             // VkShaderStageFlags stageFlags;
        0u,                                           // uint32_t offset;
        static_cast<uint32_t>(sizeof(PushConstants)), // uint32_t size;
    };

    const vk::VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
        vk::VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType sType;
        nullptr,                                           // const void* pNext;
        0u,                                                // VkPipelineLayoutCreateFlags flags;
        1u,                                                // uint32_t setLayoutCount;
        &descriptorSetLayout.get(),                        // const VkDescriptorSetLayout* pSetLayouts;
        1u,                                                // uint32_t pushConstantRangeCount;
        &pushConstantRange,                                // const VkPushConstantRange* pPushConstantRanges;
    };
    const vk::PipelineLayoutWrapper pipelineLayout(m_params.pipelineConstructionType, vkd, device,
                                                   &pipelineLayoutCreateInfo);

    // Subpass with no attachments.
    const vk::VkSubpassDescription emptySubpassDescription = {
        0u,                                  // VkSubpassDescriptionFlags flags;
        vk::VK_PIPELINE_BIND_POINT_GRAPHICS, // VkPipelineBindPoint pipelineBindPoint;
        0u,                                  // uint32_t inputAttachmentCount;
        nullptr,                             // const VkAttachmentReference* pInputAttachments;
        0u,                                  // uint32_t colorAttachmentCount;
        nullptr,                             // const VkAttachmentReference* pColorAttachments;
        nullptr,                             // const VkAttachmentReference* pResolveAttachments;
        nullptr,                             // const VkAttachmentReference* pDepthStencilAttachment;
        0u,                                  // uint32_t preserveAttachmentCount;
        nullptr,                             // const uint32_t* pPreserveAttachments;
    };

    // Unused attachment reference.
    const vk::VkAttachmentReference unusedAttachmentReference = {
        VK_ATTACHMENT_UNUSED,                         // uint32_t attachment;
        vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout layout;
    };

    // Subpass with unused attachment.
    const vk::VkSubpassDescription unusedAttachmentSubpassDescription = {
        0u,                                  // VkSubpassDescriptionFlags flags;
        vk::VK_PIPELINE_BIND_POINT_GRAPHICS, // VkPipelineBindPoint pipelineBindPoint;
        0u,                                  // uint32_t inputAttachmentCount;
        nullptr,                             // const VkAttachmentReference* pInputAttachments;
        1u,                                  // uint32_t colorAttachmentCount;
        &unusedAttachmentReference,          // const VkAttachmentReference* pColorAttachments;
        nullptr,                             // const VkAttachmentReference* pResolveAttachments;
        nullptr,                             // const VkAttachmentReference* pDepthStencilAttachment;
        0u,                                  // uint32_t preserveAttachmentCount;
        nullptr,                             // const uint32_t* pPreserveAttachments;
    };

    // Renderpass with multiple subpasses.
    vk::VkRenderPassCreateInfo renderPassCreateInfo = {
        vk::VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, // VkStructureType sType;
        nullptr,                                       // const void* pNext;
        0u,                                            // VkRenderPassCreateFlags flags;
        0u,                                            // uint32_t attachmentCount;
        nullptr,                                       // const VkAttachmentDescription* pAttachments;
        0u,                                            // uint32_t subpassCount;
        nullptr,                                       // const VkSubpassDescription* pSubpasses;
        0u,                                            // uint32_t dependencyCount;
        nullptr,                                       // const VkSubpassDependency* pDependencies;
    };

    std::vector<vk::VkSubpassDescription> subpassesVector;

    for (size_t i = 0; i < m_params.subpassCounts.size(); ++i)
        subpassesVector.push_back(emptySubpassDescription);
    renderPassCreateInfo.subpassCount = static_cast<uint32_t>(subpassesVector.size());
    renderPassCreateInfo.pSubpasses   = subpassesVector.data();
    RenderPassWrapper renderPassMultiplePasses(m_params.pipelineConstructionType, vkd, device, &renderPassCreateInfo);

    // Render pass with single subpass.
    const vk::VkAttachmentDescription colorAttachmentDescription = {
        0u,                                           // VkAttachmentDescriptionFlags flags;
        kColorFormat,                                 // VkFormat format;
        m_params.fbCount,                             // VkSampleCountFlagBits samples;
        vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,          // VkAttachmentLoadOp loadOp;
        vk::VK_ATTACHMENT_STORE_OP_STORE,             // VkAttachmentStoreOp storeOp;
        vk::VK_ATTACHMENT_LOAD_OP_DONT_CARE,          // VkAttachmentLoadOp stencilLoadOp;
        vk::VK_ATTACHMENT_STORE_OP_DONT_CARE,         // VkAttachmentStoreOp stencilStoreOp;
        vk::VK_IMAGE_LAYOUT_UNDEFINED,                // VkImageLayout initialLayout;
        vk::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout finalLayout;
    };

    if (m_params.nonEmptyFramebuffer)
    {
        renderPassCreateInfo.attachmentCount = 1u;
        renderPassCreateInfo.pAttachments    = &colorAttachmentDescription;
    }
    const bool unusedAttachmentSubpass = (m_params.nonEmptyFramebuffer && m_params.unusedAttachment);
    renderPassCreateInfo.subpassCount  = 1u;
    renderPassCreateInfo.pSubpasses =
        (unusedAttachmentSubpass ? &unusedAttachmentSubpassDescription : &emptySubpassDescription);
    RenderPassWrapper renderPassSingleSubpass(m_params.pipelineConstructionType, vkd, device, &renderPassCreateInfo);

    // Framebuffers.
    vk::VkFramebufferCreateInfo framebufferCreateInfo = {
        vk::VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // VkStructureType sType;
        nullptr,                                       // const void* pNext;
        0u,                                            // VkFramebufferCreateFlags flags;
        VK_NULL_HANDLE,                                // VkRenderPass renderPass;
        0u,                                            // uint32_t attachmentCount;
        nullptr,                                       // const VkImageView* pAttachments;
        kWidth32,                                      // uint32_t width;
        kHeight32,                                     // uint32_t height;
        1u,                                            // uint32_t layers;
    };

    // Framebuffer for multiple-subpasses render pass.
    framebufferCreateInfo.renderPass = renderPassMultiplePasses.get();
    renderPassMultiplePasses.createFramebuffer(vkd, device, &framebufferCreateInfo, std::vector<VkImage>{});

    // Framebuffer for single-subpass render pass.
    std::unique_ptr<vk::ImageWithMemory> imagePtr;
    vk::Move<vk::VkImageView> imageView;
    std::vector<vk::VkImage> images;

    if (m_params.nonEmptyFramebuffer)
    {
        const vk::VkImageCreateInfo imageCreateInfo = {
            vk::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,   // VkStructureType sType;
            nullptr,                                   // const void* pNext;
            0u,                                        // VkImageCreateFlags flags;
            vk::VK_IMAGE_TYPE_2D,                      // VkImageType imageType;
            kColorFormat,                              // VkFormat format;
            vk::makeExtent3D(kWidth32, kHeight32, 1u), // VkExtent3D extent;
            1u,                                        // uint32_t mipLevels;
            1u,                                        // uint32_t arrayLayers;
            m_params.fbCount,                          // VkSampleCountFlagBits samples;
            vk::VK_IMAGE_TILING_OPTIMAL,               // VkImageTiling tiling;
            vk::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,   // VkImageUsageFlags usage;
            vk::VK_SHARING_MODE_EXCLUSIVE,             // VkSharingMode sharingMode;
            0u,                                        // uint32_t queueFamilyIndexCount;
            nullptr,                                   // const uint32_t* pQueueFamilyIndices;
            vk::VK_IMAGE_LAYOUT_UNDEFINED,             // VkImageLayout initialLayout;
        };
        imagePtr.reset(new vk::ImageWithMemory{vkd, device, allocator, imageCreateInfo, MemoryRequirement::Any});

        const auto subresourceRange = vk::makeImageSubresourceRange(vk::VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
        imageView =
            vk::makeImageView(vkd, device, imagePtr->get(), vk::VK_IMAGE_VIEW_TYPE_2D, kColorFormat, subresourceRange);

        framebufferCreateInfo.attachmentCount = 1u;
        framebufferCreateInfo.pAttachments    = &imageView.get();
        images.push_back(**imagePtr);
    }
    framebufferCreateInfo.renderPass = renderPassSingleSubpass.get();
    renderPassSingleSubpass.createFramebuffer(vkd, device, &framebufferCreateInfo, images);

    // Shader modules and stages.
    const auto vertModule = ShaderWrapper(vkd, device, m_context.getBinaryCollection().get("vert"), 0u);
    const auto fragModule = ShaderWrapper(vkd, device, m_context.getBinaryCollection().get("frag"), 0u);

    // Vertices, input state and assembly.
    const std::vector<tcu::Vec2> vertices = {
        {-0.987f, -0.964f},
        {0.982f, -0.977f},
        {0.005f, 0.891f},
    };

    const auto vertexBinding = vk::makeVertexInputBindingDescription(
        0u, static_cast<uint32_t>(sizeof(decltype(vertices)::value_type)), vk::VK_VERTEX_INPUT_RATE_VERTEX);
    const auto vertexAttribute = vk::makeVertexInputAttributeDescription(0u, 0u, vk::VK_FORMAT_R32G32_SFLOAT, 0u);

    const vk::VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
        vk::VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                       // const void* pNext;
        0u,                                                            // VkPipelineVertexInputStateCreateFlags flags;
        1u,                                                            // uint32_t vertexBindingDescriptionCount;
        &vertexBinding,   // const VkVertexInputBindingDescription* pVertexBindingDescriptions;
        1u,               // uint32_t vertexAttributeDescriptionCount;
        &vertexAttribute, // const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
    };

    // Graphics pipelines to create output buffers.
    const std::vector<VkViewport> viewport{vk::makeViewport(kWidth32, kHeight32)};
    const std::vector<VkRect2D> scissor{vk::makeRect2D(kWidth32, kHeight32)};

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

    const vk::VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfoNoAttachments = {
        vk::VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                      // const void* pNext;
        0u,                                                           // VkPipelineColorBlendStateCreateFlags flags;
        VK_FALSE,                                                     // VkBool32 logicOpEnable;
        vk::VK_LOGIC_OP_CLEAR,                                        // VkLogicOp logicOp;
        0u,                                                           // uint32_t attachmentCount;
        nullptr,                 // const VkPipelineColorBlendAttachmentState* pAttachments;
        {0.0f, 0.0f, 0.0f, 0.0f} // float blendConstants[4];
    };

    const vk::VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfoOneAttachment = {
        vk::VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                      // const void* pNext;
        0u,                                                           // VkPipelineColorBlendStateCreateFlags flags;
        VK_FALSE,                                                     // VkBool32 logicOpEnable;
        vk::VK_LOGIC_OP_CLEAR,                                        // VkLogicOp logicOp;
        1u,                                                           // uint32_t attachmentCount;
        &colorBlendAttachmentState, // const VkPipelineColorBlendAttachmentState* pAttachments;
        {0.0f, 0.0f, 0.0f, 0.0f}    // float blendConstants[4];
    };

    vk::VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo{
        vk::VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                                      // const void* pNext;
        0u,                                                           // VkPipelineMultisampleStateCreateFlags flags;
        vk::VK_SAMPLE_COUNT_1_BIT,                                    // VkSampleCountFlagBits rasterizationSamples;
        VK_FALSE,                                                     // VkBool32 sampleShadingEnable;
        0.0f,                                                         // float minSampleShading;
        nullptr,                                                      // const VkSampleMask* pSampleMask;
        VK_FALSE,                                                     // VkBool32 alphaToCoverageEnable;
        VK_FALSE,                                                     // VkBool32 alphaToOneEnable;
    };

    std::vector<GraphicsPipelineWrapper> outputPipelines;
    outputPipelines.reserve(m_params.subpassCounts.size());
    for (const auto samples : m_params.subpassCounts)
    {
        const auto colorBlendStatePtr = (unusedAttachmentSubpass ? &colorBlendStateCreateInfoOneAttachment :
                                                                   &colorBlendStateCreateInfoNoAttachments);

        multisampleStateCreateInfo.rasterizationSamples = samples;

        outputPipelines.emplace_back(vki, vkd, physDevice, device, m_context.getDeviceExtensions(),
                                     m_params.pipelineConstructionType);
        outputPipelines.back()
            .setDefaultDepthStencilState()
            .setDefaultRasterizationState()
            .setupVertexInputState(&vertexInputStateCreateInfo)
            .setupPreRasterizationShaderState(viewport, scissor, pipelineLayout, *renderPassSingleSubpass, 0u,
                                              vertModule)
            .setupFragmentShaderState(pipelineLayout, *renderPassSingleSubpass, 0u, fragModule, nullptr,
                                      &multisampleStateCreateInfo)
            .setupFragmentOutputState(*renderPassSingleSubpass, 0u, colorBlendStatePtr, &multisampleStateCreateInfo)
            .setMonolithicPipelineLayout(pipelineLayout)
            .buildPipeline();
    }

    // Graphics pipelines with variable rate but using several subpasses.
    std::vector<GraphicsPipelineWrapper> referencePipelines;
    referencePipelines.reserve(m_params.subpassCounts.size());
    for (size_t i = 0; i < m_params.subpassCounts.size(); ++i)
    {
        multisampleStateCreateInfo.rasterizationSamples = m_params.subpassCounts[i];

        uint32_t subpass = static_cast<uint32_t>(i);
        referencePipelines.emplace_back(vki, vkd, physDevice, device, m_context.getDeviceExtensions(),
                                        m_params.pipelineConstructionType);
        referencePipelines.back()
            .setDefaultDepthStencilState()
            .setDefaultRasterizationState()
            .setupVertexInputState(&vertexInputStateCreateInfo)
            .setupPreRasterizationShaderState(viewport, scissor, pipelineLayout, *renderPassMultiplePasses, subpass,
                                              vertModule)
            .setupFragmentShaderState(pipelineLayout, *renderPassMultiplePasses, subpass, fragModule, nullptr,
                                      &multisampleStateCreateInfo)
            .setupFragmentOutputState(*renderPassMultiplePasses, subpass, &colorBlendStateCreateInfoNoAttachments,
                                      &multisampleStateCreateInfo)
            .setMonolithicPipelineLayout(pipelineLayout)
            .buildPipeline();
    }

    // Prepare vertex, reference and output buffers.
    const auto vertexBufferSize = vertices.size() * sizeof(decltype(vertices)::value_type);
    const auto vertexBufferCreateInfo =
        vk::makeBufferCreateInfo(static_cast<VkDeviceSize>(vertexBufferSize), vk::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    vk::BufferWithMemory vertexBuffer{vkd, device, allocator, vertexBufferCreateInfo, MemoryRequirement::HostVisible};
    auto &vertexAlloc = vertexBuffer.getAllocation();

    deMemcpy(vertexAlloc.getHostPtr(), vertices.data(), vertexBufferSize);
    vk::flushAlloc(vkd, device, vertexAlloc);

    for (size_t i = 0; i < referenceBuffers.size(); ++i)
    {
        zeroOutAndFlush(vkd, device, *referenceBuffers[i], bufferSizes[i]);
        zeroOutAndFlush(vkd, device, *outputBuffers[i], bufferSizes[i]);
    }

    // Prepare descriptor sets.
    const uint32_t totalSets = static_cast<uint32_t>(referenceBuffers.size() * 2u);
    vk::DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, static_cast<uint32_t>(referenceBuffers.size() * 2u));
    const auto descriptorPool =
        poolBuilder.build(vkd, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, totalSets);

    std::vector<vk::Move<vk::VkDescriptorSet>> referenceSets(referenceBuffers.size());
    std::vector<vk::Move<vk::VkDescriptorSet>> outputSets(outputBuffers.size());

    for (auto &set : referenceSets)
        set = vk::makeDescriptorSet(vkd, device, descriptorPool.get(), descriptorSetLayout.get());
    for (auto &set : outputSets)
        set = vk::makeDescriptorSet(vkd, device, descriptorPool.get(), descriptorSetLayout.get());

    vk::DescriptorSetUpdateBuilder updateBuilder;

    for (size_t i = 0; i < referenceSets.size(); ++i)
    {
        const auto descriptorBufferInfo = vk::makeDescriptorBufferInfo(referenceBuffers[i]->get(), 0u, bufferSizes[i]);
        updateBuilder.writeSingle(referenceSets[i].get(), vk::DescriptorSetUpdateBuilder::Location::binding(0u),
                                  vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorBufferInfo);
    }
    for (size_t i = 0; i < outputSets.size(); ++i)
    {
        const auto descriptorBufferInfo = vk::makeDescriptorBufferInfo(outputBuffers[i]->get(), 0u, bufferSizes[i]);
        updateBuilder.writeSingle(outputSets[i].get(), vk::DescriptorSetUpdateBuilder::Location::binding(0u),
                                  vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorBufferInfo);
    }

    updateBuilder.update(vkd, device);

    // Prepare command pool.
    const auto cmdPool = vk::makeCommandPool(vkd, device, queueIndex);
    const auto cmdBufferPtr =
        vk::allocateCommandBuffer(vkd, device, cmdPool.get(), vk::VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto cmdBuffer = cmdBufferPtr.get();

    vk::VkBufferMemoryBarrier storageBufferDevToHostBarrier = {
        vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, // VkStructureType sType;
        nullptr,                                     // const void* pNext;
        vk::VK_ACCESS_SHADER_WRITE_BIT,              // VkAccessFlags srcAccessMask;
        vk::VK_ACCESS_HOST_READ_BIT,                 // VkAccessFlags dstAccessMask;
        VK_QUEUE_FAMILY_IGNORED,                     // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                     // uint32_t dstQueueFamilyIndex;
        VK_NULL_HANDLE,                              // VkBuffer buffer;
        0u,                                          // VkDeviceSize offset;
        VK_WHOLE_SIZE,                               // VkDeviceSize size;
    };

    // Record command buffer.
    const vk::VkDeviceSize vertexBufferOffset = 0u;
    const auto renderArea                     = vk::makeRect2D(kWidth32, kHeight32);
    PushConstants pushConstants               = {static_cast<int>(kWidth), static_cast<int>(kHeight), 0};

    vk::beginCommandBuffer(vkd, cmdBuffer);

    // Render output buffers.
    renderPassSingleSubpass.begin(vkd, cmdBuffer, renderArea);
    for (size_t i = 0; i < outputBuffers.size(); ++i)
    {
        outputPipelines[i].bind(cmdBuffer);
        vkd.cmdBindDescriptorSets(cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), 0u, 1u,
                                  &outputSets[i].get(), 0u, nullptr);
        vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
        pushConstants.samples = static_cast<int>(m_params.subpassCounts[i]);
        vkd.cmdPushConstants(cmdBuffer, pipelineLayout.get(), pushConstantRange.stageFlags, pushConstantRange.offset,
                             pushConstantRange.size, &pushConstants);
        vkd.cmdDraw(cmdBuffer, static_cast<uint32_t>(vertices.size()), 1u, 0u, 0u);
    }
    renderPassSingleSubpass.end(vkd, cmdBuffer);
    for (size_t i = 0; i < outputBuffers.size(); ++i)
    {
        storageBufferDevToHostBarrier.buffer = outputBuffers[i]->get();
        vkd.cmdPipelineBarrier(cmdBuffer, vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT, 0u,
                               0u, nullptr, 1u, &storageBufferDevToHostBarrier, 0u, nullptr);
    }

    // Render reference buffers.
    renderPassMultiplePasses.begin(vkd, cmdBuffer, renderArea);
    for (size_t i = 0; i < referenceBuffers.size(); ++i)
    {
        if (i > 0)
            renderPassMultiplePasses.nextSubpass(vkd, cmdBuffer, vk::VK_SUBPASS_CONTENTS_INLINE);
        referencePipelines[i].bind(cmdBuffer);
        vkd.cmdBindDescriptorSets(cmdBuffer, vk::VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), 0u, 1u,
                                  &referenceSets[i].get(), 0u, nullptr);
        vkd.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
        pushConstants.samples = static_cast<int>(m_params.subpassCounts[i]);
        vkd.cmdPushConstants(cmdBuffer, pipelineLayout.get(), pushConstantRange.stageFlags, pushConstantRange.offset,
                             pushConstantRange.size, &pushConstants);
        vkd.cmdDraw(cmdBuffer, static_cast<uint32_t>(vertices.size()), 1u, 0u, 0u);
    }
    renderPassMultiplePasses.end(vkd, cmdBuffer);
    for (size_t i = 0; i < referenceBuffers.size(); ++i)
    {
        storageBufferDevToHostBarrier.buffer = referenceBuffers[i]->get();
        vkd.cmdPipelineBarrier(cmdBuffer, vk::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT, 0u,
                               0u, nullptr, 1u, &storageBufferDevToHostBarrier, 0u, nullptr);
    }

    vk::endCommandBuffer(vkd, cmdBuffer);

    // Run all pipelines.
    vk::submitCommandsAndWait(vkd, device, queue, cmdBuffer);

    // Invalidate reference allocs.
#undef LOG_BUFFER_CONTENTS
#ifdef LOG_BUFFER_CONTENTS
    auto &log = m_context.getTestContext().getLog();
#endif
    for (size_t i = 0; i < referenceBuffers.size(); ++i)
    {
        auto &buffer = referenceBuffers[i];
        auto &alloc  = buffer->getAllocation();
        vk::invalidateAlloc(vkd, device, alloc);

#ifdef LOG_BUFFER_CONTENTS
        std::vector<int32_t> bufferValues(bufferNumElements[i]);
        deMemcpy(bufferValues.data(), alloc.getHostPtr(), bufferSizes[i]);

        std::ostringstream msg;
        for (const auto value : bufferValues)
            msg << " " << value;
        log << tcu::TestLog::Message << "Reference buffer values with " << m_params[i] << " samples:" << msg.str()
            << tcu::TestLog::EndMessage;
#endif
    }

    for (size_t i = 0; i < outputBuffers.size(); ++i)
    {
        auto &buffer = outputBuffers[i];
        auto &alloc  = buffer->getAllocation();
        vk::invalidateAlloc(vkd, device, alloc);

#ifdef LOG_BUFFER_CONTENTS
        std::vector<int32_t> bufferValues(bufferNumElements[i]);
        deMemcpy(bufferValues.data(), alloc.getHostPtr(), bufferSizes[i]);

        std::ostringstream msg;
        for (const auto value : bufferValues)
            msg << " " << value;
        log << tcu::TestLog::Message << "Output buffer values with " << m_params[i] << " samples:" << msg.str()
            << tcu::TestLog::EndMessage;
#endif

        if (deMemCmp(alloc.getHostPtr(), referenceBuffers[i]->getAllocation().getHostPtr(),
                     static_cast<size_t>(bufferSizes[i])) != 0)
            return tcu::TestStatus::fail("Buffer mismatch in output buffer " + de::toString(i));
    }

    return tcu::TestStatus::pass("Pass");
}

using ElementsVector    = std::vector<vk::VkSampleCountFlagBits>;
using CombinationVector = std::vector<ElementsVector>;

void combinationsRecursive(const ElementsVector &elements, size_t requestedSize, CombinationVector &solutions,
                           ElementsVector &partial)
{
    if (partial.size() == requestedSize)
        solutions.push_back(partial);
    else
    {
        for (const auto &elem : elements)
        {
            partial.push_back(elem);
            combinationsRecursive(elements, requestedSize, solutions, partial);
            partial.pop_back();
        }
    }
}

CombinationVector combinations(const ElementsVector &elements, size_t requestedSize)
{
    CombinationVector solutions;
    ElementsVector partial;

    combinationsRecursive(elements, requestedSize, solutions, partial);
    return solutions;
}

/********
Z EXPORT TESTS

The tests enable alpha to coverage statically or dynamically, and play with 3 other parameters, which we can be testing or not as
outputs from the frag shader.

* Depth value
* Stencil reference value
* Sample mask

Alpha values on the left side of the framebuffer will be 0.0. On the right side they will be 1.0. This means the left side should
not have coverage, and the right side should have.

Depth value will be cleared to 1.0 and we expect to obtain 0.0 for covered pixels at the end. We will activate the depth test with a
depth compare op of "less".

* If we are testing this, we will set 0.5 from the vertex shader and 0.0 from the frag shader.
* If we are not testing this, we will set 0.0 directly from the vertex shader.

Stencil will be cleared to 0 and we expect to obtain 255 for covered pixels at the end. We will activate the stencil test with a
stencil op of "replace" for front-facing pixels, compare op "always", keep and "never" for back-facing pixels.

* If we are testing this, the stencil ref value will be 128 in the pipeline, then 255 from the frag shader.
* If we are not testing this, the reference value will be set to 255 directly in the pipeline.

Sample mask is a bit special: we'll always set it to 0xFF in the pipeline, and we normally expect all pixels to be covered.

* If we are testing this, we'll set it to 0x00 on the lower half of the framebuffer.
* If we are not testing this, we'll leave it as it is.

Expected result:

* The left side of the framebuffer will have:
  - The clear color.
  - The clear depth value.
  - The clear stencil value.

* The right side of the framebuffer will have:
  - The geometry color (typically blue).
  - The expected depth value.
  - The expected stencil value.
  - But, if we are testing the sample mask, the lower half of the right side will be like the left side.

********/
enum ZExportTestBits
{
    ZEXP_DEPTH_BIT                = 0x1,
    ZEXP_STENCIL_BIT              = 0x2, // Requires VK_EXT_shader_stencil_export
    ZEXP_SAMPLE_MASK_SHADER_BIT   = 0x4,
    ZEXP_SAMPLE_MASK_PIPELINE_BIT = 0x8,
};

using ZExportFlags = uint32_t;

struct ZExportParams
{
    const PipelineConstructionType pipelineConstructionType;
    const ZExportFlags testFlags;
    const bool dynamicAlphaToCoverage;
    const bool dynamicRendering;

    ZExportParams(PipelineConstructionType pipelineConstructionType_, ZExportFlags testFlags_,
                  bool dynamicAlphaToCoverage_, bool dynamicRendering_)
        : pipelineConstructionType(pipelineConstructionType_)
        , testFlags(testFlags_)
        , dynamicAlphaToCoverage(dynamicAlphaToCoverage_)
        , dynamicRendering(dynamicRendering_)
    {
    }

    bool testDepth(void) const
    {
        return hasFlag(ZEXP_DEPTH_BIT);
    }
    bool testStencil(void) const
    {
        return hasFlag(ZEXP_STENCIL_BIT);
    }
    bool testSampleMaskShader(void) const
    {
        return hasFlag(ZEXP_SAMPLE_MASK_SHADER_BIT);
    }
    bool testSampleMaskPipeline(void) const
    {
        return hasFlag(ZEXP_SAMPLE_MASK_PIPELINE_BIT);
    }

    static constexpr float kClearDepth    = 1.0f;
    static constexpr float kExpectedDepth = 0.0f;
    static constexpr float kBadDepth      = 0.5f;

    static constexpr uint32_t kClearStencil    = 0u;
    static constexpr uint32_t kExpectedStencil = 255u;
    static constexpr uint32_t kBadStencil      = 128u;

    static constexpr uint32_t kWidth  = 4u;
    static constexpr uint32_t kHeight = 4u;

private:
    bool hasFlag(ZExportTestBits bit) const
    {
        return ((testFlags & static_cast<ZExportFlags>(bit)) != 0u);
    }
};

void ZExportCheckSupport(Context &context, const ZExportParams params)
{
    checkPipelineConstructionRequirements(context.getInstanceInterface(), context.getPhysicalDevice(),
                                          params.pipelineConstructionType);

    if (params.dynamicRendering)
    {
        context.requireDeviceFunctionality("VK_KHR_dynamic_rendering");
    }
    else
    {
        context.requireDeviceFunctionality("VK_KHR_create_renderpass2");
        context.requireDeviceFunctionality("VK_KHR_depth_stencil_resolve");
    }

    const auto &dsResolveProperties = context.getDepthStencilResolveProperties();

    if ((dsResolveProperties.supportedDepthResolveModes & VK_RESOLVE_MODE_SAMPLE_ZERO_BIT) == 0u)
        TCU_THROW(NotSupportedError, "VK_RESOLVE_MODE_SAMPLE_ZERO_BIT not supported for depth");

    if ((dsResolveProperties.supportedStencilResolveModes & VK_RESOLVE_MODE_SAMPLE_ZERO_BIT) == 0u)
        TCU_THROW(NotSupportedError, "VK_RESOLVE_MODE_SAMPLE_ZERO_BIT not supported for stencil");

    if (params.testStencil())
        context.requireDeviceFunctionality("VK_EXT_shader_stencil_export");

    if (params.dynamicAlphaToCoverage)
    {
#ifndef CTS_USES_VULKANSC
        const auto &eds3Features = context.getExtendedDynamicState3FeaturesEXT();
        if (!eds3Features.extendedDynamicState3AlphaToCoverageEnable)
            TCU_THROW(NotSupportedError, "extendedDynamicState3AlphaToCoverageEnable not supported");
#else
        // VK_EXT_extended_dynamic_state3 is not available on vksc
        DE_ASSERT(false);
#endif // CTS_USES_VULKANSC
    }
}

void ZExportInitPrograms(SourceCollections &programCollection, const ZExportParams params)
{
    {
        const auto vertDepth = (params.testDepth() ? ZExportParams::kBadDepth : ZExportParams::kExpectedDepth);

        std::ostringstream vert;
        vert << "#version 460\n"
             << "vec2 positions[3] = vec2[](\n"
             << "    vec2(-1.0, -1.0),\n"
             << "    vec2(-1.0, 3.0),\n"
             << "    vec2(3.0, -1.0)\n"
             << ");\n"
             << "void main (void) {\n"
             << "    gl_Position = vec4(positions[gl_VertexIndex % 3], " << vertDepth << ", 1.0);\n"
             << "}\n";
        programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());
    }

    {
        std::ostringstream frag;
        frag << "#version 460\n"
             << "layout (location=0) out vec4 outColor;\n"
             << (params.testStencil() ? "#extension GL_ARB_shader_stencil_export: require\n" : "")
             << "void main (void) {\n"
             << "    const float alphaValue = ((int(gl_FragCoord.x) < " << (ZExportParams::kWidth / 2u)
             << ") ? 0.0 : 1.0);\n"
             << "    outColor = vec4(0.0, 0.0, 1.0, alphaValue);\n"
             << (params.testDepth() ? ("    gl_FragDepth = " + std::to_string(ZExportParams::kExpectedDepth) + ";\n") :
                                      "")
             << (params.testStencil() ?
                     ("    gl_FragStencilRefARB = " + std::to_string(ZExportParams::kExpectedStencil) + ";\n") :
                     "");

        if (params.testSampleMaskShader())
            frag << "    gl_SampleMask[0] = ((int(gl_FragCoord.y) >= " << (ZExportParams::kHeight / 2u)
                 << ") ? 0 : 0xFF);\n";

        frag << "}\n";
        programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
    }
}

tcu::TestStatus ZExportIterate(Context &context, const ZExportParams params)
{
    const auto &ctx = context.getContextCommonData();

    // Choose depth/stencil format.
    const auto dsFormat = findSupportedDepthStencilFormat(context, true, true);
    if (dsFormat == VK_FORMAT_UNDEFINED)
        TCU_FAIL("Unable to find supported depth/stencil format");

    const auto fbExtent    = makeExtent3D(ZExportParams::kWidth, ZExportParams::kHeight, 1u);
    const auto colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const auto colorUsage  = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const auto dsUsage     = (VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    const auto colorAspect = VK_IMAGE_ASPECT_COLOR_BIT;
    const auto dsAspect    = (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
    const auto colorSRR    = makeImageSubresourceRange(colorAspect, 0u, 1u, 0u, 1u);
    const auto dsSRR       = makeImageSubresourceRange(dsAspect, 0u, 1u, 0u, 1u);
    const auto imageType   = VK_IMAGE_TYPE_2D;
    const auto viewType    = VK_IMAGE_VIEW_TYPE_2D;
    const auto sampleCount = VK_SAMPLE_COUNT_4_BIT;
    const auto bindPoint   = VK_PIPELINE_BIND_POINT_GRAPHICS;

    // Multisample color attachment.
    const VkImageCreateInfo colorAttachmentCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        0u,                                  // VkImageCreateFlags flags;
        imageType,                           // VkImageType imageType;
        colorFormat,                         // VkFormat format;
        fbExtent,                            // VkExtent3D extent;
        1u,                                  // uint32_t mipLevels;
        1u,                                  // uint32_t arrayLayers;
        sampleCount,                         // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        colorUsage,                          // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        0u,                                  // uint32_t queueFamilyIndexCount;
        nullptr,                             // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout initialLayout;
    };
    ImageWithMemory colorAttachment(ctx.vkd, ctx.device, ctx.allocator, colorAttachmentCreateInfo,
                                    MemoryRequirement::Any);
    const auto colorAttachmentView =
        makeImageView(ctx.vkd, ctx.device, colorAttachment.get(), viewType, colorFormat, colorSRR);

    // Multisample depth/stencil attachment.
    const VkImageCreateInfo dsAttachmentCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        0u,                                  // VkImageCreateFlags flags;
        imageType,                           // VkImageType imageType;
        dsFormat,                            // VkFormat format;
        fbExtent,                            // VkExtent3D extent;
        1u,                                  // uint32_t mipLevels;
        1u,                                  // uint32_t arrayLayers;
        sampleCount,                         // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        dsUsage,                             // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        0u,                                  // uint32_t queueFamilyIndexCount;
        nullptr,                             // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout initialLayout;
    };
    ImageWithMemory dsAttachment(ctx.vkd, ctx.device, ctx.allocator, dsAttachmentCreateInfo, MemoryRequirement::Any);
    const auto dsAttachmentView = makeImageView(ctx.vkd, ctx.device, dsAttachment.get(), viewType, dsFormat, dsSRR);

    // Resolve attachments.
    VkImageCreateInfo colorResolveAttachmentCreateInfo = colorAttachmentCreateInfo;
    colorResolveAttachmentCreateInfo.samples           = VK_SAMPLE_COUNT_1_BIT;
    VkImageCreateInfo dsResolveAttachmentCreateInfo    = dsAttachmentCreateInfo;
    dsResolveAttachmentCreateInfo.samples              = VK_SAMPLE_COUNT_1_BIT;

    ImageWithMemory colorResolveAttachment(ctx.vkd, ctx.device, ctx.allocator, colorResolveAttachmentCreateInfo,
                                           MemoryRequirement::Any);
    ImageWithMemory dsResolveAttachment(ctx.vkd, ctx.device, ctx.allocator, dsResolveAttachmentCreateInfo,
                                        MemoryRequirement::Any);
    const auto colorResolveAttachmentView =
        makeImageView(ctx.vkd, ctx.device, colorResolveAttachment.get(), viewType, colorFormat, colorSRR);
    const auto dsResolveAttachmentView =
        makeImageView(ctx.vkd, ctx.device, dsResolveAttachment.get(), viewType, dsFormat, dsSRR);

    // Render pass and framebuffer.
    const VkAttachmentDescription2 colorAttachmentDesc = {
        VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
        nullptr,
        0u,                                       // VkAttachmentDescriptionFlags flags;
        colorFormat,                              // VkFormat format;
        sampleCount,                              // VkSampleCountFlagBits samples;
        VK_ATTACHMENT_LOAD_OP_CLEAR,              // VkAttachmentLoadOp loadOp;
        VK_ATTACHMENT_STORE_OP_DONT_CARE,         // VkAttachmentStoreOp storeOp;
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,          // VkAttachmentLoadOp stencilLoadOp;
        VK_ATTACHMENT_STORE_OP_DONT_CARE,         // VkAttachmentStoreOp stencilStoreOp;
        VK_IMAGE_LAYOUT_UNDEFINED,                // VkImageLayout initialLayout;
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout finalLayout;
    };
    const VkAttachmentDescription2 dsAttachmentDesc = {
        VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
        nullptr,
        0u,                                               // VkAttachmentDescriptionFlags flags;
        dsFormat,                                         // VkFormat format;
        sampleCount,                                      // VkSampleCountFlagBits samples;
        VK_ATTACHMENT_LOAD_OP_CLEAR,                      // VkAttachmentLoadOp loadOp;
        VK_ATTACHMENT_STORE_OP_DONT_CARE,                 // VkAttachmentStoreOp storeOp;
        VK_ATTACHMENT_LOAD_OP_CLEAR,                      // VkAttachmentLoadOp stencilLoadOp;
        VK_ATTACHMENT_STORE_OP_DONT_CARE,                 // VkAttachmentStoreOp stencilStoreOp;
        VK_IMAGE_LAYOUT_UNDEFINED,                        // VkImageLayout initialLayout;
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, // VkImageLayout finalLayout;
    };
    const VkAttachmentDescription2 colorResolveAttachmentDesc = {
        VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
        nullptr,
        0u,                                       // VkAttachmentDescriptionFlags flags;
        colorFormat,                              // VkFormat format;
        VK_SAMPLE_COUNT_1_BIT,                    // VkSampleCountFlagBits samples;
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,          // VkAttachmentLoadOp loadOp;
        VK_ATTACHMENT_STORE_OP_STORE,             // VkAttachmentStoreOp storeOp;
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,          // VkAttachmentLoadOp stencilLoadOp;
        VK_ATTACHMENT_STORE_OP_DONT_CARE,         // VkAttachmentStoreOp stencilStoreOp;
        VK_IMAGE_LAYOUT_UNDEFINED,                // VkImageLayout initialLayout;
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // VkImageLayout finalLayout;
    };
    const VkAttachmentDescription2 dsResolveAttachmentDesc = {
        VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
        nullptr,
        0u,                                               // VkAttachmentDescriptionFlags flags;
        dsFormat,                                         // VkFormat format;
        VK_SAMPLE_COUNT_1_BIT,                            // VkSampleCountFlagBits samples;
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,                  // VkAttachmentLoadOp loadOp;
        VK_ATTACHMENT_STORE_OP_STORE,                     // VkAttachmentStoreOp storeOp;
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,                  // VkAttachmentLoadOp stencilLoadOp;
        VK_ATTACHMENT_STORE_OP_STORE,                     // VkAttachmentStoreOp stencilStoreOp;
        VK_IMAGE_LAYOUT_UNDEFINED,                        // VkImageLayout initialLayout;
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, // VkImageLayout finalLayout;
    };

    std::vector<VkAttachmentDescription2> attachmentDescriptions;
    attachmentDescriptions.reserve(4u);
    attachmentDescriptions.push_back(colorAttachmentDesc);
    attachmentDescriptions.push_back(dsAttachmentDesc);
    attachmentDescriptions.push_back(colorResolveAttachmentDesc);
    attachmentDescriptions.push_back(dsResolveAttachmentDesc);

    const VkAttachmentReference2 colorAttachmentReference = {
        VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2, nullptr, 0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, colorAspect,
    };
    const VkAttachmentReference2 dsAttachmentReference = {
        VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,         nullptr,  1u,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, dsAspect,
    };
    const VkAttachmentReference2 colorResolveAttachmentReference = {
        VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2, nullptr, 2u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, colorAspect,
    };
    const VkAttachmentReference2 dsResolveAttachmentReference = {
        VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,         nullptr,  3u,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, dsAspect,
    };

    const VkSubpassDescriptionDepthStencilResolve dsResolveDescription = {
        VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE, // VkStructureType sType;
        nullptr,                                                     // const void* pNext;
        VK_RESOLVE_MODE_SAMPLE_ZERO_BIT,                             // VkResolveModeFlagBits depthResolveMode;
        VK_RESOLVE_MODE_SAMPLE_ZERO_BIT,                             // VkResolveModeFlagBits stencilResolveMode;
        &dsResolveAttachmentReference, // const VkAttachmentReference2* pDepthStencilResolveAttachment;
    };

    const VkSubpassDescription2 subpassDescription = {
        VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2,
        &dsResolveDescription,
        0u,                               // VkSubpassDescriptionFlags flags;
        bindPoint,                        // VkPipelineBindPoint pipelineBindPoint;
        0u,                               // uint32_t viewMask;
        0u,                               // uint32_t inputAttachmentCount;
        nullptr,                          // const VkAttachmentReference* pInputAttachments;
        1u,                               // uint32_t colorAttachmentCount;
        &colorAttachmentReference,        // const VkAttachmentReference* pColorAttachments;
        &colorResolveAttachmentReference, // const VkAttachmentReference* pResolveAttachments;
        &dsAttachmentReference,           // const VkAttachmentReference* pDepthStencilAttachment;
        0u,                               // uint32_t preserveAttachmentCount;
        nullptr,                          // const uint32_t* pPreserveAttachments;
    };

    const VkRenderPassCreateInfo2 renderPassCreateInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2, // VkStructureType sType;
        nullptr,                                     // const void* pNext;
        0u,                                          // VkRenderPassCreateFlags flags;
        de::sizeU32(attachmentDescriptions),         // uint32_t attachmentCount;
        de::dataOrNull(attachmentDescriptions),      // const VkAttachmentDescription* pAttachments;
        1u,                                          // uint32_t subpassCount;
        &subpassDescription,                         // const VkSubpassDescription* pSubpasses;
        0u,                                          // uint32_t dependencyCount;
        nullptr,                                     // const VkSubpassDependency* pDependencies;
        0u,                                          // uint32_t correlatedViewMaskCount;
        nullptr,                                     // const uint32_t* pCorrelatedViewMasks;
    };

    const std::vector<VkImage> images{
        *colorAttachment,
        *dsAttachment,
        *colorResolveAttachment,
        *dsResolveAttachment,
    };

    const std::vector<VkImageView> attachmentViews{
        colorAttachmentView.get(),
        dsAttachmentView.get(),
        colorResolveAttachmentView.get(),
        dsResolveAttachmentView.get(),
    };

    RenderPassWrapper renderPass(
        ctx.vkd, ctx.device, &renderPassCreateInfo,
        (params.dynamicRendering || isConstructionTypeShaderObject(params.pipelineConstructionType)));
    renderPass.createFramebuffer(ctx.vkd, ctx.device, de::sizeU32(attachmentViews), de::dataOrNull(images),
                                 de::dataOrNull(attachmentViews), fbExtent.width, fbExtent.height);

    // Pipeline layout.
    const PipelineLayoutWrapper pipelineLayout(params.pipelineConstructionType, ctx.vkd, ctx.device);

    // Shaders.
    const auto &binaries  = context.getBinaryCollection();
    const auto vertShader = ShaderWrapper(ctx.vkd, ctx.device, binaries.get("vert"));
    const auto fragShader = ShaderWrapper(ctx.vkd, ctx.device, binaries.get("frag"));
    const auto nullShader = ShaderWrapper();

    // Viewports and scissors.
    const std::vector<VkViewport> viewports(1u, makeViewport(fbExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(fbExtent));

    const auto frontStencilRef = (params.testStencil() ? ZExportParams::kBadStencil : ZExportParams::kExpectedStencil);
    const VkStencilOpState frontStencilOpState{
        VK_STENCIL_OP_KEEP,    // VkStencilOp                                    failOp
        VK_STENCIL_OP_REPLACE, // VkStencilOp                                    passOp
        VK_STENCIL_OP_KEEP,    // VkStencilOp                                    depthFailOp
        VK_COMPARE_OP_ALWAYS,  // VkCompareOp                                    compareOp
        0xFFu,                 // uint32_t                                        compareMask
        0xFFu,                 // uint32_t                                        writeMask
        frontStencilRef,       // uint32_t                                        reference
    };
    const auto backStencilOpState = makeStencilOpState(VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP,
                                                       VK_COMPARE_OP_NEVER, 0xFFu, 0xFFu, 0u);

    const VkPipelineDepthStencilStateCreateInfo dsStateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, // VkStructureType                                sType
        nullptr,             // const void*                                    pNext
        0u,                  // VkPipelineDepthStencilStateCreateFlags        flags
        VK_TRUE,             // VkBool32                                        depthTestEnable
        VK_TRUE,             // VkBool32                                        depthWriteEnable
        VK_COMPARE_OP_LESS,  // VkCompareOp                                    depthCompareOp
        VK_FALSE,            // VkBool32                                        depthBoundsTestEnable
        VK_TRUE,             // VkBool32                                        stencilTestEnable
        frontStencilOpState, // VkStencilOpState                                front
        backStencilOpState,  // VkStencilOpState                                back
        0.0f,                // float                                        minDepthBounds
        1.0f,                // float                                        maxDepthBounds
    };

    // Multisample state, including alpha to coverage, which is key for these tests.
    const auto staticAlphaToCoverage = (params.dynamicAlphaToCoverage ? VK_FALSE : VK_TRUE);
    const VkSampleMask sampleMask    = 0b1101;
    const VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType                                sType
        nullptr,     // const void*                                    pNext
        0u,          // VkPipelineMultisampleStateCreateFlags        flags
        sampleCount, // VkSampleCountFlagBits                        rasterizationSamples
        VK_FALSE,    // VkBool32                                        sampleShadingEnable
        1.0f,        // float                                        minSampleShading
        (params.testSampleMaskPipeline() ? &sampleMask :
                                           nullptr), // const VkSampleMask*                            pSampleMask
        staticAlphaToCoverage, // VkBool32                                        alphaToCoverageEnable
        VK_FALSE,              // VkBool32                                        alphaToOneEnable
    };

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = initVulkanStructure();

    std::vector<VkDynamicState> dynamicStates;
#ifndef CTS_USES_VULKANSC
    if (params.dynamicAlphaToCoverage)
        dynamicStates.push_back(VK_DYNAMIC_STATE_ALPHA_TO_COVERAGE_ENABLE_EXT);
#endif // CTS_USES_VULKANSC

    const VkPipelineDynamicStateCreateInfo dynamicStateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                              // const void* pNext;
        0u,                                                   // VkPipelineDynamicStateCreateFlags flags;
        de::sizeU32(dynamicStates),                           // uint32_t dynamicStateCount;
        de::dataOrNull(dynamicStates),                        // const VkDynamicState* pDynamicStates;
    };

#ifndef CTS_USES_VULKANSC
    VkPipelineRenderingCreateInfo renderingCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO, // VkStructureType sType;
        nullptr,                                          // const void* pNext;
        0u,                                               // uint32_t viewMask;
        1u,                                               // uint32_t colorAttachmentCount;
        &colorFormat,                                     // const VkFormat* pColorAttachmentFormats;
        dsFormat,                                         // VkFormat depthAttachmentFormat;
        dsFormat,                                         // VkFormat stencilAttachmentFormat;
    };

    PipelineRenderingCreateInfoWrapper renderingCreateInfoPtr(params.dynamicRendering ? &renderingCreateInfo : nullptr);
#else
    PipelineRenderingCreateInfoWrapper renderingCreateInfoPtr(nullptr);
#endif // CTS_USES_VULKANSC

    const auto fragShaderStateMSPtr = (params.dynamicRendering ? nullptr : &multisampleStateCreateInfo);

    GraphicsPipelineWrapper pipelineWrapper(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device,
                                            context.getDeviceExtensions(), params.pipelineConstructionType);
    pipelineWrapper.setDefaultRasterizationState()
        .setDefaultColorBlendState()
        .setDynamicState(&dynamicStateInfo)
        .setupVertexInputState(&vertexInputStateCreateInfo)
        .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, *renderPass, 0u, vertShader, nullptr,
                                          nullShader, nullShader, nullShader, nullptr, nullptr, renderingCreateInfoPtr)
        .setupFragmentShaderState(pipelineLayout, *renderPass, 0u, fragShader, &dsStateInfo, fragShaderStateMSPtr,
                                  nullptr, VK_NULL_HANDLE)
        .setupFragmentOutputState(*renderPass, 0u, nullptr, &multisampleStateCreateInfo)
        .setMonolithicPipelineLayout(pipelineLayout)
        .buildPipeline();

    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 0.0f);
    tcu::Vec4 geometryColor(0.0f, 0.0f, 1.0f, 1.0f); // For pixels with coverage. Must match frag shader.
    tcu::Vec4 colorThreshold(0.0f, 0.0f, 0.0f, 0.0f);

    // cover interactions between pSampleMask and alphaToCoverageEnable
    if (params.testSampleMaskPipeline())
    {
        geometryColor  = tcu::Vec4(0.0f, 0.0f, 0.75f, 0.75f); // there are 4 samples but one is masked
        colorThreshold = tcu::Vec4(0.02f);
    }

    const std::vector<VkClearValue> clearValues{
        makeClearValueColor(clearColor),
        makeClearValueDepthStencil(ZExportParams::kClearDepth, ZExportParams::kClearStencil),
    };

    beginCommandBuffer(ctx.vkd, cmdBuffer);
    renderPass.begin(ctx.vkd, cmdBuffer, scissors.at(0u), de::sizeU32(clearValues), de::dataOrNull(clearValues));
    pipelineWrapper.bind(cmdBuffer);
#ifndef CTS_USES_VULKANSC
    if (params.dynamicAlphaToCoverage)
        ctx.vkd.cmdSetAlphaToCoverageEnableEXT(cmdBuffer, VK_TRUE);
#endif // CTS_USES_VULKANSC
    ctx.vkd.cmdDraw(cmdBuffer, 3u, 1u, 0u, 0u);
    renderPass.end(ctx.vkd, cmdBuffer);
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    const tcu::UVec2 renderSize(fbExtent.width, fbExtent.height);
    const auto colorLevel = readColorAttachment(ctx.vkd, ctx.device, ctx.queue, ctx.qfIndex, ctx.allocator,
                                                colorResolveAttachment.get(), colorFormat, renderSize);
    const auto depthLevel = readDepthAttachment(ctx.vkd, ctx.device, ctx.queue, ctx.qfIndex, ctx.allocator,
                                                dsResolveAttachment.get(), dsFormat, renderSize);
    const auto stencilLevel =
        readStencilAttachment(ctx.vkd, ctx.device, ctx.queue, ctx.qfIndex, ctx.allocator, dsResolveAttachment.get(),
                              dsFormat, renderSize, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    const auto colorAccess   = colorLevel->getAccess();
    const auto depthAccess   = depthLevel->getAccess();
    const auto stencilAccess = stencilLevel->getAccess();

    const tcu::IVec3 iExtent(static_cast<int>(fbExtent.width), static_cast<int>(fbExtent.height),
                             static_cast<int>(fbExtent.depth));
    tcu::TextureLevel refColor(mapVkFormat(colorFormat), iExtent.x(), iExtent.y());
    tcu::TextureLevel refDepth(getDepthCopyFormat(dsFormat), iExtent.x(), iExtent.y());
    tcu::TextureLevel refStencil(getStencilCopyFormat(dsFormat), iExtent.x(), iExtent.y());

    auto refColorAccess   = refColor.getAccess();
    auto refDepthAccess   = refDepth.getAccess();
    auto refStencilAccess = refStencil.getAccess();

    const auto halfWidth  = iExtent.x() / 2;
    const auto halfHeight = iExtent.y() / 2;

    const tcu::Vec4 geometryColorNoAlpha(geometryColor.x(), geometryColor.y(), geometryColor.z(),
                                         0.0f); // For pixels with coverage but alpha set to 0

    // allow skipping alpha to coverage if sample mask output is used
    std::vector<bool> skipAlphaToCoverageBehaviors =
        (params.testSampleMaskShader() ? std::vector<bool>({false, true}) : std::vector<bool>({false}));

    for (bool skipAlphaToCoverage : skipAlphaToCoverageBehaviors)
    {
        // Prepare color reference.
        {
            auto topLeft     = tcu::getSubregion(refColorAccess, 0, 0, halfWidth, halfHeight);
            auto bottomLeft  = tcu::getSubregion(refColorAccess, 0, halfHeight, halfWidth, halfHeight);
            auto topRight    = tcu::getSubregion(refColorAccess, halfWidth, 0, halfWidth, halfHeight);
            auto bottomRight = tcu::getSubregion(refColorAccess, halfWidth, halfHeight, halfWidth, halfHeight);

            tcu::clear(topLeft, (skipAlphaToCoverage ? geometryColorNoAlpha : clearColor));
            tcu::clear(bottomLeft,
                       (skipAlphaToCoverage ? (params.testSampleMaskShader() ? clearColor : geometryColorNoAlpha) :
                                              clearColor));
            tcu::clear(topRight, geometryColor);
            tcu::clear(bottomRight, (params.testSampleMaskShader() ? clearColor : geometryColor));
        }
        // Prepare depth reference.
        {
            auto topLeft     = tcu::getSubregion(refDepthAccess, 0, 0, halfWidth, halfHeight);
            auto bottomLeft  = tcu::getSubregion(refDepthAccess, 0, halfHeight, halfWidth, halfHeight);
            auto topRight    = tcu::getSubregion(refDepthAccess, halfWidth, 0, halfWidth, halfHeight);
            auto bottomRight = tcu::getSubregion(refDepthAccess, halfWidth, halfHeight, halfWidth, halfHeight);

            tcu::clearDepth(topLeft,
                            (skipAlphaToCoverage ? ZExportParams::kExpectedDepth : ZExportParams::kClearDepth));
            tcu::clearDepth(bottomLeft,
                            (skipAlphaToCoverage ? (params.testSampleMaskShader() ? ZExportParams::kClearDepth :
                                                                                    ZExportParams::kExpectedDepth) :
                                                   ZExportParams::kClearDepth));
            tcu::clearDepth(topRight, ZExportParams::kExpectedDepth);
            tcu::clearDepth(bottomRight, (params.testSampleMaskShader() ? ZExportParams::kClearDepth :
                                                                          ZExportParams::kExpectedDepth));
        }
        // Prepare stencil reference.
        {
            const auto clearStencil    = static_cast<int>(ZExportParams::kClearStencil);
            const auto expectedStencil = static_cast<int>(ZExportParams::kExpectedStencil);

            auto topLeft     = tcu::getSubregion(refStencilAccess, 0, 0, halfWidth, halfHeight);
            auto bottomLeft  = tcu::getSubregion(refStencilAccess, 0, halfHeight, halfWidth, halfHeight);
            auto topRight    = tcu::getSubregion(refStencilAccess, halfWidth, 0, halfWidth, halfHeight);
            auto bottomRight = tcu::getSubregion(refStencilAccess, halfWidth, halfHeight, halfWidth, halfHeight);

            tcu::clearStencil(topLeft, (skipAlphaToCoverage ? expectedStencil : clearStencil));
            tcu::clearStencil(bottomLeft,
                              (skipAlphaToCoverage ? (params.testSampleMaskShader() ? clearStencil : expectedStencil) :
                                                     clearStencil));
            tcu::clearStencil(topRight, expectedStencil);
            tcu::clearStencil(bottomRight, (params.testSampleMaskShader() ? clearStencil : expectedStencil));
        }

        // Compare results and references.
        auto &log            = context.getTestContext().getLog();
        const auto colorOK   = tcu::floatThresholdCompare(log, "Color", "Color Result", refColorAccess, colorAccess,
                                                          colorThreshold, tcu::COMPARE_LOG_ON_ERROR);
        const auto depthOK   = tcu::dsThresholdCompare(log, "Depth", "Depth Result", refDepthAccess, depthAccess, 0.0f,
                                                       tcu::COMPARE_LOG_ON_ERROR);
        const auto stencilOK = tcu::dsThresholdCompare(log, "Stencil", "Stencil Result", refStencilAccess,
                                                       stencilAccess, 0.0f, tcu::COMPARE_LOG_ON_ERROR);

        if (colorOK && depthOK && stencilOK)
            return tcu::TestStatus::pass("Pass");
    }

    return tcu::TestStatus::fail("Unexpected color, depth or stencil result; check log for details");
}

struct SampleRateAlphaToCoverageParams
{
    PipelineConstructionType constructionType;
    bool dynamicState;

    tcu::IVec3 getExtent() const
    {
        return tcu::IVec3(3, 16, 1);
    }

    VkSampleCountFlagBits getSampleCount() const
    {
        return VK_SAMPLE_COUNT_4_BIT;
    }

    int getShiftBits() const
    {
        // When using 4 samples and expanding the multisample image into a single sample image, we need a 2x2 block for
        // each original pixel, and each pixel in the block can be addressed using 1 bit of the sample ID. If using,
        // e.g., 64 samples, we need an 8x8 block and each pixel is addressed with 3 bits in each dimension of the
        // sample ID. So, the number of bits per subblock address is log2(sqrt(sampleCount)). We also apply rounding in
        // case the result is not precise.
        const auto sampleCount = getSampleCount();
        DE_ASSERT(sampleCount == 4 || sampleCount == 16 || sampleCount == 64); // sqrt needs to be a whole number.
        const auto shiftBits = static_cast<int>(std::log2(std::sqrt(static_cast<float>(sampleCount))) + 0.5f);
        return shiftBits;
    }

    int32_t getBufferItemCount() const
    {
        // The buffer will contain values for the whole framebuffer.
        const auto fbExtent = getExtent();
        return fbExtent.x() * fbExtent.y() * fbExtent.z() * getSampleCount();
    }

    VkFormat getFormat() const
    {
        return VK_FORMAT_R8G8B8A8_UNORM;
    }

    uint32_t getRandomSeed() const
    {
        return 1730734808u;
    }

    tcu::Vec4 getClearColor() const
    {
        return tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    VkImageUsageFlags getImageUsage() const
    {
        return (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
    }
};

class SampleRateAlphaToCoverageInstance : public vkt::TestInstance
{
public:
    SampleRateAlphaToCoverageInstance(Context &context, const SampleRateAlphaToCoverageParams &params)
        : vkt::TestInstance(context)
        , m_params(params)
    {
    }
    virtual ~SampleRateAlphaToCoverageInstance(void) = default;

    tcu::TestStatus iterate(void) override;

protected:
    const SampleRateAlphaToCoverageParams m_params;
};

class SampleRateAlphaToCoverageCase : public vkt::TestCase
{
public:
    SampleRateAlphaToCoverageCase(tcu::TestContext &testCtx, const std::string &name,
                                  const SampleRateAlphaToCoverageParams &params)
        : vkt::TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual ~SampleRateAlphaToCoverageCase(void) = default;

    TestInstance *createInstance(Context &context) const override
    {
        return new SampleRateAlphaToCoverageInstance(context, m_params);
    }

    void checkSupport(Context &context) const override;
    void initPrograms(vk::SourceCollections &programCollection) const override;

protected:
    const SampleRateAlphaToCoverageParams m_params;
};

void SampleRateAlphaToCoverageCase::checkSupport(Context &context) const
{
    const auto ctx = context.getContextCommonData();

    checkPipelineConstructionRequirements(ctx.vki, ctx.physicalDevice, m_params.constructionType);

    if (m_params.dynamicState)
    {
#ifndef CTS_USES_VULKANSC
        const auto &eds3Features = context.getExtendedDynamicState3FeaturesEXT();
        if (!eds3Features.extendedDynamicState3AlphaToCoverageEnable)
            TCU_THROW(NotSupportedError, "extendedDynamicState3AlphaToCoverageEnable not supported");
#else
        TCU_THROW(NotSupportedError, "VK_EXT_extended_dynamic_state3 not supported in VulkanSC");
#endif // CTS_USES_VULKANSC
    }

    const auto imageUsage = m_params.getImageUsage();

    VkImageFormatProperties formatProperties;
    const auto result =
        ctx.vki.getPhysicalDeviceImageFormatProperties(ctx.physicalDevice, m_params.getFormat(), VK_IMAGE_TYPE_2D,
                                                       VK_IMAGE_TILING_OPTIMAL, imageUsage, 0u, &formatProperties);

    if (result == VK_ERROR_FORMAT_NOT_SUPPORTED)
        TCU_THROW(NotSupportedError, "Implementation does not support required format features");
    else if (result != VK_SUCCESS)
        TCU_FAIL(std::string("vkGetPhysicalDeviceImageFormatProperties error: ") + getResultName(result));

    const auto sampleCount = m_params.getSampleCount();
    if ((formatProperties.sampleCounts & sampleCount) != sampleCount)
        TCU_THROW(NotSupportedError, "Required sample count not supported");
}

void SampleRateAlphaToCoverageCase::initPrograms(vk::SourceCollections &programCollection) const
{
    // Full-screen triangle that saves us from having to create a vertex buffer.
    std::ostringstream vert;
    vert << "#version 460\n"
         << "const vec4 vertices[] = vec4[](\n"
         << "    vec4(-1.0, -1.0, 0.0, 1.0),\n"
         << "    vec4(-1.0,  3.0, 0.0, 1.0),\n"
         << "    vec4( 3.0, -1.0, 0.0, 1.0)\n"
         << ");\n"
         << "void main (void) {\n"
         << "    gl_Position = vertices[gl_VertexIndex % 3];\n"
         << "}\n";
    programCollection.glslSources.add("vert") << glu::VertexSource(vert.str());

    const auto fbExtent    = m_params.getExtent();
    const auto sampleCount = m_params.getSampleCount();
    const auto itemCount   = m_params.getBufferItemCount();

    DE_ASSERT(fbExtent.z() == 1);
    std::ostringstream frag;
    frag << "#version 460\n"
         << "layout (location=0) out vec4 outColor;\n"
         << "layout (set=0, binding=0, std430) readonly buffer CoverageBlock { float alpha[" << itemCount
         << "]; } coverage;\n"
         << "void main(void) {\n"
         << "    const int cols = " << fbExtent.x() << ";\n"
         << "    const int rows = " << fbExtent.y() << ";\n"
         << "    const int sampleCount = " << sampleCount << ";\n"
         << "    const int xIdx = int(gl_FragCoord.x);\n"
         << "    const int yIdx = int(gl_FragCoord.y);\n"
         << "    const int bufferIdx = yIdx * (sampleCount * cols) + xIdx * sampleCount + gl_SampleID;\n"
         << "    const float alpha = coverage.alpha[bufferIdx];\n"
         << "    outColor = vec4(0.0, 0.0, 1.0, alpha);\n"
         << "}\n";
    programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());

    // The compute shader will sample the multisample color attachment and "translate it" to a single sample image where
    // each pixel is a 2x2 pixel block that contains the values of the 4 samples.
    const auto shiftBits = m_params.getShiftBits();
    std::ostringstream comp;
    comp << "#version 460\n"
         << "layout (local_size_x=" << fbExtent.x() << ", local_size_y=" << fbExtent.y()
         << ", local_size_z=" << fbExtent.z() << ") in;\n"
         << "layout (set=0, binding=0) uniform sampler2DMS resultSampler;\n"
         << "layout (rgba8, set=0, binding=1) uniform image2D expandedImg;\n"
         << "void main(void) {\n"
         << "    const int sampleCount = " << sampleCount << ";\n"
         << "    const int shiftBits = " << shiftBits << ";\n"
         << "    const int shiftMask = (1 << shiftBits) - 1;\n"
         << "    const ivec2 invID = ivec2(gl_LocalInvocationID.xy);\n"
         << "    for (int i = 0; i < sampleCount; ++i) {\n"
         << "        const int subCol = ((i >> shiftBits) & shiftMask);\n"
         << "        const int subRow = (i & shiftMask);\n"
         << "        const int xCoord = invID.x * 2 + subCol;\n"
         << "        const int yCoord = invID.y * 2 + subRow;\n"
         << "        const vec4 resultColor = texelFetch(resultSampler, invID, i);\n"
         << "        imageStore(expandedImg, ivec2(xCoord, yCoord), resultColor);\n"
         << "    }\n"
         << "}\n";
    programCollection.glslSources.add("comp") << glu::ComputeSource(comp.str());
}

tcu::TestStatus SampleRateAlphaToCoverageInstance::iterate(void)
{
    const auto ctx         = m_context.getContextCommonData();
    const auto itemCount   = m_params.getBufferItemCount();
    const auto fbExtent    = m_params.getExtent();
    const auto apiExtent   = makeExtent3D(fbExtent);
    const auto sampleCount = m_params.getSampleCount();
    const auto shiftBits   = m_params.getShiftBits();
    const auto blockDim    = (1u << shiftBits);
    const auto shiftMask   = blockDim - 1u;
    const tcu::IVec3 expandedExtent(fbExtent.x() * blockDim, fbExtent.y() * blockDim, fbExtent.z());
    const auto middleColumn = static_cast<int>(static_cast<float>(fbExtent.x()) / 2.0);
    const auto randomSeed   = m_params.getRandomSeed();
    const auto imageFormat  = m_params.getFormat();
    const auto imageUsage   = m_params.getImageUsage();
    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);

    // Prepare coverage buffer. One value per sample, row by row, column by column, pixel by pixel.
    std::vector<float> coverage;
    coverage.reserve(itemCount);

    DE_ASSERT(fbExtent.z() == 1);
    de::Random rnd(randomSeed);

    for (int y = 0; y < fbExtent.y(); ++y)
        for (int x = 0; x < fbExtent.x(); ++x)
            for (int s = 0; s < sampleCount; ++s)
            {
                if (x < middleColumn)
                    coverage.push_back(1.0f);
                else if (x > middleColumn)
                    coverage.push_back(0.0f);
                else
                    coverage.push_back(rnd.getBool() ? 1.0f : 0.0f);
            }

    const auto coverageBufferInfo =
        makeBufferCreateInfo(static_cast<VkDeviceSize>(de::dataSize(coverage)), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    BufferWithMemory coverageBuffer(ctx.vkd, ctx.device, ctx.allocator, coverageBufferInfo,
                                    MemoryRequirement::HostVisible);
    {
        auto &alloc   = coverageBuffer.getAllocation();
        void *dataPtr = alloc.getHostPtr();
        deMemcpy(dataPtr, de::dataOrNull(coverage), de::dataSize(coverage));
        flushAlloc(ctx.vkd, ctx.device, alloc);
    }

    // Multisample color buffer.
    const VkImageCreateInfo colorBufferInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        nullptr,
        0u,
        VK_IMAGE_TYPE_2D,
        imageFormat,
        apiExtent,
        1u,
        1u,
        sampleCount,
        VK_IMAGE_TILING_OPTIMAL,
        imageUsage,
        VK_SHARING_MODE_EXCLUSIVE,
        0u,
        nullptr,
        VK_IMAGE_LAYOUT_UNDEFINED,
    };
    ImageWithMemory colorBuffer(ctx.vkd, ctx.device, ctx.allocator, colorBufferInfo, MemoryRequirement::Any);
    const auto colorSRR = makeDefaultImageSubresourceRange();
    const auto colorBufferView =
        makeImageView(ctx.vkd, ctx.device, colorBuffer.get(), VK_IMAGE_VIEW_TYPE_2D, imageFormat, colorSRR);

    // Sampler for the compute shader.
    const VkSamplerCreateInfo samplerCreateInfo = {
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        nullptr,
        0u,
        VK_FILTER_NEAREST,
        VK_FILTER_NEAREST,
        VK_SAMPLER_MIPMAP_MODE_NEAREST,
        VK_SAMPLER_ADDRESS_MODE_REPEAT,
        VK_SAMPLER_ADDRESS_MODE_REPEAT,
        VK_SAMPLER_ADDRESS_MODE_REPEAT,
        0.0f,
        VK_FALSE,
        0.0f,
        VK_FALSE,
        VK_COMPARE_OP_NEVER,
        0.0f,
        1.0f,
        VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        VK_FALSE,
    };
    const auto sampler = createSampler(ctx.vkd, ctx.device, &samplerCreateInfo);

    // Single-sample "expanded" result.
    VkImageCreateInfo expandedImgInfo = colorBufferInfo;
    expandedImgInfo.samples           = VK_SAMPLE_COUNT_1_BIT;
    expandedImgInfo.extent = makeExtent3D(apiExtent.width * blockDim, apiExtent.height * blockDim, apiExtent.depth);
    expandedImgInfo.usage =
        (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT);

    ImageWithBuffer expandedImg(ctx.vkd, ctx.device, ctx.allocator, expandedImgInfo.extent, expandedImgInfo.format,
                                expandedImgInfo.usage, expandedImgInfo.imageType);

    // Prepare descriptor pool, layouts and sets.
    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    const auto descriptorPool =
        poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 2u);

    Move<VkDescriptorSetLayout> fragSetLayout;
    Move<VkDescriptorSetLayout> compSetLayout;

    {
        DescriptorSetLayoutBuilder layoutBuilder;
        layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
        fragSetLayout = layoutBuilder.build(ctx.vkd, ctx.device);
    }
    {
        DescriptorSetLayoutBuilder layoutBuilder;
        layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT);
        layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);
        compSetLayout = layoutBuilder.build(ctx.vkd, ctx.device);
    }

    const auto fragDescSet = makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *fragSetLayout);
    const auto compDescSet = makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *compSetLayout);

    // Update descriptor sets.
    using Location = DescriptorSetUpdateBuilder::Location;
    {
        DescriptorSetUpdateBuilder updateBuilder;
        const auto bufferInfo = makeDescriptorBufferInfo(*coverageBuffer, 0ull, VK_WHOLE_SIZE);
        updateBuilder.writeSingle(*fragDescSet, Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferInfo);
        updateBuilder.update(ctx.vkd, ctx.device);
    }
    {
        DescriptorSetUpdateBuilder updateBuilder;
        const auto combinedInfo =
            makeDescriptorImageInfo(*sampler, *colorBufferView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        const auto storageInfo =
            makeDescriptorImageInfo(VK_NULL_HANDLE, expandedImg.getImageView(), VK_IMAGE_LAYOUT_GENERAL);
        updateBuilder.writeSingle(*compDescSet, Location::binding(0u), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                  &combinedInfo);
        updateBuilder.writeSingle(*compDescSet, Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &storageInfo);
        updateBuilder.update(ctx.vkd, ctx.device);
    }

    // Render pass and framebuffer for the graphics part.
    const auto attDesc     = makeAttachmentDescription(0u, imageFormat, sampleCount, VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                       VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                       VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED,
                                                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    const auto attRef      = makeAttachmentReference(0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    const auto subpassDesc = makeSubpassDescription(0u, VK_PIPELINE_BIND_POINT_GRAPHICS, 0u, nullptr, 1u, &attRef,
                                                    nullptr, nullptr, 0u, nullptr);
    const VkRenderPassCreateInfo renderPassCreateInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, nullptr, 0u, 1u, &attDesc, 1u, &subpassDesc, 0u, nullptr,
    };
    RenderPassWrapper renderPass(m_params.constructionType, ctx.vkd, ctx.device, &renderPassCreateInfo);
    renderPass.createFramebuffer(ctx.vkd, ctx.device, *colorBuffer, *colorBufferView, apiExtent.width,
                                 apiExtent.height);

    const std::vector<VkViewport> viewports(1u, makeViewport(fbExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(fbExtent));

    // Pipeline layouts.
    PipelineLayoutWrapper graphicsPipelineLayout(m_params.constructionType, ctx.vkd, ctx.device, *fragSetLayout);
    const auto computePipelineLayout = makePipelineLayout(ctx.vkd, ctx.device, *compSetLayout);

    // Shaders.
    const auto &binaries = m_context.getBinaryCollection();
    const ShaderWrapper vertModule(ctx.vkd, ctx.device, binaries.get("vert"));
    const ShaderWrapper fragModule(ctx.vkd, ctx.device, binaries.get("frag"));
    const auto compModule = createShaderModule(ctx.vkd, ctx.device, binaries.get("comp"));

    // Pipelines.
    const auto staticAlphaToCoverage = (m_params.dynamicState ? VK_FALSE : VK_TRUE);
#ifndef CTS_USES_VULKANSC
    const auto dynamicAlphaToCoverage = (m_params.dynamicState ? VK_TRUE : VK_FALSE);
#endif // CTS_USES_VULKANSC

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = initVulkanStructure();

    const VkPipelineMultisampleStateCreateInfo multiSampleStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        nullptr,
        0u,
        sampleCount,
        VK_FALSE, // Sample shading should be enabled because the frag shader uses gl_SampleID.
        0.0f,
        nullptr,
        staticAlphaToCoverage,
        VK_FALSE,
    };

    std::vector<VkDynamicState> dynamicStates;
#ifndef CTS_USES_VULKANSC
    if (m_params.dynamicState)
        dynamicStates.push_back(VK_DYNAMIC_STATE_ALPHA_TO_COVERAGE_ENABLE_EXT);
#endif // CTS_USES_VULKANSC

    const VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        nullptr,
        0u,
        de::sizeU32(dynamicStates),
        de::dataOrNull(dynamicStates),
    };

    GraphicsPipelineWrapper graphicsPipeline(ctx.vki, ctx.vkd, ctx.physicalDevice, ctx.device,
                                             m_context.getDeviceExtensions(), m_params.constructionType);

    graphicsPipeline.setDefaultTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .setDefaultRasterizationState()
        .setDefaultDepthStencilState()
        .setDefaultColorBlendState()
        .setMonolithicPipelineLayout(graphicsPipelineLayout)
        .setDynamicState(&dynamicStateCreateInfo)
        .setupVertexInputState(&vertexInputStateCreateInfo)
        .setupPreRasterizationShaderState(viewports, scissors, graphicsPipelineLayout, renderPass.get(), 0u, vertModule)
        .setupFragmentShaderState(graphicsPipelineLayout, renderPass.get(), 0u, fragModule, nullptr,
                                  &multiSampleStateCreateInfo)
        .setupFragmentOutputState(renderPass.get(), 0u, nullptr, &multiSampleStateCreateInfo)
        .buildPipeline();

    const auto computePipeline = makeComputePipeline(ctx.vkd, ctx.device, *computePipelineLayout, *compModule);

    // Submit work.
    {
        CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
        const auto cmdBuffer = *cmd.cmdBuffer;

        beginCommandBuffer(ctx.vkd, cmdBuffer);
        renderPass.begin(ctx.vkd, cmdBuffer, scissors.at(0u), clearColor);
        graphicsPipeline.bind(cmdBuffer);
        ctx.vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphicsPipelineLayout, 0u, 1u,
                                      &fragDescSet.get(), 0u, nullptr);
#ifndef CTS_USES_VULKANSC
        if (m_params.dynamicState)
            ctx.vkd.cmdSetAlphaToCoverageEnableEXT(cmdBuffer, dynamicAlphaToCoverage);
#endif // CTS_USES_VULKANSC
        ctx.vkd.cmdDraw(cmdBuffer, 3u, 1u, 0u, 0u);
        renderPass.end(ctx.vkd, cmdBuffer);

        {
            const std::vector<VkImageMemoryBarrier> imageBarriers{
                // Move multisample image to shader read optimal before the compute shader.
                makeImageMemoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, *colorBuffer, colorSRR),

                // Transition expanded image to the proper layout for writing.
                makeImageMemoryBarrier(0u, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                       VK_IMAGE_LAYOUT_GENERAL, expandedImg.getImage(), colorSRR),
            };

            const auto srcStages = (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
            const auto dstStages = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

            cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, srcStages, dstStages, de::dataOrNull(imageBarriers),
                                          imageBarriers.size());
        }

        ctx.vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *computePipeline);
        ctx.vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *computePipelineLayout, 0u, 1u,
                                      &compDescSet.get(), 0u, nullptr);
        ctx.vkd.cmdDispatch(cmdBuffer, 1u, 1u, 1u);

        // Read expanded image.
        copyImageToBuffer(ctx.vkd, cmdBuffer, expandedImg.getImage(), expandedImg.getBuffer(),
                          expandedExtent.swizzle(0, 1), VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

        endCommandBuffer(ctx.vkd, cmdBuffer);
        submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);
    }

    // Create reference image.
    const auto tcuFormat = mapVkFormat(imageFormat);
    tcu::TextureLevel referenceLevel(tcuFormat, expandedExtent.x(), expandedExtent.y(), expandedExtent.z());
    tcu::PixelBufferAccess referenceAccess = referenceLevel.getAccess();

    for (int y = 0; y < fbExtent.y(); ++y)
        for (int x = 0; x < fbExtent.x(); ++x)
            for (int s = 0; s < sampleCount; ++s)
            {
                const int index           = y * (sampleCount * fbExtent.x()) + x * sampleCount + s;
                const float coverageValue = coverage.at(index);
                const auto subX           = ((s >> shiftBits) & shiftMask);
                const auto subY           = (s & shiftMask);
                const auto xCoord         = x * blockDim + subX;
                const auto yCoord         = y * blockDim + subY;
                const tcu::Vec4 color(0.0f, 0.0f, (coverageValue > 0.0f ? 1.0f : 0.0f), 1.0f);
                referenceAccess.setPixel(color, xCoord, yCoord);
            }

    invalidateAlloc(ctx.vkd, ctx.device, expandedImg.getBufferAllocation());
    tcu::ConstPixelBufferAccess resultAccess(tcuFormat, expandedExtent, expandedImg.getBufferAllocation().getHostPtr());
    const tcu::Vec4 threshold(0.0f, 0.0f, 0.0f, 0.0f);

    auto &log = m_context.getTestContext().getLog();
    if (!tcu::floatThresholdCompare(log, "Result (2x2 pixel blocks per original pixel)", "", referenceAccess,
                                    resultAccess, threshold, tcu::COMPARE_LOG_ON_ERROR))
        TCU_FAIL("Unexpected expanded color buffer contents; check log for details --");

    return tcu::TestStatus::pass("Pass");
}

} // namespace

tcu::TestCaseGroup *createMultisampleTests(tcu::TestContext &testCtx, PipelineConstructionType pipelineConstructionType,
                                           bool useFragmentShadingRate)
{
    using TestCaseGroupPtr = de::MovePtr<tcu::TestCaseGroup>;

    const VkSampleCountFlagBits samples[] = {VK_SAMPLE_COUNT_2_BIT,  VK_SAMPLE_COUNT_4_BIT,  VK_SAMPLE_COUNT_8_BIT,
                                             VK_SAMPLE_COUNT_16_BIT, VK_SAMPLE_COUNT_32_BIT, VK_SAMPLE_COUNT_64_BIT};

    const char *groupName[]{"multisample", "multisample_with_fragment_shading_rate"};
    TestCaseGroupPtr multisampleTests(new tcu::TestCaseGroup(testCtx, groupName[useFragmentShadingRate]));

    // Rasterization samples tests
    {
        TestCaseGroupPtr rasterizationSamplesTests(new tcu::TestCaseGroup(testCtx, "raster_samples"));

        for (int samplesNdx = 0; samplesNdx < DE_LENGTH_OF_ARRAY(samples); samplesNdx++)
        {
            std::ostringstream caseName;
            caseName << "samples_" << samples[samplesNdx];

            TestCaseGroupPtr samplesTests(new tcu::TestCaseGroup(testCtx, caseName.str().c_str()));

            samplesTests->addChild(new RasterizationSamplesTest(
                testCtx, "primitive_triangle", pipelineConstructionType, samples[samplesNdx],
                GEOMETRY_TYPE_OPAQUE_TRIANGLE, 1.0f, IMAGE_BACKING_MODE_REGULAR, 0u, useFragmentShadingRate));
            samplesTests->addChild(new RasterizationSamplesTest(
                testCtx, "primitive_line", pipelineConstructionType, samples[samplesNdx], GEOMETRY_TYPE_OPAQUE_LINE,
                1.0f, IMAGE_BACKING_MODE_REGULAR, 0u, useFragmentShadingRate));
            samplesTests->addChild(new RasterizationSamplesTest(
                testCtx, "primitive_point_1px", pipelineConstructionType, samples[samplesNdx],
                GEOMETRY_TYPE_OPAQUE_POINT, 1.0f, IMAGE_BACKING_MODE_REGULAR, 0u, useFragmentShadingRate));
            samplesTests->addChild(new RasterizationSamplesTest(
                testCtx, "primitive_point", pipelineConstructionType, samples[samplesNdx], GEOMETRY_TYPE_OPAQUE_POINT,
                3.0f, IMAGE_BACKING_MODE_REGULAR, 0u, useFragmentShadingRate));

            samplesTests->addChild(new RasterizationSamplesTest(
                testCtx, "depth", pipelineConstructionType, samples[samplesNdx], GEOMETRY_TYPE_INVISIBLE_TRIANGLE, 1.0f,
                IMAGE_BACKING_MODE_REGULAR, TEST_MODE_DEPTH_BIT, useFragmentShadingRate));
            samplesTests->addChild(new RasterizationSamplesTest(
                testCtx, "stencil", pipelineConstructionType, samples[samplesNdx], GEOMETRY_TYPE_INVISIBLE_TRIANGLE,
                1.0f, IMAGE_BACKING_MODE_REGULAR, TEST_MODE_STENCIL_BIT, useFragmentShadingRate));
            samplesTests->addChild(
                new RasterizationSamplesTest(testCtx, "depth_stencil", pipelineConstructionType, samples[samplesNdx],
                                             GEOMETRY_TYPE_INVISIBLE_TRIANGLE, 1.0f, IMAGE_BACKING_MODE_REGULAR,
                                             TEST_MODE_DEPTH_BIT | TEST_MODE_STENCIL_BIT, useFragmentShadingRate));

#ifndef CTS_USES_VULKANSC
            samplesTests->addChild(new RasterizationSamplesTest(
                testCtx, "primitive_triangle_sparse", pipelineConstructionType, samples[samplesNdx],
                GEOMETRY_TYPE_OPAQUE_TRIANGLE, 1.0f, IMAGE_BACKING_MODE_SPARSE, 0u, useFragmentShadingRate));
            samplesTests->addChild(new RasterizationSamplesTest(
                testCtx, "primitive_line_sparse", pipelineConstructionType, samples[samplesNdx],
                GEOMETRY_TYPE_OPAQUE_LINE, 1.0f, IMAGE_BACKING_MODE_SPARSE, 0u, useFragmentShadingRate));
            samplesTests->addChild(new RasterizationSamplesTest(
                testCtx, "primitive_point_1px_sparse", pipelineConstructionType, samples[samplesNdx],
                GEOMETRY_TYPE_OPAQUE_POINT, 1.0f, IMAGE_BACKING_MODE_SPARSE, 0u, useFragmentShadingRate));
            samplesTests->addChild(new RasterizationSamplesTest(
                testCtx, "primitive_point_sparse", pipelineConstructionType, samples[samplesNdx],
                GEOMETRY_TYPE_OPAQUE_POINT, 3.0f, IMAGE_BACKING_MODE_SPARSE, 0u, useFragmentShadingRate));

            samplesTests->addChild(new RasterizationSamplesTest(testCtx, "depth_sparse", pipelineConstructionType,
                                                                samples[samplesNdx], GEOMETRY_TYPE_INVISIBLE_TRIANGLE,
                                                                1.0f, IMAGE_BACKING_MODE_SPARSE, TEST_MODE_DEPTH_BIT,
                                                                useFragmentShadingRate));
            samplesTests->addChild(new RasterizationSamplesTest(testCtx, "stencil_sparse", pipelineConstructionType,
                                                                samples[samplesNdx], GEOMETRY_TYPE_INVISIBLE_TRIANGLE,
                                                                1.0f, IMAGE_BACKING_MODE_SPARSE, TEST_MODE_STENCIL_BIT,
                                                                useFragmentShadingRate));
            samplesTests->addChild(new RasterizationSamplesTest(
                testCtx, "depth_stencil_sparse", pipelineConstructionType, samples[samplesNdx],
                GEOMETRY_TYPE_INVISIBLE_TRIANGLE, 1.0f, IMAGE_BACKING_MODE_SPARSE,
                TEST_MODE_DEPTH_BIT | TEST_MODE_STENCIL_BIT, useFragmentShadingRate));
#endif // CTS_USES_VULKANSC
            rasterizationSamplesTests->addChild(samplesTests.release());
        }

        multisampleTests->addChild(rasterizationSamplesTests.release());
    }

    // Raster samples consistency check
#ifndef CTS_USES_VULKANSC
    {
        TestCaseGroupPtr rasterSamplesConsistencyTests(new tcu::TestCaseGroup(testCtx, "raster_samples_consistency"));
        MultisampleTestParams paramsRegular = {pipelineConstructionType, GEOMETRY_TYPE_OPAQUE_TRIANGLE, 1.0f,
                                               IMAGE_BACKING_MODE_REGULAR, useFragmentShadingRate};
        MultisampleTestParams paramsSparse  = {pipelineConstructionType, GEOMETRY_TYPE_OPAQUE_TRIANGLE, 1.0f,
                                               IMAGE_BACKING_MODE_SPARSE, useFragmentShadingRate};

        addFunctionCaseWithPrograms(rasterSamplesConsistencyTests.get(), "unique_colors_check", checkSupport,
                                    initMultisamplePrograms, testRasterSamplesConsistency, paramsRegular);
        addFunctionCaseWithPrograms(rasterSamplesConsistencyTests.get(), "unique_colors_check_sparse", checkSupport,
                                    initMultisamplePrograms, testRasterSamplesConsistency, paramsSparse);
        multisampleTests->addChild(rasterSamplesConsistencyTests.release());
    }
#endif // CTS_USES_VULKANSC

    // minSampleShading tests
    {
        struct TestConfig
        {
            const char *name;
            float minSampleShading;
        };

        const TestConfig testConfigs[] = {
            {"min_0_0", 0.0f}, {"min_0_25", 0.25f}, {"min_0_5", 0.5f}, {"min_0_75", 0.75f}, {"min_1_0", 1.0f}};

        // Input attachments are not supported with dynamic rendering and shader objects
        if (!isConstructionTypeShaderObject(pipelineConstructionType))
        {
            TestCaseGroupPtr minSampleShadingTests(new tcu::TestCaseGroup(testCtx, "min_sample_shading"));
            {
                for (int configNdx = 0; configNdx < DE_LENGTH_OF_ARRAY(testConfigs); configNdx++)
                {
                    const TestConfig &testConfig = testConfigs[configNdx];

                    // minSampleShading is not supported by shader objects
                    if (testConfig.minSampleShading != 1.0f && isConstructionTypeShaderObject(pipelineConstructionType))
                        continue;

                    TestCaseGroupPtr minShadingValueTests(new tcu::TestCaseGroup(testCtx, testConfigs[configNdx].name));

                    for (int samplesNdx = 0; samplesNdx < DE_LENGTH_OF_ARRAY(samples); samplesNdx++)
                    {
                        std::ostringstream caseName;
                        caseName << "samples_" << samples[samplesNdx];

                        TestCaseGroupPtr samplesTests(new tcu::TestCaseGroup(testCtx, caseName.str().c_str()));

                        samplesTests->addChild(new MinSampleShadingTest(
                            testCtx, "primitive_triangle", pipelineConstructionType, samples[samplesNdx],
                            testConfig.minSampleShading, GEOMETRY_TYPE_OPAQUE_TRIANGLE, 1.0f,
                            IMAGE_BACKING_MODE_REGULAR, true, useFragmentShadingRate));
                        samplesTests->addChild(new MinSampleShadingTest(
                            testCtx, "primitive_line", pipelineConstructionType, samples[samplesNdx],
                            testConfig.minSampleShading, GEOMETRY_TYPE_OPAQUE_LINE, 1.0f, IMAGE_BACKING_MODE_REGULAR,
                            true, useFragmentShadingRate));
                        samplesTests->addChild(new MinSampleShadingTest(
                            testCtx, "primitive_point_1px", pipelineConstructionType, samples[samplesNdx],
                            testConfig.minSampleShading, GEOMETRY_TYPE_OPAQUE_POINT, 1.0f, IMAGE_BACKING_MODE_REGULAR,
                            true, useFragmentShadingRate));
                        samplesTests->addChild(new MinSampleShadingTest(
                            testCtx, "primitive_point", pipelineConstructionType, samples[samplesNdx],
                            testConfig.minSampleShading, GEOMETRY_TYPE_OPAQUE_POINT, 3.0f, IMAGE_BACKING_MODE_REGULAR,
                            true, useFragmentShadingRate));
#ifndef CTS_USES_VULKANSC
                        samplesTests->addChild(new MinSampleShadingTest(
                            testCtx, "primitive_triangle_sparse", pipelineConstructionType, samples[samplesNdx],
                            testConfig.minSampleShading, GEOMETRY_TYPE_OPAQUE_TRIANGLE, 1.0f, IMAGE_BACKING_MODE_SPARSE,
                            true, useFragmentShadingRate));
                        samplesTests->addChild(new MinSampleShadingTest(
                            testCtx, "primitive_line_sparse", pipelineConstructionType, samples[samplesNdx],
                            testConfig.minSampleShading, GEOMETRY_TYPE_OPAQUE_LINE, 1.0f, IMAGE_BACKING_MODE_SPARSE,
                            true, useFragmentShadingRate));
                        samplesTests->addChild(new MinSampleShadingTest(
                            testCtx, "primitive_point_1px_sparse", pipelineConstructionType, samples[samplesNdx],
                            testConfig.minSampleShading, GEOMETRY_TYPE_OPAQUE_POINT, 1.0f, IMAGE_BACKING_MODE_SPARSE,
                            true, useFragmentShadingRate));
                        samplesTests->addChild(new MinSampleShadingTest(
                            testCtx, "primitive_point_sparse", pipelineConstructionType, samples[samplesNdx],
                            testConfig.minSampleShading, GEOMETRY_TYPE_OPAQUE_POINT, 3.0f, IMAGE_BACKING_MODE_SPARSE,
                            true, useFragmentShadingRate));
#endif // CTS_USES_VULKANSC

                        minShadingValueTests->addChild(samplesTests.release());
                    }

                    minSampleShadingTests->addChild(minShadingValueTests.release());
                }

                multisampleTests->addChild(minSampleShadingTests.release());
            }
        }

        // Input attachments are not supported with dynamic rendering and shader objects
        if (!isConstructionTypeShaderObject(pipelineConstructionType))
        {
            TestCaseGroupPtr minSampleShadingTests(new tcu::TestCaseGroup(testCtx, "min_sample_shading_enabled"));

            for (int configNdx = 0; configNdx < DE_LENGTH_OF_ARRAY(testConfigs); configNdx++)
            {
                const TestConfig &testConfig = testConfigs[configNdx];
                TestCaseGroupPtr minShadingValueTests(new tcu::TestCaseGroup(testCtx, testConfigs[configNdx].name));

                for (int samplesNdx = 0; samplesNdx < DE_LENGTH_OF_ARRAY(samples); samplesNdx++)
                {
                    std::ostringstream caseName;
                    caseName << "samples_" << samples[samplesNdx];

                    TestCaseGroupPtr samplesTests(new tcu::TestCaseGroup(testCtx, caseName.str().c_str()));

                    samplesTests->addChild(new MinSampleShadingTest(
                        testCtx, "quad", pipelineConstructionType, samples[samplesNdx], testConfig.minSampleShading,
                        GEOMETRY_TYPE_OPAQUE_QUAD, 1.0f, IMAGE_BACKING_MODE_REGULAR, true, useFragmentShadingRate));

                    minShadingValueTests->addChild(samplesTests.release());
                }

                minSampleShadingTests->addChild(minShadingValueTests.release());
            }

            multisampleTests->addChild(minSampleShadingTests.release());
        }

        // Input attachments are not supported with dynamic rendering and shader objects
        if (!isConstructionTypeShaderObject(pipelineConstructionType))
        {
            TestCaseGroupPtr minSampleShadingTests(new tcu::TestCaseGroup(testCtx, "min_sample_shading_disabled"));

            for (int configNdx = 0; configNdx < DE_LENGTH_OF_ARRAY(testConfigs); configNdx++)
            {
                const TestConfig &testConfig = testConfigs[configNdx];
                TestCaseGroupPtr minShadingValueTests(new tcu::TestCaseGroup(testCtx, testConfigs[configNdx].name));

                for (int samplesNdx = 0; samplesNdx < DE_LENGTH_OF_ARRAY(samples); samplesNdx++)
                {
                    std::ostringstream caseName;
                    caseName << "samples_" << samples[samplesNdx];

                    TestCaseGroupPtr samplesTests(new tcu::TestCaseGroup(testCtx, caseName.str().c_str()));

                    samplesTests->addChild(new MinSampleShadingTest(
                        testCtx, "quad", pipelineConstructionType, samples[samplesNdx], testConfig.minSampleShading,
                        GEOMETRY_TYPE_OPAQUE_QUAD, 1.0f, IMAGE_BACKING_MODE_REGULAR, false, useFragmentShadingRate));

                    minShadingValueTests->addChild(samplesTests.release());
                }

                minSampleShadingTests->addChild(minShadingValueTests.release());
            }

            multisampleTests->addChild(minSampleShadingTests.release());
        }
    }

    // SampleMask tests
    {
        struct TestConfig
        {
            const char *name;
            VkSampleMask sampleMask;
        };

        const TestConfig testConfigs[] = {
            // All mask bits are off
            {"mask_all_on", 0x0},
            // All mask bits are on
            {"mask_all_off", 0xFFFFFFFF},
            // All mask elements are 0x1
            {"mask_one", 0x1},
            // All mask elements are 0xAAAAAAAA
            {"mask_random", 0xAAAAAAAA},
        };

        TestCaseGroupPtr sampleMaskTests(new tcu::TestCaseGroup(testCtx, "sample_mask"));

        for (int configNdx = 0; configNdx < DE_LENGTH_OF_ARRAY(testConfigs); configNdx++)
        {
            const TestConfig &testConfig = testConfigs[configNdx];
            TestCaseGroupPtr sampleMaskValueTests(new tcu::TestCaseGroup(testCtx, testConfig.name));

            for (int samplesNdx = 0; samplesNdx < DE_LENGTH_OF_ARRAY(samples); samplesNdx++)
            {
                std::ostringstream caseName;
                caseName << "samples_" << samples[samplesNdx];

                const uint32_t sampleMaskCount = samples[samplesNdx] / 32;
                TestCaseGroupPtr samplesTests(new tcu::TestCaseGroup(testCtx, caseName.str().c_str()));

                std::vector<VkSampleMask> mask;
                for (uint32_t maskNdx = 0; maskNdx < sampleMaskCount; maskNdx++)
                    mask.push_back(testConfig.sampleMask);

                samplesTests->addChild(new SampleMaskTest(testCtx, "primitive_triangle", pipelineConstructionType,
                                                          samples[samplesNdx], mask, GEOMETRY_TYPE_OPAQUE_TRIANGLE,
                                                          1.0f, IMAGE_BACKING_MODE_REGULAR, useFragmentShadingRate));
                samplesTests->addChild(new SampleMaskTest(testCtx, "primitive_line", pipelineConstructionType,
                                                          samples[samplesNdx], mask, GEOMETRY_TYPE_OPAQUE_LINE, 1.0f,
                                                          IMAGE_BACKING_MODE_REGULAR, useFragmentShadingRate));
                samplesTests->addChild(new SampleMaskTest(testCtx, "primitive_point_1px", pipelineConstructionType,
                                                          samples[samplesNdx], mask, GEOMETRY_TYPE_OPAQUE_POINT, 1.0f,
                                                          IMAGE_BACKING_MODE_REGULAR, useFragmentShadingRate));
                samplesTests->addChild(new SampleMaskTest(testCtx, "primitive_point", pipelineConstructionType,
                                                          samples[samplesNdx], mask, GEOMETRY_TYPE_OPAQUE_POINT, 3.0f,
                                                          IMAGE_BACKING_MODE_REGULAR, useFragmentShadingRate));
#ifndef CTS_USES_VULKANSC
                samplesTests->addChild(new SampleMaskTest(
                    testCtx, "primitive_triangle_sparse", pipelineConstructionType, samples[samplesNdx], mask,
                    GEOMETRY_TYPE_OPAQUE_TRIANGLE, 1.0f, IMAGE_BACKING_MODE_SPARSE, useFragmentShadingRate));
                samplesTests->addChild(new SampleMaskTest(testCtx, "primitive_line_sparse", pipelineConstructionType,
                                                          samples[samplesNdx], mask, GEOMETRY_TYPE_OPAQUE_LINE, 1.0f,
                                                          IMAGE_BACKING_MODE_SPARSE, useFragmentShadingRate));
                samplesTests->addChild(new SampleMaskTest(
                    testCtx, "primitive_point_1px_sparse", pipelineConstructionType, samples[samplesNdx], mask,
                    GEOMETRY_TYPE_OPAQUE_POINT, 1.0f, IMAGE_BACKING_MODE_SPARSE, useFragmentShadingRate));
                samplesTests->addChild(new SampleMaskTest(testCtx, "primitive_point_sparse", pipelineConstructionType,
                                                          samples[samplesNdx], mask, GEOMETRY_TYPE_OPAQUE_POINT, 3.0f,
                                                          IMAGE_BACKING_MODE_SPARSE, useFragmentShadingRate));
#endif // CTS_USES_VULKANSC

                sampleMaskValueTests->addChild(samplesTests.release());
            }

            sampleMaskTests->addChild(sampleMaskValueTests.release());
        }

        multisampleTests->addChild(sampleMaskTests.release());
    }

    // AlphaToOne tests
    {
        const VkSampleCountFlagBits samplesForAlphaToOne[] = {
            VK_SAMPLE_COUNT_1_BIT,  VK_SAMPLE_COUNT_2_BIT,  VK_SAMPLE_COUNT_4_BIT, VK_SAMPLE_COUNT_8_BIT,
            VK_SAMPLE_COUNT_16_BIT, VK_SAMPLE_COUNT_32_BIT, VK_SAMPLE_COUNT_64_BIT};
        TestCaseGroupPtr alphaToOneTests(new tcu::TestCaseGroup(testCtx, "alpha_to_one"));

        for (int samplesNdx = 0; samplesNdx < DE_LENGTH_OF_ARRAY(samplesForAlphaToOne); samplesNdx++)
        {
            std::ostringstream caseName;
            caseName << "samples_" << samplesForAlphaToOne[samplesNdx];

            alphaToOneTests->addChild(new AlphaToOneTest(testCtx, caseName.str(), pipelineConstructionType,
                                                         samplesForAlphaToOne[samplesNdx], IMAGE_BACKING_MODE_REGULAR,
                                                         useFragmentShadingRate));
#ifndef CTS_USES_VULKANSC
            caseName << "_sparse";
            alphaToOneTests->addChild(new AlphaToOneTest(testCtx, caseName.str(), pipelineConstructionType,
                                                         samplesForAlphaToOne[samplesNdx], IMAGE_BACKING_MODE_SPARSE,
                                                         useFragmentShadingRate));
#endif // CTS_USES_VULKANSC
        }

        multisampleTests->addChild(alphaToOneTests.release());
    }

    // AlphaToCoverageEnable tests
    {
        TestCaseGroupPtr alphaToCoverageTests(new tcu::TestCaseGroup(testCtx, "alpha_to_coverage"));

        for (int samplesNdx = 0; samplesNdx < DE_LENGTH_OF_ARRAY(samples); samplesNdx++)
        {
            std::ostringstream caseName;
            caseName << "samples_" << samples[samplesNdx];

            TestCaseGroupPtr samplesTests(new tcu::TestCaseGroup(testCtx, caseName.str().c_str()));

            samplesTests->addChild(new AlphaToCoverageTest(testCtx, "alpha_opaque", pipelineConstructionType,
                                                           samples[samplesNdx], GEOMETRY_TYPE_OPAQUE_QUAD,
                                                           IMAGE_BACKING_MODE_REGULAR, useFragmentShadingRate, false));
            samplesTests->addChild(new AlphaToCoverageTest(testCtx, "alpha_translucent", pipelineConstructionType,
                                                           samples[samplesNdx], GEOMETRY_TYPE_TRANSLUCENT_QUAD,
                                                           IMAGE_BACKING_MODE_REGULAR, useFragmentShadingRate, false));
            samplesTests->addChild(new AlphaToCoverageTest(testCtx, "alpha_invisible", pipelineConstructionType,
                                                           samples[samplesNdx], GEOMETRY_TYPE_INVISIBLE_QUAD,
                                                           IMAGE_BACKING_MODE_REGULAR, useFragmentShadingRate, false));
            samplesTests->addChild(new AlphaToCoverageTest(
                testCtx, "alpha_invisible_check_depth", pipelineConstructionType, samples[samplesNdx],
                GEOMETRY_TYPE_INVISIBLE_QUAD, IMAGE_BACKING_MODE_REGULAR, useFragmentShadingRate, true));
#ifndef CTS_USES_VULKANSC
            samplesTests->addChild(new AlphaToCoverageTest(testCtx, "alpha_opaque_sparse", pipelineConstructionType,
                                                           samples[samplesNdx], GEOMETRY_TYPE_OPAQUE_QUAD,
                                                           IMAGE_BACKING_MODE_SPARSE, useFragmentShadingRate, false));
            samplesTests->addChild(new AlphaToCoverageTest(
                testCtx, "alpha_translucent_sparse", pipelineConstructionType, samples[samplesNdx],
                GEOMETRY_TYPE_TRANSLUCENT_QUAD, IMAGE_BACKING_MODE_SPARSE, useFragmentShadingRate, false));
            samplesTests->addChild(new AlphaToCoverageTest(testCtx, "alpha_invisible_sparse", pipelineConstructionType,
                                                           samples[samplesNdx], GEOMETRY_TYPE_INVISIBLE_QUAD,
                                                           IMAGE_BACKING_MODE_SPARSE, useFragmentShadingRate, false));
            samplesTests->addChild(new AlphaToCoverageTest(
                testCtx, "alpha_invisible_sparse_check_depth", pipelineConstructionType, samples[samplesNdx],
                GEOMETRY_TYPE_INVISIBLE_QUAD, IMAGE_BACKING_MODE_SPARSE, useFragmentShadingRate, true));
#endif // CTS_USES_VULKANSC

            alphaToCoverageTests->addChild(samplesTests.release());
        }
        multisampleTests->addChild(alphaToCoverageTests.release());
    }

    // AlphaToCoverageEnable without color buffer tests
    {
        TestCaseGroupPtr alphaToCoverageNoColorAttachmentTests(
            new tcu::TestCaseGroup(testCtx, "alpha_to_coverage_no_color_attachment"));

        for (int samplesNdx = 0; samplesNdx < DE_LENGTH_OF_ARRAY(samples); samplesNdx++)
        {
            std::ostringstream caseName;
            caseName << "samples_" << samples[samplesNdx];

            TestCaseGroupPtr samplesTests(new tcu::TestCaseGroup(testCtx, caseName.str().c_str()));

            samplesTests->addChild(new AlphaToCoverageNoColorAttachmentTest(
                testCtx, "alpha_opaque", pipelineConstructionType, samples[samplesNdx], GEOMETRY_TYPE_OPAQUE_QUAD,
                IMAGE_BACKING_MODE_REGULAR, useFragmentShadingRate));
#ifndef CTS_USES_VULKANSC
            samplesTests->addChild(new AlphaToCoverageNoColorAttachmentTest(
                testCtx, "alpha_opaque_sparse", pipelineConstructionType, samples[samplesNdx],
                GEOMETRY_TYPE_OPAQUE_QUAD, IMAGE_BACKING_MODE_SPARSE, useFragmentShadingRate));
#endif // CTS_USES_VULKANSC

            alphaToCoverageNoColorAttachmentTests->addChild(samplesTests.release());
        }
        multisampleTests->addChild(alphaToCoverageNoColorAttachmentTests.release());
    }

    // AlphaToCoverageEnable with unused color attachment:
    // Set color output at location 0 as unused, but use the alpha write to control coverage for rendering to color buffer at location 1.
    {
        TestCaseGroupPtr alphaToCoverageColorUnusedAttachmentTests(
            new tcu::TestCaseGroup(testCtx, "alpha_to_coverage_unused_attachment"));

        for (int samplesNdx = 0; samplesNdx < DE_LENGTH_OF_ARRAY(samples); samplesNdx++)
        {
            std::ostringstream caseName;
            caseName << "samples_" << samples[samplesNdx];

            TestCaseGroupPtr samplesTests(new tcu::TestCaseGroup(testCtx, caseName.str().c_str()));

            samplesTests->addChild(new AlphaToCoverageColorUnusedAttachmentTest(
                testCtx, "alpha_opaque", pipelineConstructionType, samples[samplesNdx], GEOMETRY_TYPE_OPAQUE_QUAD,
                IMAGE_BACKING_MODE_REGULAR, useFragmentShadingRate));
#ifndef CTS_USES_VULKANSC
            samplesTests->addChild(new AlphaToCoverageColorUnusedAttachmentTest(
                testCtx, "alpha_opaque_sparse", pipelineConstructionType, samples[samplesNdx],
                GEOMETRY_TYPE_OPAQUE_QUAD, IMAGE_BACKING_MODE_SPARSE, useFragmentShadingRate));
#endif // CTS_USES_VULKANSC
            samplesTests->addChild(new AlphaToCoverageColorUnusedAttachmentTest(
                testCtx, "alpha_invisible", pipelineConstructionType, samples[samplesNdx], GEOMETRY_TYPE_INVISIBLE_QUAD,
                IMAGE_BACKING_MODE_REGULAR, useFragmentShadingRate));
#ifndef CTS_USES_VULKANSC
            samplesTests->addChild(new AlphaToCoverageColorUnusedAttachmentTest(
                testCtx, "alpha_invisible_sparse", pipelineConstructionType, samples[samplesNdx],
                GEOMETRY_TYPE_INVISIBLE_QUAD, IMAGE_BACKING_MODE_SPARSE, useFragmentShadingRate));
#endif // CTS_USES_VULKANSC

            alphaToCoverageColorUnusedAttachmentTests->addChild(samplesTests.release());
        }
        multisampleTests->addChild(alphaToCoverageColorUnusedAttachmentTests.release());
    }

#ifndef CTS_USES_VULKANSC
    if (!useFragmentShadingRate)
    {
        TestCaseGroupPtr sampleRateA2CGroup(new tcu::TestCaseGroup(testCtx, "sample_rate_a2c"));
        for (const bool dynamicA2C : {false, true})
        {
            const auto testName = (dynamicA2C ? "dynamic_a2c" : "static_a2c");
            const SampleRateAlphaToCoverageParams params{
                pipelineConstructionType,
                dynamicA2C,
            };
            sampleRateA2CGroup->addChild(new SampleRateAlphaToCoverageCase(testCtx, testName, params));
        }
        multisampleTests->addChild(sampleRateA2CGroup.release());
    }
#endif // CTS_USES_VULKANSC

#ifndef CTS_USES_VULKANSC
    // not all tests need to be repeated for FSR
    if (useFragmentShadingRate == false)
    {
        // Sampling from a multisampled image texture (texelFetch)
        multisampleTests->addChild(createMultisampleSampledImageTests(testCtx, pipelineConstructionType));

        // Load/store on a multisampled rendered image (different kinds of access: color attachment write, storage image, etc.)
        multisampleTests->addChild(createMultisampleStorageImageTests(testCtx, pipelineConstructionType));

        // Sampling from a multisampled image texture (texelFetch), checking supersample positions
        multisampleTests->addChild(createMultisampleStandardSamplePositionTests(testCtx, pipelineConstructionType));

        // Sampling from a multisampled image texture (texelFetch), checking if samples are mapped correctly
        if (pipelineConstructionType == vk::PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC ||
            pipelineConstructionType == vk::PIPELINE_CONSTRUCTION_TYPE_FAST_LINKED_LIBRARY ||
            pipelineConstructionType == vk::PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_UNLINKED_SPIRV)
        {
            multisampleTests->addChild(createMultisampleSamplesMappingOrderTests(testCtx, pipelineConstructionType));
        }

        // VK_AMD_shader_fragment_mask
        multisampleTests->addChild(createMultisampleShaderFragmentMaskTests(testCtx, pipelineConstructionType));

        // Multisample resolve tests where a render area is less than an attachment size.
        multisampleTests->addChild(
            createMultisampleResolveRenderpassRenderAreaTests(testCtx, pipelineConstructionType));

        // VK_EXT_multisampled_render_to_single_sampled
        {
            multisampleTests->addChild(createMultisampledRenderToSingleSampledTests(testCtx, pipelineConstructionType));
            // Take advantage of the code for this extension's tests to add some normal multisampling tests
            multisampleTests->addChild(createMultisampledMiscTests(testCtx, pipelineConstructionType));
        }
    }

    // VK_EXT_sample_locations
    multisampleTests->addChild(
        createMultisampleSampleLocationsTests(testCtx, pipelineConstructionType, useFragmentShadingRate, false));

    if (pipelineConstructionType == vk::PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC ||
        pipelineConstructionType == vk::PIPELINE_CONSTRUCTION_TYPE_FAST_LINKED_LIBRARY ||
        pipelineConstructionType == vk::PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_UNLINKED_SPIRV)
    {
        multisampleTests->addChild(
            createMultisampleSampleLocationsTests(testCtx, pipelineConstructionType, useFragmentShadingRate, true));
    }

    // VK_AMD_mixed_attachment
    multisampleTests->addChild(
        createMultisampleMixedAttachmentSamplesTests(testCtx, pipelineConstructionType, useFragmentShadingRate));

    // Sample mask with and without vk_ext_post_depth_coverage
    {
        const vk::VkSampleCountFlagBits standardSamplesSet[] = {vk::VK_SAMPLE_COUNT_2_BIT, vk::VK_SAMPLE_COUNT_4_BIT,
                                                                vk::VK_SAMPLE_COUNT_8_BIT, vk::VK_SAMPLE_COUNT_16_BIT};

        TestCaseGroupPtr sampleMaskWithDepthTestGroup(new tcu::TestCaseGroup(testCtx, "sample_mask_with_depth_test"));

        for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(standardSamplesSet); ++ndx)
        {
            std::ostringstream caseName;
            caseName << "samples_" << standardSamplesSet[ndx];

            sampleMaskWithDepthTestGroup->addChild(
                new SampleMaskWithDepthTestTest(testCtx, caseName.str(), pipelineConstructionType,
                                                standardSamplesSet[ndx], false, useFragmentShadingRate));

            caseName << "_post_depth_coverage";
            sampleMaskWithDepthTestGroup->addChild(
                new SampleMaskWithDepthTestTest(testCtx, caseName.str(), pipelineConstructionType,
                                                standardSamplesSet[ndx], true, useFragmentShadingRate));
        }
        multisampleTests->addChild(sampleMaskWithDepthTestGroup.release());
    }

    if ((pipelineConstructionType == vk::PIPELINE_CONSTRUCTION_TYPE_MONOLITHIC ||
         pipelineConstructionType == vk::PIPELINE_CONSTRUCTION_TYPE_FAST_LINKED_LIBRARY ||
         pipelineConstructionType == vk::PIPELINE_CONSTRUCTION_TYPE_SHADER_OBJECT_UNLINKED_SPIRV) &&
        !useFragmentShadingRate)
    {
        multisampleTests->addChild(createMultisampleResolveMaint10Tests(testCtx, pipelineConstructionType));
    }

#endif // CTS_USES_VULKANSC

    // Input attachments are not supported with dynamic rendering and shader objects
    if (!isConstructionTypeShaderObject(pipelineConstructionType))
    {
        //Conservative rasterization test
        struct TestConfig
        {
            const char *name;
            bool enableMinSampleShading;
            const float minSampleShading;
            const bool enableSampleMask;
            VkSampleMask sampleMask;
            bool enablePostDepthCoverage;
        };

        const TestConfig testConfigs[] = {
            // Only conservative rendering applied
            {"plain_conservative", false, 0.0f, false, 0x0, false},
            // Post depth coverage enabled
            {"post_depth_coverage", false, 0.0f, false, 0x0, true},
            // minSampleMask set to 0.25f
            {"min_0_25", true, 0.25f, false, 0x0, false},
            // minSampleMask set to 0.5f
            {"min_0_5", true, 0.5f, false, 0x0, false},
            // minSampleMask set to 0.75f
            {"min_0_75", true, 0.75f, false, 0x0, false},
            // minSampleMask set to 1.0f
            {"min_0_1_0", true, 1.0f, false, 0x0, false},
            // All mask bits are on
            {"mask_all_off", false, 0.0f, true, 0x0, false},
            // All mask bits are off
            {"mask_all_on", false, 0.0f, true, 0xFFFFFFFF, false},
            // All mask elements are 0xAAAAAAAA
            {"mask_half_on", false, 0.0f, true, 0xAAAAAAAA, false},
        };

        const vk::VkSampleCountFlagBits standardSamplesSet[] = {vk::VK_SAMPLE_COUNT_2_BIT, vk::VK_SAMPLE_COUNT_4_BIT,
                                                                vk::VK_SAMPLE_COUNT_8_BIT, vk::VK_SAMPLE_COUNT_16_BIT};

        enum vk::VkConservativeRasterizationModeEXT rasterizationMode[] = {
            vk::VK_CONSERVATIVE_RASTERIZATION_MODE_OVERESTIMATE_EXT,
            vk::VK_CONSERVATIVE_RASTERIZATION_MODE_UNDERESTIMATE_EXT};

        // Conservative rendering
        TestCaseGroupPtr conservativeGroup(new tcu::TestCaseGroup(testCtx, "conservative_with_full_coverage"));

        for (int modeNdx = 0; modeNdx < DE_LENGTH_OF_ARRAY(rasterizationMode); ++modeNdx)
        {
            const char *modeName = (modeNdx == 0 ? "overestimate" : "underestimate");
            TestCaseGroupPtr modesGroup(new tcu::TestCaseGroup(testCtx, modeName));

            for (int samplesNdx = 0; samplesNdx < DE_LENGTH_OF_ARRAY(standardSamplesSet); ++samplesNdx)
            {
                std::string caseName = "samples_" + std::to_string(standardSamplesSet[samplesNdx]) + "_";

                for (int configNdx = 0; configNdx < DE_LENGTH_OF_ARRAY(testConfigs); configNdx++)
                {
                    const TestConfig &testConfig = testConfigs[configNdx];

                    modesGroup->addChild(new SampleMaskWithConservativeTest(
                        testCtx, caseName + testConfig.name, pipelineConstructionType, standardSamplesSet[samplesNdx],
                        rasterizationMode[modeNdx], testConfig.enableMinSampleShading, testConfig.minSampleShading,
                        testConfig.enableSampleMask, testConfig.sampleMask, testConfig.enablePostDepthCoverage,
                        useFragmentShadingRate));
                }
            }

            conservativeGroup->addChild(modesGroup.release());
        }

        multisampleTests->addChild(conservativeGroup.release());

        TestCaseGroupPtr compatibleRenderPassGroup(new tcu::TestCaseGroup(testCtx, "compatible_render_pass"));
        compatibleRenderPassGroup->addChild(
            new CompatibleRenderPassTest(testCtx, "static", pipelineConstructionType, false));
        compatibleRenderPassGroup->addChild(
            new CompatibleRenderPassTest(testCtx, "dynamic", pipelineConstructionType, true));
        multisampleTests->addChild(compatibleRenderPassGroup.release());
    }

    {
        static const std::vector<vk::VkSampleCountFlagBits> kSampleCounts = {
            vk::VK_SAMPLE_COUNT_1_BIT,  vk::VK_SAMPLE_COUNT_2_BIT,  vk::VK_SAMPLE_COUNT_4_BIT,
            vk::VK_SAMPLE_COUNT_8_BIT,  vk::VK_SAMPLE_COUNT_16_BIT, vk::VK_SAMPLE_COUNT_32_BIT,
            vk::VK_SAMPLE_COUNT_64_BIT,
        };

        static const std::array<bool, 2> unusedAttachmentFlag = {{false, true}};

        {
            // Tests for multisample variable rate in subpasses
            TestCaseGroupPtr variableRateGroup(new tcu::TestCaseGroup(testCtx, "variable_rate"));

            // 2 and 3 subpasses should be good enough.
            static const std::vector<size_t> combinationSizes = {2, 3};

            // Basic cases.
            for (const auto size : combinationSizes)
            {
                const auto combs = combinations(kSampleCounts, size);
                for (const auto &comb : combs)
                {
                    // Check sample counts actually vary between some of the subpasses.
                    std::set<vk::VkSampleCountFlagBits> uniqueVals(begin(comb), end(comb));
                    if (uniqueVals.size() < 2)
                        continue;

                    std::ostringstream name;

                    bool first = true;
                    for (const auto &count : comb)
                    {
                        name << (first ? "" : "_") << count;
                        first = false;
                    }

                    const VariableRateTestCase::TestParams params = {
                        pipelineConstructionType,  // PipelineConstructionType pipelineConstructionType;
                        false,                     // bool nonEmptyFramebuffer;
                        vk::VK_SAMPLE_COUNT_1_BIT, // vk::VkSampleCountFlagBits fbCount;
                        false,                     // bool unusedAttachment;
                        comb,                      // SampleCounts subpassCounts;
                        useFragmentShadingRate,    // bool useFragmentShadingRate;
                    };
                    variableRateGroup->addChild(new VariableRateTestCase(testCtx, name.str(), params));
                }
            }

            // Cases with non-empty framebuffers: only 2 subpasses to avoid a large number of combinations.
            {
                // Use one more sample count for the framebuffer attachment. It will be taken from the last item.
                auto combs = combinations(kSampleCounts, 2 + 1);
                for (auto &comb : combs)
                {
                    // Framebuffer sample count.
                    const auto fbCount = comb.back();
                    comb.pop_back();

                    // Check sample counts actually vary between some of the subpasses.
                    std::set<vk::VkSampleCountFlagBits> uniqueVals(begin(comb), end(comb));
                    if (uniqueVals.size() < 2)
                        continue;

                    for (const auto flag : unusedAttachmentFlag)
                    {
                        std::ostringstream name;

                        bool first = true;
                        for (const auto &count : comb)
                        {
                            name << (first ? "" : "_") << count;
                            first = false;
                        }

                        name << "_fb_" << fbCount;

                        if (flag)
                        {
                            name << "_unused";
                        }

                        const VariableRateTestCase::TestParams params = {
                            pipelineConstructionType, // PipelineConstructionType pipelineConstructionType;
                            true,                     // bool nonEmptyFramebuffer;
                            fbCount,                  // vk::VkSampleCountFlagBits fbCount;
                            flag,                     // bool unusedAttachment;
                            comb,                     // SampleCounts subpassCounts;
                            useFragmentShadingRate,   // bool useFragmentShadingRate;
                        };
                        variableRateGroup->addChild(new VariableRateTestCase(testCtx, name.str(), params));
                    }
                }
            }

            multisampleTests->addChild(variableRateGroup.release());
        }

        {
            // Tests for mixed sample count in empty subpass and framebuffer
            TestCaseGroupPtr mixedCountGroup(new tcu::TestCaseGroup(testCtx, "mixed_count"));

            const auto combs = combinations(kSampleCounts, 2);
            for (const auto &comb : combs)
            {
                // Check different sample count.
                DE_ASSERT(comb.size() == 2u);
                const auto &fbCount    = comb[0];
                const auto &emptyCount = comb[1];

                if (fbCount == emptyCount)
                    continue;

                const std::string fbCountStr    = de::toString(fbCount);
                const std::string emptyCountStr = de::toString(emptyCount);

                for (const auto flag : unusedAttachmentFlag)
                {
                    const std::string nameSuffix = (flag ? "unused" : "");
                    const std::string descSuffix =
                        (flag ? "one unused attachment reference" : "no attachment references");
                    const std::string name =
                        fbCountStr + "_" + emptyCountStr + (nameSuffix.empty() ? "" : "_") + nameSuffix;

                    const VariableRateTestCase::TestParams params{
                        pipelineConstructionType, // PipelineConstructionType pipelineConstructionType;
                        true,                     // bool nonEmptyFramebuffer;
                        fbCount,                  // vk::VkSampleCountFlagBits fbCount;
                        flag,                     // bool unusedAttachment;
                        VariableRateTestCase::SampleCounts(1u, emptyCount), // SampleCounts subpassCounts;
                        useFragmentShadingRate,                             // bool useFragmentShadingRate;
                    };
                    mixedCountGroup->addChild(new VariableRateTestCase(testCtx, name, params));
                }
            }

            multisampleTests->addChild(mixedCountGroup.release());
        }

        if (!useFragmentShadingRate)
        {
            // Tests using alpha to coverage combined with depth/stencil/mask writes in the frag shader
            TestCaseGroupPtr zExportGroup(new tcu::TestCaseGroup(testCtx, "z_export"));

            const struct
            {
                ZExportFlags flags;
                const char *name;
            } flagsCases[] = {
                {(ZEXP_DEPTH_BIT), "depth"},
                {(ZEXP_STENCIL_BIT), "stencil"},
                {(ZEXP_SAMPLE_MASK_SHADER_BIT), "sample_mask"},
                {(ZEXP_SAMPLE_MASK_PIPELINE_BIT), "sample_mask_pipeline"},
                {(ZEXP_DEPTH_BIT | ZEXP_STENCIL_BIT), "depth_stencil"},
                {(ZEXP_DEPTH_BIT | ZEXP_STENCIL_BIT | ZEXP_SAMPLE_MASK_SHADER_BIT), "write_all"},
            };

            for (const auto &flagsCase : flagsCases)
            {
                for (const bool dynamicAlphaToCoverage : {false, true})
                    for (const bool dynamicRendering : {false, true})
                    {
#ifdef CTS_USES_VULKANSC
                        if (dynamicAlphaToCoverage || dynamicRendering)
                            continue;
#endif // CTS_USES_VULKANSC
                        if (dynamicRendering && !isConstructionTypeLibrary(pipelineConstructionType))
                            continue;

                        const auto testName = std::string(flagsCase.name) + "_" +
                                              (dynamicAlphaToCoverage ? "dynamic" : "static") +
                                              "_atc" // atc = alpha to coverage
                                              + (dynamicRendering ? "_dynamic_rendering" : "");
                        const ZExportParams params(pipelineConstructionType, flagsCase.flags, dynamicAlphaToCoverage,
                                                   dynamicRendering);

                        addFunctionCaseWithPrograms(zExportGroup.get(), testName, ZExportCheckSupport,
                                                    ZExportInitPrograms, ZExportIterate, params);
                    }
            }

            multisampleTests->addChild(zExportGroup.release());
        }
    }

    if (!useFragmentShadingRate)
    {
        TestCaseGroupPtr a2cWa2oneGrp(new tcu::TestCaseGroup(testCtx, "a2c_with_a2one"));
        for (const bool dynamicA2C : {false, true})
            for (const bool dynamicA2One : {false, true})
                for (const bool exportFragDepth : {false, true})
                    for (const bool sampleRateShadingEnable : {false, true})
                    {
#ifdef CTS_USES_VULKANSC
                        if (dynamicA2C || dynamicA2One)
                            continue;
#endif
                        const A2CplusA2OneParams params{
                            pipelineConstructionType, dynamicA2C, dynamicA2One, exportFragDepth,
                            sampleRateShadingEnable,
                        };

                        std::string testName;
                        if (dynamicA2C)
                            testName += "dynamic_a2c";
                        if (dynamicA2One)
                            testName += (testName.empty() ? "" : "_") + std::string("dynamic_a2one");
                        if (testName.empty())
                            testName = "static";

                        if (params.exportFragDepth)
                            testName += "_export_frag_depth";

                        if (params.sampleShadingEnable)
                            testName += "_with_sample_rate_shading";

                        addFunctionCaseWithPrograms(a2cWa2oneGrp.get(), testName, A2CplusA2OneSupport,
                                                    A2CplusA2OnePrograms, A2CplusA2OneRun, params);
                    }

        multisampleTests->addChild(a2cWa2oneGrp.release());
    }

    return multisampleTests.release();
}

} // namespace pipeline
} // namespace vkt
