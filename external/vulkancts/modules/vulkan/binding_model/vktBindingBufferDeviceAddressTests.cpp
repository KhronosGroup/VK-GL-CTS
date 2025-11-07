/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017-2019 The Khronos Group Inc.
 * Copyright (c) 2018-2019 NVIDIA Corporation
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
 * \brief Tests for VK_EXT_buffer_device_address.
 *//*--------------------------------------------------------------------*/

#include "vktBindingBufferDeviceAddressTests.hpp"
#include "vktTestCase.hpp"

#include "vkQueryUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"

#include "deDefs.h"
#include "deRandom.hpp"
#include "deUniquePtr.hpp"

#include "tcuTestCase.hpp"

#include <string>
#include <sstream>

namespace vkt
{
namespace BindingModel
{
namespace
{
using namespace vk;
using namespace std;

typedef de::MovePtr<Unique<VkBuffer>> VkBufferSp;
typedef de::MovePtr<Allocation> AllocationSp;

static const uint32_t DIM = 8;

typedef enum
{
    BASE_UBO = 0,
    BASE_SSBO,
} Base;

#define ENABLE_RAYTRACING 0

typedef enum
{
    STAGE_COMPUTE = 0,
    STAGE_VERTEX,
    STAGE_FRAGMENT,
    STAGE_RAYGEN,
} Stage;

typedef enum
{
    BT_SINGLE = 0,
    BT_MULTI,
    BT_REPLAY,
} BufType;

typedef enum
{
    LAYOUT_STD140 = 0,
    LAYOUT_SCALAR,
} Layout;

typedef enum
{
    CONVERT_NONE = 0,
    CONVERT_UINT64,
    CONVERT_UVEC2,
    CONVERT_U64CMP,
    CONVERT_UVEC2CMP,
    CONVERT_UVEC2TOU64,
    CONVERT_U64TOUVEC2,
} Convert;

typedef enum
{
    OFFSET_ZERO = 0,
    OFFSET_NONZERO,
} MemoryOffset;

struct CaseDef
{
    uint32_t set;
    uint32_t depth;
    Base base;
    Stage stage;
    Convert convertUToPtr;
    bool storeInLocal;
    BufType bufType;
    Layout layout;
    MemoryOffset memoryOffset;
};

class BufferAddressTestInstance : public TestInstance
{
public:
    BufferAddressTestInstance(Context &context, const CaseDef &data);
    ~BufferAddressTestInstance(void);
    tcu::TestStatus iterate(void);
    virtual void fillBuffer(const std::vector<uint8_t *> &cpuAddrs, const std::vector<uint64_t> &gpuAddrs,
                            uint32_t bufNum, uint32_t curDepth) const;

private:
    CaseDef m_data;

    enum
    {
        WIDTH  = 256,
        HEIGHT = 256
    };
};

BufferAddressTestInstance::BufferAddressTestInstance(Context &context, const CaseDef &data)
    : vkt::TestInstance(context)
    , m_data(data)
{
}

BufferAddressTestInstance::~BufferAddressTestInstance(void)
{
}

class BufferAddressTestCase : public TestCase
{
public:
    BufferAddressTestCase(tcu::TestContext &context, const char *name, const CaseDef data);
    ~BufferAddressTestCase(void);
    virtual void initPrograms(SourceCollections &programCollection) const;
    virtual TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;
    virtual void checkBuffer(std::stringstream &checks, uint32_t bufNum, uint32_t curDepth,
                             const std::string &prefix) const;

private:
    CaseDef m_data;
};

BufferAddressTestCase::BufferAddressTestCase(tcu::TestContext &context, const char *name, const CaseDef data)
    : vkt::TestCase(context, name)
    , m_data(data)
{
}

BufferAddressTestCase::~BufferAddressTestCase(void)
{
}

void BufferAddressTestCase::checkSupport(Context &context) const
{
    if (!context.isBufferDeviceAddressSupported())
        TCU_THROW(NotSupportedError, "Physical storage buffer pointers not supported");

    if (m_data.stage == STAGE_VERTEX && !context.getDeviceFeatures().vertexPipelineStoresAndAtomics)
        TCU_THROW(NotSupportedError, "Vertex pipeline stores and atomics not supported");

    if (m_data.set >= context.getDeviceProperties().limits.maxBoundDescriptorSets)
        TCU_THROW(NotSupportedError, "descriptor set number not supported");

#ifndef CTS_USES_VULKANSC
    bool isBufferDeviceAddressWithCaptureReplaySupported =
        (context.isDeviceFunctionalitySupported("VK_KHR_buffer_device_address") &&
         context.getBufferDeviceAddressFeatures().bufferDeviceAddressCaptureReplay) ||
        (context.isDeviceFunctionalitySupported("VK_EXT_buffer_device_address") &&
         context.getBufferDeviceAddressFeaturesEXT().bufferDeviceAddressCaptureReplay);
#else
    bool isBufferDeviceAddressWithCaptureReplaySupported =
        (context.isDeviceFunctionalitySupported("VK_KHR_buffer_device_address") &&
         context.getBufferDeviceAddressFeatures().bufferDeviceAddressCaptureReplay);
#endif

    if (m_data.bufType == BT_REPLAY && !isBufferDeviceAddressWithCaptureReplaySupported)
        TCU_THROW(NotSupportedError, "Capture/replay of physical storage buffer pointers not supported");

    if (m_data.layout == LAYOUT_SCALAR && !context.getScalarBlockLayoutFeatures().scalarBlockLayout)
        TCU_THROW(NotSupportedError, "Scalar block layout not supported");

#if ENABLE_RAYTRACING
    if (m_data.stage == STAGE_RAYGEN && !context.isDeviceFunctionalitySupported("VK_NV_ray_tracing"))
    {
        TCU_THROW(NotSupportedError, "Ray tracing not supported");
    }
#endif

    const bool needsInt64 = (m_data.convertUToPtr == CONVERT_UINT64 || m_data.convertUToPtr == CONVERT_U64CMP ||
                             m_data.convertUToPtr == CONVERT_U64TOUVEC2 || m_data.convertUToPtr == CONVERT_UVEC2TOU64);

    const bool needsKHR = (m_data.convertUToPtr == CONVERT_UVEC2 || m_data.convertUToPtr == CONVERT_UVEC2CMP ||
                           m_data.convertUToPtr == CONVERT_U64TOUVEC2 || m_data.convertUToPtr == CONVERT_UVEC2TOU64);

    if (needsInt64 && !context.getDeviceFeatures().shaderInt64)
        TCU_THROW(NotSupportedError, "Int64 not supported");
    if (needsKHR && !context.isDeviceFunctionalitySupported("VK_KHR_buffer_device_address"))
        TCU_THROW(NotSupportedError, "VK_KHR_buffer_device_address not supported");
}

void BufferAddressTestCase::checkBuffer(std::stringstream &checks, uint32_t bufNum, uint32_t curDepth,
                                        const std::string &prefix) const
{
    string newPrefix = prefix;
    if (curDepth > 0)
    {
        if (m_data.convertUToPtr == CONVERT_UINT64 || m_data.convertUToPtr == CONVERT_UVEC2TOU64)
            newPrefix = "T1(uint64_t(T1(" + newPrefix + ")))";
        else if (m_data.convertUToPtr == CONVERT_UVEC2 || m_data.convertUToPtr == CONVERT_U64TOUVEC2)
            newPrefix = "T1(uvec2(T1(" + newPrefix + ")))";
    }

    if (m_data.storeInLocal && curDepth != 0)
    {
        std::string localName = "l" + de::toString(bufNum);
        checks << "   " << ((bufNum & 1) ? "restrict " : "") << "T1 " << localName << " = " << newPrefix << ";\n";
        newPrefix = localName;
    }

    checks << "   accum |= " << newPrefix << ".a[0] - " << bufNum * 3 + 0 << ";\n";
    checks << "   accum |= " << newPrefix << ".a[pc.identity[1]] - " << bufNum * 3 + 1 << ";\n";
    checks << "   accum |= " << newPrefix << ".b - " << bufNum * 3 + 2 << ";\n";
    checks << "   accum |= int(" << newPrefix << ".e[0][0] - " << bufNum * 3 + 3 << ");\n";
    checks << "   accum |= int(" << newPrefix << ".e[0][1] - " << bufNum * 3 + 5 << ");\n";
    checks << "   accum |= int(" << newPrefix << ".e[1][0] - " << bufNum * 3 + 4 << ");\n";
    checks << "   accum |= int(" << newPrefix << ".e[1][1] - " << bufNum * 3 + 6 << ");\n";

    if (m_data.layout == LAYOUT_SCALAR)
    {
        checks << "   f = " << newPrefix << ".f;\n";
        checks << "   accum |= f.x - " << bufNum * 3 + 7 << ";\n";
        checks << "   accum |= f.y - " << bufNum * 3 + 8 << ";\n";
        checks << "   accum |= f.z - " << bufNum * 3 + 9 << ";\n";
    }

    const std::string localPrefix = "l" + de::toString(bufNum);

    if (m_data.convertUToPtr == CONVERT_U64CMP || m_data.convertUToPtr == CONVERT_UVEC2CMP)
    {
        const std::string type = ((m_data.convertUToPtr == CONVERT_U64CMP) ? "uint64_t" : "uvec2");

        checks << "   " << type << " " << localPrefix << "c0 = " << type << "(" << newPrefix << ".c[0]);\n";
        checks << "   " << type << " " << localPrefix << "c1 = " << type << "(" << newPrefix
               << ".c[pc.identity[1]]);\n";
        checks << "   " << type << " " << localPrefix << "d  = " << type << "(" << newPrefix << ".d);\n";
    }

    if (curDepth != m_data.depth)
    {
        // Check non-null pointers and inequality among them.
        if (m_data.convertUToPtr == CONVERT_U64CMP)
        {
            checks << "   if (" << localPrefix << "c0 == zero ||\n"
                   << "       " << localPrefix << "c1 == zero ||\n"
                   << "       " << localPrefix << "d  == zero ||\n"
                   << "       " << localPrefix << "c0 == " << localPrefix << "c1 ||\n"
                   << "       " << localPrefix << "c1 == " << localPrefix << "d  ||\n"
                   << "       " << localPrefix << "c0 == " << localPrefix << "d  ) {\n"
                   << "     accum |= 1;\n"
                   << "   }\n";
        }
        else if (m_data.convertUToPtr == CONVERT_UVEC2CMP)
        {
            checks << "   if (all(equal(" << localPrefix << "c0, zero)) ||\n"
                   << "       all(equal(" << localPrefix << "c1, zero)) ||\n"
                   << "       all(equal(" << localPrefix << "d , zero)) ||\n"
                   << "       all(equal(" << localPrefix << "c0, " << localPrefix << "c1)) ||\n"
                   << "       all(equal(" << localPrefix << "c1, " << localPrefix << "d )) ||\n"
                   << "       all(equal(" << localPrefix << "c0, " << localPrefix << "d )) ) {\n"
                   << "     accum |= 1;\n"
                   << "   }\n";
        }

        checkBuffer(checks, bufNum * 3 + 1, curDepth + 1, newPrefix + ".c[0]");
        checkBuffer(checks, bufNum * 3 + 2, curDepth + 1, newPrefix + ".c[pc.identity[1]]");
        checkBuffer(checks, bufNum * 3 + 3, curDepth + 1, newPrefix + ".d");
    }
    else
    {
        // Check null pointers nonexplicitly.
        if (m_data.convertUToPtr == CONVERT_U64CMP)
        {
            checks << "   if (!(" << localPrefix << "c0 == " << localPrefix << "c1 &&\n"
                   << "         " << localPrefix << "c1 == " << localPrefix << "d  &&\n"
                   << "         " << localPrefix << "c0 == " << localPrefix << "d  )) {\n"
                   << "     accum |= 1;\n"
                   << "   }\n";
        }
        else if (m_data.convertUToPtr == CONVERT_UVEC2CMP)
        {
            checks << "   if (!(all(equal(" << localPrefix << "c0, " << localPrefix << "c1)) &&\n"
                   << "         all(equal(" << localPrefix << "c1, " << localPrefix << "d )) &&\n"
                   << "         all(equal(" << localPrefix << "c0, " << localPrefix << "d )) )) {\n"
                   << "     accum |= 1;\n"
                   << "   }\n";
        }
    }
}

void BufferAddressTestInstance::fillBuffer(const std::vector<uint8_t *> &cpuAddrs,
                                           const std::vector<uint64_t> &gpuAddrs, uint32_t bufNum,
                                           uint32_t curDepth) const
{
    uint8_t *buf = cpuAddrs[bufNum];

    uint32_t aStride   = m_data.layout == LAYOUT_SCALAR ? 1 : 4; // (in deUint32s)
    uint32_t cStride   = m_data.layout == LAYOUT_SCALAR ? 1 : 2; // (in deUint64s)
    uint32_t matStride = m_data.layout == LAYOUT_SCALAR ? 2 : 4; // (in floats)

    // a
    ((uint32_t *)(buf + 0))[0]       = bufNum * 3 + 0;
    ((uint32_t *)(buf + 0))[aStride] = bufNum * 3 + 1;
    // b
    ((uint32_t *)(buf + 32))[0] = bufNum * 3 + 2;
    if (m_data.layout == LAYOUT_SCALAR)
    {
        // f
        ((uint32_t *)(buf + 36))[0] = bufNum * 3 + 7;
        ((uint32_t *)(buf + 36))[1] = bufNum * 3 + 8;
        ((uint32_t *)(buf + 36))[2] = bufNum * 3 + 9;
    }
    // e
    ((float *)(buf + 96))[0]             = (float)(bufNum * 3 + 3);
    ((float *)(buf + 96))[1]             = (float)(bufNum * 3 + 4);
    ((float *)(buf + 96))[matStride]     = (float)(bufNum * 3 + 5);
    ((float *)(buf + 96))[matStride + 1] = (float)(bufNum * 3 + 6);

    if (curDepth != m_data.depth)
    {
        // c
        ((uint64_t *)(buf + 48))[0]       = gpuAddrs[bufNum * 3 + 1];
        ((uint64_t *)(buf + 48))[cStride] = gpuAddrs[bufNum * 3 + 2];
        // d
        ((uint64_t *)(buf + 80))[0] = gpuAddrs[bufNum * 3 + 3];

        fillBuffer(cpuAddrs, gpuAddrs, bufNum * 3 + 1, curDepth + 1);
        fillBuffer(cpuAddrs, gpuAddrs, bufNum * 3 + 2, curDepth + 1);
        fillBuffer(cpuAddrs, gpuAddrs, bufNum * 3 + 3, curDepth + 1);
    }
    else
    {
        // c
        ((uint64_t *)(buf + 48))[0]       = 0ull;
        ((uint64_t *)(buf + 48))[cStride] = 0ull;
        // d
        ((uint64_t *)(buf + 80))[0] = 0ull;
    }
}

void BufferAddressTestCase::initPrograms(SourceCollections &programCollection) const
{
    std::stringstream decls, checks, localDecls;

    std::string baseStorage   = m_data.base == BASE_UBO ? "uniform" : "buffer";
    std::string memberStorage = "buffer";

    decls << "layout(r32ui, set = " << m_data.set << ", binding = 0) uniform uimage2D image0_0;\n";
    decls << "layout(buffer_reference) " << memberStorage << " T1;\n";

    std::string refType;
    switch (m_data.convertUToPtr)
    {
    case CONVERT_UINT64:
    case CONVERT_U64TOUVEC2:
        refType = "uint64_t";
        break;

    case CONVERT_UVEC2:
    case CONVERT_UVEC2TOU64:
        refType = "uvec2";
        break;

    default:
        refType = "T1";
        break;
    }

    std::string layout = m_data.layout == LAYOUT_SCALAR ? "scalar" : "std140";
    decls
        << "layout(set = " << m_data.set << ", binding = 1, " << layout << ") " << baseStorage
        << " T2 {\n"
           "   layout(offset = 0) int a[2]; // stride = 4 for scalar, 16 for std140\n"
           "   layout(offset = 32) int b;\n"
        << ((m_data.layout == LAYOUT_SCALAR) ? "   layout(offset = 36) ivec3 f;\n" : "") << "   layout(offset = 48) "
        << refType
        << " c[2]; // stride = 8 for scalar, 16 for std140\n"
           "   layout(offset = 80) "
        << refType
        << " d;\n"
           "   layout(offset = 96, row_major) mat2 e; // tightly packed for scalar, 16 byte matrix stride for std140\n"
           "} x;\n";
    decls
        << "layout(buffer_reference, " << layout << ") " << memberStorage
        << " T1 {\n"
           "   layout(offset = 0) int a[2]; // stride = 4 for scalar, 16 for std140\n"
           "   layout(offset = 32) int b;\n"
        << ((m_data.layout == LAYOUT_SCALAR) ? "   layout(offset = 36) ivec3 f;\n" : "") << "   layout(offset = 48) "
        << refType
        << " c[2]; // stride = 8 for scalar, 16 for std140\n"
           "   layout(offset = 80) "
        << refType
        << " d;\n"
           "   layout(offset = 96, row_major) mat2 e; // tightly packed for scalar, 16 byte matrix stride for std140\n"
           "};\n";

    if (m_data.convertUToPtr == CONVERT_U64CMP)
        localDecls << "  uint64_t zero = uint64_t(0);\n";
    else if (m_data.convertUToPtr == CONVERT_UVEC2CMP)
        localDecls << "  uvec2 zero = uvec2(0, 0);\n";

    checkBuffer(checks, 0, 0, "x");

    std::stringstream pushdecl;
    pushdecl << "layout (push_constant, std430) uniform Block { int identity[32]; } pc;\n";

    vk::ShaderBuildOptions::Flags flags = vk::ShaderBuildOptions::Flags(0);
    if (m_data.layout == LAYOUT_SCALAR)
        flags = vk::ShaderBuildOptions::FLAG_ALLOW_SCALAR_OFFSETS;

    // The conversion and comparison in uvec2 form test needs SPIR-V 1.5 for OpBitcast.
    const vk::SpirvVersion spirvVersion =
        ((m_data.convertUToPtr == CONVERT_UVEC2CMP) ? vk::SPIRV_VERSION_1_5 : vk::SPIRV_VERSION_1_0);

    switch (m_data.stage)
    {
    default:
        DE_ASSERT(0); // Fallthrough
    case STAGE_COMPUTE:
    {
        std::stringstream css;
        css << "#version 450 core\n"
               "#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable\n"
               "#extension GL_EXT_buffer_reference : enable\n"
               "#extension GL_EXT_scalar_block_layout : enable\n"
               "#extension GL_EXT_buffer_reference_uvec2 : enable\n"
            << pushdecl.str() << decls.str()
            << "layout(local_size_x = 1, local_size_y = 1) in;\n"
               "void main()\n"
               "{\n"
               "  int accum = 0, temp;\n"
               "  ivec3 f;\n"
            << localDecls.str() << checks.str()
            << "  uvec4 color = (accum != 0) ? uvec4(0,0,0,0) : uvec4(1,0,0,1);\n"
               "  imageStore(image0_0, ivec2(gl_GlobalInvocationID.xy), color);\n"
               "}\n";

        programCollection.glslSources.add("test")
            << glu::ComputeSource(css.str())
            << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, spirvVersion, flags);
        break;
    }
#if ENABLE_RAYTRACING
    case STAGE_RAYGEN:
    {
        std::stringstream css;
        css << "#version 460 core\n"
               "#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable\n"
               "#extension GL_EXT_buffer_reference : enable\n"
               "#extension GL_EXT_scalar_block_layout : enable\n"
               "#extension GL_EXT_buffer_reference_uvec2 : enable\n"
               "#extension GL_NV_ray_tracing : require\n"
            << pushdecl.str() << decls.str()
            << "void main()\n"
               "{\n"
               "  int accum = 0, temp;\n"
               "  ivec3 f;\n"
            << localDecls.str() << checks.str()
            << "  uvec4 color = (accum != 0) ? uvec4(0,0,0,0) : uvec4(1,0,0,1);\n"
               "  imageStore(image0_0, ivec2(gl_LaunchIDNV.xy), color);\n"
               "}\n";

        programCollection.glslSources.add("test")
            << glu::RaygenSource(css.str())
            << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, spirvVersion, flags);
        break;
    }
#endif
    case STAGE_VERTEX:
    {
        std::stringstream vss;
        vss << "#version 450 core\n"
               "#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable\n"
               "#extension GL_EXT_buffer_reference : enable\n"
               "#extension GL_EXT_scalar_block_layout : enable\n"
               "#extension GL_EXT_buffer_reference_uvec2 : enable\n"
            << pushdecl.str() << decls.str()
            << "void main()\n"
               "{\n"
               "  int accum = 0, temp;\n"
               "  ivec3 f;\n"
            << localDecls.str() << checks.str()
            << "  uvec4 color = (accum != 0) ? uvec4(0,0,0,0) : uvec4(1,0,0,1);\n"
               "  imageStore(image0_0, ivec2(gl_VertexIndex % "
            << DIM << ", gl_VertexIndex / " << DIM
            << "), color);\n"
               "  gl_PointSize = 1.0f;\n"
               "}\n";

        programCollection.glslSources.add("test")
            << glu::VertexSource(vss.str())
            << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, spirvVersion, flags);
        break;
    }
    case STAGE_FRAGMENT:
    {
        std::stringstream vss;
        vss << "#version 450 core\n"
               "void main()\n"
               "{\n"
               // full-viewport quad
               "  gl_Position = vec4( 2.0*float(gl_VertexIndex&2) - 1.0, 4.0*(gl_VertexIndex&1)-1.0, 1.0 - 2.0 * "
               "float(gl_VertexIndex&1), 1);\n"
               "}\n";

        programCollection.glslSources.add("vert") << glu::VertexSource(vss.str());

        std::stringstream fss;
        fss << "#version 450 core\n"
               "#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable\n"
               "#extension GL_EXT_buffer_reference : enable\n"
               "#extension GL_EXT_scalar_block_layout : enable\n"
               "#extension GL_EXT_buffer_reference_uvec2 : enable\n"
            << pushdecl.str() << decls.str()
            << "void main()\n"
               "{\n"
               "  int accum = 0, temp;\n"
               "  ivec3 f;\n"
            << localDecls.str() << checks.str()
            << "  uvec4 color = (accum != 0) ? uvec4(0,0,0,0) : uvec4(1,0,0,1);\n"
               "  imageStore(image0_0, ivec2(gl_FragCoord.x, gl_FragCoord.y), color);\n"
               "}\n";

        programCollection.glslSources.add("test")
            << glu::FragmentSource(fss.str())
            << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, spirvVersion, flags);
        break;
    }
    }
}

