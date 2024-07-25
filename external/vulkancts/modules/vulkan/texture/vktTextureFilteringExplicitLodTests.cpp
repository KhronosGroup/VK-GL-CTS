/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 Google Inc.
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
 * \brief Texture filtering tests with explicit LOD instructions
 *//*--------------------------------------------------------------------*/

#include "vktTextureFilteringExplicitLodTests.hpp"

#include "vkDefs.hpp"

#include "vktSampleVerifier.hpp"
#include "vktShaderExecutor.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTextureTestUtil.hpp"

#include "vkDeviceUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkPlatform.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkStrUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkCmdUtil.hpp"

#include "tcuTexLookupVerifier.hpp"
#include "tcuTestLog.hpp"
#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuVector.hpp"

#include "deClock.h"
#include "deMath.h"
#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"
#include "deSharedPtr.hpp"

#include <sstream>
#include <string>
#include <vector>

namespace vkt
{
namespace texture
{

using namespace tcu;
using namespace vk;
using std::string;

namespace
{

std::vector<de::SharedPtr<tcu::FloatFormat>> getPrecision(VkFormat format, int fpPrecisionDelta)
{
    std::vector<de::SharedPtr<tcu::FloatFormat>> floatFormats;
    de::SharedPtr<tcu::FloatFormat> fp16(
        new tcu::FloatFormat(-14, 15, std::max(0, 10 + fpPrecisionDelta), false, tcu::YES));
    de::SharedPtr<tcu::FloatFormat> fp32(new tcu::FloatFormat(-126, 127, std::max(0, 23 + fpPrecisionDelta), true));
    const tcu::TextureFormat tcuFormat          = mapVkFormat(format);
    const tcu::TextureChannelClass channelClass = tcu::getTextureChannelClass(tcuFormat.type);
    const tcu::IVec4 channelDepth               = tcu::getTextureFormatBitDepth(tcuFormat);

    for (int channelIdx = 0; channelIdx < 4; channelIdx++)
    {
        switch (channelClass)
        {
        case TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
            floatFormats.push_back(de::SharedPtr<tcu::FloatFormat>(
                new tcu::NormalizedFormat(std::max(0, channelDepth[channelIdx] + fpPrecisionDelta - 1))));
            break;

        case TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
            floatFormats.push_back(de::SharedPtr<tcu::FloatFormat>(
                new tcu::NormalizedFormat(std::max(0, channelDepth[channelIdx] + fpPrecisionDelta))));
            break;

        case TEXTURECHANNELCLASS_FLOATING_POINT:
            if (channelDepth[channelIdx] == 16)
            {
                floatFormats.push_back(fp16);
            }
            else
            {
                DE_ASSERT(channelDepth[channelIdx] == 32 || channelDepth[channelIdx] == 0);
                floatFormats.push_back(fp32);
            }
            break;

        default:
            DE_FATAL("Unexpected channel class.");
            break;
        }
    }

    return floatFormats;
}

using namespace shaderexecutor;

string genSamplerDeclaration(const ImageViewParameters &imParams, const SamplerParameters &samplerParams)
{
    string result = "sampler";

    switch (imParams.dim)
    {
    case IMG_DIM_1D:
        result += "1D";
        break;

    case IMG_DIM_2D:
        result += "2D";
        break;

    case IMG_DIM_3D:
        result += "3D";
        break;

    case IMG_DIM_CUBE:
        result += "Cube";
        break;

    default:
        break;
    }

    if (imParams.isArrayed)
    {
        result += "Array";
    }

    if (samplerParams.isCompare)
    {
        result += "Shadow";
    }

    return result;
}

string genLookupCode(const ImageViewParameters &imParams, const SamplerParameters &samplerParams,
                     const SampleLookupSettings &lookupSettings)
{
    int dim = -1;

    switch (imParams.dim)
    {
    case IMG_DIM_1D:
        dim = 1;
        break;

    case IMG_DIM_2D:
        dim = 2;
        break;

    case IMG_DIM_3D:
        dim = 3;
        break;

    case IMG_DIM_CUBE:
        dim = 3;
        break;

    default:
        dim = 0;
        break;
    }

    DE_ASSERT(dim >= 1 && dim <= 3);

    int numCoordComp = dim;

    if (lookupSettings.isProjective)
    {
        ++numCoordComp;
    }

    int numArgComp          = numCoordComp;
    bool hasSeparateCompare = false;

    if (imParams.isArrayed)
    {
        DE_ASSERT(!lookupSettings.isProjective && "Can't do a projective lookup on an arrayed image!");

        ++numArgComp;
    }

    if (samplerParams.isCompare && numCoordComp == 4)
    {
        hasSeparateCompare = true;
    }
    else if (samplerParams.isCompare)
    {
        ++numArgComp;
    }

    // Build coordinate input to texture*() function

    string arg = "vec";
    arg += (char)(numArgComp + '0');
    arg += "(vec";
    arg += (char)(numCoordComp + '0');
    arg += "(coord)";

    int numZero = numArgComp - numCoordComp;

    if (imParams.isArrayed)
    {
        arg += ", layer";
        --numZero;
    }

    if (samplerParams.isCompare && !hasSeparateCompare)
    {
        arg += ", dRef";
        --numZero;
    }

    for (int ndx = 0; ndx < numZero; ++ndx)
    {
        arg += ", 0.0";
    }

    arg += ")";

    // Build call to texture*() function

    string code;

    code += "result = texture";

    if (lookupSettings.isProjective)
    {
        code += "Proj";
    }

    if (lookupSettings.lookupLodMode == LOOKUP_LOD_MODE_DERIVATIVES)
    {
        code += "Grad";
    }
    else if (lookupSettings.lookupLodMode == LOOKUP_LOD_MODE_LOD)
    {
        code += "Lod";
    }

    code += "(testSampler, ";
    code += arg;

    if (samplerParams.isCompare && hasSeparateCompare)
    {
        code += ", dRef";
    }

    if (lookupSettings.lookupLodMode == LOOKUP_LOD_MODE_DERIVATIVES)
    {
        code += ", vec";
        code += (char)(numCoordComp + '0');
        code += "(dPdx), ";
        code += "vec";
        code += (char)(numCoordComp + '0');
        code += "(dPdy)";
    }
    else if (lookupSettings.lookupLodMode == LOOKUP_LOD_MODE_LOD)
    {
        code += ", lod";
    }

    code += ");";

    return code;
}

void initializeImage(Context &ctx, VkImage im, const ConstPixelBufferAccess *pba, ImageViewParameters imParams)
{
    const DeviceInterface &vkd = ctx.getDeviceInterface();
    const VkDevice dev         = ctx.getDevice();
    const uint32_t uqfi        = ctx.getUniversalQueueFamilyIndex();

    const VkDeviceSize bufSize = getPixelSize(mapVkFormat(imParams.format)) * imParams.arrayLayers * imParams.size[0] *
                                 imParams.size[1] * imParams.size[2] * 2;

    const VkBufferCreateInfo bufCreateInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // sType
        DE_NULL,                              // pNext
        0,                                    // flags
        bufSize,                              // size
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,     // usage
        VK_SHARING_MODE_EXCLUSIVE,            // sharingMode
        1,                                    // queueFamilyIndexCount
        &uqfi                                 // pQueueFamilyIndices
    };

