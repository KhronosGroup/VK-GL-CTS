/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2024 The Khronos Group Inc.
 * Copyright (c) 2024 Valve Corporation.
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
 * \brief Device Generated Commands EXT Graphics Draw Tests
 *//*--------------------------------------------------------------------*/

#include "vktDGCGraphicsDrawCountTestsExt.hpp"
#include "vktTestCase.hpp"
#include "vktDGCUtilExt.hpp"
#include "vktDGCUtilCommon.hpp"
#include "vktTestCaseUtil.hpp"

#include "vkTypeUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkBarrierUtil.hpp"

#include "tcuImageCompare.hpp"

#include "deUniquePtr.hpp"
#include "deRandom.hpp"

#include <numeric>
#include <vector>
#include <cstddef>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <utility>

namespace vkt
{
namespace DGC
{

using namespace vk;

namespace
{

/*
GENERAL MECHANISM BEHIND THESE TESTS:

Create a framebuffer of 32x32 pixels.
  - This gives us a total of 1024 pixels to draw over.
Create one triangle to cover each pixel and store their vertices in a vertex buffer.
Divide the 1024 pixels in 16 pseudorandom chunks.
  - For that, choose a number of pixels randomly between 1 and 64 pixels for the first 15 chunks.
  - For the last chunk, choose the remaining pixels.
For each of those chunks, create a VkDrawIndirectCommand structure.
  - vertexCount is the number of pixels in each chunk * 3.
  - firstVertex is the number of pixels in the previous chunks * 3.
  - Choose pseudorandomly one of 256 InstanceIndex values for each pixel:
    - Value in [0, 16, 32, 48, 64...] for firstInstance
    - Value in [1..16] for instanceCount
    - InstanceIndex will be a pseudorandom number in 0..255.
Pseudorandomly choose to split the list of chunks in 4 (buffers)
  - Similar to dividing the pixels in chunks.
Pseudorandomly choose how many extra structures to put in the middle for padding in each buffer.
 - For example, from 0 to 7.
With that, create 4 VkDrawIndirectCountIndirectCommandEXT structures:
 - bufferAddress will vary in each of the 4 buffers.
 - stride will depend on the pseudorandom padding in each buffer.
 - commandCount will be the number of chunks assigned to each buffer.

Clear framebuffer to (0, 0, 0, 1.0)
Draw (InstanceIndex / 256.0, 0, 1.0, 1.0) in the fragment shader.

When testing execution sets with this, we will take the chance to test also:
- Shader IO
- Built-ins
- Descriptor sets.

Descriptor sets and IO:

In the vertex shader, we'll use 4 readonly storage buffers as descriptor bindings (1 for each sequence), containing:

- binding=0:  8 odd positive numbers: 0, 2, 4, 6, 8, 10, 12, 14
- binding=1: 12 even positive numbers: 1, 3, ...
- binding=2: 16 odd negative numbers: -2, -4, ...
- binding=3: 20 even negative numbers: -1, -3, ...

And 4 variants of the vertex and fragment shaders, numbered 0 to 3. Each sequence will use 1 vertex and fragment shader variant, and
will work with 1 of the 4 buffers.

- Vertex shader i reads numbers from binding i, and stores each in an out flat int variable, in some order that depends on the
  VertexIndex, for example. What matters is that, for variant 0 we'll have 16 IO variables and the number of IO variables changes
  (increasing) for each sequence and shader.
- Fragment shader i will read those numbers from IO (4 fragment shaders, different amount of IO variables) and calculate the total
  sum.
- The sum will be the same for all pixels of the sequence.
- As we know how many pixels are drawn by each sequence, we'll store the expected results in a storage buffer for each pixel.
- The fragment shader will check the sum against the expected result for the pixel (using gl_FragCoord to access a storage buffer
  with the results) and will:
    - Write 0 in the green channel if correct.
    - Write 1 in the green channel if not.

For built-ins:

- Position and PointSize are set normally.
- We'll store a Vec4 of extra data for each vertex.
  - One of them will be the clip distance and the other one will be the cull distance.
  - In 1/8 (pseudorandom) of the pixels, we'll store a negative clip distance.
  - In 1/8 (pseudorandom) of the pixels, we'll store a negative cull distance.
- When verifying results, those pixels should not be covered.

*/

using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

constexpr uint32_t kSequenceCount       = 4u;
constexpr uint32_t kPerTriangleVertices = 3u;
constexpr uint32_t kMaxInstanceIndex    = 255u;
constexpr uint32_t kVertexChunkOffset   = 1000u;
constexpr uint32_t kPipelineShaders     = 2u; // Each "pipeline" contains a vertex and a frag shader.

enum class TestType
{
    DRAW_COUNT = 0,
    DRAW_INDEXED_COUNT,
    DRAW_INDEXED_COUNT_INDEX_TOKEN, // Same as the previous one, but using an index buffer token.
};

enum class PreprocessType
{
    NONE = 0,
    SAME_STATE_CMD_BUFFER,
    OTHER_STATE_CMD_BUFFER,
};

struct TestParams
{
    TestType testType;
    PreprocessType preprocessType;
    bool checkDrawParams;
    bool useExecutionSet;
    bool useShaderObjects;
    bool unorderedSequences;

    uint32_t getRandomSeed(void) const
    {
        // Other members not used because we want to make sure results don't
        // change if we use the same pseudorandom sequence.
        const uint32_t rndSeed = ((static_cast<int>(testType) << 26u) | (useExecutionSet << 25u) |
                                  (useShaderObjects << 24u) | static_cast<uint32_t>(checkDrawParams));

        return rndSeed;
    }

    bool doPreprocess(void) const
    {
        return (preprocessType != PreprocessType::NONE);
    }

    bool indexedDraws(void) const
    {
        return (testType == TestType::DRAW_INDEXED_COUNT || testType == TestType::DRAW_INDEXED_COUNT_INDEX_TOKEN);
    }

