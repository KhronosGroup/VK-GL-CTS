/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2026 Arm Ltd.
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
 */
/*!
 * \file
 * \brief Data Graph Image Aliasing Tests
 */
/*--------------------------------------------------------------------*/

#include "vktDataGraphImageAliasingTests.hpp"

#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"
#include "tcuTestLog.hpp"

#include "vkBarrierUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkDataGraphPipelineConstructionUtil.hpp"
#include "vkDataGraphSessionWithMemory.hpp"
#include "vkDefs.hpp"
#include "vkMemUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"

#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"

#include "vkTensorMemoryUtil.hpp"
#include "vkTensorWithMemory.hpp"

#include "tosa/vktDataGraphTosaSpirv.hpp"
#include "tosa/vktDataGraphTosaUtil.hpp"

#include "spirv-tools/libspirv.hpp"

#include <cstdint>
#include <vector>
#include <sstream>

#include "../tensor/vktTensorTestsUtil.hpp"
#include "vktDataGraphTestProvider.hpp"
#include "vktDataGraphTestUtil.hpp"

namespace vkt
{
namespace dataGraph
{

namespace
{

static inline tcu::TestStatus imageAliasingTest(Context &m_context, TestParams m_params)
{
    const DeviceInterface &vk       = m_context.getDeviceInterface();
    const VkDevice device           = m_context.getDevice();
    const VkQueue queue             = m_context.getUniversalQueue();
    const uint32_t queueFamilyIndex = m_context.getUniversalQueueFamilyIndex();
    Allocator &allocator            = m_context.getDefaultAllocator();

    // getDataGraphTest cannot return nullptr as will throw an exception in case of errors
    std::unique_ptr<DataGraphTest> graphTest{DataGraphTestProvider::getDataGraphTest(m_context, "TOSA", m_params)};
    std::vector<DataGraphTestResource> testResources(graphTest->numResources());

    /* Create tensors */

    for (size_t i = 0; i < graphTest->numResources(); i++)
    {
        const auto &ri = graphTest->resourceInfo(i);
        auto &tr       = testResources.at(i);

        tr.dimensions = ri.params.dimensions;
        tr.strides    = ri.params.strides;
        if (ri.isTensor())
        {
            tr.strides.resize(tr.dimensions.size());
        }
        tr.desc = makeTensorDescription(ri.params.tiling, ri.params.format, tr.dimensions, tr.strides,
                                        VK_TENSOR_USAGE_DATA_GRAPH_BIT_ARM);
        if (ri.isTensor())
        {
            /* create an image aliased tensor */
            /* ImageAliasedTensorWithMemory updates the tensor strides from the aliased image layout before creating the tensor. */
            VkTensorCreateInfoARM createInfo = makeTensorCreateInfo(&tr.desc);
            tr.tensor                        = de::MovePtr<TensorWithMemory>(new ImageAliasedTensorWithMemory(
                vk, device, allocator, createInfo, tr.strides, vk::MemoryRequirement::Any));

            tr.view = makeTensorView(vk, device, tr.tensor->get(), ri.params.format);

            /* fill host and tensor data */
            graphTest->initData(i, &*tr.tensor);
        }
        else
        {
            /* fill only host data, e.g. for constants */
            graphTest->initData(i, nullptr, {0, ri.sparsityInfo});
        }
    }

    /* Create descriptor set */

    DescriptorSetLayoutBuilder setLayoutBuilder;
    for (size_t i = 0; i < graphTest->numResources(); i++)
    {
        const auto &ri = graphTest->resourceInfo(i);
        if (ri.isTensor())
        {
            /* constants do not need to be in the descriptor set */
            setLayoutBuilder.addSingleIndexedBinding(VK_DESCRIPTOR_TYPE_TENSOR_ARM, VK_SHADER_STAGE_ALL, ri.binding);
        }
    }
    const Unique<VkDescriptorSetLayout> descriptorSetLayout(setLayoutBuilder.build(vk, device));

    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_TENSOR_ARM, static_cast<uint32_t>(graphTest->numTensors()));
    const Unique<VkDescriptorPool> descriptorPool(
        poolBuilder.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

    const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

    DescriptorSetUpdateBuilder updatebuilder;
    for (size_t i = 0; i < graphTest->numResources(); i++)
    {
        const auto &ri = graphTest->resourceInfo(i);
        auto &tr       = testResources.at(i);
        if (ri.isTensor())
        {
            tr.writeDesc = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_TENSOR_ARM, nullptr, 1, &tr.view.get()};
            updatebuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(ri.binding),
                                      VK_DESCRIPTOR_TYPE_TENSOR_ARM, &tr.writeDesc);
        }
    }
    updatebuilder.update(vk, device);