    Unique<VkBuffer> buf(createBuffer(vkd, dev, &bufCreateInfo));

    VkMemoryRequirements bufMemReq;
    vkd.getBufferMemoryRequirements(dev, buf.get(), &bufMemReq);

    de::UniquePtr<Allocation> bufMem(ctx.getDefaultAllocator().allocate(bufMemReq, MemoryRequirement::HostVisible));
    VK_CHECK(vkd.bindBufferMemory(dev, buf.get(), bufMem->getMemory(), bufMem->getOffset()));

    std::vector<VkBufferImageCopy> copyRegions;

    uint8_t *const bufMapPtr = reinterpret_cast<uint8_t *>(bufMem->getHostPtr());
    uint8_t *bufCurPtr       = bufMapPtr;

    for (int level = 0; level < imParams.levels; ++level)
    {
        const IVec3 curLevelSize = pba[level].getSize();

        const std::size_t copySize = getPixelSize(mapVkFormat(imParams.format)) * curLevelSize[0] * curLevelSize[1] *
                                     curLevelSize[2] * imParams.arrayLayers;

        deMemcpy(bufCurPtr, pba[level].getDataPtr(), copySize);

        const VkImageSubresourceLayers curSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, (uint32_t)level, 0,
                                                         (uint32_t)imParams.arrayLayers};

        const VkBufferImageCopy curRegion = {
            (VkDeviceSize)(bufCurPtr - bufMapPtr),
            0,
            0,
            curSubresource,
            {0U, 0U, 0U},
            {(uint32_t)curLevelSize[0], (uint32_t)curLevelSize[1], (uint32_t)curLevelSize[2]}};

        copyRegions.push_back(curRegion);

        bufCurPtr += copySize;
    }

    flushAlloc(vkd, dev, *bufMem);

    copyBufferToImage(vkd, dev, ctx.getUniversalQueue(), ctx.getUniversalQueueFamilyIndex(), buf.get(), bufSize,
                      copyRegions, DE_NULL, VK_IMAGE_ASPECT_COLOR_BIT, imParams.levels, imParams.arrayLayers, im);
}

struct TestCaseData
{
    std::vector<ConstPixelBufferAccess> pba;
    ImageViewParameters imParams;
    SamplerParameters samplerParams;
    SampleLookupSettings sampleLookupSettings;
    glu::ShaderType shaderType;
};

VkSamplerCreateInfo mapSamplerCreateInfo(const SamplerParameters &samplerParams)
{
    VkSamplerCreateInfo samplerCreateInfo = {
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,             // sType
        DE_NULL,                                           // pNext
        0U,                                                // flags
        samplerParams.magFilter,                           // magFilter
        samplerParams.minFilter,                           // minFilter
        samplerParams.mipmapFilter,                        // mipmapMode
        samplerParams.wrappingModeU,                       // addressModeU
        samplerParams.wrappingModeV,                       // addressModeV
        samplerParams.wrappingModeW,                       // addressMoveW
        samplerParams.lodBias,                             // mipLodBias
        VK_FALSE,                                          // anisotropyEnable
        1.0f,                                              // maxAnisotropy
        VK_FALSE,                                          // compareEnable
        VK_COMPARE_OP_NEVER,                               // compareOp
        samplerParams.minLod,                              // minLod
        samplerParams.maxLod,                              // maxLod
        samplerParams.borderColor,                         // borderColor
        samplerParams.isUnnormalized ? VK_TRUE : VK_FALSE, // unnormalizedCoordinates
    };

    if (samplerParams.isCompare)
    {
        samplerCreateInfo.compareEnable = VK_TRUE;

        DE_FATAL("Not implemented");
    }

    return samplerCreateInfo;
}

VkImageType mapImageType(ImgDim dim)
{
    VkImageType imType;

    switch (dim)
    {
    case IMG_DIM_1D:
        imType = VK_IMAGE_TYPE_1D;
        break;

    case IMG_DIM_2D:
    case IMG_DIM_CUBE:
        imType = VK_IMAGE_TYPE_2D;
        break;

    case IMG_DIM_3D:
        imType = VK_IMAGE_TYPE_3D;
        break;

    default:
        imType = VK_IMAGE_TYPE_LAST;
        break;
    }

    return imType;
}

VkImageViewType mapImageViewType(const ImageViewParameters &imParams)
{
    VkImageViewType imViewType;

    if (imParams.isArrayed)
    {
        switch (imParams.dim)
        {
        case IMG_DIM_1D:
            imViewType = VK_IMAGE_VIEW_TYPE_1D_ARRAY;
            break;

        case IMG_DIM_2D:
            imViewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            break;

        case IMG_DIM_CUBE:
            imViewType = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
            break;

        default:
            imViewType = VK_IMAGE_VIEW_TYPE_LAST;
            break;
        }
    }
    else
    {
        switch (imParams.dim)
        {
        case IMG_DIM_1D:
            imViewType = VK_IMAGE_VIEW_TYPE_1D;
            break;

        case IMG_DIM_2D:
            imViewType = VK_IMAGE_VIEW_TYPE_2D;
            break;

        case IMG_DIM_3D:
            imViewType = VK_IMAGE_VIEW_TYPE_3D;
            break;

        case IMG_DIM_CUBE:
            imViewType = VK_IMAGE_VIEW_TYPE_CUBE;
            break;

        default:
            imViewType = VK_IMAGE_VIEW_TYPE_LAST;
            break;
        }
    }

    return imViewType;
}

class DataGenerator
{
public:
    virtual ~DataGenerator(void)
    {
    }