TestInstance *BufferAddressTestCase::createInstance(Context &context) const
{
    return new BufferAddressTestInstance(context, m_data);
}

VkBufferCreateInfo makeBufferCreateInfo(const void *pNext, const VkDeviceSize bufferSize,
                                        const VkBufferUsageFlags usage, const VkBufferCreateFlags flags)
{
    const VkBufferCreateInfo bufferCreateInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
        pNext,                                // const void* pNext;
        flags,                                // VkBufferCreateFlags flags;
        bufferSize,                           // VkDeviceSize size;
        usage,                                // VkBufferUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
        0u,                                   // uint32_t queueFamilyIndexCount;
        nullptr,                              // const uint32_t* pQueueFamilyIndices;
    };
    return bufferCreateInfo;
}

tcu::TestStatus BufferAddressTestInstance::iterate(void)
{
    const InstanceInterface &vki       = m_context.getInstanceInterface();
    const DeviceInterface &vk          = m_context.getDeviceInterface();
    const VkPhysicalDevice &physDevice = m_context.getPhysicalDevice();
    const VkDevice device              = m_context.getDevice();
    Allocator &allocator               = m_context.getDefaultAllocator();
    const bool useKHR                  = m_context.isDeviceFunctionalitySupported("VK_KHR_buffer_device_address");

    const bool isComputeOnly = m_context.getTestContext().getCommandLine().isComputeOnly();
    VkFlags allShaderStages =
        isComputeOnly ? VK_SHADER_STAGE_COMPUTE_BIT :
                        VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    VkFlags allPipelineStages = isComputeOnly ?
                                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT :
                                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

#if ENABLE_RAYTRACING
    if (m_data.stage == STAGE_RAYGEN)
    {
        allShaderStages   = VK_SHADER_STAGE_RAYGEN_BIT_NV;
        allPipelineStages = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV;
    }
#endif

    VkPhysicalDeviceProperties2 properties;
    deMemset(&properties, 0, sizeof(properties));
    properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;

#if ENABLE_RAYTRACING
    VkPhysicalDeviceRayTracingPropertiesNV rayTracingProperties;
    deMemset(&rayTracingProperties, 0, sizeof(rayTracingProperties));
    rayTracingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PROPERTIES_NV;

    if (m_context.isDeviceFunctionalitySupported("VK_NV_ray_tracing"))
    {
        properties.pNext = &rayTracingProperties;
    }
#endif

    m_context.getInstanceInterface().getPhysicalDeviceProperties2(m_context.getPhysicalDevice(), &properties);

    VkPipelineBindPoint bindPoint;

    switch (m_data.stage)
    {
    case STAGE_COMPUTE:
        bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
        break;
#if ENABLE_RAYTRACING
    case STAGE_RAYGEN:
        bindPoint = VK_PIPELINE_BIND_POINT_RAY_TRACING_NV;
        break;
#endif
    default:
        bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        break;
    }

    Move<vk::VkDescriptorPool> descriptorPool;
    Move<vk::VkDescriptorSet> descriptorSet;

    VkDescriptorPoolCreateFlags poolCreateFlags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    VkDescriptorSetLayoutBinding bindings[2];
    bindings[0] = {
        0,                                // uint32_t binding;
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, // VkDescriptorType descriptorType;
        1,                                // uint32_t descriptorCount;
        allShaderStages,                  // VkShaderStageFlags stageFlags;
        nullptr                           // const VkSampler* pImmutableSamplers;
    };
    bindings[1] = {
        1, // uint32_t binding;
        m_data.base == BASE_UBO ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER :
                                  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // VkDescriptorType descriptorType;
        1,                                                           // uint32_t descriptorCount;
        allShaderStages,                                             // VkShaderStageFlags stageFlags;
        nullptr                                                      // const VkSampler* pImmutableSamplers;
    };

    // Create a layout and allocate a descriptor set for it.
    VkDescriptorSetLayoutCreateInfo setLayoutCreateInfo = {vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                                           nullptr,

                                                           0, (uint32_t)2, &bindings[0]};

    Move<vk::VkDescriptorSetLayout> descriptorSetLayout =
        vk::createDescriptorSetLayout(vk, device, &setLayoutCreateInfo);

    setLayoutCreateInfo.bindingCount = 0;
    Move<vk::VkDescriptorSetLayout> emptyDescriptorSetLayout =
        vk::createDescriptorSetLayout(vk, device, &setLayoutCreateInfo);

    vk::DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(bindings[1].descriptorType, 1);
    poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1);

    descriptorPool = poolBuilder.build(vk, device, poolCreateFlags, 1u);
    descriptorSet  = makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout);

    VkDeviceSize align = de::max(de::max(properties.properties.limits.minUniformBufferOffsetAlignment,
                                         properties.properties.limits.minStorageBufferOffsetAlignment),
                                 (VkDeviceSize)128 /*sizeof(T1)*/);

    uint32_t numBindings = 1;
    for (uint32_t d = 0; d < m_data.depth; ++d)
    {
        numBindings = numBindings * 3 + 1;
    }