    bool indexBufferToken(void) const
    {
        return (testType == TestType::DRAW_INDEXED_COUNT_INDEX_TOKEN);
    }
};

void checkDrawCountSupport(Context &context, TestParams params)
{
    const auto stages                 = (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    const auto bindStages             = (params.useExecutionSet ? stages : static_cast<VkShaderStageFlags>(0u));
    const auto bindStagesPipeline     = (params.useShaderObjects ? 0u : bindStages);
    const auto bindStagesShaderObject = (params.useShaderObjects ? bindStages : 0u);

    checkDGCExtSupport(context, stages, bindStagesPipeline, bindStagesShaderObject);

    const auto &dgcProperties = context.getDeviceGeneratedCommandsPropertiesEXT();
    if (!dgcProperties.deviceGeneratedCommandsMultiDrawIndirectCount)
        TCU_THROW(NotSupportedError, "deviceGeneratedCommandsMultiDrawIndirectCount not supported");

    if (params.useShaderObjects)
    {
        context.requireDeviceFunctionality("VK_EXT_shader_object");

        if (params.useExecutionSet && dgcProperties.maxIndirectShaderObjectCount == 0u)
            TCU_THROW(NotSupportedError, "maxIndirectShaderObjectCount is zero");
    }

    if (params.checkDrawParams)
        context.requireDeviceFunctionality("VK_KHR_shader_draw_parameters");
}

template <typename T>
class RangeGen
{
public:
    RangeGen(T start, T step) : m_current(start), m_step(step)
    {
    }

    T operator++()
    {
        m_current += m_step;
        return m_current;
    }
    T operator++(int)
    {
        T prev = m_current;
        m_current += m_step;
        return prev;
    }
    operator T()
    {
        return m_current;
    }

private:
    T m_current;
    T m_step;
};

using BufferDataVec = std::vector<std::vector<int32_t>>;

BufferDataVec getInputBuffers(void)
{
    //  - binding=0:  8 odd positive numbers: 0, 2, 4, 6, 8, 10, 12, 14
    //  - binding=1: 12 even positive numbers: 1, 3, ...
    //  - binding=2: 16 odd negative numbers: -2, -4, ...
    //  - binding=3: 20 even negative numbers: -1, -3, ...
    DE_ASSERT(kSequenceCount == 4u);
    const std::vector<size_t> bufferSizes{8u, 12u, 16u, 20u};
    const std::vector<int32_t> rangeStarts{0, 1, -2, -1};
    const std::vector<int32_t> rangeSteps{2, 2, -2, -2};

    BufferDataVec buffers(kSequenceCount);
    for (uint32_t i = 0u; i < kSequenceCount; ++i)
    {
        auto &buffer = buffers.at(i);
        buffer.resize(bufferSizes.at(i));
        RangeGen generator(rangeStarts.at(i), rangeSteps.at(i));
        std::iota(begin(buffer), end(buffer), generator);
    }

    return buffers;
}

struct VertexData
{
    tcu::Vec4 position;
    tcu::Vec4 extraData; // 0: clip distance, 1: cull distance

    VertexData(const tcu::Vec4 &position_, const tcu::Vec4 &extraData_) : position(position_), extraData(extraData_)
    {
    }
};

void initDrawCountPrograms(vk::SourceCollections &programCollection, TestParams params)
{
    std::vector<uint32_t> ioSizes;
    uint32_t shaderVariants = 1u;

    if (params.useExecutionSet)
    {
        const auto inputBuffers = getInputBuffers();

        shaderVariants = de::sizeU32(inputBuffers);

        std::transform(begin(inputBuffers), end(inputBuffers), std::back_inserter(ioSizes),
                       [](const std::vector<int32_t> &vec) { return de::sizeU32(vec); });
    }

    const uint32_t locationOffset = 5u; // For I/O vars, to leave some room for other things we may want to pass.
    const bool checkDrawParams    = params.checkDrawParams;

    std::ostringstream vertBindings;
    std::string vertBindingsDecl;
    std::string fragBindingsDecl;
    std::string pushConstantDecl;
    uint32_t nextFragBinding = 0u;

    // When using multiple shader variants, we'll test bindings and shader IO as described above.
    if (params.useExecutionSet)
    {
        for (size_t i = 0u; i < ioSizes.size(); ++i)
        {
            vertBindings << "layout (set=0, binding=" << i << ", std430) readonly buffer Buffer" << i
                         << " { int values[" << ioSizes.at(i) << "]; } buffer" << i << ";\n";
        }
        vertBindingsDecl = vertBindings.str();

        // Note frag shader bindings use separate sets.
        fragBindingsDecl += "layout (set=1, binding=" + std::to_string(nextFragBinding++) +
                            ", std430) readonly buffer ExpectedAccum { int values[]; } ea;\n";
    }

    if (checkDrawParams)
        fragBindingsDecl += "layout (set=1, binding=" + std::to_string(nextFragBinding++) +
                            ", std430) readonly buffer ExpectedDrawParams { ivec4 values[]; } edp;\n";

    if (params.useExecutionSet || checkDrawParams)
        pushConstantDecl += "layout (push_constant, std430) uniform PushConstantBlock { uvec2 dim; } pc;\n";

    for (uint32_t i = 0u; i < shaderVariants; ++i)
    {
        const uint32_t ioVarCount = (params.useExecutionSet ? ioSizes.at(i) : 0u);
        const auto nameSuffix     = (params.useExecutionSet ? std::to_string(i) : std::string());

        std::ostringstream outVarsDecl;
        std::ostringstream inVarsDecl;
        std::ostringstream outVarsWrite;
        std::ostringstream inVarsRead;

        for (uint32_t j = 0u; j < ioVarCount; ++j)
        {
            const auto location = j + locationOffset;

            outVarsDecl << "layout (location=" << location << ") out flat int iovar" << j << ";\n";
            inVarsDecl << "layout (location=" << location << ") in flat int iovar" << j << ";\n";
            outVarsWrite << "    iovar" << j << " = buffer" << i << ".values[" << j << "];\n";
            inVarsRead << "    accum += iovar" << j << ";\n";
        }

        std::ostringstream vert;
        vert << "#version 460\n"
             << "layout (location=0) in vec4 inPos;\n"
             << "layout (location=1) in vec4 inExtraData;\n"
             << "layout (location=0) out flat int outInstanceIndex;\n"
             << (checkDrawParams ? "layout (location=1) out flat int drawIndex;\n" : "") << "\n"
             << (checkDrawParams ? "layout (location=2) out flat int baseVertex;\n" : "") << "\n"
             << (checkDrawParams ? "layout (location=3) out flat int baseInstance;\n" : "") << "\n"
             << vertBindingsDecl << "\n"
             << outVarsDecl.str() << "\n"
             << "out gl_PerVertex {\n"
             << "    vec4  gl_Position;\n"
             << "    float gl_PointSize;\n"
             << "    float gl_ClipDistance[1];\n"
             << "    float gl_CullDistance[1];\n"
             << "};\n"
             << "void main (void) {\n"
             << "    gl_Position = inPos;\n"
             << "    gl_PointSize = 1.0;\n"
             << "    gl_ClipDistance[0] = inExtraData.x;\n"
             << "    gl_CullDistance[0] = inExtraData.y;\n"
             << "    outInstanceIndex = gl_InstanceIndex;\n"
             << (checkDrawParams ? "    drawIndex = gl_DrawID;\n" : "")
             << (checkDrawParams ? "    baseVertex = gl_BaseVertex;\n" : "")
             << (checkDrawParams ? "    baseInstance = gl_BaseInstance;\n" : "") << outVarsWrite.str() << "}\n";
        const auto vertName = "vert" + nameSuffix;
        programCollection.glslSources.add(vertName) << glu::VertexSource(vert.str());

        const bool pixelIdxNeeded = (params.useExecutionSet || checkDrawParams);
        std::ostringstream frag;
        frag << "#version 460\n"
             << "layout (location=0) in flat int inInstanceIndex;\n"
             << (checkDrawParams ? "layout (location=1) in flat int drawIndex;\n" : "")
             << (checkDrawParams ? "layout (location=2) in flat int baseVertex;\n" : "")
             << (checkDrawParams ? "layout (location=3) in flat int baseInstance;\n" : "")
             << "layout (location=0) out vec4 outColor;\n"
             << "\n"
             << fragBindingsDecl << pushConstantDecl << "\n"
             << inVarsDecl.str() << "\n"
             << "void main (void) {\n"
             << (pixelIdxNeeded ?
                     "    const uint pixelIdx = uint(gl_FragCoord.y) * pc.dim.x + uint(gl_FragCoord.x);\n" :
                     "")
             << (params.useExecutionSet ? "    int accum = 0;\n" : "") << inVarsRead.str()
             << "    const float red   = float(inInstanceIndex) / " << kMaxInstanceIndex << ".0;\n"
             << "    const float green = "
             << (params.useExecutionSet ? "((accum == ea.values[pixelIdx]) ? 0.0 : 1.0)" : "0.0") << ";\n"
             << "    bool blueOK = true;\n"
             << (checkDrawParams ? "    blueOK = (blueOK && (drawIndex == edp.values[pixelIdx].x));\n" : "")
             << (checkDrawParams ? "    blueOK = (blueOK && (baseVertex == edp.values[pixelIdx].y));\n" : "")
             << (checkDrawParams ? "    blueOK = (blueOK && (baseInstance == edp.values[pixelIdx].z));\n" : "")
             << "    const float blue  = (blueOK ? 1.0 : 0.0);\n"
             << "    outColor = vec4(red, green, blue, 1.0);\n"
             << "}\n";
        const auto fragName = "frag" + nameSuffix;
        programCollection.glslSources.add(fragName) << glu::FragmentSource(frag.str());
    }
}

using DGCBufferPtr        = std::unique_ptr<DGCBuffer>;
using BufferWithMemoryPtr = std::unique_ptr<BufferWithMemory>;
using BufferVec           = std::vector<BufferWithMemoryPtr>;

struct SequenceInfo
{
    DGCBufferPtr buffer;
    uint32_t chunkCount;
    uint32_t stride;
    uint32_t vertexCount;
};

Move<VkShaderEXT> makeSingleShader(const DeviceInterface &vkd, VkDevice device, VkShaderStageFlagBits stage,
                                   const ProgramBinary &binary, const std::vector<VkDescriptorSetLayout> &setLayouts,
                                   const std::vector<VkPushConstantRange> &pcRanges)
{
    VkShaderStageFlags nextStage = 0u;
    if (stage == VK_SHADER_STAGE_VERTEX_BIT)
        nextStage |= VK_SHADER_STAGE_FRAGMENT_BIT;
    else if (stage == VK_SHADER_STAGE_FRAGMENT_BIT)
        ;
    else
        DE_ASSERT(false);

    const VkShaderCreateInfoEXT createInfo = {
        VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT, //  VkStructureType sType;
        nullptr,                                  //  const void* pNext;
        0u,                                       //  VkShaderCreateFlagsEXT flags;
        stage,                                    //  VkShaderStageFlagBits stage;
        nextStage,                                //  VkShaderStageFlags nextStage;
        VK_SHADER_CODE_TYPE_SPIRV_EXT,            //  VkShaderCodeTypeEXT codeType;
        binary.getSize(),                         //  size_t codeSize;
        binary.getBinary(),                       //  const void* pCode;
        "main",                                   //  const char* pName;
        de::sizeU32(setLayouts),                  //  uint32_t setLayoutCount;
        de::dataOrNull(setLayouts),               //  const VkDescriptorSetLayout* pSetLayouts;
        de::sizeU32(pcRanges),                    //  uint32_t pushConstantRangeCount;
        de::dataOrNull(pcRanges),                 //  const VkPushConstantRange* pPushConstantRanges;
        nullptr,                                  //  const VkSpecializationInfo* pSpecializationInfo;
    };

    binary.setUsed();

    return createShader(vkd, device, createInfo);
}

tcu::TestStatus testDrawCountRun(Context &context, TestParams params)
{
    const auto ctx = context.getContextCommonData();
    const tcu::IVec3 fbExtent(32, 32, 1);
    const auto vkExtent       = makeExtent3D(fbExtent);
    const auto floatExtent    = fbExtent.asFloat();
    const auto pixelCountU    = vkExtent.width * vkExtent.height * vkExtent.depth;
    const auto kChunkCount    = 16u;
    const auto chunkMaxPixels = static_cast<int>(pixelCountU / kChunkCount); // Does not apply to the last chunk.
    const auto maxIndirectDraws =
        static_cast<int>(kChunkCount / kSequenceCount); // Per draw count dispatch. Doesn't apply to last.
    const auto stageFlags = (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    const auto bindPoint  = VK_PIPELINE_BIND_POINT_GRAPHICS;
    const auto descType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

    // Pseudorandom number generator.
    const auto randomSeed = params.getRandomSeed();
    de::Random rnd(randomSeed);

    // Generate one triangle around the center of each pixel.
    const float pixelWidth  = 2.0f / floatExtent.x();
    const float pixelHeight = 2.0f / floatExtent.y();
    const float horMargin   = pixelWidth / 4.0f;
    const float verMargin   = pixelHeight / 4.0f;

    // Converts to framebuffer range [-1,1]
    const auto normalize = [](int v, int total)
    { return ((static_cast<float>(v) + 0.5f) / static_cast<float>(total)) * 2.0f - 1.0f; };

    // These will be chosen pseudorandomly for each pixel.
    const std::vector<float> clipDistances{0.75f, 0.0f, -0.5f, 1.25f, 20.0f, 2.0f, 0.25f, 1.0f};
    const std::vector<float> cullDistances{0.75f, 0.0f, 0.5f, 1.25f, 20.0f, 2.0f, -0.25f, 1.0f};

    const int lastClip = static_cast<int>(clipDistances.size()) - 1;
    const int lastCull = static_cast<int>(cullDistances.size()) - 1;

    // Vertex buffer data.
    std::vector<VertexData> vertices;
    vertices.reserve(pixelCountU * kPerTriangleVertices);

    for (int y = 0; y < fbExtent.y(); ++y)
        for (int x = 0; x < fbExtent.x(); ++x)
        {
            const float xCenter = normalize(x, fbExtent.x());
            const float yCenter = normalize(y, fbExtent.y());

            const float clip = clipDistances.at(rnd.getInt(0, lastClip));
            const float cull = cullDistances.at(rnd.getInt(0, lastCull));

            const tcu::Vec4 extraData(clip, cull, 0.0f, 0.0f);

            vertices.emplace_back(tcu::Vec4(xCenter - horMargin, yCenter + verMargin, 0.0f, 1.0f), extraData);
            vertices.emplace_back(tcu::Vec4(xCenter + horMargin, yCenter + verMargin, 0.0f, 1.0f), extraData);
            vertices.emplace_back(tcu::Vec4(xCenter, yCenter - verMargin, 0.0f, 1.0f), extraData);
        }

    const auto vertexBufferSize           = static_cast<VkDeviceSize>(de::dataSize(vertices));
    const auto vertexBufferUsage          = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    const auto vertexBufferInfo           = makeBufferCreateInfo(vertexBufferSize, vertexBufferUsage);
    const VkDeviceSize vertexBufferOffset = 0ull;

    BufferWithMemory vertexBuffer(ctx.vkd, ctx.device, ctx.allocator, vertexBufferInfo, MemoryRequirement::HostVisible);
    auto &vertexBufferAlloc = vertexBuffer.getAllocation();
    void *vertexBufferData  = vertexBufferAlloc.getHostPtr();

    deMemcpy(vertexBufferData, de::dataOrNull(vertices), de::dataSize(vertices));

    // Divide pixels in chunks of pseudorandom sizes.
    std::vector<uint32_t> chunkSizes(kChunkCount, 0u);
    {
        uint32_t total = 0u;
        for (uint32_t i = 0u; i < kChunkCount - 1u; ++i)
        {
            const uint32_t chunkSize = static_cast<uint32_t>(rnd.getInt(1, chunkMaxPixels));
            chunkSizes.at(i)         = chunkSize;
            total += chunkSize;
        }
        // Last chunk contains the remaining pixels.
        chunkSizes.back() = pixelCountU - total;
    }

    // Draw operation per chunk.
    std::vector<VkDrawIndirectCommand> chunkDraws;
    std::vector<VkDrawIndexedIndirectCommand> chunkIndexedDraws;

    if (params.testType == TestType::DRAW_COUNT)
        chunkDraws.reserve(kChunkCount);
    else if (params.indexedDraws())
        chunkIndexedDraws.reserve(kChunkCount);
    else
        DE_ASSERT(false);

    {
        const uint32_t firstInstanceStart = 0u;
        const uint32_t firstInstanceStep  = 16u;
        const int maxInstanceCount        = 16u;
        RangeGen firstInstanceRange(firstInstanceStart, firstInstanceStep);

        std::vector<uint32_t> firstInstances(16u, 0u);
        std::iota(begin(firstInstances), end(firstInstances), firstInstanceRange);

        uint32_t prevPixels = 0u;
        for (uint32_t i = 0u; i < kChunkCount; ++i)
        {
            const auto &chunkSize = chunkSizes.at(i);

            const auto vertexCount   = chunkSize * kPerTriangleVertices;
            const auto instanceCount = static_cast<uint32_t>(rnd.getInt(1, maxInstanceCount));
            const auto firstVertex   = prevPixels * kPerTriangleVertices;
            const auto firstInstance = firstInstances.at(rnd.getInt(0, static_cast<int>(firstInstances.size() - 1)));
            const auto chunkOffset   = kVertexChunkOffset + i;

            if (params.testType == TestType::DRAW_COUNT)
            {
                const VkDrawIndirectCommand cmd{
                    vertexCount,
                    instanceCount,
                    firstVertex,
                    firstInstance,
                };
                chunkDraws.push_back(cmd);
            }
            else if (params.indexedDraws())
            {
                const VkDrawIndexedIndirectCommand cmd{
                    vertexCount,                        //  uint32_t    indexCount;
                    instanceCount,                      //  uint32_t    instanceCount;
                    firstVertex,                        //  uint32_t    firstIndex;
                    -static_cast<int32_t>(chunkOffset), //  int32_t     vertexOffset;
                    firstInstance,                      //  uint32_t    firstInstance;
                };
                chunkIndexedDraws.push_back(cmd);
            }
            else
                DE_ASSERT(false);

            prevPixels += chunkSize;
        }
    }

    // Create indirect buffers for the sequences.
    std::vector<SequenceInfo> sequenceInfos;
    sequenceInfos.reserve(kSequenceCount);

    {
        uint32_t prevChunks = 0u;

        for (uint32_t i = 0u; i < kSequenceCount; ++i)
        {
            sequenceInfos.emplace_back();
            auto &seqInfo = sequenceInfos.back();

            const auto seqChunks = ((i < kSequenceCount - 1u) ? static_cast<uint32_t>(rnd.getInt(1, maxIndirectDraws)) :
                                                                (kChunkCount - prevChunks));
            const auto extraPadding = static_cast<uint32_t>(rnd.getInt(0, 7));
            const auto totalStructs = extraPadding + 1u;
            const auto structSize   = ((params.testType == TestType::DRAW_COUNT) ? sizeof(VkDrawIndirectCommand) :
                                                                                   sizeof(VkDrawIndexedIndirectCommand));
            const auto stride       = totalStructs * structSize;
            const auto bufferSize   = stride * seqChunks;

            seqInfo.chunkCount = seqChunks;
            seqInfo.stride     = static_cast<uint32_t>(stride);
            seqInfo.buffer.reset(new DGCBuffer(ctx.vkd, ctx.device, ctx.allocator, bufferSize));

            // Copy indirect commands to the buffer.
            auto &bufferAlloc = seqInfo.buffer->getAllocation();
            char *bufferData  = reinterpret_cast<char *>(bufferAlloc.getHostPtr());

            deMemset(bufferData, 0, static_cast<size_t>(seqInfo.buffer->getSize()));
            uint32_t vertexCount = 0u;

            for (uint32_t j = 0; j < seqInfo.chunkCount; ++j)
            {
                const auto chunkIdx = prevChunks + j;
                const auto dstPtr   = bufferData + stride * j;
                const auto srcPtr   = ((params.testType == TestType::DRAW_COUNT) ?
                                           reinterpret_cast<const void *>(&chunkDraws.at(chunkIdx)) :
                                           reinterpret_cast<const void *>(&chunkIndexedDraws.at(chunkIdx)));
                const auto chunkVertexCount =
                    ((params.testType == TestType::DRAW_COUNT) ? chunkDraws.at(chunkIdx).vertexCount :
                                                                 chunkIndexedDraws.at(chunkIdx).indexCount);
                deMemcpy(dstPtr, srcPtr, structSize);
                vertexCount += chunkVertexCount;
            }

            seqInfo.vertexCount = vertexCount;
            prevChunks += seqChunks;
        }
    }

    // Index buffer if needed. For indexed draws, we're going to draw vertices
    // in reverse order, which means storing indices in reverse order in the
    // index buffer. In addition, to check that vertexOffset is correctly read
    // per draw, we're going to apply an offset to the index values stored in
    // each chunk, with the offset being slightly different in each chunk.
    std::vector<uint32_t> indices;
    std::vector<BufferWithMemoryPtr> indexBuffers;
    const VkBufferUsageFlags extraIndexBufferFlags =
        (params.indexBufferToken() ? (VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) :
                                     0u);
    const MemoryRequirement extraIndexBufferMemReqs =
        (params.indexBufferToken() ? MemoryRequirement::DeviceAddress : MemoryRequirement::Any);

    if (params.indexedDraws())
    {
        // Indices in reverse order.
        indices.reserve(vertices.size());

        uint32_t processedCount = 0u;
        for (size_t i = 0u; i < chunkSizes.size(); ++i)
        {
            const auto chunkSize         = chunkSizes.at(i);
            const auto chunkVertexCount  = chunkSize * kPerTriangleVertices;
            const auto chunkVertexOffset = static_cast<uint32_t>(kVertexChunkOffset + i); // Varies a bit per chunk.

            for (uint32_t j = 0u; j < chunkVertexCount; ++j)
            {
                const auto forwardIndex = processedCount + j;
                const auto reverseIndex = static_cast<uint32_t>(vertices.size() - 1u) - forwardIndex;
                const auto storedIndex  = reverseIndex + chunkVertexOffset;

                indices.push_back(storedIndex);
            }

            processedCount += chunkVertexCount;
        }

        DE_ASSERT(vertices.size() == indices.size());

        const auto indexBufferSize  = static_cast<VkDeviceSize>(de::dataSize(indices));
        const auto indexBufferUsage = (VK_BUFFER_USAGE_INDEX_BUFFER_BIT | extraIndexBufferFlags);
        const auto indexBufferInfo  = makeBufferCreateInfo(indexBufferSize, indexBufferUsage);

        // Store indices in one or more index buffers. When using index buffers, all buffers will be the same size but
        // each buffer will only contain the appropriate chunks of real data and the rest will be zero-ed out.
        const std::vector<uint32_t> singleSeqVertCount{pixelCountU * kPerTriangleVertices};
        std::vector<uint32_t> multiSeqVertCount(kSequenceCount, 0u);
        std::transform(begin(sequenceInfos), end(sequenceInfos), begin(multiSeqVertCount),
                       [](const SequenceInfo &s) { return s.vertexCount; });

        const auto &indexChunks =
            ((params.testType == TestType::DRAW_INDEXED_COUNT) ? singleSeqVertCount : multiSeqVertCount);

        processedCount = 0u;
        for (uint32_t i = 0u; i < de::sizeU32(indexChunks); ++i)
        {
            const auto chunkIndexCount = indexChunks.at(i);

            indexBuffers.emplace_back(new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, indexBufferInfo,
                                                           (MemoryRequirement::HostVisible | extraIndexBufferMemReqs)));
            auto &indexBuffer        = indexBuffers.back();
            auto &indexBufferAlloc   = indexBuffer->getAllocation();
            char *indexBufferBasePtr = reinterpret_cast<char *>(indexBufferAlloc.getHostPtr());

            // Zero-out the whole buffer first.
            deMemset(indexBufferBasePtr, 0, de::dataSize(indices));

            // Copy the chunk to its own index buffer.
            {
                const auto chunkSizeBytes = chunkIndexCount * DE_SIZEOF32(uint32_t);
                const auto srcPtr         = &indices.at(processedCount);
                const auto dstPtr         = indexBufferBasePtr + processedCount * DE_SIZEOF32(uint32_t);
                deMemcpy(dstPtr, srcPtr, chunkSizeBytes);
            }

            processedCount += chunkIndexCount;
        }
    }

    // Create token data for the draw count tokens.
    std::vector<VkDrawIndirectCountIndirectCommandEXT> drawTokenData;
    drawTokenData.reserve(kSequenceCount);

    uint32_t maxDrawCount = 0u;
    for (uint32_t i = 0u; i < kSequenceCount; ++i)
    {
        const auto &seqInfo = sequenceInfos.at(i);

        drawTokenData.emplace_back(VkDrawIndirectCountIndirectCommandEXT{
            seqInfo.buffer->getDeviceAddress(),
            seqInfo.stride,
            seqInfo.chunkCount,
        });

        if (seqInfo.chunkCount > maxDrawCount)
            maxDrawCount = seqInfo.chunkCount;
    }
    if (rnd.getBool())
        maxDrawCount *= 2u;

    // Create token data for the index buffer tokens, if used.
    std::vector<VkBindIndexBufferIndirectCommandEXT> indexBufferTokenData;
    if (params.indexBufferToken())
    {
        for (uint32_t i = 0u; i < kSequenceCount; ++i)
        {
            indexBufferTokenData.push_back(VkBindIndexBufferIndirectCommandEXT{
                getBufferDeviceAddress(ctx.vkd, ctx.device, indexBuffers.at(i)->get()),
                static_cast<uint32_t>(de::dataSize(indices)),
                VK_INDEX_TYPE_UINT32,
            });
        }
    }

    // Color framebuffer.
    const auto colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const auto colorUsage =
        (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    ImageWithBuffer colorBuffer(ctx.vkd, ctx.device, ctx.allocator, vkExtent, colorFormat, colorUsage,
                                VK_IMAGE_TYPE_2D);
    const auto colorSRR = makeDefaultImageSubresourceRange();

    const std::vector<VkViewport> viewports(1u, makeViewport(vkExtent));
    const std::vector<VkRect2D> scissors(1u, makeRect2D(vkExtent));

    Move<VkRenderPass> renderPass;
    Move<VkFramebuffer> framebuffer;

    if (!params.useShaderObjects)
    {
        renderPass  = makeRenderPass(ctx.vkd, ctx.device, colorFormat);
        framebuffer = makeFramebuffer(ctx.vkd, ctx.device, *renderPass, colorBuffer.getImageView(), vkExtent.width,
                                      vkExtent.height);
    }

    // Input buffers. Used with execution sets.
    const auto inputBuffers     = (params.useExecutionSet ? getInputBuffers() : BufferDataVec());
    const auto inputBufferCount = de::sizeU32(inputBuffers);

    Move<VkDescriptorSetLayout> vertSetLayout;
    Move<VkDescriptorSetLayout> fragSetLayout;
    std::vector<VkDescriptorSetLayout> setLayouts;
    std::vector<VkPushConstantRange> pcRanges;

    Move<VkDescriptorPool> descriptorPool;
    Move<VkDescriptorSet> vertDescSet;
    Move<VkDescriptorSet> fragDescSet;

    BufferVec vertBuffers;
    BufferVec fragBuffers;

    // Only used with execution sets.
    const auto pcSize   = DE_SIZEOF32(tcu::UVec2);
    const auto pcStages = static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_FRAGMENT_BIT);
    const auto pcRange  = makePushConstantRange(pcStages, 0u, pcSize);
    const auto pcData   = fbExtent.asUint().swizzle(0, 1);

    if (params.useExecutionSet || params.checkDrawParams)
    {
        uint32_t fragBufferCount = 0u;
        uint32_t vertBufferCount = 0u;

        // Frag shader will always use set 1, so set 0 can be empty.
        {
            DescriptorSetLayoutBuilder vertLayoutBuilder;
            for (uint32_t i = 0u; i < inputBufferCount; ++i)
            {
                if (params.useExecutionSet)
                    vertLayoutBuilder.addSingleBinding(descType, VK_SHADER_STAGE_VERTEX_BIT);
            }
            vertSetLayout   = vertLayoutBuilder.build(ctx.vkd, ctx.device);
            vertBufferCount = inputBufferCount;
        }

        DescriptorSetLayoutBuilder fragLayoutBuilder;
        if (params.useExecutionSet)
        {
            fragLayoutBuilder.addSingleBinding(descType, VK_SHADER_STAGE_FRAGMENT_BIT);
            ++fragBufferCount;
        }
        if (params.checkDrawParams)
        {
            fragLayoutBuilder.addSingleBinding(descType, VK_SHADER_STAGE_FRAGMENT_BIT);
            ++fragBufferCount;
        }
        fragSetLayout = fragLayoutBuilder.build(ctx.vkd, ctx.device);

        setLayouts.push_back(*vertSetLayout);
        setLayouts.push_back(*fragSetLayout);
        pcRanges.push_back(pcRange);

        DescriptorPoolBuilder poolBuilder;
        poolBuilder.addType(descType, vertBufferCount + fragBufferCount);
        descriptorPool =
            poolBuilder.build(ctx.vkd, ctx.device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, kPipelineShaders);

        if (params.useExecutionSet)
            vertDescSet = makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *vertSetLayout);
        fragDescSet = makeDescriptorSet(ctx.vkd, ctx.device, *descriptorPool, *fragSetLayout);

        const auto bufferUsage = static_cast<VkBufferUsageFlags>(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        for (uint32_t i = 0u; i < inputBufferCount; ++i)
        {
            const auto &inputBuffer = inputBuffers.at(i);
            const auto bufferSize   = static_cast<VkDeviceSize>(de::dataSize(inputBuffer));
            const auto createInfo   = makeBufferCreateInfo(bufferSize, bufferUsage);
            vertBuffers.emplace_back(
                new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, createInfo, MemoryRequirement::HostVisible));

            auto &bufferAlloc   = vertBuffers.back()->getAllocation();
            void *bufferDataPtr = bufferAlloc.getHostPtr();
            deMemcpy(bufferDataPtr, de::dataOrNull(inputBuffer), de::dataSize(inputBuffer));
        }

        if (params.useExecutionSet)
        {
            // Calculate expected accumulated values.
            std::vector<int32_t> expectedAccums(pixelCountU,
                                                0); // Accumulated values for each pixel (this goes into the buffer).
            std::vector<int32_t> bufferAccums(inputBuffers.size(), 0); // Accumulated values for each input buffer.
            std::vector<uint32_t> seqSizesInPixels(inputBuffers.size(), 0u); // Number of pixels in each sequence.

            uint32_t prevChunks = 0u;
            for (size_t seqIdx = 0u; seqIdx < sequenceInfos.size(); ++seqIdx)
            {
                const auto &seqInfo = sequenceInfos.at(seqIdx);
                uint32_t seqPixels  = 0u;

                for (uint32_t i = 0u; i < seqInfo.chunkCount; ++i)
                {
                    const auto chunkIdx   = prevChunks + i;
                    const auto pixelCount = chunkSizes.at(chunkIdx);

                    seqPixels += pixelCount;
                }

                seqSizesInPixels.at(seqIdx) = seqPixels;
                prevChunks += seqInfo.chunkCount;
            }

            for (size_t i = 0u; i < inputBuffers.size(); ++i)
            {
                const auto &inputBuffer = inputBuffers.at(i);
                bufferAccums.at(i)      = std::accumulate(begin(inputBuffer), end(inputBuffer), 0);
            }

            // Using the accumulated values for each input buffer and the number of
            // pixels in each sequence, set the expected accumulated value in each
            // pixel.
            uint32_t prevPixels = 0u;
            for (size_t i = 0u; i < seqSizesInPixels.size(); ++i)
            {
                const auto &seqPixels = seqSizesInPixels.at(i);
                for (uint32_t j = 0u; j < seqPixels; ++j)
                {
                    const auto pixelIdx         = prevPixels + j;
                    expectedAccums.at(pixelIdx) = bufferAccums.at(i);
                }
                prevPixels += seqPixels;
            }

            // Indexed draws happen in reverse order.
            if (params.indexedDraws())
                std::reverse(begin(expectedAccums), end(expectedAccums));

            const auto bufferSize = static_cast<VkDeviceSize>(de::dataSize(expectedAccums));
            const auto createInfo = makeBufferCreateInfo(bufferSize, bufferUsage);
            fragBuffers.emplace_back(
                new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, createInfo, MemoryRequirement::HostVisible));

            auto &bufferAlloc   = fragBuffers.back()->getAllocation();
            void *bufferDataPtr = bufferAlloc.getHostPtr();
            deMemcpy(bufferDataPtr, de::dataOrNull(expectedAccums), de::dataSize(expectedAccums));
        }

        if (params.checkDrawParams)
        {
            std::vector<tcu::IVec4> expectedDrawIndices;
            expectedDrawIndices.reserve(pixelCountU);

            uint32_t prevChunks = 0u;
            for (uint32_t i = 0u; i < kSequenceCount; ++i)
            {
                uint32_t drawIdx    = 0u; // Resets at the start of each sequence.
                const auto &seqInfo = sequenceInfos.at(i);

                for (uint32_t j = 0u; j < seqInfo.chunkCount; ++j)
                {
                    const auto chunkIdx  = prevChunks + j;
                    const auto chunkSize = chunkSizes.at(chunkIdx);
                    const auto baseVertex =
                        (params.testType == TestType::DRAW_COUNT ? chunkDraws.at(chunkIdx).firstVertex :
                                                                   chunkIndexedDraws.at(chunkIdx).vertexOffset);
                    const auto baseInstance =
                        (params.testType == TestType::DRAW_COUNT ? chunkDraws.at(chunkIdx).firstInstance :
                                                                   chunkIndexedDraws.at(chunkIdx).firstInstance);

                    for (uint32_t k = 0u; k < chunkSize; ++k)
                        expectedDrawIndices.push_back(tcu::UVec4(drawIdx, baseVertex, baseInstance, 0).asInt());

                    ++drawIdx; // Increases with each draw.
                }

                prevChunks += seqInfo.chunkCount;
            }

            // Indexed draws happen in reverse order.
            if (params.indexedDraws())
                std::reverse(begin(expectedDrawIndices), end(expectedDrawIndices));

            const auto bufferSize = static_cast<VkDeviceSize>(de::dataSize(expectedDrawIndices));
            const auto createInfo = makeBufferCreateInfo(bufferSize, bufferUsage);
            fragBuffers.emplace_back(
                new BufferWithMemory(ctx.vkd, ctx.device, ctx.allocator, createInfo, MemoryRequirement::HostVisible));

            auto &bufferAlloc   = fragBuffers.back()->getAllocation();
            void *bufferDataPtr = bufferAlloc.getHostPtr();
            deMemcpy(bufferDataPtr, de::dataOrNull(expectedDrawIndices), de::dataSize(expectedDrawIndices));
        }

        // Update descriptors with each buffer.
        DescriptorSetUpdateBuilder updateBuilder;
        using Location = DescriptorSetUpdateBuilder::Location;

        for (uint32_t i = 0u; i < de::sizeU32(vertBuffers); ++i)
        {
            const auto bufferInfo = makeDescriptorBufferInfo(vertBuffers.at(i)->get(), 0ull, VK_WHOLE_SIZE);
            updateBuilder.writeSingle(*vertDescSet, Location::binding(i), descType, &bufferInfo);
        }
        for (uint32_t i = 0u; i < de::sizeU32(fragBuffers); ++i)
        {
            const auto bufferInfo = makeDescriptorBufferInfo(fragBuffers.at(i)->get(), 0ull, VK_WHOLE_SIZE);
            updateBuilder.writeSingle(*fragDescSet, Location::binding(i), descType, &bufferInfo);
        }
        updateBuilder.update(ctx.vkd, ctx.device);
    }