    virtual bool generate(void) = 0;

    virtual std::vector<ConstPixelBufferAccess> getPba(void) const = 0;
    virtual std::vector<SampleArguments> getSampleArgs(void) const = 0;

protected:
    DataGenerator(void)
    {
    }
};

class TextureFilteringTestInstance : public TestInstance
{
public:
    TextureFilteringTestInstance(Context &ctx, const TestCaseData &testCaseData, const ShaderSpec &shaderSpec,
                                 de::MovePtr<DataGenerator> gen);

    virtual TestStatus iterate(void)
    {
        return runTest();
    }

protected:
    TestStatus runTest(void);
    bool isSupported(void);
    void createResources(void);
    void execute(void);
    TestStatus verify(void);

    tcu::Sampler mapTcuSampler(void) const;

    const glu::ShaderType m_shaderType;
    const ShaderSpec m_shaderSpec;
    const ImageViewParameters m_imParams;
    const SamplerParameters m_samplerParams;
    const SampleLookupSettings m_sampleLookupSettings;

    std::vector<SampleArguments> m_sampleArguments;
    uint32_t m_numSamples;

    de::MovePtr<Allocation> m_imAllocation;
    Move<VkImage> m_im;
    Move<VkImageView> m_imView;
    Move<VkSampler> m_sampler;

    Move<VkDescriptorSetLayout> m_extraResourcesLayout;
    Move<VkDescriptorPool> m_extraResourcesPool;
    Move<VkDescriptorSet> m_extraResourcesSet;

    de::MovePtr<ShaderExecutor> m_executor;

    std::vector<ConstPixelBufferAccess> m_levels;
    de::MovePtr<DataGenerator> m_gen;

    std::vector<Vec4> m_resultSamples;
    std::vector<Vec4> m_resultCoords;
};

TextureFilteringTestInstance::TextureFilteringTestInstance(Context &ctx, const TestCaseData &testCaseData,
                                                           const ShaderSpec &shaderSpec, de::MovePtr<DataGenerator> gen)
    : TestInstance(ctx)
    , m_shaderType(testCaseData.shaderType)
    , m_shaderSpec(shaderSpec)
    , m_imParams(testCaseData.imParams)
    , m_samplerParams(testCaseData.samplerParams)
    , m_sampleLookupSettings(testCaseData.sampleLookupSettings)
    , m_numSamples(0)
    , m_levels(testCaseData.pba)
    , m_gen(gen.release())
{
    for (uint8_t compNdx = 0; compNdx < 3; ++compNdx)
        DE_ASSERT(m_imParams.size[compNdx] > 0);
}

TestStatus TextureFilteringTestInstance::runTest(void)
{
    if (!isSupported())
        TCU_THROW(NotSupportedError, "Unsupported combination of filtering and image format");

    TCU_CHECK(m_gen->generate());
    m_levels = m_gen->getPba();

    m_sampleArguments = m_gen->getSampleArgs();
    m_numSamples      = (uint32_t)m_sampleArguments.size();

    createResources();
    initializeImage(m_context, m_im.get(), &m_levels[0], m_imParams);

    uint64_t startTime, endTime;

    startTime = deGetMicroseconds();
    execute();
    endTime = deGetMicroseconds();

    m_context.getTestContext().getLog() << TestLog::Message << "Execution time: " << endTime - startTime << "us"
                                        << TestLog::EndMessage;

    startTime = deGetMicroseconds();

#ifdef CTS_USES_VULKANSC
    // skip costly verification in main process
    if (!m_context.getTestContext().getCommandLine().isSubProcess())
        return TestStatus::pass("Success");
#endif // CTS_USES_VULKANSC

    TestStatus result = verify();
    endTime           = deGetMicroseconds();

    m_context.getTestContext().getLog() << TestLog::Message << "Verification time: " << endTime - startTime << "us"
                                        << TestLog::EndMessage;

    return result;
}

TestStatus TextureFilteringTestInstance::verify(void)
{
    // \todo [2016-06-24 collinbaker] Handle cubemaps

    const int coordBits                = (int)m_context.getDeviceProperties().limits.subTexelPrecisionBits;
    const int mipmapBits               = (int)m_context.getDeviceProperties().limits.mipmapPrecisionBits;
    const int maxPrintedFailures       = 5;
    int failCount                      = 0;
    int warningCount                   = 0;
    const tcu::TextureFormat tcuFormat = mapVkFormat(m_imParams.format);
    std::vector<de::SharedPtr<tcu::FloatFormat>> strictPrecision  = getPrecision(m_imParams.format, 0);
    std::vector<de::SharedPtr<tcu::FloatFormat>> relaxedPrecision = tcuFormat.type == tcu::TextureFormat::HALF_FLOAT ?
                                                                        getPrecision(m_imParams.format, -6) :
                                                                        getPrecision(m_imParams.format, -2);
    const bool allowRelaxedPrecision =
        (tcuFormat.type == tcu::TextureFormat::HALF_FLOAT || tcuFormat.type == tcu::TextureFormat::SNORM_INT8) &&
        (m_samplerParams.minFilter == VK_FILTER_LINEAR || m_samplerParams.magFilter == VK_FILTER_LINEAR);

    const SampleVerifier verifier(m_imParams, m_samplerParams, m_sampleLookupSettings, coordBits, mipmapBits,
                                  strictPrecision, strictPrecision, m_levels);

    const SampleVerifier relaxedVerifier(m_imParams, m_samplerParams, m_sampleLookupSettings, coordBits, mipmapBits,
                                         strictPrecision, relaxedPrecision, m_levels);

    for (uint32_t sampleNdx = 0; sampleNdx < m_numSamples; ++sampleNdx)
    {
        bool compareOK = verifier.verifySample(m_sampleArguments[sampleNdx], m_resultSamples[sampleNdx]);
        if (compareOK)
            continue;
        if (allowRelaxedPrecision)
        {
            m_context.getTestContext().getLog()
                << tcu::TestLog::Message
                << "Warning: Strict validation failed, re-trying with lower precision for SNORM8 format or half float"
                << tcu::TestLog::EndMessage;

            compareOK = relaxedVerifier.verifySample(m_sampleArguments[sampleNdx], m_resultSamples[sampleNdx]);
            if (compareOK)
            {
                warningCount++;
                continue;
            }
        }
        if (failCount++ < maxPrintedFailures)
        {
            // Re-run with report logging
            std::string report;
            verifier.verifySampleReport(m_sampleArguments[sampleNdx], m_resultSamples[sampleNdx], report);

            m_context.getTestContext().getLog() << TestLog::Section("Failed sample", "Failed sample")
                                                << TestLog::Message << "Sample " << sampleNdx << ".\n"
                                                << "\tCoordinate: " << m_sampleArguments[sampleNdx].coord << "\n"
                                                << "\tLOD: " << m_sampleArguments[sampleNdx].lod << "\n"
                                                << "\tGPU Result: " << m_resultSamples[sampleNdx] << "\n\n"
                                                << "Failure report:\n"
                                                << report << "\n"
                                                << TestLog::EndMessage << TestLog::EndSection;
        }
    }

    m_context.getTestContext().getLog() << TestLog::Message << "Passed " << m_numSamples - failCount << " out of "
                                        << m_numSamples << "." << TestLog::EndMessage;

    if (failCount > 0)
        return TestStatus::fail("Verification failed");
    else if (warningCount > 0)
        return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "Inaccurate filtering results");