#ifndef CTS_USES_VULKANSC
    VkBufferDeviceAddressCreateInfoEXT addressCreateInfoEXT = {
        VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_CREATE_INFO_EXT, // VkStructureType  sType;
        nullptr,                                                 // const void*  pNext;
        0x000000000ULL,                                          // VkDeviceSize         deviceAddress
    };
#endif

    VkBufferOpaqueCaptureAddressCreateInfo bufferOpaqueCaptureAddressCreateInfo = {
        VK_STRUCTURE_TYPE_BUFFER_OPAQUE_CAPTURE_ADDRESS_CREATE_INFO, // VkStructureType  sType;
        nullptr,                                                     // const void*  pNext;
        0x000000000ULL,                                              // VkDeviceSize         opaqueCaptureAddress
    };

    std::vector<uint8_t *> cpuAddrs(numBindings);
    std::vector<VkDeviceAddress> gpuAddrs(numBindings);
    std::vector<uint64_t> opaqueBufferAddrs(numBindings);
    std::vector<uint64_t> opaqueMemoryAddrs(numBindings);

    VkBufferDeviceAddressInfo bufferDeviceAddressInfo = {
        VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, // VkStructureType  sType;
        nullptr,                                      // const void*  pNext;
        VK_NULL_HANDLE,                               // VkBuffer             buffer
    };

    VkDeviceMemoryOpaqueCaptureAddressInfo deviceMemoryOpaqueCaptureAddressInfo = {
        VK_STRUCTURE_TYPE_DEVICE_MEMORY_OPAQUE_CAPTURE_ADDRESS_INFO, // VkStructureType  sType;
        nullptr,                                                     // const void*  pNext;
        VK_NULL_HANDLE,                                              // VkDeviceMemory  memory;
    };

    bool multiBuffer          = m_data.bufType != BT_SINGLE;
    bool offsetNonZero        = m_data.memoryOffset == OFFSET_NONZERO;
    uint32_t numBuffers       = multiBuffer ? numBindings : 1;
    VkDeviceSize bufferSize   = multiBuffer ? align : (align * numBindings);
    VkDeviceSize memoryOffset = 0;

    vector<VkBufferSp> buffers(numBuffers);
    vector<AllocationSp> allocations(numBuffers);

    VkBufferCreateInfo bufferCreateInfo =
        makeBufferCreateInfo(nullptr, bufferSize,
                             VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                             m_data.bufType == BT_REPLAY ? VK_BUFFER_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT : 0);

    // VkMemoryAllocateFlags to be filled out later
    VkMemoryAllocateFlagsInfo allocFlagsInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO, //    VkStructureType    sType
        nullptr,                                      //    const void*        pNext
        0,                                            //    VkMemoryAllocateFlags    flags
        0,                                            //    uint32_t                 deviceMask
    };

    VkMemoryOpaqueCaptureAddressAllocateInfo memoryOpaqueCaptureAddressAllocateInfo = {
        VK_STRUCTURE_TYPE_MEMORY_OPAQUE_CAPTURE_ADDRESS_ALLOCATE_INFO, // VkStructureType    sType;
        nullptr,                                                       // const void*        pNext;
        0,                                                             // uint64_t           opaqueCaptureAddress;
    };

    if (useKHR)
        allocFlagsInfo.flags |= VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

    if (useKHR && m_data.bufType == BT_REPLAY)
    {
        allocFlagsInfo.flags |= VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT;
        allocFlagsInfo.pNext = &memoryOpaqueCaptureAddressAllocateInfo;
    }

    for (uint32_t i = 0; i < numBuffers; ++i)
    {
        buffers[i] = VkBufferSp(new Unique<VkBuffer>(createBuffer(vk, device, &bufferCreateInfo)));

        // query opaque capture address before binding memory
        if (useKHR && m_data.bufType == BT_REPLAY)
        {
            bufferDeviceAddressInfo.buffer = **buffers[i];
            opaqueBufferAddrs[i]           = vk.getBufferOpaqueCaptureAddress(device, &bufferDeviceAddressInfo);
        }

        VkMemoryRequirements memReq = getBufferMemoryRequirements(vk, device, **buffers[i]);
        if (offsetNonZero)
        {
            memoryOffset = memReq.alignment;
            memReq.size += memoryOffset;
        }

        allocations[i] = AllocationSp(
            allocateExtended(vki, vk, physDevice, device, memReq, MemoryRequirement::HostVisible, &allocFlagsInfo));

        if (useKHR && m_data.bufType == BT_REPLAY)
        {
            deviceMemoryOpaqueCaptureAddressInfo.memory = allocations[i]->getMemory();
            opaqueMemoryAddrs[i] =
                vk.getDeviceMemoryOpaqueCaptureAddress(device, &deviceMemoryOpaqueCaptureAddressInfo);
        }

        VK_CHECK(vk.bindBufferMemory(device, **buffers[i], allocations[i]->getMemory(), memoryOffset));
    }

    if (m_data.bufType == BT_REPLAY)
    {
        for (uint32_t i = 0; i < numBuffers; ++i)
        {
            bufferDeviceAddressInfo.buffer = **buffers[i];
            gpuAddrs[i]                    = vk.getBufferDeviceAddress(device, &bufferDeviceAddressInfo);
        }
        buffers.clear();
        buffers.resize(numBuffers);
        allocations.clear();
        allocations.resize(numBuffers);

#ifndef CTS_USES_VULKANSC
        bufferCreateInfo.pNext = useKHR ? (void *)&bufferOpaqueCaptureAddressCreateInfo : (void *)&addressCreateInfoEXT;
#else
        bufferCreateInfo.pNext = (void *)&bufferOpaqueCaptureAddressCreateInfo;
#endif

        for (int32_t i = numBuffers - 1; i >= 0; --i)
        {
#ifndef CTS_USES_VULKANSC
            addressCreateInfoEXT.deviceAddress = gpuAddrs[i];
#endif
            bufferOpaqueCaptureAddressCreateInfo.opaqueCaptureAddress   = opaqueBufferAddrs[i];
            memoryOpaqueCaptureAddressAllocateInfo.opaqueCaptureAddress = opaqueMemoryAddrs[i];

            buffers[i]     = VkBufferSp(new Unique<VkBuffer>(createBuffer(vk, device, &bufferCreateInfo)));
            allocations[i] = AllocationSp(allocateExtended(vki, vk, physDevice, device,
                                                           getBufferMemoryRequirements(vk, device, **buffers[i]),
                                                           MemoryRequirement::HostVisible, &allocFlagsInfo));
            VK_CHECK(vk.bindBufferMemory(device, **buffers[i], allocations[i]->getMemory(), 0));

            bufferDeviceAddressInfo.buffer = **buffers[i];
            VkDeviceSize newAddr           = vk.getBufferDeviceAddress(device, &bufferDeviceAddressInfo);

            if (newAddr != gpuAddrs[i])
                return tcu::TestStatus(QP_TEST_RESULT_FAIL, "address mismatch");
        }
    }

    // Create a buffer and compute the address for each "align" bytes.
    for (uint32_t i = 0; i < numBindings; ++i)
    {
        bufferDeviceAddressInfo.buffer = **buffers[multiBuffer ? i : 0];
        gpuAddrs[i]                    = vk.getBufferDeviceAddress(device, &bufferDeviceAddressInfo);

        cpuAddrs[i] = (uint8_t *)allocations[multiBuffer ? i : 0]->getHostPtr() + memoryOffset;
        if (!multiBuffer)
        {
            cpuAddrs[i] = cpuAddrs[i] + align * i;
            gpuAddrs[i] = gpuAddrs[i] + align * i;
        }
        //printf("addr 0x%08x`%08x\n", (unsigned)(gpuAddrs[i]>>32), (unsigned)(gpuAddrs[i]));
    }

    fillBuffer(cpuAddrs, gpuAddrs, 0, 0);

    for (uint32_t i = 0; i < numBuffers; ++i)
        flushAlloc(vk, device, *allocations[i]);

    const VkQueue queue             = m_context.getUniversalQueue();
    Move<VkCommandPool> cmdPool     = createCommandPool(vk, device, 0, m_context.getUniversalQueueFamilyIndex());
    Move<VkCommandBuffer> cmdBuffer = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    beginCommandBuffer(vk, *cmdBuffer, 0u);

    // Push constants are used for dynamic indexing. PushConstant[i] = i.

    const VkPushConstantRange pushConstRange = {
        allShaderStages, // VkShaderStageFlags    stageFlags
        0,               // uint32_t                offset
        128              // uint32_t                size
    };

    uint32_t nonEmptySetLimit = m_data.base == BASE_UBO ?
                                    properties.properties.limits.maxPerStageDescriptorUniformBuffers :
                                    properties.properties.limits.maxPerStageDescriptorStorageBuffers;
    nonEmptySetLimit = de::min(nonEmptySetLimit, properties.properties.limits.maxPerStageDescriptorStorageImages);

    vector<vk::VkDescriptorSetLayout> descriptorSetLayoutsRaw(m_data.set + 1);
    for (size_t i = 0; i < m_data.set + 1; ++i)
    {
        // use nonempty descriptor sets to consume resources until we run out of descriptors
        if (i < nonEmptySetLimit - 1 || i == m_data.set)
            descriptorSetLayoutsRaw[i] = descriptorSetLayout.get();
        else
            descriptorSetLayoutsRaw[i] = emptyDescriptorSetLayout.get();
    }

    const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // sType
        nullptr,                                       // pNext
        (VkPipelineLayoutCreateFlags)0,
        m_data.set + 1,              // setLayoutCount
        &descriptorSetLayoutsRaw[0], // pSetLayouts
        1u,                          // pushConstantRangeCount
        &pushConstRange,             // pPushConstantRanges
    };

    Move<VkPipelineLayout> pipelineLayout = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo, NULL);

    // PushConstant[i] = i
    for (uint32_t i = 0; i < (uint32_t)(128 / sizeof(uint32_t)); ++i)
    {
        vk.cmdPushConstants(*cmdBuffer, *pipelineLayout, allShaderStages, (uint32_t)(i * sizeof(uint32_t)),
                            (uint32_t)sizeof(uint32_t), &i);
    }

    de::MovePtr<BufferWithMemory> copyBuffer;
    copyBuffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
        vk, device, allocator,
        makeBufferCreateInfo(nullptr, DIM * DIM * sizeof(uint32_t), VK_BUFFER_USAGE_TRANSFER_DST_BIT, 0),
        MemoryRequirement::HostVisible));

    const VkImageCreateInfo imageCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        (VkImageCreateFlags)0u,              // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
        VK_FORMAT_R32_UINT,                  // VkFormat format;
        {
            DIM,                 // uint32_t width;
            DIM,                 // uint32_t height;
            1u                   // uint32_t depth;
        },                       // VkExtent3D extent;
        1u,                      // uint32_t mipLevels;
        1u,                      // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,   // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL, // VkImageTiling tiling;
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
            VK_IMAGE_USAGE_TRANSFER_DST_BIT, // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        0u,                                  // uint32_t queueFamilyIndexCount;
        nullptr,                             // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED            // VkImageLayout initialLayout;
    };

    VkImageViewCreateInfo imageViewCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType sType;
        nullptr,                                  // const void* pNext;
        (VkImageViewCreateFlags)0u,               // VkImageViewCreateFlags flags;
        VK_NULL_HANDLE,                           // VkImage image;
        VK_IMAGE_VIEW_TYPE_2D,                    // VkImageViewType viewType;
        VK_FORMAT_R32_UINT,                       // VkFormat format;
        {
            VK_COMPONENT_SWIZZLE_R, // VkComponentSwizzle r;
            VK_COMPONENT_SWIZZLE_G, // VkComponentSwizzle g;
            VK_COMPONENT_SWIZZLE_B, // VkComponentSwizzle b;
            VK_COMPONENT_SWIZZLE_A  // VkComponentSwizzle a;
        },                          // VkComponentMapping  components;
        {
            VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
            0u,                        // uint32_t baseMipLevel;
            1u,                        // uint32_t levelCount;
            0u,                        // uint32_t baseArrayLayer;
            1u                         // uint32_t layerCount;
        }                              // VkImageSubresourceRange subresourceRange;
    };

    de::MovePtr<ImageWithMemory> image;
    Move<VkImageView> imageView;

    image = de::MovePtr<ImageWithMemory>(
        new ImageWithMemory(vk, device, allocator, imageCreateInfo, MemoryRequirement::Any));
    imageViewCreateInfo.image = **image;
    imageView                 = createImageView(vk, device, &imageViewCreateInfo, NULL);

    VkDescriptorImageInfo imageInfo   = makeDescriptorImageInfo(VK_NULL_HANDLE, *imageView, VK_IMAGE_LAYOUT_GENERAL);
    VkDescriptorBufferInfo bufferInfo = makeDescriptorBufferInfo(**buffers[0], 0, align);

    VkWriteDescriptorSet w = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, // sType
        nullptr,                                // pNext
        *descriptorSet,                         // dstSet
        (uint32_t)0,                            // dstBinding
        0,                                      // dstArrayElement
        1u,                                     // descriptorCount
        bindings[0].descriptorType,             // descriptorType
        &imageInfo,                             // pImageInfo
        &bufferInfo,                            // pBufferInfo
        nullptr,                                // pTexelBufferView
    };
    vk.updateDescriptorSets(device, 1, &w, 0, NULL);

    w.dstBinding     = 1;
    w.descriptorType = bindings[1].descriptorType;
    vk.updateDescriptorSets(device, 1, &w, 0, NULL);

    vk.cmdBindDescriptorSets(*cmdBuffer, bindPoint, *pipelineLayout, m_data.set, 1, &descriptorSet.get(), 0, nullptr);

    Move<VkPipeline> pipeline;
    Move<VkRenderPass> renderPass;
    Move<VkFramebuffer> framebuffer;
    de::MovePtr<BufferWithMemory> sbtBuffer;

    m_context.getTestContext().touchWatchdogAndDisableIntervalTimeLimit();

    if (m_data.stage == STAGE_COMPUTE)
    {
        const Unique<VkShaderModule> shader(
            createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0));

        const VkPipelineShaderStageCreateInfo shaderCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            nullptr,
            (VkPipelineShaderStageCreateFlags)0,
            VK_SHADER_STAGE_COMPUTE_BIT, // stage
            *shader,                     // shader
            "main",
            nullptr, // pSpecializationInfo
        };

        const VkComputePipelineCreateInfo pipelineCreateInfo = {
            VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            nullptr,
            0u,               // flags
            shaderCreateInfo, // cs
            *pipelineLayout,  // layout
            VK_NULL_HANDLE,   // basePipelineHandle
            0u,               // basePipelineIndex
        };
        pipeline = createComputePipeline(vk, device, VK_NULL_HANDLE, &pipelineCreateInfo, NULL);
    }