    const auto pipelineLayout =
        makePipelineLayout(ctx.vkd, ctx.device, de::sizeU32(setLayouts), de::dataOrNull(setLayouts),
                           de::sizeU32(pcRanges), de::dataOrNull(pcRanges));

    // Shader modules.
    const auto &binaries      = context.getBinaryCollection();
    const auto shaderSetCount = (params.useExecutionSet ? kSequenceCount : 1u);

    using ModuleVec = std::vector<Move<VkShaderModule>>;
    ModuleVec vertModules;
    ModuleVec fragModules;

    using ShaderVec = std::vector<Move<VkShaderEXT>>;
    ShaderVec vertShaders;
    ShaderVec fragShaders;

    using DGCShaderExtPtr = std::unique_ptr<DGCShaderExt>;
    using DGCShaderVec    = std::vector<DGCShaderExtPtr>;
    DGCShaderVec vertShadersDGC;
    DGCShaderVec fragShadersDGC;

    const auto &meshFeatures = context.getMeshShaderFeaturesEXT();
    const auto &features     = context.getDeviceFeatures();

    const auto tessFeature = (features.tessellationShader == VK_TRUE);
    const auto geomFeature = (features.geometryShader == VK_TRUE);

    if (!params.useShaderObjects)
    {
        vertModules.reserve(shaderSetCount);
        fragModules.reserve(shaderSetCount);

        for (uint32_t i = 0u; i < shaderSetCount; ++i)
        {
            const auto suffix   = (params.useExecutionSet ? std::to_string(i) : std::string());
            const auto vertName = "vert" + suffix;
            const auto fragName = "frag" + suffix;
            vertModules.push_back(createShaderModule(ctx.vkd, ctx.device, binaries.get(vertName)));
            fragModules.push_back(createShaderModule(ctx.vkd, ctx.device, binaries.get(fragName)));
        }
    }
    else
    {
        std::vector<VkDescriptorSetLayout> vertSetLayouts;
        std::vector<VkDescriptorSetLayout> fragSetLayouts;

        if (*vertSetLayout != VK_NULL_HANDLE)
        {
            vertSetLayouts.push_back(*vertSetLayout);
            fragSetLayouts.push_back(*vertSetLayout);
        }

        if (*fragSetLayout != VK_NULL_HANDLE)
            fragSetLayouts.push_back(*fragSetLayout);

        const std::vector<VkPushConstantRange> vertPCRanges;
        const std::vector<VkPushConstantRange> &fragPCRanges = pcRanges;

        // Otherwise we need to modify the vectors above.
        DE_ASSERT(pcStages == static_cast<VkShaderStageFlags>(VK_SHADER_STAGE_FRAGMENT_BIT));

        if (params.useExecutionSet)
        {
            vertShadersDGC.reserve(shaderSetCount);
            fragShadersDGC.reserve(shaderSetCount);
        }
        else
        {
            vertShaders.reserve(shaderSetCount);
            fragShaders.reserve(shaderSetCount);
        }

        for (uint32_t i = 0u; i < shaderSetCount; ++i)
        {
            const auto suffix   = (params.useExecutionSet ? std::to_string(i) : std::string());
            const auto vertName = "vert" + suffix;
            const auto fragName = "frag" + suffix;

            if (params.useExecutionSet)
            {
                vertShadersDGC.emplace_back(new DGCShaderExt(ctx.vkd, ctx.device, VK_SHADER_STAGE_VERTEX_BIT, 0u,
                                                             binaries.get(vertName), vertSetLayouts, vertPCRanges,
                                                             tessFeature, geomFeature));
                fragShadersDGC.emplace_back(new DGCShaderExt(ctx.vkd, ctx.device, VK_SHADER_STAGE_FRAGMENT_BIT, 0u,
                                                             binaries.get(fragName), fragSetLayouts, fragPCRanges,
                                                             tessFeature, geomFeature));
            }
            else
            {
                vertShaders.push_back(makeSingleShader(ctx.vkd, ctx.device, VK_SHADER_STAGE_VERTEX_BIT,
                                                       binaries.get(vertName), vertSetLayouts, vertPCRanges));
                fragShaders.push_back(makeSingleShader(ctx.vkd, ctx.device, VK_SHADER_STAGE_FRAGMENT_BIT,
                                                       binaries.get(fragName), fragSetLayouts, fragPCRanges));
            }
        }
    }