    return TestStatus::pass("Success");
}

void TextureFilteringTestInstance::execute(void)
{
    std::vector<float> coords, layers, dRefs, dPdxs, dPdys, lods;

    for (uint32_t ndx = 0; ndx < m_numSamples; ++ndx)
    {
        const SampleArguments &sampleArgs = m_sampleArguments[ndx];

        for (uint8_t compNdx = 0; compNdx < 4; ++compNdx)
        {
            coords.push_back(sampleArgs.coord[compNdx]);
            dPdxs.push_back(sampleArgs.dPdx[compNdx]);
            dPdys.push_back(sampleArgs.dPdy[compNdx]);
        }

        layers.push_back(sampleArgs.layer);
        dRefs.push_back(sampleArgs.dRef);
        lods.push_back(sampleArgs.lod);
    }

    const void *inputs[6] = {reinterpret_cast<const void *>(&coords[0]), reinterpret_cast<const void *>(&layers[0]),
                             reinterpret_cast<const void *>(&dRefs[0]),  reinterpret_cast<const void *>(&dPdxs[0]),
                             reinterpret_cast<const void *>(&dPdys[0]),  reinterpret_cast<const void *>(&lods[0])};

    // Staging buffers; data will be copied into vectors of Vec4
    // \todo [2016-06-24 collinbaker] Figure out if I actually need to
    // use staging buffers
    std::vector<float> resultSamplesTemp(m_numSamples * 4);
    std::vector<float> resultCoordsTemp(m_numSamples * 4);

    void *outputs[2] = {reinterpret_cast<void *>(&resultSamplesTemp[0]),
                        reinterpret_cast<void *>(&resultCoordsTemp[0])};

    m_executor->execute(m_numSamples, inputs, outputs, *m_extraResourcesSet);

    m_resultSamples.resize(m_numSamples);
    m_resultCoords.resize(m_numSamples);

    for (uint32_t ndx = 0; ndx < m_numSamples; ++ndx)
    {
        m_resultSamples[ndx] = Vec4(resultSamplesTemp[4 * ndx + 0], resultSamplesTemp[4 * ndx + 1],
                                    resultSamplesTemp[4 * ndx + 2], resultSamplesTemp[4 * ndx + 3]);

        m_resultCoords[ndx] = Vec4(resultCoordsTemp[4 * ndx + 0], resultCoordsTemp[4 * ndx + 1],
                                   resultCoordsTemp[4 * ndx + 2], resultCoordsTemp[4 * ndx + 3]);
    }
}