#if ENABLE_RAYTRACING
    else if (m_data.stage == STAGE_RAYGEN)
    {
        const Unique<VkShaderModule> shader(
            createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0));

        const VkPipelineShaderStageCreateInfo shaderCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            nullptr,
            (VkPipelineShaderStageCreateFlags)0,
            VK_SHADER_STAGE_RAYGEN_BIT_NV, // stage
            *shader,                       // shader
            "main",
            nullptr, // pSpecializationInfo
        };

        VkRayTracingShaderGroupCreateInfoNV group = {
            VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV,
            nullptr,
            VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV, // type
            0,                                           // generalShader
            VK_SHADER_UNUSED_NV,                         // closestHitShader
            VK_SHADER_UNUSED_NV,                         // anyHitShader
            VK_SHADER_UNUSED_NV,                         // intersectionShader
        };

        VkRayTracingPipelineCreateInfoNV pipelineCreateInfo = {
            VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_NV, // sType
            nullptr,                                               // pNext
            0,                                                     // flags
            1,                                                     // stageCount
            &shaderCreateInfo,                                     // pStages
            1,                                                     // groupCount
            &group,                                                // pGroups
            0,                                                     // maxRecursionDepth
            *pipelineLayout,                                       // layout
            VK_NULL_HANDLE,                                        // basePipelineHandle
            0u,                                                    // basePipelineIndex
        };

        pipeline = createRayTracingPipelineNV(vk, device, nullptr, &pipelineCreateInfo, NULL);

        sbtBuffer     = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
            vk, device, allocator,
            makeBufferCreateInfo(nullptr, rayTracingProperties.shaderGroupHandleSize,
                                     VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, 0),
            MemoryRequirement::HostVisible));
        uint32_t *ptr = (uint32_t *)sbtBuffer->getAllocation().getHostPtr();
        invalidateAlloc(vk, device, sbtBuffer->getAllocation());

        vk.getRayTracingShaderGroupHandlesNV(device, *pipeline, 0, 1, rayTracingProperties.shaderGroupHandleSize, ptr);
    }
#endif
    else
    {

        const vk::VkSubpassDescription subpassDesc = {
            (vk::VkSubpassDescriptionFlags)0,
            vk::VK_PIPELINE_BIND_POINT_GRAPHICS, // pipelineBindPoint
            0u,                                  // inputCount
            nullptr,                             // pInputAttachments
            0u,                                  // colorCount
            nullptr,                             // pColorAttachments
            nullptr,                             // pResolveAttachments
            nullptr,                             // depthStencilAttachment
            0u,                                  // preserveCount
            nullptr,                             // pPreserveAttachments
        };
        const vk::VkRenderPassCreateInfo renderPassParams = {
            vk::VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, // sType
            nullptr,                                       // pNext
            (vk::VkRenderPassCreateFlags)0,
            0u,           // attachmentCount
            nullptr,      // pAttachments
            1u,           // subpassCount
            &subpassDesc, // pSubpasses
            0u,           // dependencyCount
            nullptr,      // pDependencies
        };

        renderPass = createRenderPass(vk, device, &renderPassParams);

        const vk::VkFramebufferCreateInfo framebufferParams = {
            vk::VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // sType
            nullptr,                                       // pNext
            (vk::VkFramebufferCreateFlags)0,
            *renderPass, // renderPass
            0u,          // attachmentCount
            nullptr,     // pAttachments
            DIM,         // width
            DIM,         // height
            1u,          // layers
        };

        framebuffer = createFramebuffer(vk, device, &framebufferParams);

        const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                   // const void* pNext;
            (VkPipelineVertexInputStateCreateFlags)0,                  // VkPipelineVertexInputStateCreateFlags flags;
            0u,                                                        // uint32_t vertexBindingDescriptionCount;
            nullptr, // const VkVertexInputBindingDescription* pVertexBindingDescriptions;
            0u,      // uint32_t vertexAttributeDescriptionCount;
            nullptr  // const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
        };

        const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                     // const void* pNext;
            (VkPipelineInputAssemblyStateCreateFlags)0, // VkPipelineInputAssemblyStateCreateFlags flags;
            (m_data.stage == STAGE_VERTEX) ? VK_PRIMITIVE_TOPOLOGY_POINT_LIST :
                                             VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, // VkPrimitiveTopology topology;
            VK_FALSE                                                               // VkBool32 primitiveRestartEnable;
        };

        const VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                    // const void* pNext;
            (VkPipelineRasterizationStateCreateFlags)0,          // VkPipelineRasterizationStateCreateFlags flags;
            VK_FALSE,                                            // VkBool32 depthClampEnable;
            (m_data.stage == STAGE_VERTEX) ? VK_TRUE : VK_FALSE, // VkBool32 rasterizerDiscardEnable;
            VK_POLYGON_MODE_FILL,                                // VkPolygonMode polygonMode;
            VK_CULL_MODE_NONE,                                   // VkCullModeFlags cullMode;
            VK_FRONT_FACE_CLOCKWISE,                             // VkFrontFace frontFace;
            VK_FALSE,                                            // VkBool32 depthBiasEnable;
            0.0f,                                                // float depthBiasConstantFactor;
            0.0f,                                                // float depthBiasClamp;
            0.0f,                                                // float depthBiasSlopeFactor;
            1.0f                                                 // float lineWidth;
        };

        const VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType                            sType
            nullptr,               // const void*                                pNext
            0u,                    // VkPipelineMultisampleStateCreateFlags    flags
            VK_SAMPLE_COUNT_1_BIT, // VkSampleCountFlagBits                    rasterizationSamples
            VK_FALSE,              // VkBool32                                    sampleShadingEnable
            1.0f,                  // float                                    minSampleShading
            nullptr,               // const VkSampleMask*                        pSampleMask
            VK_FALSE,              // VkBool32                                    alphaToCoverageEnable
            VK_FALSE               // VkBool32                                    alphaToOneEnable
        };

        VkViewport viewport = makeViewport(DIM, DIM);
        VkRect2D scissor    = makeRect2D(DIM, DIM);

        const VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, // VkStructureType                            sType
            nullptr,                                               // const void*                                pNext
            (VkPipelineViewportStateCreateFlags)0,                 // VkPipelineViewportStateCreateFlags        flags
            1u,        // uint32_t                                    viewportCount
            &viewport, // const VkViewport*                        pViewports
            1u,        // uint32_t                                    scissorCount
            &scissor   // const VkRect2D*                            pScissors
        };

        Move<VkShaderModule> fs;
        Move<VkShaderModule> vs;

        uint32_t numStages;
        if (m_data.stage == STAGE_VERTEX)
        {
            vs        = createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0);
            fs        = createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0); // bogus
            numStages = 1u;
        }
        else
        {
            vs        = createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0);
            fs        = createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0);
            numStages = 2u;
        }

        const VkPipelineShaderStageCreateInfo shaderCreateInfo[2] = {
            {
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, (VkPipelineShaderStageCreateFlags)0,
                VK_SHADER_STAGE_VERTEX_BIT, // stage
                *vs,                        // shader
                "main",
                nullptr, // pSpecializationInfo
            },
            {
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, (VkPipelineShaderStageCreateFlags)0,
                VK_SHADER_STAGE_FRAGMENT_BIT, // stage
                *fs,                          // shader
                "main",
                nullptr, // pSpecializationInfo
            }};

        const VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {
            VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                         // const void* pNext;
            (VkPipelineCreateFlags)0,                        // VkPipelineCreateFlags flags;
            numStages,                                       // uint32_t stageCount;
            &shaderCreateInfo[0],                            // const VkPipelineShaderStageCreateInfo* pStages;
            &vertexInputStateCreateInfo,   // const VkPipelineVertexInputStateCreateInfo* pVertexInputState;
            &inputAssemblyStateCreateInfo, // const VkPipelineInputAssemblyStateCreateInfo* pInputAssemblyState;
            nullptr,                       // const VkPipelineTessellationStateCreateInfo* pTessellationState;
            &viewportStateCreateInfo,      // const VkPipelineViewportStateCreateInfo* pViewportState;
            &rasterizationStateCreateInfo, // const VkPipelineRasterizationStateCreateInfo* pRasterizationState;
            &multisampleStateCreateInfo,   // const VkPipelineMultisampleStateCreateInfo* pMultisampleState;
            nullptr,                       // const VkPipelineDepthStencilStateCreateInfo* pDepthStencilState;
            nullptr,                       // const VkPipelineColorBlendStateCreateInfo* pColorBlendState;
            nullptr,                       // const VkPipelineDynamicStateCreateInfo* pDynamicState;
            pipelineLayout.get(),          // VkPipelineLayout layout;
            renderPass.get(),              // VkRenderPass renderPass;
            0u,                            // uint32_t subpass;
            VK_NULL_HANDLE,                // VkPipeline basePipelineHandle;
            0                              // int basePipelineIndex;
        };

        pipeline = createGraphicsPipeline(vk, device, VK_NULL_HANDLE, &graphicsPipelineCreateInfo);
    }

    m_context.getTestContext().touchWatchdogAndEnableIntervalTimeLimit();

    const VkImageMemoryBarrier imageBarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType        sType
                                               nullptr,                                // const void*            pNext
                                               0u,                           // VkAccessFlags        srcAccessMask
                                               VK_ACCESS_TRANSFER_WRITE_BIT, // VkAccessFlags        dstAccessMask
                                               VK_IMAGE_LAYOUT_UNDEFINED,    // VkImageLayout        oldLayout
                                               VK_IMAGE_LAYOUT_GENERAL,      // VkImageLayout        newLayout
                                               VK_QUEUE_FAMILY_IGNORED, // uint32_t                srcQueueFamilyIndex
                                               VK_QUEUE_FAMILY_IGNORED, // uint32_t                dstQueueFamilyIndex
                                               **image,                 // VkImage                image
                                               {
                                                   VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags    aspectMask
                                                   0u,                        // uint32_t                baseMipLevel
                                                   1u,                        // uint32_t                mipLevels,
                                                   0u,                        // uint32_t                baseArray
                                                   1u,                        // uint32_t                arraySize
                                               }};

    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          (VkDependencyFlags)0, 0, nullptr, 0, nullptr, 1, &imageBarrier);

    vk.cmdBindPipeline(*cmdBuffer, bindPoint, *pipeline);

    VkImageSubresourceRange range = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    VkClearValue clearColor       = makeClearValueColorU32(0, 0, 0, 0);

    VkMemoryBarrier memBarrier = {
        VK_STRUCTURE_TYPE_MEMORY_BARRIER, // sType
        nullptr,                          // pNext
        0u,                               // srcAccessMask
        0u,                               // dstAccessMask
    };

    vk.cmdClearColorImage(*cmdBuffer, **image, VK_IMAGE_LAYOUT_GENERAL, &clearColor.color, 1, &range);

    memBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    memBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, allPipelineStages, 0, 1, &memBarrier, 0, nullptr,
                          0, nullptr);

    if (m_data.stage == STAGE_COMPUTE)
    {
        vk.cmdDispatch(*cmdBuffer, DIM, DIM, 1);
    }
#if ENABLE_RAYTRACING
    else if (m_data.stage == STAGE_RAYGEN)
    {
        vk.cmdTraceRaysNV(*cmdBuffer, **sbtBuffer, 0, nullptr, 0, 0, nullptr, 0, 0, nullptr, 0, 0, DIM, DIM, 1);
    }
#endif
    else
    {
        beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer, makeRect2D(DIM, DIM), 0, nullptr,
                        VK_SUBPASS_CONTENTS_INLINE);
        // Draw a point cloud for vertex shader testing, and a single quad for fragment shader testing
        if (m_data.stage == STAGE_VERTEX)
        {
            vk.cmdDraw(*cmdBuffer, DIM * DIM, 1u, 0u, 0u);
        }
        else
        {
            vk.cmdDraw(*cmdBuffer, 4u, 1u, 0u, 0u);
        }
        endRenderPass(vk, *cmdBuffer);
    }

    memBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    memBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
    vk.cmdPipelineBarrier(*cmdBuffer, allPipelineStages, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &memBarrier, 0, nullptr,
                          0, nullptr);

    const VkBufferImageCopy copyRegion = makeBufferImageCopy(
        makeExtent3D(DIM, DIM, 1u), makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u));
    vk.cmdCopyImageToBuffer(*cmdBuffer, **image, VK_IMAGE_LAYOUT_GENERAL, **copyBuffer, 1u, &copyRegion);

    endCommandBuffer(vk, *cmdBuffer);

    submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

    uint32_t *ptr = (uint32_t *)copyBuffer->getAllocation().getHostPtr();
    invalidateAlloc(vk, device, copyBuffer->getAllocation());

    qpTestResult res = QP_TEST_RESULT_PASS;

    for (uint32_t i = 0; i < DIM * DIM; ++i)
    {
        if (ptr[i] != 1)
        {
            res = QP_TEST_RESULT_FAIL;
        }
    }

    return tcu::TestStatus(res, qpGetTestResultName(res));
}