    /* Create DataGraph pipeline */

    DataGraphPipelineWrapper pipeline(vk, device);
    pipeline.setDescriptorSetLayout(descriptorSetLayout.get());
    pipeline.addShaderModule(graphTest->shaderModule());

    for (size_t i = 0; i < graphTest->numResources(); i++)
    {
        const auto &ri = graphTest->resourceInfo(i);
        auto &tr       = testResources.at(i);
        if (ri.isTensor())
        {
            pipeline.addTensor(tr.desc, ri.descriptorSet, ri.binding);
        }
        else
        {
            pipeline.addConstant(tr.desc, ri.hostData, ri.id, ri.sparsityInfo);
        }
    }
    pipeline.buildPipeline(VK_NULL_HANDLE);

    /* Create DataGraph pipeline session */

    VkDataGraphPipelineSessionCreateInfoARM sessionCreateInfo = initVulkanStructure();
    sessionCreateInfo.dataGraphPipeline                       = pipeline.get();
    const DataGraphSessionWithMemory dataGraphSession(vk, device, allocator, sessionCreateInfo,
                                                      vk::MemoryRequirement::Any, m_params.sessionMemory);

    const Unique<VkCommandPool> cmdPool(makeCommandPool(vk, device, queueFamilyIndex));
    const Unique<VkCommandBuffer> cmdBuffer(
        allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

    // Start recording commands

    beginCommandBuffer(vk, cmdBuffer.get());

    pipeline.bind(cmdBuffer.get());
    vk.cmdBindDescriptorSets(cmdBuffer.get(), VK_PIPELINE_BIND_POINT_DATA_GRAPH_ARM, pipeline.getPipelineLayout(), 0u,
                             1u, &descriptorSet.get(), 0u, nullptr);

    vk.cmdDispatchDataGraphARM(cmdBuffer.get(), *dataGraphSession, nullptr);

    endCommandBuffer(vk, cmdBuffer.get());

    // Wait for completion

    submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

    // Validate the results

    for (size_t i = 0; i < graphTest->numResources(); i++)
    {
        const auto &ri = graphTest->resourceInfo(i);
        auto &tr       = testResources.at(i);

        if (ri.isTensor() && ri.requiresVerify())
        {
            auto testStatus = graphTest->verifyData(i, &*tr.tensor);
            if (testStatus.isFail())
            {
                return testStatus;
            }
        }
    }

    return tcu::TestStatus::pass("test succeeded");
}

static inline void getImageAliasingShaderParams(VkFormat format, std::string &imageFormat, std::string &imageType,
                                                std::string &bufferValueType, bool &isInteger)
{
    if (format == VK_FORMAT_R8_SINT)
    {
        imageFormat     = "r8i";
        imageType       = "iimage2D";
        bufferValueType = "int";
        isInteger       = true;
    }
    else if (format == VK_FORMAT_R32_SINT)
    {
        imageFormat     = "r32i";
        imageType       = "iimage2D";
        bufferValueType = "int";
        isInteger       = true;
    }
    else if (format == VK_FORMAT_R32_SFLOAT)
    {
        imageFormat     = "r32f";
        imageType       = "image2D";
        bufferValueType = "float";
        isInteger       = false;
    }
    else if (format == VK_FORMAT_R16_SFLOAT)
    {
        imageFormat     = "r16f";
        imageType       = "image2D";
        bufferValueType = "float";
        isInteger       = false;
    }
    else
    {
        TCU_THROW(InternalError, "Unsupported image aliasing format");
    }
}

static inline std::string getImageAliasingFillShaderSource(VkFormat format)
{
    std::string imageFormat;
    std::string imageType;
    std::string bufferValueType;
    bool isInteger = false;
    getImageAliasingShaderParams(format, imageFormat, imageType, bufferValueType, isInteger);

    std::ostringstream cs;

    // Declare workgroup size and tensor dimensions used by both shader variants.
    cs << "#version 450\n"
       << "layout(local_size_x=64, local_size_y=1, local_size_z=1) in;\n"
       << "layout(push_constant) uniform PushConstants {\n"
       << "  uint dim0; uint dim1; uint dim2; uint dim3; uint elementCount;\n"
       << "} pc;\n";

    // Fill shaders use a source-values buffer and a writeonly aliased image.
    cs << "layout(set=0, binding=0) readonly buffer Values { " << bufferValueType << " values[]; } valueBuffer;\n"
       << "layout(set=0, binding=1, " << imageFormat << ") uniform writeonly " << imageType << " img;\n";

    // Convert a linear index into packed tensor coordinates.
    cs << "void linearIndexToTensorCoordinates(uint index, out uint coord_0, out uint coord_1, out uint coord_2, out "
          "uint coord_3){\n"
       << "  uint stride0 = pc.dim1 * pc.dim2 * pc.dim3;\n"
       << "  uint stride1 = pc.dim2 * pc.dim3;\n"
       << "  uint stride2 = pc.dim3;\n"
       << "  coord_0 = index / stride0;\n"
       << "  uint rem0 = index % stride0;\n"
       << "  coord_1 = rem0 / stride1;\n"
       << "  uint rem1 = rem0 % stride1;\n"
       << "  coord_2 = rem1 / stride2;\n"
       << "  coord_3 = rem1 % stride2;\n"
       << "}\n";

    // Use one invocation per packed tensor element and map the 2D image to coord_0=0, coord_3=0.
    cs << "void main(){\n"
       << "  uint index = gl_GlobalInvocationID.x;\n"
       << "  if (index >= pc.elementCount) return;\n"
       << "  uint coord_0; uint coord_1; uint coord_2; uint coord_3;\n"
       << "  linearIndexToTensorCoordinates(index, coord_0, coord_1, coord_2, coord_3);\n"
       << "  if (coord_0 != 0u || coord_3 != 0u) return;\n"
       << "  ivec2 pixel = ivec2(coord_2, coord_1);\n";

    if (isInteger)
    {
        cs << "  imageStore(img, pixel, ivec4(valueBuffer.values[index], 0, 0, 1));\n";
    }
    else
    {
        cs << "  imageStore(img, pixel, vec4(valueBuffer.values[index], 0.0, 0.0, 1.0));\n";
    }

    cs << "}\n";
    return cs.str();
}

static inline std::string getImageAliasingVerifyShaderSource(VkFormat format)
{
    std::string imageFormat;
    std::string imageType;
    std::string bufferValueType;
    bool isInteger = false;
    getImageAliasingShaderParams(format, imageFormat, imageType, bufferValueType, isInteger);

    std::ostringstream cs;

    cs << "#version 450\n"
       << "layout(local_size_x=64, local_size_y=1, local_size_z=1) in;\n"
       << "layout(push_constant) uniform PushConstants {\n"
       << "  uint dim0; uint dim1; uint dim2; uint dim3; uint elementCount;\n"
       << "} pc;\n";

    // Verify shaders use an expected-values buffer, a mismatch counter buffer and a readonly aliased image.
    cs << "layout(set=0, binding=0) readonly buffer Values { " << bufferValueType << " values[]; } valueBuffer;\n"
       << "layout(set=0, binding=1) buffer Mismatches { uint count; } mismatches;\n"
       << "layout(set=0, binding=2, " << imageFormat << ") uniform readonly " << imageType << " img;\n";

    // Convert a linear index into packed tensor coordinates.
    cs << "void linearIndexToTensorCoordinates(uint index, out uint coord_0, out uint coord_1, out uint coord_2, out "
          "uint coord_3){\n"
       << "  uint stride0 = pc.dim1 * pc.dim2 * pc.dim3;\n"
       << "  uint stride1 = pc.dim2 * pc.dim3;\n"
       << "  uint stride2 = pc.dim3;\n"
       << "  coord_0 = index / stride0;\n"
       << "  uint rem0 = index % stride0;\n"
       << "  coord_1 = rem0 / stride1;\n"
       << "  uint rem1 = rem0 % stride1;\n"
       << "  coord_2 = rem1 / stride2;\n"
       << "  coord_3 = rem1 % stride2;\n"
       << "}\n";

    // Use one invocation per packed tensor element.
    cs << "void main(){\n"
       << "  uint index = gl_GlobalInvocationID.x;\n"
       << "  if (index >= pc.elementCount) return;\n"
       << "  uint coord_0; uint coord_1; uint coord_2; uint coord_3;\n"
       << "  linearIndexToTensorCoordinates(index, coord_0, coord_1, coord_2, coord_3);\n"
       << "  if (coord_0 != 0u || coord_3 != 0u) return;\n"
       << "  ivec2 pixel = ivec2(coord_2, coord_1);\n";

    if (isInteger)
    {
        cs << "  if (imageLoad(img, pixel).r != valueBuffer.values[index]) atomicAdd(mismatches.count, 1u);\n";
    }
    else
    {
        // Compare floating-point buffer values against image pixels with format-specific tolerance.
        cs << "  float expected = valueBuffer.values[index];\n"
           << "  float got = imageLoad(img, pixel).r;\n"
           << ((format == VK_FORMAT_R16_SFLOAT) ? "  float tolerance = max(1.0, abs(expected) * 0.002);\n" :
                                                  "  float tolerance = 0.1;\n")
           << "  if (abs(got - expected) > tolerance) atomicAdd(mismatches.count, 1u);\n";
    }

    cs << "}\n";
    return cs.str();
}

static inline void initPrograms(vk::SourceCollections &programCollection, TestParams params)
{
    DE_UNREF(params);

    for (const VkFormat format : {VK_FORMAT_R8_SINT, VK_FORMAT_R32_SINT, VK_FORMAT_R32_SFLOAT, VK_FORMAT_R16_SFLOAT})
    {
        programCollection.glslSources.add(getImageAliasingFillShaderName(format))
            << glu::ComputeSource(getImageAliasingFillShaderSource(format));
        programCollection.glslSources.add(getImageAliasingVerifyShaderName(format))
            << glu::ComputeSource(getImageAliasingVerifyShaderSource(format));
    }
}

static inline void checkImageAliasingSupport(Context &ctx, TestParams testParams)
{
    const VkImageTiling imageTiling      = VK_IMAGE_TILING_LINEAR;
    const VkTensorTilingARM tensorTiling = VK_TENSOR_TILING_LINEAR_ARM;
    const std::vector<VkFormat> formats  = getVkFormats(testParams.formats);

    DE_ASSERT(testParams.tiling == tensorTiling);

    for (const auto format : formats)
    {
        if (!tensor::formatSupportImageFlags(ctx, format, imageTiling, VK_IMAGE_USAGE_STORAGE_BIT))
            TCU_THROW(NotSupportedError, "Chosen image format/tiling does not support storage usage");

        if (!tensor::formatSupportTensorFlags(ctx, format, tensorTiling, VK_FORMAT_FEATURE_2_TENSOR_SHADER_BIT_ARM))
            TCU_THROW(NotSupportedError, "Chosen tensor format/tiling does not support tensor shader access");
    }

    ctx.requireDeviceFunctionality("VK_ARM_tensors");

    if (!tensor::deviceSupportsShaderTensorAccess(ctx))
        TCU_THROW(NotSupportedError, "Device does not support shader tensor access");

    if (!tensor::deviceSupportsShaderStagesTensorAccess(ctx, VK_SHADER_STAGE_COMPUTE_BIT))
        TCU_THROW(NotSupportedError, "Device does not support shader tensor access in compute stage");

    TestParams::checkSupport(ctx, testParams);
}

static inline void imageAliasingTests(tcu::TestCaseGroup *group)
{
    const auto &paramsVariations =
        getTestParamsVariations({"TOSA"}, {false}, {{ONE, ONE, NONE}, {ONE, ONE, MANY}},
                                {{TENSOR_STRIDES_IMAGE_ALIASING, TENSOR_STRIDES_IMAGE_ALIASING, TENSOR_STRIDES_PACKED}},
                                {false, true}, {VK_TENSOR_TILING_LINEAR_ARM}, {SparsityVariation::NONE}, true);

    for (const auto &params : paramsVariations)
    {
        addFunctionCaseWithPrograms(group, de::toString(params), checkImageAliasingSupport, initPrograms,
                                    imageAliasingTest, params);
    }
}

} // namespace

void imageAliasingTestsGroup(tcu::TestCaseGroup *group)
{
    addTestGroup(group, "submitPipeline", imageAliasingTests);
}

} // namespace dataGraph
} // namespace vkt