void TextureFilteringTestInstance::createResources(void)
{
    // Create VkImage

    const DeviceInterface &vkd = m_context.getDeviceInterface();
    const VkDevice device      = m_context.getDevice();

    const uint32_t queueFamily             = m_context.getUniversalQueueFamilyIndex();
    const VkImageCreateFlags imCreateFlags = (m_imParams.dim == IMG_DIM_CUBE) ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;

    const VkImageCreateInfo imCreateInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                            DE_NULL,
                                            imCreateFlags,
                                            mapImageType(m_imParams.dim),
                                            m_imParams.format,
                                            makeExtent3D(m_imParams.size[0], m_imParams.size[1], m_imParams.size[2]),
                                            (uint32_t)m_imParams.levels,
                                            (uint32_t)m_imParams.arrayLayers,
                                            VK_SAMPLE_COUNT_1_BIT,
                                            VK_IMAGE_TILING_OPTIMAL,
                                            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                            VK_SHARING_MODE_EXCLUSIVE,
                                            1,
                                            &queueFamily,
                                            VK_IMAGE_LAYOUT_UNDEFINED};

    m_im = createImage(vkd, device, &imCreateInfo);

    // Allocate memory for image

    VkMemoryRequirements imMemReq;
    vkd.getImageMemoryRequirements(device, m_im.get(), &imMemReq);

    m_imAllocation = m_context.getDefaultAllocator().allocate(imMemReq, MemoryRequirement::Any);
    VK_CHECK(vkd.bindImageMemory(device, m_im.get(), m_imAllocation->getMemory(), m_imAllocation->getOffset()));

    // Create VkImageView

    // \todo [2016-06-23 collinbaker] Pick aspectMask based on image type (i.e. support depth and/or stencil images)
    DE_ASSERT(m_imParams.dim != IMG_DIM_CUBE); // \todo Support cube maps
    const VkImageSubresourceRange imViewSubresourceRange = {
        VK_IMAGE_ASPECT_COLOR_BIT,       // aspectMask
        0,                               // baseMipLevel
        (uint32_t)m_imParams.levels,     // levelCount
        0,                               // baseArrayLayer
        (uint32_t)m_imParams.arrayLayers // layerCount
    };

    const VkComponentMapping imViewCompMap = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B,
                                              VK_COMPONENT_SWIZZLE_A};

    const VkImageViewCreateInfo imViewCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // sType
        DE_NULL,                                  // pNext
        0,                                        // flags
        m_im.get(),                               // image
        mapImageViewType(m_imParams),             // viewType
        m_imParams.format,                        // format
        imViewCompMap,                            // components
        imViewSubresourceRange                    // subresourceRange
    };

    m_imView = createImageView(vkd, device, &imViewCreateInfo);

    // Create VkSampler

    const VkSamplerCreateInfo samplerCreateInfo = mapSamplerCreateInfo(m_samplerParams);
    m_sampler                                   = createSampler(vkd, device, &samplerCreateInfo);

    // Create additional descriptors

    {
        const VkDescriptorSetLayoutBinding bindings[] = {
            {0u, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1u, VK_SHADER_STAGE_ALL, DE_NULL},
        };
        const VkDescriptorSetLayoutCreateInfo layoutInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            DE_NULL,
            (VkDescriptorSetLayoutCreateFlags)0u,
            DE_LENGTH_OF_ARRAY(bindings),
            bindings,
        };

        m_extraResourcesLayout = createDescriptorSetLayout(vkd, device, &layoutInfo);
    }

    {
        const VkDescriptorPoolSize poolSizes[] = {
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1u},
        };
        const VkDescriptorPoolCreateInfo poolInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            DE_NULL,
            (VkDescriptorPoolCreateFlags)VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
            1u, // maxSets
            DE_LENGTH_OF_ARRAY(poolSizes),
            poolSizes,
        };

        m_extraResourcesPool = createDescriptorPool(vkd, device, &poolInfo);
    }

    {
        const VkDescriptorSetAllocateInfo allocInfo = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            DE_NULL,
            *m_extraResourcesPool,
            1u,
            &m_extraResourcesLayout.get(),
        };

        m_extraResourcesSet = allocateDescriptorSet(vkd, device, &allocInfo);
    }

    {
        const VkDescriptorImageInfo imageInfo      = {*m_sampler, *m_imView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        const VkWriteDescriptorSet descriptorWrite = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            DE_NULL,
            *m_extraResourcesSet,
            0u, // dstBinding
            0u, // dstArrayElement
            1u,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            &imageInfo,
            (const VkDescriptorBufferInfo *)DE_NULL,
            (const VkBufferView *)DE_NULL,
        };

        vkd.updateDescriptorSets(device, 1u, &descriptorWrite, 0u, DE_NULL);
    }

    m_executor =
        de::MovePtr<ShaderExecutor>(createExecutor(m_context, m_shaderType, m_shaderSpec, *m_extraResourcesLayout));
}

VkFormatFeatureFlags getRequiredFormatFeatures(const SamplerParameters &samplerParams)
{
    VkFormatFeatureFlags features = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;

    if (samplerParams.minFilter == VK_FILTER_LINEAR || samplerParams.magFilter == VK_FILTER_LINEAR ||
        samplerParams.mipmapFilter == VK_SAMPLER_MIPMAP_MODE_LINEAR)
    {
        features |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
    }

    return features;
}

bool TextureFilteringTestInstance::isSupported(void)
{
    const VkImageCreateFlags imCreateFlags = (m_imParams.dim == IMG_DIM_CUBE) ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
    const VkFormatFeatureFlags reqImFeatures = getRequiredFormatFeatures(m_samplerParams);

    const VkImageFormatProperties imFormatProperties = getPhysicalDeviceImageFormatProperties(
        m_context.getInstanceInterface(), m_context.getPhysicalDevice(), m_imParams.format,
        mapImageType(m_imParams.dim), VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, imCreateFlags);
    const VkFormatProperties formatProperties = getPhysicalDeviceFormatProperties(
        m_context.getInstanceInterface(), m_context.getPhysicalDevice(), m_imParams.format);

    // \todo [2016-06-23 collinbaker] Check image parameters against imFormatProperties
    DE_UNREF(imFormatProperties);

    return (formatProperties.optimalTilingFeatures & reqImFeatures) == reqImFeatures;
}

class TextureFilteringTestCase : public TestCase
{
public:
    TextureFilteringTestCase(tcu::TestContext &testCtx, const char *name) : TestCase(testCtx, name)
    {
    }

    void initSpec(void);

    void checkSupport(Context &context) const
    {
        util::checkTextureSupport(context, m_testCaseData.imParams.format);
    }

    virtual void initPrograms(vk::SourceCollections &programCollection) const
    {
        generateSources(m_testCaseData.shaderType, m_shaderSpec, programCollection);
    }

    virtual de::MovePtr<DataGenerator> createGenerator(void) const = 0;

    virtual TestInstance *createInstance(Context &ctx) const
    {
        return new TextureFilteringTestInstance(ctx, m_testCaseData, m_shaderSpec, createGenerator());
    }

protected:
    de::MovePtr<ShaderExecutor> m_executor;
    TestCaseData m_testCaseData;
    ShaderSpec m_shaderSpec;
};