class CaptureReplayTestCase : public TestCase
{
public:
    CaptureReplayTestCase(tcu::TestContext &context, const char *name, uint32_t seed);
    ~CaptureReplayTestCase(void);
    virtual void initPrograms(SourceCollections &programCollection) const
    {
        DE_UNREF(programCollection);
    }
    virtual TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;

private:
    uint32_t m_seed;
};

CaptureReplayTestCase::CaptureReplayTestCase(tcu::TestContext &context, const char *name, uint32_t seed)
    : vkt::TestCase(context, name)
    , m_seed(seed)
{
}

CaptureReplayTestCase::~CaptureReplayTestCase(void)
{
}

void CaptureReplayTestCase::checkSupport(Context &context) const
{
    if (!context.isBufferDeviceAddressSupported())
        TCU_THROW(NotSupportedError, "Physical storage buffer pointers not supported");

#ifndef CTS_USES_VULKANSC
    bool isBufferDeviceAddressWithCaptureReplaySupported =
        (context.isDeviceFunctionalitySupported("VK_KHR_buffer_device_address") &&
         context.getBufferDeviceAddressFeatures().bufferDeviceAddressCaptureReplay) ||
        (context.isDeviceFunctionalitySupported("VK_EXT_buffer_device_address") &&
         context.getBufferDeviceAddressFeaturesEXT().bufferDeviceAddressCaptureReplay);
#else
    bool isBufferDeviceAddressWithCaptureReplaySupported =
        (context.isDeviceFunctionalitySupported("VK_KHR_buffer_device_address") &&
         context.getBufferDeviceAddressFeatures().bufferDeviceAddressCaptureReplay);
#endif

    if (!isBufferDeviceAddressWithCaptureReplaySupported)
        TCU_THROW(NotSupportedError, "Capture/replay of physical storage buffer pointers not supported");
}

class CaptureReplayTestInstance : public TestInstance
{
public:
    CaptureReplayTestInstance(Context &context, uint32_t seed);
    ~CaptureReplayTestInstance(void);
    tcu::TestStatus iterate(void);

private:
    uint32_t m_seed;
};

CaptureReplayTestInstance::CaptureReplayTestInstance(Context &context, uint32_t seed)
    : vkt::TestInstance(context)
    , m_seed(seed)
{
}

CaptureReplayTestInstance::~CaptureReplayTestInstance(void)
{
}

TestInstance *CaptureReplayTestCase::createInstance(Context &context) const
{
    return new CaptureReplayTestInstance(context, m_seed);
}

tcu::TestStatus CaptureReplayTestInstance::iterate(void)
{
    const InstanceInterface &vki       = m_context.getInstanceInterface();
    const DeviceInterface &vk          = m_context.getDeviceInterface();
    const VkPhysicalDevice &physDevice = m_context.getPhysicalDevice();
    const VkDevice device              = m_context.getDevice();
    const bool useKHR                  = m_context.isDeviceFunctionalitySupported("VK_KHR_buffer_device_address");
    de::Random rng(m_seed);

#ifndef CTS_USES_VULKANSC
    VkBufferDeviceAddressCreateInfoEXT addressCreateInfoEXT = {
        VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_CREATE_INFO_EXT, // VkStructureType  sType;
        nullptr,                                                 // const void*  pNext;
        0x000000000ULL,                                          // VkDeviceSize         deviceAddress
    };
#endif

    VkBufferOpaqueCaptureAddressCreateInfo bufferOpaqueCaptureAddressCreateInfo = {
        VK_STRUCTURE_TYPE_BUFFER_OPAQUE_CAPTURE_ADDRESS_CREATE_INFO, // VkStructureType  sType;
        nullptr,                                                     // const void*  pNext;
        0x000000000ULL,                                              // VkDeviceSize         opaqueCaptureAddress
    };

    const uint32_t numBuffers = 100;
    std::vector<VkDeviceSize> bufferSizes(numBuffers);
    // random sizes, powers of two [4K, 4MB]
    for (uint32_t i = 0; i < numBuffers; ++i)
        bufferSizes[i] = 4096 << (rng.getUint32() % 11);

    std::vector<VkDeviceAddress> gpuAddrs(numBuffers);
    std::vector<uint64_t> opaqueBufferAddrs(numBuffers);
    std::vector<uint64_t> opaqueMemoryAddrs(numBuffers);

    VkBufferDeviceAddressInfo bufferDeviceAddressInfo = {
        VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, // VkStructureType  sType;
        nullptr,                                      // const void*  pNext;
        VK_NULL_HANDLE,                               // VkBuffer             buffer
    };

    VkDeviceMemoryOpaqueCaptureAddressInfo deviceMemoryOpaqueCaptureAddressInfo = {
        VK_STRUCTURE_TYPE_DEVICE_MEMORY_OPAQUE_CAPTURE_ADDRESS_INFO, // VkStructureType  sType;
        nullptr,                                                     // const void*  pNext;
        VK_NULL_HANDLE,                                              // VkDeviceMemory  memory;
    };

    vector<VkBufferSp> buffers(numBuffers);
    vector<AllocationSp> allocations(numBuffers);

    VkBufferCreateInfo bufferCreateInfo =
        makeBufferCreateInfo(nullptr, 0,
                             VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                             VK_BUFFER_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT);

    // VkMemoryAllocateFlags to be filled out later
    VkMemoryAllocateFlagsInfo allocFlagsInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO, //    VkStructureType    sType
        nullptr,                                      //    const void*        pNext
        0,                                            //    VkMemoryAllocateFlags    flags
        0,                                            //    uint32_t                 deviceMask
    };

    VkMemoryOpaqueCaptureAddressAllocateInfo memoryOpaqueCaptureAddressAllocateInfo = {
        VK_STRUCTURE_TYPE_MEMORY_OPAQUE_CAPTURE_ADDRESS_ALLOCATE_INFO, // VkStructureType    sType;
        nullptr,                                                       // const void*        pNext;
        0,                                                             // uint64_t           opaqueCaptureAddress;
    };

    if (useKHR)
        allocFlagsInfo.flags |= VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

    if (useKHR)
    {
        allocFlagsInfo.flags |= VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT;
        allocFlagsInfo.pNext = &memoryOpaqueCaptureAddressAllocateInfo;
    }

    for (uint32_t i = 0; i < numBuffers; ++i)
    {
        bufferCreateInfo.size = bufferSizes[i];
        buffers[i]            = VkBufferSp(new Unique<VkBuffer>(createBuffer(vk, device, &bufferCreateInfo)));

        // query opaque capture address before binding memory
        if (useKHR)
        {
            bufferDeviceAddressInfo.buffer = **buffers[i];
            opaqueBufferAddrs[i]           = vk.getBufferOpaqueCaptureAddress(device, &bufferDeviceAddressInfo);
        }

        allocations[i] = AllocationSp(allocateExtended(vki, vk, physDevice, device,
                                                       getBufferMemoryRequirements(vk, device, **buffers[i]),
                                                       MemoryRequirement::HostVisible, &allocFlagsInfo));

        if (useKHR)
        {
            deviceMemoryOpaqueCaptureAddressInfo.memory = allocations[i]->getMemory();
            opaqueMemoryAddrs[i] =
                vk.getDeviceMemoryOpaqueCaptureAddress(device, &deviceMemoryOpaqueCaptureAddressInfo);
        }

        VK_CHECK(vk.bindBufferMemory(device, **buffers[i], allocations[i]->getMemory(), 0));
    }

    for (uint32_t i = 0; i < numBuffers; ++i)
    {
        bufferDeviceAddressInfo.buffer = **buffers[i];
        gpuAddrs[i]                    = vk.getBufferDeviceAddress(device, &bufferDeviceAddressInfo);
    }
    buffers.clear();
    buffers.resize(numBuffers);
    allocations.clear();
    allocations.resize(numBuffers);

#ifndef CTS_USES_VULKANSC
    bufferCreateInfo.pNext = useKHR ? (void *)&bufferOpaqueCaptureAddressCreateInfo : (void *)&addressCreateInfoEXT;
#else
    bufferCreateInfo.pNext = (void *)&bufferOpaqueCaptureAddressCreateInfo;
#endif

    for (int32_t i = numBuffers - 1; i >= 0; --i)
    {
#ifndef CTS_USES_VULKANSC
        addressCreateInfoEXT.deviceAddress = gpuAddrs[i];
#endif
        bufferOpaqueCaptureAddressCreateInfo.opaqueCaptureAddress   = opaqueBufferAddrs[i];
        memoryOpaqueCaptureAddressAllocateInfo.opaqueCaptureAddress = opaqueMemoryAddrs[i];

        bufferCreateInfo.size = bufferSizes[i];
        buffers[i]            = VkBufferSp(new Unique<VkBuffer>(createBuffer(vk, device, &bufferCreateInfo)));
        allocations[i]        = AllocationSp(allocateExtended(vki, vk, physDevice, device,
                                                              getBufferMemoryRequirements(vk, device, **buffers[i]),
                                                              MemoryRequirement::HostVisible, &allocFlagsInfo));
        VK_CHECK(vk.bindBufferMemory(device, **buffers[i], allocations[i]->getMemory(), 0));

        bufferDeviceAddressInfo.buffer = **buffers[i];
        VkDeviceSize newAddr           = vk.getBufferDeviceAddress(device, &bufferDeviceAddressInfo);

        if (newAddr != gpuAddrs[i])
            return tcu::TestStatus(QP_TEST_RESULT_FAIL, "address mismatch");
    }

    return tcu::TestStatus(QP_TEST_RESULT_PASS, qpGetTestResultName(QP_TEST_RESULT_PASS));
}

class MemoryModelOffsetTestInstance : public TestInstance
{
public:
    MemoryModelOffsetTestInstance(Context &context) : vkt::TestInstance(context)
    {
    }
    ~MemoryModelOffsetTestInstance(void)
    {
    }

private:
    tcu::TestStatus iterate(void);
};

tcu::TestStatus MemoryModelOffsetTestInstance::iterate(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();
    Allocator &allocator      = m_context.getDefaultAllocator();
    const VkQueue queue       = m_context.getUniversalQueue();
    tcu::TestLog &log         = m_context.getTestContext().getLog();

    Move<VkCommandPool> cmdPool     = createCommandPool(vk, device, 0, m_context.getUniversalQueueFamilyIndex());
    Move<VkCommandBuffer> cmdBuffer = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    BufferWithMemory bdaBuffer(
        vk, device, allocator,
        makeBufferCreateInfo(nullptr, 256u * sizeof(uint32_t), VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, 0),
        MemoryRequirement::DeviceAddress);
    BufferWithMemory inBuffer(vk, device, allocator,
                              makeBufferCreateInfo(nullptr, 16u, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 0u),
                              MemoryRequirement::HostVisible);

    VkBufferDeviceAddressInfo bufferDeviceAddressInfo = {
        VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, // VkStructureType    sType;
        nullptr,                                      // const void*        pNext;
        *bdaBuffer,                                   // VkBuffer           buffer
    };

    VkDeviceAddress bdaAddress = vk.getBufferDeviceAddress(device, &bufferDeviceAddressInfo);

    VkDeviceAddress *inBufferPtr = reinterpret_cast<VkDeviceAddress *>(inBuffer.getAllocation().getHostPtr());
    inBufferPtr[0]               = bdaAddress;
    inBufferPtr[1]               = 0; // set SSBO.a and SSBO.b to be zero

    const Unique<VkShaderModule> shaderModule(
        createShaderModule(vk, device, m_context.getBinaryCollection().get("comp"), 0));

    DescriptorSetLayoutBuilder descriptorBuilder;
    descriptorBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT);

    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

    const auto descriptorSetLayout(descriptorBuilder.build(vk, device));
    const Move<vk::VkDescriptorPool> descriptorPool =
        poolBuilder.build(vk, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1);
    const Move<vk::VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

    VkDescriptorBufferInfo bufferInfo;
    bufferInfo.buffer = *inBuffer;
    bufferInfo.offset = 0u;
    bufferInfo.range  = VK_WHOLE_SIZE;

    vk::DescriptorSetUpdateBuilder updateBuilder;
    updateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                              vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferInfo);
    updateBuilder.update(vk, device);

    const Unique<VkPipelineLayout> pipelineLayout(makePipelineLayout(vk, device, *descriptorSetLayout));
    const Unique<VkPipeline> pipeline(makeComputePipeline(vk, device, *pipelineLayout, *shaderModule));

    beginCommandBuffer(vk, *cmdBuffer);
    vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0u, 1u, &*descriptorSet, 0u,
                             nullptr);
    vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);
    vk.cmdDispatch(*cmdBuffer, 1u, 1u, 1u);
    endCommandBuffer(vk, *cmdBuffer);
    submitCommandsAndWait(vk, device, queue, *cmdBuffer);
    invalidateAlloc(vk, device, inBuffer.getAllocation());

    auto in_buffer_u32_ptr = (uint32_t *)inBufferPtr;
    uint32_t expected      = (uint32_t)bdaAddress + 128u * (uint32_t)sizeof(uint32_t);
    if (in_buffer_u32_ptr[3] != (uint32_t)bdaAddress + 128u * sizeof(uint32_t))
    {
        log << tcu::TestLog::Message << "Expected value at index 3 in storage buffer was " << expected
            << ", but actual value is " << in_buffer_u32_ptr[3] << tcu::TestLog::EndMessage;
        return tcu::TestStatus::fail("Fail");
    }

    return tcu::TestStatus::pass("Pass");
}