    const std::vector<VkVertexInputBindingDescription> vertexBindings{
        makeVertexInputBindingDescription(0u, DE_SIZEOF32(VertexData), VK_VERTEX_INPUT_RATE_VERTEX),
    };

    const std::vector<VkVertexInputAttributeDescription> vertexAttributes{
        makeVertexInputAttributeDescription(0u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT,
                                            static_cast<uint32_t>(offsetof(VertexData, position))),
        makeVertexInputAttributeDescription(1u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT,
                                            static_cast<uint32_t>(offsetof(VertexData, extraData))),
    };

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, //   VkStructureType                             sType;
        nullptr,                        //   const void*                                 pNext;
        0u,                             //   VkPipelineVertexInputStateCreateFlags       flags;
        de::sizeU32(vertexBindings),    //   uint32_t                                    vertexBindingDescriptionCount;
        de::dataOrNull(vertexBindings), //   const VkVertexInputBindingDescription*      pVertexBindingDescriptions;
        de::sizeU32(vertexAttributes), //   uint32_t                                    vertexAttributeDescriptionCount;
        de::dataOrNull(vertexAttributes), //   const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions;
    };

    std::vector<Move<VkPipeline>> pipelines;

    if (!params.useShaderObjects)
    {
        for (uint32_t i = 0u; i < shaderSetCount; ++i)
        {
            const auto createFlags =
                static_cast<VkPipelineCreateFlags2KHR>(VK_PIPELINE_CREATE_2_INDIRECT_BINDABLE_BIT_EXT);

            const VkPipelineCreateFlags2CreateInfoKHR pipelineCreateFlags = {
                VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO_KHR, //   VkStructureType             sType;
                nullptr,                                                   //   const void*                 pNext;
                createFlags,                                               //   VkPipelineCreateFlags2KHR   flags;
            };

            const void *pNext = (params.useExecutionSet ? &pipelineCreateFlags : nullptr);

            pipelines.push_back(
                makeGraphicsPipeline(ctx.vkd, ctx.device, *pipelineLayout, *vertModules.at(i), VK_NULL_HANDLE,
                                     VK_NULL_HANDLE, VK_NULL_HANDLE, *fragModules.at(i), *renderPass, viewports,
                                     scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0u, 0u, &vertexInputStateCreateInfo,
                                     nullptr, nullptr, nullptr, nullptr, nullptr, pNext, 0u));
        }
    }