void TextureFilteringTestCase::initSpec(void)
{
    m_shaderSpec.source =
        genLookupCode(m_testCaseData.imParams, m_testCaseData.samplerParams, m_testCaseData.sampleLookupSettings);
    m_shaderSpec.source += "\nsampledCoord = coord;";

    m_shaderSpec.outputs.push_back(Symbol("result", glu::VarType(glu::TYPE_FLOAT_VEC4, glu::PRECISION_HIGHP)));
    m_shaderSpec.outputs.push_back(Symbol("sampledCoord", glu::VarType(glu::TYPE_FLOAT_VEC4, glu::PRECISION_HIGHP)));
    m_shaderSpec.inputs.push_back(Symbol("coord", glu::VarType(glu::TYPE_FLOAT_VEC4, glu::PRECISION_HIGHP)));
    m_shaderSpec.inputs.push_back(Symbol("layer", glu::VarType(glu::TYPE_FLOAT, glu::PRECISION_HIGHP)));
    m_shaderSpec.inputs.push_back(Symbol("dRef", glu::VarType(glu::TYPE_FLOAT, glu::PRECISION_HIGHP)));
    m_shaderSpec.inputs.push_back(Symbol("dPdx", glu::VarType(glu::TYPE_FLOAT_VEC4, glu::PRECISION_HIGHP)));
    m_shaderSpec.inputs.push_back(Symbol("dPdy", glu::VarType(glu::TYPE_FLOAT_VEC4, glu::PRECISION_HIGHP)));
    m_shaderSpec.inputs.push_back(Symbol("lod", glu::VarType(glu::TYPE_FLOAT, glu::PRECISION_HIGHP)));

    m_shaderSpec.globalDeclarations =
        "layout(set=" + de::toString((int)EXTRA_RESOURCES_DESCRIPTOR_SET_INDEX) + ", binding=0) uniform highp ";
    m_shaderSpec.globalDeclarations += genSamplerDeclaration(m_testCaseData.imParams, m_testCaseData.samplerParams);
    m_shaderSpec.globalDeclarations += " testSampler;";
}

class Texture2DGradientTestCase : public TextureFilteringTestCase
{
public:
    Texture2DGradientTestCase(TestContext &testCtx, const char *name, TextureFormat format, IVec3 dimensions,
                              VkFilter magFilter, VkFilter minFilter, VkSamplerMipmapMode mipmapFilter,
                              VkSamplerAddressMode wrappingMode, bool useDerivatives)

        : TextureFilteringTestCase(testCtx, name)
        , m_format(format)
        , m_dimensions(dimensions)
        , m_magFilter(magFilter)
        , m_minFilter(minFilter)
        , m_mipmapFilter(mipmapFilter)
        , m_wrappingMode(wrappingMode)
        , m_useDerivatives(useDerivatives)
    {
        m_testCaseData = genTestCaseData();
        initSpec();
    }

protected:
    class Generator;

    virtual de::MovePtr<DataGenerator> createGenerator(void) const;

    TestCaseData genTestCaseData()
    {
        // Generate grid

        const SampleLookupSettings sampleLookupSettings = {
            m_useDerivatives ? LOOKUP_LOD_MODE_DERIVATIVES : LOOKUP_LOD_MODE_LOD, // lookupLodMode
            false,                                                                // hasLodBias
            false,                                                                // isProjective
        };

        const SamplerParameters samplerParameters = {m_magFilter,
                                                     m_minFilter,
                                                     m_mipmapFilter,
                                                     m_wrappingMode,
                                                     m_wrappingMode,
                                                     m_wrappingMode,
                                                     VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
                                                     0.0f,
                                                     -1.0f,
                                                     50.0f,
                                                     false,
                                                     false};

        const uint8_t numLevels = (uint8_t)(1 + deLog2Floor32(de::max(m_dimensions[0], m_dimensions[1])));

        const ImageViewParameters imParameters = {
            IMG_DIM_2D, mapTextureFormat(m_format), m_dimensions, numLevels, false, 1,
        };

        const TestCaseData data = {std::vector<ConstPixelBufferAccess>(), imParameters, samplerParameters,
                                   sampleLookupSettings, glu::SHADERTYPE_FRAGMENT};

        return data;
    }

private:
    const TextureFormat m_format;
    const IVec3 m_dimensions;
    const VkFilter m_magFilter;
    const VkFilter m_minFilter;
    const VkSamplerMipmapMode m_mipmapFilter;
    const VkSamplerAddressMode m_wrappingMode;
    const bool m_useDerivatives;
};

class Texture2DGradientTestCase::Generator : public DataGenerator
{
public:
    Generator(const Texture2DGradientTestCase *testCase) : m_testCase(testCase)
    {
    }

    virtual ~Generator(void)
    {
        delete m_tex.release();
    }

    virtual bool generate(void)
    {
        m_tex = de::MovePtr<Texture2D>(
            new Texture2D(m_testCase->m_format, m_testCase->m_dimensions[0], m_testCase->m_dimensions[1]));

        const uint8_t numLevels =
            (uint8_t)(1 + deLog2Floor32(de::max(m_testCase->m_dimensions[0], m_testCase->m_dimensions[1])));

        const TextureFormatInfo fmtInfo = getTextureFormatInfo(m_testCase->m_format);

        const Vec4 cBias  = fmtInfo.valueMin;
        const Vec4 cScale = fmtInfo.valueMax - fmtInfo.valueMin;

        for (uint8_t levelNdx = 0; levelNdx < numLevels; ++levelNdx)
        {
            const Vec4 gMin = Vec4(0.0f, 0.0f, 0.0f, 1.0f) * cScale + cBias;
            const Vec4 gMax = Vec4(1.0f, 1.0f, 1.0f, 0.0f) * cScale + cBias;

            m_tex->allocLevel(levelNdx);
            fillWithComponentGradients(m_tex->getLevel(levelNdx), gMin, gMax);
        }

        return true;
    }

    virtual std::vector<ConstPixelBufferAccess> getPba(void) const
    {
        std::vector<ConstPixelBufferAccess> pba;

        const uint8_t numLevels = (uint8_t)m_tex->getNumLevels();

        for (uint8_t levelNdx = 0; levelNdx < numLevels; ++levelNdx)
        {
            pba.push_back(m_tex->getLevel(levelNdx));
        }

        return pba;
    }