class MemoryModelOffsetTestCase : public TestCase
{
public:
    MemoryModelOffsetTestCase(tcu::TestContext &context, const char *name) : vkt::TestCase(context, name)
    {
    }
    ~MemoryModelOffsetTestCase(void)
    {
    }
    virtual void initPrograms(SourceCollections &programCollection) const;
    virtual TestInstance *createInstance(Context &context) const
    {
        return new MemoryModelOffsetTestInstance(context);
    }
    virtual void checkSupport(Context &context) const;
};

void MemoryModelOffsetTestCase::initPrograms(SourceCollections &programCollection) const
{
    const SpirVAsmBuildOptions spvOptions(programCollection.usedVulkanVersion, SPIRV_VERSION_1_5);

    const char *spv_shader_source = R"(
               OpCapability Shader
               OpCapability Int64
               OpCapability VulkanMemoryModel
               OpCapability PhysicalStorageBufferAddresses
          %2 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel PhysicalStorageBuffer64 Vulkan
               OpEntryPoint GLCompute %main "main" %_ %sharedSkip
               OpExecutionMode %main LocalSize 1 1 1
               OpDecorate %SSBO Block
               OpMemberDecorate %SSBO 0 Offset 0
               OpMemberDecorate %SSBO 1 Offset 8
               OpMemberDecorate %SSBO 2 Offset 12
               OpDecorate %_runtimearr_uint ArrayStride 4
               OpDecorate %Node Block
               OpMemberDecorate %Node 0 Offset 0
               OpDecorate %_ Binding 0
               OpDecorate %_ DescriptorSet 0
       %void = OpTypeVoid
          %4 = OpTypeFunction %void
      %ulong = OpTypeInt 64 0
          %9 = OpTypeFunction %void %ulong
               OpTypeForwardPointer %_ptr_PhysicalStorageBuffer_Node PhysicalStorageBuffer
       %uint = OpTypeInt 32 0
       %SSBO = OpTypeStruct %_ptr_PhysicalStorageBuffer_Node %uint %uint
%_runtimearr_uint = OpTypeRuntimeArray %uint
       %Node = OpTypeStruct %_runtimearr_uint
%_ptr_PhysicalStorageBuffer_Node = OpTypePointer PhysicalStorageBuffer %Node
%_ptr_StorageBuffer_SSBO = OpTypePointer StorageBuffer %SSBO
          %_ = OpVariable %_ptr_StorageBuffer_SSBO StorageBuffer
        %int = OpTypeInt 32 1
      %int_2 = OpConstant %int 2
%_ptr_StorageBuffer_uint = OpTypePointer StorageBuffer %uint
      %int_0 = OpConstant %int 0
%_ptr_StorageBuffer__ptr_PhysicalStorageBuffer_Node = OpTypePointer StorageBuffer %_ptr_PhysicalStorageBuffer_Node
    %int_128 = OpConstant %int 128
%_ptr_PhysicalStorageBuffer_uint = OpTypePointer PhysicalStorageBuffer %uint
       %bool = OpTypeBool
%_ptr_Workgroup_bool = OpTypePointer Workgroup %bool
 %sharedSkip = OpVariable %_ptr_Workgroup_bool Workgroup
       %main = OpFunction %void None %4
          %6 = OpLabel
         %28 = OpAccessChain %_ptr_StorageBuffer__ptr_PhysicalStorageBuffer_Node %_ %int_0
         %29 = OpLoad %_ptr_PhysicalStorageBuffer_Node %28
         %32 = OpAccessChain %_ptr_PhysicalStorageBuffer_uint %29 %int_0 %int_128

         %param = OpConvertPtrToU %ulong %32
         %36 = OpFunctionCall %void %foo_u641_ %param
               OpReturn
               OpFunctionEnd
  %foo_u641_ = OpFunction %void None %9
          %x = OpFunctionParameter %ulong
         %12 = OpLabel
         %23 = OpUConvert %uint %x
         %25 = OpAccessChain %_ptr_StorageBuffer_uint %_ %int_2
               OpStore %25 %23
               OpReturn
               OpFunctionEnd
        )";

    programCollection.spirvAsmSources.add("comp") << spv_shader_source << spvOptions;
}

void MemoryModelOffsetTestCase::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality("VK_KHR_buffer_device_address");

    if (!context.isDeviceFunctionalitySupported("VK_KHR_vulkan_memory_model"))
        TCU_THROW(NotSupportedError, "Vulkan memory model not supported");

    VkPhysicalDeviceVulkanMemoryModelFeatures vkMemModelFeatures = context.getVulkanMemoryModelFeatures();
    if (!vkMemModelFeatures.vulkanMemoryModel)
        TCU_THROW(NotSupportedError, "vulkanMemoryModel not supported");

    if (!vkMemModelFeatures.vulkanMemoryModelDeviceScope)
        TCU_THROW(NotSupportedError, "vulkanMemoryModelDeviceScope not supported");

    context.requireDeviceCoreFeature(vkt::DEVICE_CORE_FEATURE_SHADER_INT64);
}

class FragmentStoreTestInstance : public TestInstance
{
public:
    FragmentStoreTestInstance(Context &context) : vkt::TestInstance(context)
    {
    }
    ~FragmentStoreTestInstance(void)
    {
    }

private:
    tcu::TestStatus iterate(void);
};

tcu::TestStatus FragmentStoreTestInstance::iterate(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();
    Allocator &allocator      = m_context.getDefaultAllocator();
    const VkQueue queue       = m_context.getUniversalQueue();
    tcu::TestLog &log         = m_context.getTestContext().getLog();

    Move<VkCommandPool> cmdPool     = createCommandPool(vk, device, 0, m_context.getUniversalQueueFamilyIndex());
    Move<VkCommandBuffer> cmdBuffer = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    const VkBufferUsageFlags bufferUsageFlags =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    BufferWithMemory printBuffer(vk, device, allocator, makeBufferCreateInfo(nullptr, 1024u, bufferUsageFlags, 0),
                                 MemoryRequirement::DeviceAddress | MemoryRequirement::HostVisible);
    BufferWithMemory rootNodeBuffer(vk, device, allocator, makeBufferCreateInfo(nullptr, 64u, bufferUsageFlags, 0),
                                    MemoryRequirement::DeviceAddress | MemoryRequirement::HostVisible);
    BufferWithMemory rootNodePtrBuffer(vk, device, allocator, makeBufferCreateInfo(nullptr, 64u, bufferUsageFlags, 0),
                                       MemoryRequirement::DeviceAddress | MemoryRequirement::HostVisible);

    VkBufferDeviceAddressInfo bufferDeviceAddressInfo = {
        VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, // VkStructureType    sType;
        nullptr,                                      // const void*        pNext;
        *printBuffer,                                 // VkBuffer           buffer
    };

    VkDeviceAddress printBufferAddress    = vk.getBufferDeviceAddress(device, &bufferDeviceAddressInfo);
    bufferDeviceAddressInfo.buffer        = *rootNodeBuffer;
    VkDeviceAddress rootNodeBufferAddress = vk.getBufferDeviceAddress(device, &bufferDeviceAddressInfo);

    uint32_t *printBufferPtr = reinterpret_cast<uint32_t *>(printBuffer.getAllocation().getHostPtr());
    memset(printBufferPtr, 0, 1024u);
    printBufferPtr[0] = uint32_t(1024u / sizeof(uint32_t));

    VkDeviceAddress *rootNodeBufferPtr =
        reinterpret_cast<VkDeviceAddress *>(rootNodeBuffer.getAllocation().getHostPtr());
    rootNodeBufferPtr[0] = printBufferAddress;

    VkDeviceAddress *rootNodePtrBufferPtr =
        reinterpret_cast<VkDeviceAddress *>(rootNodePtrBuffer.getAllocation().getHostPtr());
    rootNodePtrBufferPtr[0] = rootNodeBufferAddress;

    const Unique<VkShaderModule> vertModule(
        createShaderModule(vk, device, m_context.getBinaryCollection().get("vert")));
    const Unique<VkShaderModule> fragModule(
        createShaderModule(vk, device, m_context.getBinaryCollection().get("frag")));

    DescriptorSetLayoutBuilder descriptorBuilder;
    descriptorBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_FRAGMENT_BIT);

    DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

    const auto descriptorSetLayout(descriptorBuilder.build(vk, device));
    const Move<vk::VkDescriptorPool> descriptorPool =
        poolBuilder.build(vk, device, vk::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1);
    const Move<vk::VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

    VkDescriptorBufferInfo bufferInfo;
    bufferInfo.buffer = *rootNodePtrBuffer;
    bufferInfo.offset = 0u;
    bufferInfo.range  = VK_WHOLE_SIZE;

    vk::DescriptorSetUpdateBuilder updateBuilder;
    updateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u),
                              vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferInfo);
    updateBuilder.update(vk, device);

    VkAttachmentDescription attachmentDescription = {};
    attachmentDescription.format                  = VK_FORMAT_R8G8B8A8_UNORM;
    attachmentDescription.samples                 = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescription.loadOp                  = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescription.storeOp                 = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescription.initialLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
    attachmentDescription.finalLayout             = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentReference = {
        0u,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1u;
    subpass.pColorAttachments    = &colorAttachmentReference;

    VkRenderPassCreateInfo renderPassParams = vk::initVulkanStructure();
    renderPassParams.attachmentCount        = 1u;
    renderPassParams.pAttachments           = &attachmentDescription;
    renderPassParams.subpassCount           = 1u;
    renderPassParams.pSubpasses             = &subpass;

    Move<VkRenderPass> renderPass = createRenderPass(vk, device, &renderPassParams);

    VkImageCreateInfo imageCreateInfo = vk::initVulkanStructure();
    imageCreateInfo.imageType         = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format            = VK_FORMAT_R8G8B8A8_UNORM;
    imageCreateInfo.extent            = {32u, 32u, 1u};
    imageCreateInfo.mipLevels         = 1u;
    imageCreateInfo.arrayLayers       = 1u;
    imageCreateInfo.samples           = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling            = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage             = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    imageCreateInfo.initialLayout     = VK_IMAGE_LAYOUT_UNDEFINED;
    ImageWithMemory image(vk, device, allocator, imageCreateInfo, MemoryRequirement::Any);
    Move<VkImageView> imageView = makeImageView(vk, device, *image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM,
                                                makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u));

    VkFramebufferCreateInfo framebufferParams = vk::initVulkanStructure();
    framebufferParams.renderPass              = *renderPass;
    framebufferParams.attachmentCount         = 1u;
    framebufferParams.pAttachments            = &*imageView;
    framebufferParams.width                   = 32u;
    framebufferParams.height                  = 32u;
    framebufferParams.layers                  = 1u;

    Move<VkFramebuffer> framebuffer = createFramebuffer(vk, device, &framebufferParams);

    std::vector<VkViewport> viewports = {makeViewport(32u, 32u)};
    std::vector<VkRect2D> scissors    = {makeRect2D(32u, 32u)};

    VkPipelineVertexInputStateCreateInfo vertexInputState = vk::initVulkanStructure();

    const Unique<VkPipelineLayout> pipelineLayout(makePipelineLayout(vk, device, *descriptorSetLayout));
    const Unique<VkPipeline> pipeline(makeGraphicsPipeline(
        vk, device, *pipelineLayout, *vertModule, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, *fragModule,
        *renderPass, viewports, scissors, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0u, 0u, &vertexInputState));

    VkClearValue clearValue = makeClearValueColorU32(0u, 0u, 0u, 1u);

    VkRenderPassBeginInfo renderPassBeginInfo = vk::initVulkanStructure();
    renderPassBeginInfo.renderPass            = *renderPass;
    renderPassBeginInfo.framebuffer           = *framebuffer;
    renderPassBeginInfo.renderArea            = {{0, 0}, {32u, 32u}};
    renderPassBeginInfo.clearValueCount       = 1u;
    renderPassBeginInfo.pClearValues          = &clearValue;

    beginCommandBuffer(vk, *cmdBuffer);
    vk.cmdBeginRenderPass(*cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
    vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u, &*descriptorSet, 0u,
                             nullptr);
    vk.cmdDraw(*cmdBuffer, 3u, 1u, 0u, 0u);
    vk.cmdEndRenderPass(*cmdBuffer);
    endCommandBuffer(vk, *cmdBuffer);
    submitCommandsAndWait(vk, device, queue, *cmdBuffer);

    invalidateAlloc(vk, device, printBuffer.getAllocation());

    const uint32_t expectedValues[] = {256,        26,         13, 2, 67, 40, 0, 0, 0, 4, 1093140480, 1093140480, 0,
                                       1093140480, 1093140480, 13, 2, 67, 40, 0, 0, 0, 4, 1093140480, 1094189056, 0,
                                       1093140480, 1094189056};

    for (uint32_t i = 0; i < DE_LENGTH_OF_ARRAY(expectedValues); ++i)
    {
        if (printBufferPtr[i] != expectedValues[i])
        {
            log << tcu::TestLog::Message << "Expected value at index " << i << " in print buffer was "
                << expectedValues[i] << ", but actual value is " << printBufferPtr[i] << tcu::TestLog::EndMessage;
            return tcu::TestStatus::fail("Fail");
        }
    }

    return tcu::TestStatus::pass("Pass");
}