    // Indirect commands layout.
    VkIndirectCommandsLayoutUsageFlagsEXT cmdsLayoutFlags = 0u;

    if (params.doPreprocess())
        cmdsLayoutFlags |= VK_INDIRECT_COMMANDS_LAYOUT_USAGE_EXPLICIT_PREPROCESS_BIT_EXT;

    if (params.unorderedSequences)
        cmdsLayoutFlags |= VK_INDIRECT_COMMANDS_LAYOUT_USAGE_UNORDERED_SEQUENCES_BIT_EXT;

    // We do not pass the pipeline layout because we don't have push constants or sequence index tokens.
    IndirectCommandsLayoutBuilderExt cmdsLayoutBuilder(cmdsLayoutFlags, stageFlags, VK_NULL_HANDLE);

    if (params.useExecutionSet)
    {
        const auto executionSetType =
            (params.useShaderObjects ? VK_INDIRECT_EXECUTION_SET_INFO_TYPE_SHADER_OBJECTS_EXT :
                                       VK_INDIRECT_EXECUTION_SET_INFO_TYPE_PIPELINES_EXT);
        cmdsLayoutBuilder.addExecutionSetToken(cmdsLayoutBuilder.getStreamRange(), executionSetType, stageFlags);
    }

    if (params.indexBufferToken())
        cmdsLayoutBuilder.addIndexBufferToken(cmdsLayoutBuilder.getStreamRange(),
                                              VK_INDIRECT_COMMANDS_INPUT_MODE_VULKAN_INDEX_BUFFER_EXT);