    virtual std::vector<SampleArguments> getSampleArgs(void) const
    {
        std::vector<SampleArguments> args;

        if (m_testCase->m_useDerivatives)
        {
            struct
            {
                Vec4 dPdx;
                Vec4 dPdy;
            } derivativePairs[] = {{Vec4(0.0f, 0.0f, 0.0f, 0.0f), Vec4(0.0f, 0.0f, 0.0f, 0.0f)},
                                   {Vec4(1.0f, 1.0f, 1.0f, 0.0f), Vec4(1.0f, 1.0f, 1.0f, 0.0f)},
                                   {Vec4(0.0f, 0.0f, 0.0f, 0.0f), Vec4(1.0f, 1.0f, 1.0f, 0.0f)},
                                   {Vec4(1.0f, 1.0f, 1.0f, 0.0f), Vec4(0.0f, 0.0f, 0.0f, 0.0f)},
                                   {Vec4(2.0f, 2.0f, 2.0f, 0.0f), Vec4(2.0f, 2.0f, 2.0f, 0.0f)}};

            for (int32_t i = 0; i < 2 * m_testCase->m_dimensions[0] + 1; ++i)
            {
                for (int32_t j = 0; j < 2 * m_testCase->m_dimensions[1] + 1; ++j)
                {
                    for (uint32_t derivNdx = 0; derivNdx < DE_LENGTH_OF_ARRAY(derivativePairs); ++derivNdx)
                    {
                        SampleArguments cur = SampleArguments();
                        cur.coord           = Vec4((float)i / (float)(2 * m_testCase->m_dimensions[0]),
                                                   (float)j / (float)(2 * m_testCase->m_dimensions[1]), 0.0f, 0.0f);
                        cur.dPdx            = derivativePairs[derivNdx].dPdx;
                        cur.dPdy            = derivativePairs[derivNdx].dPdy;

                        args.push_back(cur);
                    }
                }
            }
        }
        else
        {
            const float lodList[] = {-1.0, -0.5, 0.0, 0.5, 1.0, 1.5, 2.0};

            for (int32_t i = 0; i < 2 * m_testCase->m_dimensions[0] + 1; ++i)
            {
                for (int32_t j = 0; j < 2 * m_testCase->m_dimensions[1] + 1; ++j)
                {
                    for (uint32_t lodNdx = 0; lodNdx < DE_LENGTH_OF_ARRAY(lodList); ++lodNdx)
                    {
                        SampleArguments cur = SampleArguments();
                        cur.coord           = Vec4((float)i / (float)(2 * m_testCase->m_dimensions[0]),
                                                   (float)j / (float)(2 * m_testCase->m_dimensions[1]), 0.0f, 0.0f);
                        cur.lod             = lodList[lodNdx];

                        args.push_back(cur);
                    }
                }
            }
        }

        return args;
    }

private:
    const Texture2DGradientTestCase *m_testCase;
    de::MovePtr<Texture2D> m_tex;
};

de::MovePtr<DataGenerator> Texture2DGradientTestCase::createGenerator(void) const
{
    return de::MovePtr<DataGenerator>(new Generator(this));
}

TestCaseGroup *create2DFormatTests(TestContext &testCtx)
{
    de::MovePtr<TestCaseGroup> tests(new TestCaseGroup(testCtx, "formats"));

    const VkFormat formats[] = {
        VK_FORMAT_B4G4R4A4_UNORM_PACK16, VK_FORMAT_R5G6B5_UNORM_PACK16, VK_FORMAT_A1R5G5B5_UNORM_PACK16,
        VK_FORMAT_R8_UNORM, VK_FORMAT_R8_SNORM, VK_FORMAT_R8G8_UNORM, VK_FORMAT_R8G8_SNORM, VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_R8G8B8A8_SNORM,
        //        VK_FORMAT_R8G8B8A8_SRGB,
        VK_FORMAT_B8G8R8A8_UNORM,
        //        VK_FORMAT_B8G8R8A8_SRGB,
        VK_FORMAT_A8B8G8R8_UNORM_PACK32, VK_FORMAT_A8B8G8R8_SNORM_PACK32,
        //        VK_FORMAT_A8B8G8R8_SRGB_PACK32,
        VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_FORMAT_R16_SFLOAT, VK_FORMAT_R16G16_SFLOAT,
        VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT,
        //        VK_FORMAT_B10G11R11_UFLOAT_PACK32,
        //        VK_FORMAT_E5B9G9R9_UFLOAT_PACK32
    };

    const IVec3 size(32, 32, 1);

    for (uint32_t formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(formats); ++formatNdx)
    {
        const std::string prefix = de::toLower(std::string(getFormatName(formats[formatNdx])).substr(10));

        Texture2DGradientTestCase *testCaseNearest = new Texture2DGradientTestCase(
            testCtx, (prefix + "_nearest").c_str(), mapVkFormat(formats[formatNdx]), size, VK_FILTER_NEAREST,
            VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_SAMPLER_ADDRESS_MODE_REPEAT, false);

        tests->addChild(testCaseNearest);

        Texture2DGradientTestCase *testCaseLinear = new Texture2DGradientTestCase(
            testCtx, (prefix + "_linear").c_str(), mapVkFormat(formats[formatNdx]), size, VK_FILTER_LINEAR,
            VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, false);

        tests->addChild(testCaseLinear);
    }

    return tests.release();
}