class FragmentStoreTestCase : public TestCase
{
public:
    FragmentStoreTestCase(tcu::TestContext &context, const char *name) : vkt::TestCase(context, name)
    {
    }
    ~FragmentStoreTestCase(void)
    {
    }
    virtual void initPrograms(SourceCollections &programCollection) const;
    virtual TestInstance *createInstance(Context &context) const
    {
        return new FragmentStoreTestInstance(context);
    }
    virtual void checkSupport(Context &context) const;
};

void FragmentStoreTestCase::initPrograms(SourceCollections &programCollection) const
{
    std::stringstream vss;
    vss << "#version 450\n"
           "vec2 vertices[3];\n"
           "void main(){\n"
           "    vertices[0] = vec2(-1.0, -1.0);\n"
           "    vertices[1] = vec2( 1.0, -1.0);\n"
           "    vertices[2] = vec2( 0.0,  1.0);\n"
           "    gl_Position = vec4(vertices[gl_VertexIndex % 3], 0.0, 1.0);\n"
           "}\n";

    programCollection.glslSources.add("vert") << glu::VertexSource(vss.str());

    const char *spv_shader_source = R"(
; SPIR-V
; Version: 1.0
; Generator: Khronos Glslang Reference Front End; 11
; Bound: 192
; Schema: 0
               OpCapability Shader
               OpCapability PhysicalStorageBufferAddresses
               OpExtension "SPV_KHR_non_semantic_info"
               OpExtension "SPV_KHR_physical_storage_buffer"
               OpExtension "SPV_KHR_storage_buffer_storage_class"
          %1 = OpExtInstImport "GLSL.std.450"
         %45 = OpExtInstImport "NonSemantic.DebugPrintf"
               OpMemoryModel PhysicalStorageBuffer64 GLSL450
               OpEntryPoint Fragment %main "main" %gl_FragCoord %outColor
               OpExecutionMode %main OriginUpperLeft
         %40 = OpString "gl_FragCoord.xy %1.2f, %1.2f
"
               OpSource GLSL 450
               OpSourceExtension "GL_EXT_debug_printf"
               OpName %main "main"
               OpName %gl_FragCoord "gl_FragCoord"
               OpName %outColor "outColor"
               OpName %inst_debug_printf_13 "inst_debug_printf_13"
               OpDecorate %gl_FragCoord BuiltIn FragCoord
               OpDecorate %outColor Location 0
               OpDecorate %_runtimearr_uint ArrayStride 4
               OpDecorate %_struct_64 Block
               OpMemberDecorate %_struct_64 0 Offset 0
               OpMemberDecorate %_struct_64 1 Offset 4
               OpMemberDecorate %_struct_64 2 Offset 8
               OpDecorate %_struct_66 Block
               OpMemberDecorate %_struct_66 0 Offset 0
               OpDecorate %_struct_68 Block
               OpMemberDecorate %_struct_68 0 Offset 0
               OpDecorate %70 DescriptorSet 0
               OpDecorate %70 Binding 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
       %bool = OpTypeBool
      %float = OpTypeFloat 32
    %v4float = OpTypeVector %float 4
%_ptr_Input_v4float = OpTypePointer Input %v4float
%gl_FragCoord = OpVariable %_ptr_Input_v4float Input
       %uint = OpTypeInt 32 0
     %uint_0 = OpConstant %uint 0
%_ptr_Input_float = OpTypePointer Input %float
   %float_10 = OpConstant %float 10
   %float_11 = OpConstant %float 11
     %uint_1 = OpConstant %uint 1
   %float_12 = OpConstant %float 12
%_ptr_Output_v4float = OpTypePointer Output %v4float
   %outColor = OpVariable %_ptr_Output_v4float Output
     %v4uint = OpTypeVector %uint 4
     %uint_4 = OpConstant %uint 4
    %uint_67 = OpConstant %uint 67
    %uint_40 = OpConstant %uint 40
%_runtimearr_uint = OpTypeRuntimeArray %uint
 %_struct_64 = OpTypeStruct %uint %uint %_runtimearr_uint
%_ptr_PhysicalStorageBuffer__struct_64 = OpTypePointer PhysicalStorageBuffer %_struct_64
 %_struct_66 = OpTypeStruct %_ptr_PhysicalStorageBuffer__struct_64
%_ptr_PhysicalStorageBuffer__struct_66 = OpTypePointer PhysicalStorageBuffer %_struct_66
 %_struct_68 = OpTypeStruct %_ptr_PhysicalStorageBuffer__struct_66
%_ptr_StorageBuffer__struct_68 = OpTypePointer StorageBuffer %_struct_68
         %70 = OpVariable %_ptr_StorageBuffer__struct_68 StorageBuffer
         %71 = OpTypeFunction %void %uint %uint %uint %uint %uint %uint %uint %uint %uint %uint %uint
%_ptr_StorageBuffer__ptr_PhysicalStorageBuffer__struct_66 = OpTypePointer StorageBuffer %_ptr_PhysicalStorageBuffer__struct_66
%_ptr_PhysicalStorageBuffer__ptr_PhysicalStorageBuffer__struct_64 = OpTypePointer PhysicalStorageBuffer %_ptr_PhysicalStorageBuffer__struct_64
%_ptr_PhysicalStorageBuffer_uint = OpTypePointer PhysicalStorageBuffer %uint
    %uint_13 = OpConstant %uint 13
     %uint_2 = OpConstant %uint 2
     %uint_3 = OpConstant %uint 3
     %uint_5 = OpConstant %uint 5
     %uint_6 = OpConstant %uint 6
     %uint_7 = OpConstant %uint 7
     %uint_8 = OpConstant %uint 8
     %uint_9 = OpConstant %uint 9
    %uint_10 = OpConstant %uint 10
    %uint_11 = OpConstant %uint 11
    %uint_12 = OpConstant %uint 12
       %main = OpFunction %void None %3
          %5 = OpLabel
         %52 = OpLoad %v4float %gl_FragCoord
         %53 = OpBitcast %v4uint %52
         %54 = OpCompositeExtract %uint %53 0
         %55 = OpCompositeExtract %uint %53 1
         %56 = OpCompositeConstruct %v4uint %uint_4 %54 %55 %uint_0
         %14 = OpAccessChain %_ptr_Input_float %gl_FragCoord %uint_0
         %15 = OpLoad %float %14
         %17 = OpFOrdGreaterThan %bool %15 %float_10
               OpSelectionMerge %19 None
               OpBranchConditional %17 %18 %19
         %18 = OpLabel
         %20 = OpAccessChain %_ptr_Input_float %gl_FragCoord %uint_0
         %21 = OpLoad %float %20
         %23 = OpFOrdLessThan %bool %21 %float_11
               OpBranch %19
         %19 = OpLabel
         %24 = OpPhi %bool %17 %5 %23 %18
               OpSelectionMerge %26 None
               OpBranchConditional %24 %25 %26
         %25 = OpLabel
         %28 = OpAccessChain %_ptr_Input_float %gl_FragCoord %uint_1
         %29 = OpLoad %float %28
         %30 = OpFOrdGreaterThan %bool %29 %float_10
               OpSelectionMerge %32 None
               OpBranchConditional %30 %31 %32
         %31 = OpLabel
         %33 = OpAccessChain %_ptr_Input_float %gl_FragCoord %uint_1
         %34 = OpLoad %float %33
         %36 = OpFOrdLessThan %bool %34 %float_12
               OpBranch %32
         %32 = OpLabel
         %37 = OpPhi %bool %30 %25 %36 %31
               OpSelectionMerge %39 None
               OpBranchConditional %37 %38 %39
         %38 = OpLabel
         %41 = OpAccessChain %_ptr_Input_float %gl_FragCoord %uint_0
         %42 = OpLoad %float %41
         %43 = OpAccessChain %_ptr_Input_float %gl_FragCoord %uint_1
         %44 = OpLoad %float %43
         %60 = OpBitcast %uint %42
         %61 = OpBitcast %uint %44
         %59 = OpFunctionCall %void %inst_debug_printf_13 %uint_67 %uint_40 %uint_0 %uint_0 %uint_0 %uint_4 %54 %55 %uint_0 %60 %61
         %46 = OpExtInst %void %45 1 %40 %42 %44
               OpBranch %39
         %39 = OpLabel
               OpBranch %26
         %26 = OpLabel
         %49 = OpLoad %v4float %gl_FragCoord
               OpStore %outColor %49
               OpReturn
               OpFunctionEnd
%inst_debug_printf_13 = OpFunction %void None %71
         %72 = OpFunctionParameter %uint
         %73 = OpFunctionParameter %uint
         %74 = OpFunctionParameter %uint
         %75 = OpFunctionParameter %uint
         %76 = OpFunctionParameter %uint
         %77 = OpFunctionParameter %uint
         %78 = OpFunctionParameter %uint
         %79 = OpFunctionParameter %uint
         %80 = OpFunctionParameter %uint
         %81 = OpFunctionParameter %uint
         %82 = OpFunctionParameter %uint
         %83 = OpLabel
         %90 = OpAccessChain %_ptr_StorageBuffer__ptr_PhysicalStorageBuffer__struct_66 %70 %uint_0
         %91 = OpLoad %_ptr_PhysicalStorageBuffer__struct_66 %90
         %92 = OpAccessChain %_ptr_PhysicalStorageBuffer__ptr_PhysicalStorageBuffer__struct_64 %91 %uint_0
         %93 = OpLoad %_ptr_PhysicalStorageBuffer__struct_64 %92 Aligned 4
         %94 = OpAccessChain %_ptr_PhysicalStorageBuffer_uint %93 %uint_1
         %95 = OpAtomicIAdd %uint %94 %uint_4 %uint_0 %uint_13
         %96 = OpIAdd %uint %95 %uint_13
         %97 = OpAccessChain %_ptr_StorageBuffer__ptr_PhysicalStorageBuffer__struct_66 %70 %uint_0
         %98 = OpLoad %_ptr_PhysicalStorageBuffer__struct_66 %97
         %99 = OpAccessChain %_ptr_PhysicalStorageBuffer__ptr_PhysicalStorageBuffer__struct_64 %98 %uint_0
        %100 = OpLoad %_ptr_PhysicalStorageBuffer__struct_64 %99 Aligned 4
        %101 = OpAccessChain %_ptr_PhysicalStorageBuffer_uint %100 %uint_0
        %102 = OpLoad %uint %101 Aligned 4
        %103 = OpULessThanEqual %bool %96 %102
               OpSelectionMerge %85 None
               OpBranchConditional %103 %84 %85
         %84 = OpLabel
        %104 = OpIAdd %uint %95 %uint_0
        %105 = OpAccessChain %_ptr_StorageBuffer__ptr_PhysicalStorageBuffer__struct_66 %70 %uint_0
        %106 = OpLoad %_ptr_PhysicalStorageBuffer__struct_66 %105
        %107 = OpAccessChain %_ptr_PhysicalStorageBuffer__ptr_PhysicalStorageBuffer__struct_64 %106 %uint_0
        %108 = OpLoad %_ptr_PhysicalStorageBuffer__struct_64 %107 Aligned 4
        %109 = OpAccessChain %_ptr_PhysicalStorageBuffer_uint %108 %uint_2 %104
               OpStore %109 %uint_13 Aligned 4
        %111 = OpIAdd %uint %95 %uint_1
        %112 = OpAccessChain %_ptr_StorageBuffer__ptr_PhysicalStorageBuffer__struct_66 %70 %uint_0
        %113 = OpLoad %_ptr_PhysicalStorageBuffer__struct_66 %112
        %114 = OpAccessChain %_ptr_PhysicalStorageBuffer__ptr_PhysicalStorageBuffer__struct_64 %113 %uint_0
        %115 = OpLoad %_ptr_PhysicalStorageBuffer__struct_64 %114 Aligned 4
        %116 = OpAccessChain %_ptr_PhysicalStorageBuffer_uint %115 %uint_2 %111
               OpStore %116 %uint_2 Aligned 4
        %117 = OpIAdd %uint %95 %uint_2
        %118 = OpAccessChain %_ptr_StorageBuffer__ptr_PhysicalStorageBuffer__struct_66 %70 %uint_0
        %119 = OpLoad %_ptr_PhysicalStorageBuffer__struct_66 %118
        %120 = OpAccessChain %_ptr_PhysicalStorageBuffer__ptr_PhysicalStorageBuffer__struct_64 %119 %uint_0
        %121 = OpLoad %_ptr_PhysicalStorageBuffer__struct_64 %120 Aligned 4
        %122 = OpAccessChain %_ptr_PhysicalStorageBuffer_uint %121 %uint_2 %117
               OpStore %122 %72 Aligned 4
        %123 = OpIAdd %uint %95 %uint_3
        %125 = OpAccessChain %_ptr_StorageBuffer__ptr_PhysicalStorageBuffer__struct_66 %70 %uint_0
        %126 = OpLoad %_ptr_PhysicalStorageBuffer__struct_66 %125
        %127 = OpAccessChain %_ptr_PhysicalStorageBuffer__ptr_PhysicalStorageBuffer__struct_64 %126 %uint_0
        %128 = OpLoad %_ptr_PhysicalStorageBuffer__struct_64 %127 Aligned 4
        %129 = OpAccessChain %_ptr_PhysicalStorageBuffer_uint %128 %uint_2 %123
               OpStore %129 %73 Aligned 4
        %130 = OpIAdd %uint %95 %uint_4
        %131 = OpAccessChain %_ptr_StorageBuffer__ptr_PhysicalStorageBuffer__struct_66 %70 %uint_0
        %132 = OpLoad %_ptr_PhysicalStorageBuffer__struct_66 %131
        %133 = OpAccessChain %_ptr_PhysicalStorageBuffer__ptr_PhysicalStorageBuffer__struct_64 %132 %uint_0
        %134 = OpLoad %_ptr_PhysicalStorageBuffer__struct_64 %133 Aligned 4
        %135 = OpAccessChain %_ptr_PhysicalStorageBuffer_uint %134 %uint_2 %130
               OpStore %135 %74 Aligned 4
        %136 = OpIAdd %uint %95 %uint_5
        %138 = OpAccessChain %_ptr_StorageBuffer__ptr_PhysicalStorageBuffer__struct_66 %70 %uint_0
        %139 = OpLoad %_ptr_PhysicalStorageBuffer__struct_66 %138
        %140 = OpAccessChain %_ptr_PhysicalStorageBuffer__ptr_PhysicalStorageBuffer__struct_64 %139 %uint_0
        %141 = OpLoad %_ptr_PhysicalStorageBuffer__struct_64 %140 Aligned 4
        %142 = OpAccessChain %_ptr_PhysicalStorageBuffer_uint %141 %uint_2 %136
               OpStore %142 %75 Aligned 4
        %143 = OpIAdd %uint %95 %uint_6
        %145 = OpAccessChain %_ptr_StorageBuffer__ptr_PhysicalStorageBuffer__struct_66 %70 %uint_0
        %146 = OpLoad %_ptr_PhysicalStorageBuffer__struct_66 %145
        %147 = OpAccessChain %_ptr_PhysicalStorageBuffer__ptr_PhysicalStorageBuffer__struct_64 %146 %uint_0
        %148 = OpLoad %_ptr_PhysicalStorageBuffer__struct_64 %147 Aligned 4
        %149 = OpAccessChain %_ptr_PhysicalStorageBuffer_uint %148 %uint_2 %143
               OpStore %149 %76 Aligned 4
        %150 = OpIAdd %uint %95 %uint_7
        %152 = OpAccessChain %_ptr_StorageBuffer__ptr_PhysicalStorageBuffer__struct_66 %70 %uint_0
        %153 = OpLoad %_ptr_PhysicalStorageBuffer__struct_66 %152
        %154 = OpAccessChain %_ptr_PhysicalStorageBuffer__ptr_PhysicalStorageBuffer__struct_64 %153 %uint_0
        %155 = OpLoad %_ptr_PhysicalStorageBuffer__struct_64 %154 Aligned 4
        %156 = OpAccessChain %_ptr_PhysicalStorageBuffer_uint %155 %uint_2 %150
               OpStore %156 %77 Aligned 4
        %157 = OpIAdd %uint %95 %uint_8
        %159 = OpAccessChain %_ptr_StorageBuffer__ptr_PhysicalStorageBuffer__struct_66 %70 %uint_0
        %160 = OpLoad %_ptr_PhysicalStorageBuffer__struct_66 %159
        %161 = OpAccessChain %_ptr_PhysicalStorageBuffer__ptr_PhysicalStorageBuffer__struct_64 %160 %uint_0
        %162 = OpLoad %_ptr_PhysicalStorageBuffer__struct_64 %161 Aligned 4
        %163 = OpAccessChain %_ptr_PhysicalStorageBuffer_uint %162 %uint_2 %157
               OpStore %163 %78 Aligned 4
        %164 = OpIAdd %uint %95 %uint_9
        %166 = OpAccessChain %_ptr_StorageBuffer__ptr_PhysicalStorageBuffer__struct_66 %70 %uint_0
        %167 = OpLoad %_ptr_PhysicalStorageBuffer__struct_66 %166
        %168 = OpAccessChain %_ptr_PhysicalStorageBuffer__ptr_PhysicalStorageBuffer__struct_64 %167 %uint_0
        %169 = OpLoad %_ptr_PhysicalStorageBuffer__struct_64 %168 Aligned 4
        %170 = OpAccessChain %_ptr_PhysicalStorageBuffer_uint %169 %uint_2 %164
               OpStore %170 %79 Aligned 4
        %171 = OpIAdd %uint %95 %uint_10
        %173 = OpAccessChain %_ptr_StorageBuffer__ptr_PhysicalStorageBuffer__struct_66 %70 %uint_0
        %174 = OpLoad %_ptr_PhysicalStorageBuffer__struct_66 %173
        %175 = OpAccessChain %_ptr_PhysicalStorageBuffer__ptr_PhysicalStorageBuffer__struct_64 %174 %uint_0
        %176 = OpLoad %_ptr_PhysicalStorageBuffer__struct_64 %175 Aligned 4
        %177 = OpAccessChain %_ptr_PhysicalStorageBuffer_uint %176 %uint_2 %171
               OpStore %177 %80 Aligned 4
        %178 = OpIAdd %uint %95 %uint_11
        %180 = OpAccessChain %_ptr_StorageBuffer__ptr_PhysicalStorageBuffer__struct_66 %70 %uint_0
        %181 = OpLoad %_ptr_PhysicalStorageBuffer__struct_66 %180
        %182 = OpAccessChain %_ptr_PhysicalStorageBuffer__ptr_PhysicalStorageBuffer__struct_64 %181 %uint_0
        %183 = OpLoad %_ptr_PhysicalStorageBuffer__struct_64 %182 Aligned 4
        %184 = OpAccessChain %_ptr_PhysicalStorageBuffer_uint %183 %uint_2 %178
               OpStore %184 %81 Aligned 4
        %185 = OpIAdd %uint %95 %uint_12
        %187 = OpAccessChain %_ptr_StorageBuffer__ptr_PhysicalStorageBuffer__struct_66 %70 %uint_0
        %188 = OpLoad %_ptr_PhysicalStorageBuffer__struct_66 %187
        %189 = OpAccessChain %_ptr_PhysicalStorageBuffer__ptr_PhysicalStorageBuffer__struct_64 %188 %uint_0
        %190 = OpLoad %_ptr_PhysicalStorageBuffer__struct_64 %189 Aligned 4
        %191 = OpAccessChain %_ptr_PhysicalStorageBuffer_uint %190 %uint_2 %185
               OpStore %191 %82 Aligned 4
               OpBranch %85
         %85 = OpLabel
               OpReturn
               OpFunctionEnd
    )";

    programCollection.spirvAsmSources.add("frag") << spv_shader_source;
}