    if (params.testType == TestType::DRAW_COUNT)
        cmdsLayoutBuilder.addDrawCountToken(cmdsLayoutBuilder.getStreamRange());
    else if (params.indexedDraws())
        cmdsLayoutBuilder.addDrawIndexedCountToken(cmdsLayoutBuilder.getStreamRange());
    else
        DE_ASSERT(false);

    const auto cmdsLayout = cmdsLayoutBuilder.build(ctx.vkd, ctx.device);

    // Indirect execution set, if needed.
    ExecutionSetManagerPtr executionSetManager;
    VkIndirectExecutionSetEXT indirectExecutionSet = VK_NULL_HANDLE;

    if (params.useExecutionSet)
    {
        if (params.useShaderObjects)
        {
            const std::vector<VkDescriptorSetLayout> vertSetLayouts{*vertSetLayout};
            const std::vector<VkDescriptorSetLayout> fragSetLayouts{VK_NULL_HANDLE, *fragSetLayout};

            const std::vector<IESStageInfo> stagesInfo = {
                IESStageInfo{vertShadersDGC.at(0u)->get(), vertSetLayouts},
                IESStageInfo{fragShadersDGC.at(0u)->get(), fragSetLayouts},
            };

            executionSetManager = makeExecutionSetManagerShader(ctx.vkd, ctx.device, stagesInfo, pcRanges,
                                                                shaderSetCount * kPipelineShaders);

            // Note we start at 1 and rely on the initial entry set above.
            for (uint32_t i = 1u; i < shaderSetCount; ++i)
            {
                executionSetManager->addShader(i * kPipelineShaders + 0u, vertShadersDGC.at(i)->get());
                executionSetManager->addShader(i * kPipelineShaders + 1u, fragShadersDGC.at(i)->get());
            }
            executionSetManager->update();
            indirectExecutionSet = executionSetManager->get();
        }
        else
        {
            executionSetManager =
                makeExecutionSetManagerPipeline(ctx.vkd, ctx.device, *pipelines.at(0u), kSequenceCount);
            for (uint32_t i = 0u; i < shaderSetCount; ++i)
                executionSetManager->addPipeline(i, *pipelines.at(i));
            executionSetManager->update();
            indirectExecutionSet = executionSetManager->get();
        }
    }