TestCaseGroup *create2DDerivTests(TestContext &testCtx)
{
    de::MovePtr<TestCaseGroup> tests(new TestCaseGroup(testCtx, "derivatives"));

    const VkFormat format                   = VK_FORMAT_R8G8B8A8_UNORM;
    const VkSamplerAddressMode wrappingMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    const IVec3 size                        = IVec3(16, 16, 1);

    const VkFilter filters[2] = {VK_FILTER_NEAREST, VK_FILTER_LINEAR};

    const VkSamplerMipmapMode mipmapFilters[2] = {
        VK_SAMPLER_MIPMAP_MODE_NEAREST,
        VK_SAMPLER_MIPMAP_MODE_LINEAR,
    };

    for (int magFilterNdx = 0; magFilterNdx < DE_LENGTH_OF_ARRAY(filters); ++magFilterNdx)
    {
        for (int minFilterNdx = 0; minFilterNdx < DE_LENGTH_OF_ARRAY(filters); ++minFilterNdx)
        {
            for (int mipmapFilterNdx = 0; mipmapFilterNdx < DE_LENGTH_OF_ARRAY(mipmapFilters); ++mipmapFilterNdx)
            {
                std::ostringstream caseName;

                switch (filters[magFilterNdx])
                {
                case VK_FILTER_NEAREST:
                    caseName << "nearest";
                    break;

                case VK_FILTER_LINEAR:
                    caseName << "linear";
                    break;

                default:
                    break;
                }

                switch (filters[minFilterNdx])
                {
                case VK_FILTER_NEAREST:
                    caseName << "_nearest";
                    break;

                case VK_FILTER_LINEAR:
                    caseName << "_linear";
                    break;

                default:
                    break;
                }

                caseName << "_mipmap";

                switch (mipmapFilters[mipmapFilterNdx])
                {
                case VK_SAMPLER_MIPMAP_MODE_NEAREST:
                    caseName << "_nearest";
                    break;

                case VK_SAMPLER_MIPMAP_MODE_LINEAR:
                    caseName << "_linear";
                    break;

                default:
                    break;
                }

                Texture2DGradientTestCase *testCase = new Texture2DGradientTestCase(
                    testCtx, caseName.str().c_str(), mapVkFormat(format), size, filters[magFilterNdx],
                    filters[minFilterNdx], mipmapFilters[mipmapFilterNdx], wrappingMode, true);

                tests->addChild(testCase);
            }
        }
    }

    return tests.release();
}

TestCaseGroup *create2DSizeTests(TestContext &testCtx)
{
    // Various size and filtering combinations
    de::MovePtr<TestCaseGroup> tests(new TestCaseGroup(testCtx, "sizes"));

    const VkFilter filters[2] = {VK_FILTER_NEAREST, VK_FILTER_LINEAR};

    const VkSamplerMipmapMode mipmapFilters[2] = {VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_SAMPLER_MIPMAP_MODE_LINEAR};

    const VkSamplerAddressMode wrappingModes[2] = {VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                                   VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE};

    const IVec3 sizes[] = {IVec3(2, 2, 1),   IVec3(2, 3, 1),   IVec3(3, 7, 1),   IVec3(4, 8, 1),    IVec3(31, 55, 1),
                           IVec3(32, 32, 1), IVec3(32, 64, 1), IVec3(57, 35, 1), IVec3(128, 128, 1)};

    for (uint32_t sizeNdx = 0; sizeNdx < DE_LENGTH_OF_ARRAY(sizes); ++sizeNdx)
    {
        for (uint32_t magFilterNdx = 0; magFilterNdx < 2; ++magFilterNdx)
        {
            for (uint32_t minFilterNdx = 0; minFilterNdx < 2; ++minFilterNdx)
            {
                for (uint32_t mipmapFilterNdx = 0; mipmapFilterNdx < 2; ++mipmapFilterNdx)
                {
                    for (uint32_t wrappingModeNdx = 0; wrappingModeNdx < 2; ++wrappingModeNdx)
                    {
                        std::ostringstream caseName;

                        caseName << sizes[sizeNdx][0] << "x" << sizes[sizeNdx][1];

                        switch (filters[magFilterNdx])
                        {
                        case VK_FILTER_NEAREST:
                            caseName << "_nearest";
                            break;

                        case VK_FILTER_LINEAR:
                            caseName << "_linear";
                            break;

                        default:
                            break;
                        }

                        switch (filters[minFilterNdx])
                        {
                        case VK_FILTER_NEAREST:
                            caseName << "_nearest";
                            break;

                        case VK_FILTER_LINEAR:
                            caseName << "_linear";
                            break;

                        default:
                            break;
                        }

                        switch (mipmapFilters[mipmapFilterNdx])
                        {
                        case VK_SAMPLER_MIPMAP_MODE_NEAREST:
                            caseName << "_mipmap_nearest";
                            break;

                        case VK_SAMPLER_MIPMAP_MODE_LINEAR:
                            caseName << "_mipmap_linear";
                            break;

                        default:
                            break;
                        }

                        switch (wrappingModes[wrappingModeNdx])
                        {
                        case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE:
                            caseName << "_clamp";
                            break;

                        case VK_SAMPLER_ADDRESS_MODE_REPEAT:
                            caseName << "_repeat";
                            break;

                        default:
                            break;
                        }

                        Texture2DGradientTestCase *testCase = new Texture2DGradientTestCase(
                            testCtx, caseName.str().c_str(), mapVkFormat(VK_FORMAT_R8G8B8A8_UNORM), sizes[sizeNdx],
                            filters[magFilterNdx], filters[minFilterNdx], mipmapFilters[mipmapFilterNdx],
                            wrappingModes[wrappingModeNdx], false);

                        tests->addChild(testCase);
                    }
                }
            }
        }
    }

    return tests.release();
}

TestCaseGroup *create2DTests(TestContext &testCtx)
{
    de::MovePtr<TestCaseGroup> tests(new TestCaseGroup(testCtx, "2d"));

    tests->addChild(create2DSizeTests(testCtx));
    tests->addChild(create2DFormatTests(testCtx));
    tests->addChild(create2DDerivTests(testCtx));

    return tests.release();
}

} // namespace

TestCaseGroup *createExplicitLodTests(TestContext &testCtx)
{
    // Texture filtering with explicit LOD
    de::MovePtr<TestCaseGroup> tests(new TestCaseGroup(testCtx, "explicit_lod"));

    tests->addChild(create2DTests(testCtx));

    return tests.release();
}

} // namespace texture
} // namespace vkt