void FragmentStoreTestCase::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality("VK_KHR_buffer_device_address");

    context.requireDeviceCoreFeature(vkt::DEVICE_CORE_FEATURE_FRAGMENT_STORES_AND_ATOMICS);
}

} // namespace

tcu::TestCaseGroup *createBufferDeviceAddressTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "buffer_device_address"));

    typedef struct
    {
        uint32_t count;
        const char *name;
    } TestGroupCase;

    TestGroupCase setCases[] = {
        {0, "set0"}, {3, "set3"}, {7, "set7"}, {15, "set15"}, {31, "set31"},
    };

    TestGroupCase depthCases[] = {
        {1, "depth1"},
        {2, "depth2"},
        {3, "depth3"},
    };

    TestGroupCase baseCases[] = {
        {BASE_UBO, "baseubo"},
        {BASE_SSBO, "basessbo"},
    };

    TestGroupCase cvtCases[] = {
        // load reference
        {CONVERT_NONE, "load"},
        // load and convert reference
        {CONVERT_UINT64, "convert"},
        // load and convert reference to uvec2
        {CONVERT_UVEC2, "convertuvec2"},
        // load, convert and compare references as uint64_t
        {CONVERT_U64CMP, "convertchecku64"},
        // load, convert and compare references as uvec2
        {CONVERT_UVEC2CMP, "convertcheckuv2"},
        // load reference as uint64_t and convert it to uvec2
        {CONVERT_UVEC2TOU64, "crossconvertu2p"},
        // load reference as uvec2 and convert it to uint64_t
        {CONVERT_U64TOUVEC2, "crossconvertp2u"},
    };

    TestGroupCase storeCases[] = {
        // don't store intermediate reference
        {0, "nostore"},
        // store intermediate reference
        {1, "store"},
    };

    TestGroupCase btCases[] = {
        // single buffer
        {BT_SINGLE, "single"},
        // multiple buffers
        {BT_MULTI, "multi"},
        // multiple buffers and VK_BUFFER_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT_EXT
        {BT_REPLAY, "replay"},
    };

    TestGroupCase layoutCases[] = {
        {LAYOUT_STD140, "std140"},
        {LAYOUT_SCALAR, "scalar"},
    };

    TestGroupCase stageCases[] = {
        {STAGE_COMPUTE, "comp"},
        {STAGE_FRAGMENT, "frag"},
        {STAGE_VERTEX, "vert"},
#if ENABLE_RAYTRACING
        // raygen
        {STAGE_RAYGEN, "rgen"},
#endif
    };

    TestGroupCase offsetCases[] = {
        {OFFSET_ZERO, "offset_zero"},
        {OFFSET_NONZERO, "offset_nonzero"},
    };

    for (int setNdx = 0; setNdx < DE_LENGTH_OF_ARRAY(setCases); setNdx++)
    {
        de::MovePtr<tcu::TestCaseGroup> setGroup(new tcu::TestCaseGroup(testCtx, setCases[setNdx].name));
        for (int depthNdx = 0; depthNdx < DE_LENGTH_OF_ARRAY(depthCases); depthNdx++)
        {
            de::MovePtr<tcu::TestCaseGroup> depthGroup(new tcu::TestCaseGroup(testCtx, depthCases[depthNdx].name));
            for (int baseNdx = 0; baseNdx < DE_LENGTH_OF_ARRAY(baseCases); baseNdx++)
            {
                de::MovePtr<tcu::TestCaseGroup> baseGroup(new tcu::TestCaseGroup(testCtx, baseCases[baseNdx].name));
                for (int cvtNdx = 0; cvtNdx < DE_LENGTH_OF_ARRAY(cvtCases); cvtNdx++)
                {
                    de::MovePtr<tcu::TestCaseGroup> cvtGroup(new tcu::TestCaseGroup(testCtx, cvtCases[cvtNdx].name));
                    for (int storeNdx = 0; storeNdx < DE_LENGTH_OF_ARRAY(storeCases); storeNdx++)
                    {
                        de::MovePtr<tcu::TestCaseGroup> storeGroup(
                            new tcu::TestCaseGroup(testCtx, storeCases[storeNdx].name));
                        for (int btNdx = 0; btNdx < DE_LENGTH_OF_ARRAY(btCases); btNdx++)
                        {
                            de::MovePtr<tcu::TestCaseGroup> btGroup(
                                new tcu::TestCaseGroup(testCtx, btCases[btNdx].name));
                            for (int layoutNdx = 0; layoutNdx < DE_LENGTH_OF_ARRAY(layoutCases); layoutNdx++)
                            {
                                de::MovePtr<tcu::TestCaseGroup> layoutGroup(
                                    new tcu::TestCaseGroup(testCtx, layoutCases[layoutNdx].name));
                                for (int stageNdx = 0; stageNdx < DE_LENGTH_OF_ARRAY(stageCases); stageNdx++)
                                {
                                    for (int offsetNdx = 0; offsetNdx < DE_LENGTH_OF_ARRAY(offsetCases); offsetNdx++)
                                    {
                                        CaseDef c = {
                                            setCases[setNdx].count,                     // uint32_t set;
                                            depthCases[depthNdx].count,                 // uint32_t depth;
                                            (Base)baseCases[baseNdx].count,             // Base base;
                                            (Stage)stageCases[stageNdx].count,          // Stage stage;
                                            (Convert)cvtCases[cvtNdx].count,            // Convert convertUToPtr;
                                            !!storeCases[storeNdx].count,               // bool storeInLocal;
                                            (BufType)btCases[btNdx].count,              // BufType bufType;
                                            (Layout)layoutCases[layoutNdx].count,       // Layout layout;
                                            (MemoryOffset)offsetCases[offsetNdx].count, // Memory Offset;
                                        };

                                        // Skip more complex test cases for most descriptor sets, to reduce runtime.
                                        if (c.set != 3 && (c.depth == 3 || c.layout != LAYOUT_STD140))
                                            continue;

                                        // Memory offset tests are only for single buffer test cases.
                                        if (c.memoryOffset == OFFSET_NONZERO && c.bufType != BT_SINGLE)
                                            continue;

                                        std::ostringstream caseName;
                                        caseName << stageCases[stageNdx].name;
                                        if (c.memoryOffset == OFFSET_NONZERO)
                                            caseName << "_offset_nonzero";

                                        layoutGroup->addChild(
                                            new BufferAddressTestCase(testCtx, caseName.str().c_str(), c));
                                    }
                                }
                                btGroup->addChild(layoutGroup.release());
                            }
                            storeGroup->addChild(btGroup.release());
                        }
                        cvtGroup->addChild(storeGroup.release());
                    }
                    baseGroup->addChild(cvtGroup.release());
                }
                depthGroup->addChild(baseGroup.release());
            }
            setGroup->addChild(depthGroup.release());
        }
        group->addChild(setGroup.release());
    }

    de::MovePtr<tcu::TestCaseGroup> capGroup(new tcu::TestCaseGroup(testCtx, "capture_replay_stress"));
    for (uint32_t i = 0; i < 10; ++i)
    {
        capGroup->addChild(new CaptureReplayTestCase(testCtx, (std::string("seed_") + de::toString(i)).c_str(), i));
    }
    group->addChild(capGroup.release());

    de::MovePtr<tcu::TestCaseGroup> memoryModelGroup(new tcu::TestCaseGroup(testCtx, "op_access_chain"));
    {
        memoryModelGroup->addChild(new MemoryModelOffsetTestCase(testCtx, "memory_model_offset"));
        memoryModelGroup->addChild(new FragmentStoreTestCase(testCtx, "fragment_store"));
    }
    group->addChild(memoryModelGroup.release());
    return group.release();
}

} // namespace BindingModel
} // namespace vkt