    // DGC buffer contents.
    std::vector<uint32_t> dgcData;
    dgcData.reserve((kSequenceCount * cmdsLayoutBuilder.getStreamStride()) / sizeof(uint32_t));

    for (uint32_t i = 0u; i < kSequenceCount; ++i)
    {
        if (params.useExecutionSet)
        {
            if (params.useShaderObjects)
            {
                pushBackElement(dgcData, i * kPipelineShaders + 0u);
                pushBackElement(dgcData, i * kPipelineShaders + 1u);
            }
            else
                pushBackElement(dgcData, i);
        }
        if (params.indexBufferToken())
            pushBackElement(dgcData, indexBufferTokenData.at(i));
        pushBackElement(dgcData, drawTokenData.at(i));
    }

    // DGC buffer with those contents.
    const auto dgcBufferSize = static_cast<VkDeviceSize>(de::dataSize(dgcData));
    DGCBuffer dgcBuffer(ctx.vkd, ctx.device, ctx.allocator, dgcBufferSize);
    auto &dgcBufferAlloc = dgcBuffer.getAllocation();
    void *dgcBufferData  = dgcBufferAlloc.getHostPtr();

    deMemcpy(dgcBufferData, de::dataOrNull(dgcData), de::dataSize(dgcData));

    // Preprocess buffer.
    const auto prepPipeline =
        (indirectExecutionSet == VK_NULL_HANDLE && !params.useShaderObjects ? *pipelines.at(0u) : VK_NULL_HANDLE);

    std::vector<VkShaderEXT> prepShaders;
    if (indirectExecutionSet == VK_NULL_HANDLE && params.useShaderObjects)
    {
        prepShaders.push_back(*vertShaders.at(0));
        prepShaders.push_back(*fragShaders.at(0));
    }
    const std::vector<VkShaderEXT> *shadersVecPtr = (prepShaders.empty() ? nullptr : &prepShaders);
    PreprocessBufferExt preprocessBuffer(ctx.vkd, ctx.device, ctx.allocator, indirectExecutionSet, *cmdsLayout,
                                         kSequenceCount, maxDrawCount, prepPipeline, shadersVecPtr);

    // Command pool and buffer.
    CommandPoolWithBuffer cmd(ctx.vkd, ctx.device, ctx.qfIndex);
    const auto cmdBuffer = *cmd.cmdBuffer;

    const tcu::Vec4 fbClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    // Generated commands info.
    const DGCGenCmdsInfo cmdsInfo(
        stageFlags,                          //   VkShaderStageFlags          shaderStages;
        indirectExecutionSet,                //   VkIndirectExecutionSetEXT   indirectExecutionSet;
        *cmdsLayout,                         //   VkIndirectCommandsLayoutEXT indirectCommandsLayout;
        dgcBuffer.getDeviceAddress(),        //   VkDeviceAddress             indirectAddress;
        dgcBuffer.getSize(),                 //   VkDeviceSize                indirectAddressSize;
        preprocessBuffer.getDeviceAddress(), //   VkDeviceAddress             preprocessAddress;
        preprocessBuffer.getSize(),          //   VkDeviceSize                preprocessSize;
        kSequenceCount,                      //   uint32_t                    maxSequenceCount;
        0ull,                                //   VkDeviceAddress             sequenceCountAddress;
        pixelCountU,                         //   uint32_t                    maxDrawCount;
        prepPipeline, shadersVecPtr);

    // When preprocessing, we need to use a command buffer to record state.
    // The preprocessing step needs to happen outside the render pass.
    Move<VkCommandBuffer> separateStateCmdBuffer;

    // A command buffer we want to record state into.
    // .first is the command buffer itself.
    // .second, if not NULL, means we'll record a preprocess command with it as the state command buffer.
    using StateCmdBuffer                 = std::pair<VkCommandBuffer, VkCommandBuffer>;
    const VkCommandBuffer kNullCmdBuffer = VK_NULL_HANDLE; // Workaround for types and emplace_back below.
    std::vector<StateCmdBuffer> stateCmdBuffers;

    // Sequences and iterations for the different cases:
    //     - PreprocessType::NONE
    //         - Only one loop iteration.
    //         - Iteration 0: .first = main cmd buffer, .second = NULL
    //             - No preprocess, bind state
    //         - Execute.
    //     - PreprocessType::OTHER_STATE_CMD_BUFFER
    //         - Iteration 0: .first = state cmd buffer, .second = NULL
    //             - No preprocess, bind state
    //         - Iteration 1: .first = main cmd buffer, .second = state cmd buffer
    //             - Preprocess with state cmd buffer, bind state on main
    //         - Execute.
    //     - PreprocessType::SAME_STATE_CMD_BUFFER
    //         - Iteration 0: .first = main cmd buffer, .second = NULL
    //             - No preprocess, bind state
    //         - Iteration 1: .first = main cmd buffer, .second = main cmd buffer
    //             - Preprocess with main cmd buffer, break
    //         - Execute.
    switch (params.preprocessType)
    {
    case PreprocessType::NONE:
        stateCmdBuffers.emplace_back(cmdBuffer, kNullCmdBuffer);
        break;
    case PreprocessType::SAME_STATE_CMD_BUFFER:
        stateCmdBuffers.emplace_back(cmdBuffer, kNullCmdBuffer);
        stateCmdBuffers.emplace_back(cmdBuffer, cmdBuffer);
        break;
    case PreprocessType::OTHER_STATE_CMD_BUFFER:
        separateStateCmdBuffer =
            allocateCommandBuffer(ctx.vkd, ctx.device, *cmd.cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        stateCmdBuffers.emplace_back(*separateStateCmdBuffer, kNullCmdBuffer);
        stateCmdBuffers.emplace_back(cmdBuffer, *separateStateCmdBuffer);
        break;
    default:
        DE_ASSERT(false);
    }

    // Record pre-execution state to all needed command buffers.
    VkCommandBuffer prevCmdBuffer = VK_NULL_HANDLE;
    for (const auto &stateCmdBufferPair : stateCmdBuffers)
    {
        const auto &recCmdBuffer = stateCmdBufferPair.first;

        // Only begin each command buffer once.
        if (recCmdBuffer != prevCmdBuffer)
        {
            beginCommandBuffer(ctx.vkd, recCmdBuffer);
            prevCmdBuffer = recCmdBuffer;
        }

        if (stateCmdBufferPair.second != VK_NULL_HANDLE)
        {
            ctx.vkd.cmdPreprocessGeneratedCommandsEXT(recCmdBuffer, &cmdsInfo.get(), stateCmdBufferPair.second);
            separateStateCmdBuffer = Move<VkCommandBuffer>(); // Delete separate state command buffer right away.

            preprocessToExecuteBarrierExt(ctx.vkd, recCmdBuffer);

            // Break for iteration 1 of PreprocessType::SAME_STATE_CMD_BUFFER. See above.
            if (stateCmdBufferPair.first == stateCmdBufferPair.second)
                break;
        }

        if (params.useExecutionSet || params.checkDrawParams)
        {
            const std::vector<VkDescriptorSet> descriptorSets{*vertDescSet, *fragDescSet};
            ctx.vkd.cmdBindDescriptorSets(recCmdBuffer, bindPoint, *pipelineLayout, 0u, de::sizeU32(descriptorSets),
                                          de::dataOrNull(descriptorSets), 0u, nullptr);
            ctx.vkd.cmdPushConstants(recCmdBuffer, *pipelineLayout, pcStages, 0u, pcSize, &pcData);
        }

        ctx.vkd.cmdBindVertexBuffers(recCmdBuffer, 0u, 1u, &vertexBuffer.get(), &vertexBufferOffset);
        if (params.testType == TestType::DRAW_INDEXED_COUNT)
            ctx.vkd.cmdBindIndexBuffer(recCmdBuffer, indexBuffers.at(0u)->get(), 0ull, VK_INDEX_TYPE_UINT32);

        if (!params.useShaderObjects)
            ctx.vkd.cmdBindPipeline(recCmdBuffer, bindPoint, *pipelines.at(0u)); // Execution set or not.
        else
        {
            std::map<VkShaderStageFlagBits, VkShaderEXT> boundShaders;
            if (meshFeatures.meshShader)
                boundShaders[VK_SHADER_STAGE_MESH_BIT_EXT] = VK_NULL_HANDLE;
            if (meshFeatures.taskShader)
                boundShaders[VK_SHADER_STAGE_TASK_BIT_EXT] = VK_NULL_HANDLE;
            if (features.tessellationShader)
            {
                boundShaders[VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT]    = VK_NULL_HANDLE;
                boundShaders[VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT] = VK_NULL_HANDLE;
            }
            if (features.geometryShader)
                boundShaders[VK_SHADER_STAGE_GEOMETRY_BIT] = VK_NULL_HANDLE;

            if (params.useExecutionSet)
            {
                boundShaders[VK_SHADER_STAGE_VERTEX_BIT]   = vertShadersDGC.at(0u)->get();
                boundShaders[VK_SHADER_STAGE_FRAGMENT_BIT] = fragShadersDGC.at(0u)->get();
            }
            else
            {
                boundShaders[VK_SHADER_STAGE_VERTEX_BIT]   = *vertShaders.at(0u);
                boundShaders[VK_SHADER_STAGE_FRAGMENT_BIT] = *fragShaders.at(0u);
            }

            {
                std::vector<VkShaderStageFlagBits> stages;
                std::vector<VkShaderEXT> shaders;

                stages.reserve(boundShaders.size());
                shaders.reserve(boundShaders.size());

                for (const auto &stageShader : boundShaders)
                {
                    stages.push_back(stageShader.first);
                    shaders.push_back(stageShader.second);
                }

                DE_ASSERT(shaders.size() == stages.size());
                ctx.vkd.cmdBindShadersEXT(recCmdBuffer, de::sizeU32(shaders), de::dataOrNull(stages),
                                          de::dataOrNull(shaders));
            }
        }

        if (params.useShaderObjects)
            bindShaderObjectState(ctx.vkd, getDeviceCreationExtensions(context), recCmdBuffer, viewports, scissors,
                                  VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0u, &vertexInputStateCreateInfo, nullptr,
                                  nullptr, nullptr, nullptr);
    }

    if (params.useShaderObjects)
    {
        const auto clearColor = makeClearValueColor(fbClearColor);
        const auto preClearBarrier =
            makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, colorBuffer.getImage(), colorSRR);
        const auto postClearBarrier = makeImageMemoryBarrier(
            VK_ACCESS_TRANSFER_WRITE_BIT, (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, colorBuffer.getImage(),
            colorSRR);

        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT, &preClearBarrier);
        ctx.vkd.cmdClearColorImage(cmdBuffer, colorBuffer.getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   &clearColor.color, 1u, &colorSRR);
        cmdPipelineImageMemoryBarrier(ctx.vkd, cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, &postClearBarrier);
        beginRendering(ctx.vkd, cmdBuffer, colorBuffer.getImageView(), scissors.at(0u), clearColor /*not used*/,
                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    }
    else
        beginRenderPass(ctx.vkd, cmdBuffer, *renderPass, *framebuffer, scissors.at(0u), fbClearColor);

    {
        const VkBool32 isPreprocessed = makeVkBool(params.doPreprocess());
        ctx.vkd.cmdExecuteGeneratedCommandsEXT(cmdBuffer, isPreprocessed, &cmdsInfo.get());
    }

    if (params.useShaderObjects)
        endRendering(ctx.vkd, cmdBuffer);
    else
        endRenderPass(ctx.vkd, cmdBuffer);

    copyImageToBuffer(ctx.vkd, cmdBuffer, colorBuffer.getImage(), colorBuffer.getBuffer(), fbExtent.swizzle(0, 1));
    endCommandBuffer(ctx.vkd, cmdBuffer);
    submitCommandsAndWait(ctx.vkd, ctx.device, ctx.queue, cmdBuffer);

    // Generate reference image.
    const auto tcuFormat = mapVkFormat(colorFormat);
    tcu::TextureLevel refLevel(tcuFormat, fbExtent.x(), fbExtent.y(), fbExtent.z());
    auto refAccess = refLevel.getAccess();

    const auto maxInstanceIndex = static_cast<float>(kMaxInstanceIndex);
    const bool indexed          = (params.indexedDraws());
    const auto totalDraws       = (indexed ? chunkIndexedDraws.size() : chunkDraws.size());
    uint32_t prevPixels         = 0u;

    for (size_t drawIdx = 0u; drawIdx < totalDraws; ++drawIdx)
    {
        const auto vertexCount =
            (indexed ? chunkIndexedDraws.at(drawIdx).indexCount : chunkDraws.at(drawIdx).vertexCount);
        const auto firstInstance =
            (indexed ? chunkIndexedDraws.at(drawIdx).firstInstance : chunkDraws.at(drawIdx).firstInstance);
        const auto instanceCount =
            (indexed ? chunkIndexedDraws.at(drawIdx).instanceCount : chunkDraws.at(drawIdx).instanceCount);

        DE_ASSERT(vertexCount % kPerTriangleVertices == 0u);
        const auto chunkPixels = vertexCount / kPerTriangleVertices;

        for (uint32_t i = 0u; i < chunkPixels; ++i)
        {
            const auto curPixel   = prevPixels + i;
            const auto pixelIdx   = (indexed ? (pixelCountU - 1u - curPixel) : curPixel); // Reversed for indexed draws.
            const auto row        = static_cast<int>(pixelIdx / vkExtent.width);
            const auto col        = static_cast<int>(pixelIdx % vkExtent.width);
            const auto redValue   = static_cast<float>(firstInstance + (instanceCount - 1u)) / maxInstanceIndex;
            const auto &extraData = vertices.at(pixelIdx * kPerTriangleVertices).extraData;
            const bool blank = (extraData.x() < 0.0f || extraData.y() < 0.0f); // Filtered by clip or cull distance.

            const tcu::Vec4 color(redValue, 0.0f, 1.0f, 1.0f);
            refAccess.setPixel((blank ? fbClearColor : color), col, row);
        }
        prevPixels += chunkPixels;
    }

    // Reference access.
    auto &colorAlloc = colorBuffer.getBufferAllocation();
    invalidateAlloc(ctx.vkd, ctx.device, colorAlloc);

    const tcu::ConstPixelBufferAccess resAccess(tcuFormat, fbExtent, colorAlloc.getHostPtr());

    const float colorThreshold = 0.005f; // 1/255 < 0.005f < 2/255.
    const tcu::Vec4 threshold(colorThreshold, colorThreshold, colorThreshold, colorThreshold);
    auto &log = context.getTestContext().getLog();
    if (!tcu::floatThresholdCompare(log, "Result", "", refAccess, resAccess, threshold, tcu::COMPARE_LOG_ON_ERROR))
        TCU_FAIL("Unexpected result found in color buffer; check log for details");

    return tcu::TestStatus::pass("Pass");
}

} // anonymous namespace

tcu::TestCaseGroup *createDGCGraphicsDrawCountTestsExt(tcu::TestContext &testCtx)
{
    GroupPtr mainGroup(new tcu::TestCaseGroup(testCtx, "draw_count"));

    GroupPtr drawCountGroup(new tcu::TestCaseGroup(testCtx, "token_draw_count"));
    GroupPtr drawIndexedCountGroup(new tcu::TestCaseGroup(testCtx, "token_draw_indexed_count"));

    const struct
    {
        PreprocessType preprocessType;
        const char *suffix;
    } preprocessCases[] = {
        {PreprocessType::NONE, ""},
        {PreprocessType::SAME_STATE_CMD_BUFFER, "_preprocess_same_state_cmd_buffer"},
        {PreprocessType::OTHER_STATE_CMD_BUFFER, "_preprocess_separate_state_cmd_buffer"},
    };

    const struct
    {
        TestType testType;
        const char *suffix;
    } testTypeCases[] = {
        {TestType::DRAW_COUNT, ""},
        {TestType::DRAW_INDEXED_COUNT, ""}, // Also no suffix but will go into a different test group.
        {TestType::DRAW_INDEXED_COUNT_INDEX_TOKEN, "_with_index_buffer_token"},
    };

    for (const auto &testTypeCase : testTypeCases)
        for (const bool executionSets : {false, true})
            for (const bool shaderObjects : {false, true})
                for (const auto &preProcessCase : preprocessCases)
                    for (const bool unordered : {false, true})
                        for (const bool checkDrawParams : {false, true})
                        {
                            const TestParams params{testTypeCase.testType, preProcessCase.preprocessType,
                                                    checkDrawParams,       executionSets,
                                                    shaderObjects,         unordered};

                            const std::string testName =
                                std::string() + (shaderObjects ? "shader_objects" : "pipelines") +
                                (executionSets ? "_execution_set" : "") + preProcessCase.suffix +
                                (unordered ? "_unordered" : "") + (checkDrawParams ? "_check_draw_params" : "") +
                                testTypeCase.suffix;

                            const auto group =
                                (params.indexedDraws() ? drawIndexedCountGroup.get() : drawCountGroup.get());
                            addFunctionCaseWithPrograms(group, testName, checkDrawCountSupport, initDrawCountPrograms,
                                                        testDrawCountRun, params);
                        }

    mainGroup->addChild(drawCountGroup.release());
    mainGroup->addChild(drawIndexedCountGroup.release());

    return mainGroup.release();
}

} // namespace DGC
} // namespace vkt
