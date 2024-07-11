/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
 * Copyright (c) 2018-2020 NVIDIA Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "Licensehelper
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
 * \brief Vulkan Reconvergence tests
 *//*--------------------------------------------------------------------*/

#include "vktReconvergenceTests.hpp"

#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkQueryUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkObjUtil.hpp"

#include "vktTestGroupUtil.hpp"
#include "vktTestCase.hpp"
#include "vktAmberTestCase.hpp"

#include "deDefs.h"
#include "deFloat16.h"
#include "deMath.h"
#include "deRandom.h"
#include "deSharedPtr.hpp"
#include "deString.h"

#include "tcuTestCase.hpp"
#include "tcuTestLog.hpp"

#include <array>
#include <bitset>
#include <functional>
#include <map>
#include <numeric>
#include <random>
#include <string>
#include <sstream>
#include <set>
#include <type_traits>
#include <vector>
#include <memory>
#include <cmath>
#include <initializer_list>

#include <iostream>

// #define INCLUDE_GRAPHICS_TESTS

namespace vkt
{
namespace Reconvergence
{
namespace
{
using namespace vk;
using namespace std;

#define ARRAYSIZE(x) (sizeof(x) / sizeof(x[0]))
#define ROUNDUP(x__, multipler__) ((((x__) + ((multipler__)-1)) / (multipler__)) * (multipler__))
#define ROUNDDOWN(x__, multipler__) (((x__) / (multipler__)) * (multipler__))
constexpr uint32_t MAX_INVOCATIONS_ALL_TESTS = 64 * 64;
typedef std::bitset<MAX_INVOCATIONS_ALL_TESTS> bitset_inv_t;
//constexpr bitset_inv_t MAGIC_BALLOT = 0x12345678;

typedef enum
{
    TT_SUCF_ELECT,  // subgroup_uniform_control_flow using elect (subgroup_basic)
    TT_SUCF_BALLOT, // subgroup_uniform_control_flow using ballot (subgroup_ballot)
    TT_WUCF_ELECT,  // workgroup uniform control flow using elect (subgroup_basic)
    TT_WUCF_BALLOT, // workgroup uniform control flow using ballot (subgroup_ballot)
    TT_MAXIMAL,     // maximal reconvergence
} TestType;

static_assert(VK_TRUE == 1, "VK_TRUE must equal 1");

struct CaseDef
{
    VkShaderStageFlagBits shaderStage;
    TestType testType;
    uint32_t maxNesting;
    uint32_t seed;
    // In the case of compute shader below sizes would be local_size_x and local_size_y respectively.
    // In the case of fragment shader these sizes would define framebuffer dimensions.
    uint32_t sizeX;
    uint32_t sizeY;

    bool isWUCF() const
    {
        return testType == TT_WUCF_ELECT || testType == TT_WUCF_BALLOT;
    }
    bool isSUCF() const
    {
        return testType == TT_SUCF_ELECT || testType == TT_SUCF_BALLOT;
    }
    bool isUCF() const
    {
        return isWUCF() || isSUCF();
    }
    bool isElect() const
    {
        return testType == TT_WUCF_ELECT || testType == TT_SUCF_ELECT;
    }

    bool verify() const
    {
        return (sizeX * sizeY) <= MAX_INVOCATIONS_ALL_TESTS;
    }
};

template <class T, class P = T (*)[1], class R = decltype(std::begin(*std::declval<P>()))>
static auto makeStdBeginEnd(void *p, uint32_t n) -> std::pair<R, R>
{
    auto tmp   = std::begin(*P(p));
    auto begin = tmp;
    std::advance(tmp, n);
    return {begin, tmp};
}

template <class R>
using add_ref = typename std::add_lvalue_reference<R>::type;
template <class R>
using add_cref = typename std::add_lvalue_reference<typename std::add_const<R>::type>::type;
template <class X>
using add_ptr = std::add_pointer_t<X>;
template <class X>
using add_cptr = std::add_pointer_t<std::add_const_t<X>>;

template <class RndIter>
RndIter max_element(RndIter first, RndIter last)
{
    RndIter max = last;
    if (first != last)
    {
        for (max = first, ++first; first != last; ++first)
        {
            if (*first > *max)
                max = first;
        }
    }
    return max;
}

template <class RndIter, class Selector>
RndIter max_element(RndIter first, RndIter last, Selector selector)
{
    RndIter max = last;
    if (first != last)
    {
        for (max = first, ++first; first != last; ++first)
        {
            if (selector(*first) > selector(*max))
                max = first;
        }
    }
    return max;
}

struct Ballot : public std::bitset<128>
{
    typedef std::bitset<128> super;
    Ballot() : super()
    {
    }
    Ballot(add_cref<super> ballot, uint32_t printbits = 128u) : super(ballot), m_bits(printbits)
    {
    }
    Ballot(add_cref<tcu::UVec4> ballot, uint32_t printbits = 128u) : super(), m_bits(printbits)
    {
        *this = ballot;
    }
    Ballot(uint64_t val, uint32_t printbits = 128u) : super(val), m_bits(printbits)
    {
    }
    static Ballot withSetBit(uint32_t bit)
    {
        Ballot b;
        b.set(bit);
        return b;
    }
    constexpr uint32_t size() const
    {
        return static_cast<uint32_t>(super::size());
    }
    operator tcu::UVec4() const
    {
        tcu::UVec4 result;
        super ballot(*this);
        const super mask = 0xFFFFFFFF;
        for (uint32_t k = 0; k < 4u; ++k)
        {
            result[k] = uint32_t((ballot & mask).to_ulong());
            ballot >>= 32;
        }
        return result;
    }
    add_ref<Ballot> operator=(add_cref<tcu::UVec4> vec)
    {
        for (uint32_t k = 0; k < 4u; ++k)
        {
            (*this) <<= 32;
            (*this) |= vec[3 - k];
        }
        return *this;
    }
    DE_UNUSED_FUNCTION uint32_t getw() const
    {
        return m_bits;
    }
    DE_UNUSED_FUNCTION void setw(uint32_t bits)
    {
        m_bits = bits;
    }
    DE_UNUSED_FUNCTION friend add_ref<std::ostream> operator<<(add_ref<std::ostream> str, add_cref<Ballot> ballot)
    {
        for (uint32_t i = 0u; i < ballot.m_bits && i < 128u; ++i)
        {
            str << (ballot[ballot.m_bits - i - 1u] ? '1' : '0');
        }
        return str;
    }

protected:
    uint32_t m_bits;
};

struct Ballots : protected std::vector<std::bitset<128>>
{
    typedef std::vector<value_type> super;
    static const constexpr uint32_t subgroupInvocationSize = static_cast<uint32_t>(value_type().size());
    Ballots() : super()
    {
    }
    explicit Ballots(uint32_t subgroupCount, add_cref<value_type> ballot = {}) : super(subgroupCount)
    {
        if (ballot.any())
            *this = ballot;
    }
    Ballots(add_cref<Ballots> other) : super(upcast(other))
    {
    }
    using super::operator[];
    using super::at;
    /**
     * @brief size method
     * @return Returns the number of bits that the Ballots holds.
     */
    uint32_t size() const
    {
        return static_cast<uint32_t>(super::size() * subgroupInvocationSize);
    }
    /**
     * @brief count method
     * @return Returns the number of bits that are set to true.
     */
    uint32_t count() const
    {
        uint32_t n = 0u;
        for (add_cref<value_type> b : *this)
            n += static_cast<uint32_t>(b.count());
        return n;
    }
    /**
     * @brief count method
     * @return Returns the number of bits that are set to true in given subgroup.
     */
    uint32_t count(uint32_t subgroup) const
    {
        DE_ASSERT(subgroup < subgroupCount());
        return static_cast<uint32_t>(at(subgroup).count());
    }
    uint32_t subgroupCount() const
    {
        return static_cast<uint32_t>(super::size());
    }
    bool test(uint32_t bit) const
    {
        DE_ASSERT(bit < size());
        return at(bit / subgroupInvocationSize).test(bit % subgroupInvocationSize);
    }
    bool set(uint32_t bit, bool value = true)
    {
        DE_ASSERT(bit <= size());
        const bool before = test(bit);
        at(bit / subgroupInvocationSize).set((bit % subgroupInvocationSize), value);
        return before;
    }
    void full()
    {
        const uint32_t bb = size();
        for (uint32_t b = 0u; b < bb; ++b)
            set(b);
    }
    add_ref<Ballots> setn(uint32_t bits)
    {
        for (uint32_t i = 0u; i < bits; ++i)
            set(i);
        return *this;
    }
    bool all() const
    {
        const uint32_t gg = subgroupCount();
        for (uint32_t g = 0u; g < gg; ++g)
        {
            if (false == at(g).all())
                return false;
        }
        return (gg != 0u);
    }
    bool none() const
    {
        const uint32_t gg = subgroupCount();
        for (uint32_t g = 0u; g < gg; ++g)
        {
            if (false == at(g).none())
                return false;
        }
        return (gg != 0u);
    }
    bool any() const
    {
        bool res          = false;
        const uint32_t gg = subgroupCount();
        for (uint32_t g = 0u; g < gg; ++g)
            res |= super::at(g).any();
        return res;
    }
    static uint32_t findBit(uint32_t otherFullyQualifiedInvocationID, uint32_t otherSubgroupSize)
    {
        return (((otherFullyQualifiedInvocationID / otherSubgroupSize) * subgroupInvocationSize) +
                (otherFullyQualifiedInvocationID % otherSubgroupSize));
    }
    inline add_cref<super> upcast(add_cref<Ballots> other) const
    {
        return static_cast<add_cref<super>>(other);
    }
    add_ref<Ballots> operator&=(add_cref<Ballots> other)
    {
        DE_ASSERT(subgroupCount() == other.subgroupCount());
        const uint32_t gg = subgroupCount();
        for (uint32_t g = 0u; g < gg; ++g)
            super::at(g) = super::at(g) & upcast(other).at(g);
        return *this;
    }
    Ballots operator&(add_cref<Ballots> other) const
    {
        Ballots res(*this);
        res &= other;
        return res;
    }
    add_ref<Ballots> operator|=(add_cref<Ballots> other)
    {
        DE_ASSERT(subgroupCount() == other.subgroupCount());
        const uint32_t gg = subgroupCount();
        for (uint32_t g = 0u; g < gg; ++g)
            super::at(g) = super::at(g) | upcast(other).at(g);
        return *this;
    }
    Ballots operator|(add_cref<Ballots> other) const
    {
        Ballots res(*this);
        res |= other;
        return res;
    }
    add_ref<Ballots> operator<<=(uint32_t bits)
    {
        return ((*this) = ((*this) << bits));
    }
    Ballots operator<<(uint32_t bits) const
    {
        Ballots res(subgroupCount());
        if (bits < size() && bits != 0u)
        {
            for (uint32_t b = 0; b < bits; ++b)
                res.set((b + bits), test(b));
        }
        return res;
    }
    Ballots operator~() const
    {
        Ballots res(*this);
        const uint32_t gg = subgroupCount();
        for (uint32_t g = 0u; g < gg; ++g)
            res.at(g) = super::at(g).operator~();
        return res;
    }
    bool operator==(add_cref<Ballots> other) const
    {
        if (super::size() == upcast(other).size())
        {
            const uint32_t gg = subgroupCount();
            for (uint32_t g = 0u; g < gg; ++g)
            {
                if (at(g) != other[g])
                    return false;
            }
            return true;
        }
        return false;
    }
    add_ref<Ballots> operator=(add_cref<Ballots> other)
    {
        DE_ASSERT((subgroupCount() == other.subgroupCount()));
        const uint32_t gg = subgroupCount();
        for (uint32_t g = 0u; g < gg; ++g)
            at(g) = other.at(g);
        return *this;
    }
    add_ref<Ballots> operator=(add_cref<value_type> forAllGroups)
    {
        DE_ASSERT(super::size() >= 1u);
        const uint32_t gg = subgroupCount();
        for (uint32_t g = 0u; g < gg; ++g)
            at(g) = forAllGroups;
        return *this;
    }
};

uint64_t subgroupSizeToMask(uint32_t subgroupSize)
{
    if (subgroupSize == 64)
        return ~0ULL;
    else
        return (1ULL << subgroupSize) - 1;
}

Ballot subgroupSizeToMask(uint32_t subgroupSize, uint32_t subgroupCount)
{
    DE_UNREF(subgroupCount);
    Ballot b;
    DE_ASSERT(subgroupSize <= b.size());
    for (uint32_t i = 0; i < subgroupSize; ++i)
        b.set(i);
    return b;
}

// Take a 64-bit integer, mask it to the subgroup size, and then
// replicate it for each subgroup
bitset_inv_t bitsetFromU64(uint64_t mask, uint32_t subgroupSize)
{
    mask &= subgroupSizeToMask(subgroupSize);
    bitset_inv_t result(mask);
    for (uint32_t i = 0; i < result.size() / subgroupSize - 1; ++i)
    {
        result = (result << subgroupSize) | bitset_inv_t(mask);
    }
    return result;
}

Ballots ballotsFromU64(uint64_t maskValue, uint32_t subgroupSize, uint32_t subgroupCount)
{
    Ballot b(maskValue);
    b &= subgroupSizeToMask(subgroupSize, subgroupCount);
    Ballots result(subgroupCount);
    for (uint32_t g = 0; g < subgroupCount; ++g)
        result.at(g) = b;
    return result;
}

Ballots ballotsFromBallot(Ballot b, uint32_t subgroupSize, uint32_t subgroupCount)
{
    b &= subgroupSizeToMask(subgroupSize, subgroupCount);
    Ballots result(subgroupCount);
    for (uint32_t g = 0; g < subgroupCount; ++g)
        result.at(g) = b;
    return result;
}

// Pick out the mask for the subgroup that invocationID is a member of
uint64_t bitsetToU64(const bitset_inv_t &bitset, uint32_t subgroupSize, uint32_t invocationID)
{
    bitset_inv_t copy(bitset);
    copy >>= (invocationID / subgroupSize) * subgroupSize;
    copy &= bitset_inv_t(subgroupSizeToMask(subgroupSize));
    uint64_t mask = copy.to_ullong();
    mask &= subgroupSizeToMask(subgroupSize);
    return mask;
}

// Pick out the mask for the subgroup that invocationID is a member of
Ballot bitsetToBallot(const Ballots &bitset, uint32_t subgroupSize, uint32_t invocationID)
{
    return bitset.at(invocationID / subgroupSize) & subgroupSizeToMask(subgroupSize, bitset.subgroupCount());
}

Ballot bitsetToBallot(uint64_t value, uint32_t subgroupCount, uint32_t subgroupSize, uint32_t invocationID)
{
    Ballots bs = ballotsFromU64(value, subgroupSize, subgroupCount);
    return bitsetToBallot(bs, subgroupSize, invocationID);
}

static int findLSB(uint64_t value)
{
    for (int i = 0; i < 64; i++)
    {
        if (value & (1ULL << i))
            return i;
    }
    return -1;
}

template <uint32_t N>
static uint32_t findLSB(add_cref<std::bitset<N>> value)
{
    for (uint32_t i = 0u; i < N; ++i)
    {
        if (value.test(i))
            return i;
    }
    return std::numeric_limits<uint32_t>::max();
}

// For each subgroup, pick out the elected invocationID, and accumulate
// a bitset of all of them
static bitset_inv_t bitsetElect(const bitset_inv_t &value, int32_t subgroupSize)
{
    bitset_inv_t ret; // zero initialized

    for (int32_t i = 0; i < (int32_t)value.size(); i += subgroupSize)
    {
        uint64_t mask = bitsetToU64(value, subgroupSize, i);
        int lsb       = findLSB(mask);
        ret |= bitset_inv_t(lsb == -1 ? 0 : (1ULL << lsb)) << i;
    }
    return ret;
}

static Ballots bitsetElect(add_cref<Ballots> value)
{
    Ballots ret(value.subgroupCount());
    for (uint32_t g = 0u; g < value.subgroupCount(); ++g)
    {
        const uint32_t lsb = findLSB<Ballots::subgroupInvocationSize>(value.at(g));
        if (lsb != std::numeric_limits<uint32_t>::max())
        {
            ret.at(g).set(lsb);
        }
    }
    return ret;
}

struct PushConstant
{
    int32_t invocationStride;
    uint32_t width;
    uint32_t height;
    uint32_t primitiveStride;
    uint32_t subgroupStride;
    uint32_t enableInvocationIndex;
};

struct Vertex
{
    // Traditional POD structure that mimics a vertex.
    // Be carefull before do any changes in this structure
    // because it is strictly mapped to VK_FORMAT_R32G32B32A32_SFLOAT
    // when graphics pipeline is constructed.
    float x, y, z, w;
};

typedef Vertex Triangle[3];

std::pair<vk::VkPhysicalDeviceSubgroupProperties, vk::VkPhysicalDeviceProperties2> getSubgroupProperties(
    vkt::Context &context)
{
    vk::VkPhysicalDeviceSubgroupProperties subgroupProperties;
    deMemset(&subgroupProperties, 0, sizeof(subgroupProperties));
    subgroupProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;

    vk::VkPhysicalDeviceProperties2 properties2;
    deMemset(&properties2, 0, sizeof(properties2));
    properties2.sType = vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    properties2.pNext = &subgroupProperties;

    context.getInstanceInterface().getPhysicalDeviceProperties2(context.getPhysicalDevice(), &properties2);

    return {subgroupProperties, properties2};
}

class ReconvergenceTestInstance : public TestInstance
{
public:
    // { vert, frag, tesc, tese, geom }; if any
    using Shaders = std::vector<Move<VkShaderModule>>;

    ReconvergenceTestInstance(Context &context, const CaseDef &data)
        : TestInstance(context)
        , m_data(data)
        , m_subgroupSize(getSubgroupProperties(context).first.subgroupSize)
    {
    }
    ~ReconvergenceTestInstance(void) = default;

    Move<VkPipeline> createComputePipeline(const VkPipelineLayout pipelineLayout, const VkShaderModule computeShader);
    Move<VkPipeline> createGraphicsPipeline(const VkPipelineLayout pipelineLayout, const VkRenderPass renderPass,
                                            const uint32_t width, const uint32_t height, const Shaders &shaders,
                                            const VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                                            const uint32_t patchControlPoints  = 0u);

protected:
    const CaseDef m_data;
    const uint32_t m_subgroupSize;
};

class ReconvergenceTestComputeInstance : public ReconvergenceTestInstance
{
public:
    ReconvergenceTestComputeInstance(Context &context, const CaseDef &data) : ReconvergenceTestInstance(context, data)
    {
    }
    ~ReconvergenceTestComputeInstance(void) = default;

    virtual tcu::TestStatus iterate(void) override;
    qpTestResult_e calculateAndLogResult(const tcu::UVec4 *result, const std::vector<tcu::UVec4> &ref,
                                         uint32_t invocationStride, uint32_t subgroupSize, uint32_t shaderMaxLoc);
};

class ReconvergenceTestGraphicsInstance : public ReconvergenceTestInstance
{
public:
    ReconvergenceTestGraphicsInstance(Context &context, const CaseDef &data) : ReconvergenceTestInstance(context, data)
    {
    }
    ~ReconvergenceTestGraphicsInstance(void) = default;

    auto makeRenderPassBeginInfo(const VkRenderPass renderPass, const VkFramebuffer framebuffer)
        -> VkRenderPassBeginInfo;
    virtual auto recordDrawingAndSubmit(const VkCommandBuffer cmdBuffer, const VkPipelineLayout pipelineLayout,
                                        const VkPipeline pipeline, const VkDescriptorSet descriptorSet,
                                        const PushConstant &pushConstant, const VkRenderPassBeginInfo &renderPassInfo,
                                        const VkBuffer vertexBuffer, const uint32_t vertexCount, const VkImage image)
        -> void;
    virtual auto generateVertices(const uint32_t primitiveCount, const VkPrimitiveTopology topology,
                                  const uint32_t patchSize = 1) -> std::vector<tcu::Vec4>;
    virtual auto createVertexBufferAndFlush(const std::vector<tcu::Vec4> &vertices) -> de::MovePtr<BufferWithMemory>;
    virtual auto createVertexBufferAndFlush(uint32_t cellsHorz, uint32_t cellsVert, VkPrimitiveTopology topology)
        -> de::MovePtr<BufferWithMemory>;
    virtual auto createShaders(void) -> Shaders = 0;

    enum PrintMode
    {
        None,
        ThreadsInColumns,
        OutLocsInColumns,
        IntuitiveThreadsOutlocs,
        Console
    };

    virtual auto calculateAndLogResult(const uint64_t *result, const std::vector<uint64_t> &ref,
                                       uint32_t invocationStride, uint32_t subgroupSize, uint32_t shaderMaxLocs,
                                       uint32_t primitiveCount, PrintMode printMode) -> qpTestResult_e;
};

class ReconvergenceTestFragmentInstance : public ReconvergenceTestGraphicsInstance
{
    struct Arrangement
    {
    };
    friend class FragmentRandomProgram;

public:
    ReconvergenceTestFragmentInstance(Context &context, const CaseDef &data)
        : ReconvergenceTestGraphicsInstance(context, data)
    {
    }
    ~ReconvergenceTestFragmentInstance(void) = default;
    virtual auto createShaders(void) -> std::vector<Move<VkShaderModule>> override;
    auto callAuxiliaryShader(tcu::TestStatus &status, uint32_t triangleCount) -> std::vector<uint32_t>;
    auto makeImageCreateInfo(VkFormat format) const -> VkImageCreateInfo;
    virtual auto createVertexBufferAndFlush(uint32_t cellsHorz, uint32_t cellsVert, VkPrimitiveTopology topology)
        -> de::MovePtr<BufferWithMemory> override;
    virtual auto iterate(void) -> tcu::TestStatus override;
    auto calculateAndLogResultEx(tcu::TestLog &log, const tcu::UVec4 *result, const std::vector<tcu::UVec4> &ref,
                                 const uint32_t maxLoc, const Arrangement &a, const PrintMode printMode)
        -> qpTestResult_e;
};

class ReconvergenceTestVertexInstance : public ReconvergenceTestGraphicsInstance
{
public:
    ReconvergenceTestVertexInstance(Context &context, const CaseDef &data)
        : ReconvergenceTestGraphicsInstance(context, data)
    {
    }
    ~ReconvergenceTestVertexInstance(void) = default;
    virtual auto createShaders(void) -> std::vector<Move<VkShaderModule>> override;
    virtual auto createVertexBufferAndFlush(uint32_t cellsHorz, uint32_t cellsVert, VkPrimitiveTopology topology)
        -> de::MovePtr<BufferWithMemory> override;

    virtual auto iterate(void) -> tcu::TestStatus override;
    auto calculateAndLogResultEx(add_ref<tcu::TestLog> log, const tcu::UVec4 *result,
                                 const std::vector<tcu::UVec4> &ref, const uint32_t maxLoc, const PrintMode printMode)
        -> qpTestResult_e;
};

class ReconvergenceTestTessCtrlInstance : public ReconvergenceTestGraphicsInstance
{
public:
    ReconvergenceTestTessCtrlInstance(Context &context, const CaseDef &data)
        : ReconvergenceTestGraphicsInstance(context, data)
    {
    }
    ~ReconvergenceTestTessCtrlInstance(void) = default;
    virtual auto createShaders(void) -> std::vector<Move<VkShaderModule>> override;
    virtual auto iterate(void) -> tcu::TestStatus override;
};

class ReconvergenceTestTessEvalInstance : public ReconvergenceTestGraphicsInstance
{
public:
    ReconvergenceTestTessEvalInstance(Context &context, add_cref<CaseDef> data)
        : ReconvergenceTestGraphicsInstance(context, data)
    {
    }
    ~ReconvergenceTestTessEvalInstance(void) = default;
    virtual auto createShaders(void) -> std::vector<Move<VkShaderModule>> override;
    virtual auto iterate(void) -> tcu::TestStatus override;
};

class ReconvergenceTestGeometryInstance : public ReconvergenceTestGraphicsInstance
{
public:
    ReconvergenceTestGeometryInstance(Context &context, add_cref<CaseDef> data)
        : ReconvergenceTestGraphicsInstance(context, data)
    {
    }
    ~ReconvergenceTestGeometryInstance(void) = default;
    virtual auto createShaders(void) -> std::vector<Move<VkShaderModule>> override;
    virtual auto createVertexBufferAndFlush(uint32_t cellsHorz, uint32_t cellsVert, VkPrimitiveTopology topology)
        -> de::MovePtr<BufferWithMemory> override;

    virtual auto iterate(void) -> tcu::TestStatus override;
    auto calculateAndLogResultEx(add_ref<tcu::TestLog> log, const tcu::UVec4 *result,
                                 const std::vector<tcu::UVec4> &ref, const uint32_t maxLoc, const PrintMode printMode)
        -> qpTestResult_e;
};

Move<VkPipeline> ReconvergenceTestInstance::createGraphicsPipeline(const VkPipelineLayout pipelineLayout,
                                                                   const VkRenderPass renderPass, const uint32_t width,
                                                                   const uint32_t height, const Shaders &shaders,
                                                                   const VkPrimitiveTopology topology,
                                                                   const uint32_t patchControlPoints)
{
    const DeviceInterface &vkd = m_context.getDeviceInterface();
    const VkDevice device      = m_context.getDevice();
    const uint32_t subpass     = 0;

    const std::vector<VkViewport> viewports{makeViewport(width, height)};
    const std::vector<VkRect2D> scissors{makeRect2D(width, height)};

    enum ShaderIndex
    {
        IVERT = 0,
        IFRAG,
        ITESC,
        ITESE,
        IGEOM
    };
    VkShaderModule handles[5] = {DE_NULL}; // { vert, frag, tesc, tese, geom }

    for (uint32_t i = 0; i < (uint32_t)ARRAYSIZE(handles); ++i)
    {
        handles[i] = (i < (uint32_t)shaders.size()) ? *shaders[i] : DE_NULL;
    }

    return makeGraphicsPipeline(vkd, device, pipelineLayout, handles[IVERT], handles[ITESC], handles[ITESE],
                                handles[IGEOM], handles[IFRAG], renderPass, viewports, scissors, topology, subpass,
                                patchControlPoints);
}

Move<VkPipeline> ReconvergenceTestInstance::createComputePipeline(const VkPipelineLayout pipelineLayout,
                                                                  const VkShaderModule computeShader)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();

    const uint32_t specData[2]                                               = {m_data.sizeX, m_data.sizeY};
    const vk::VkSpecializationMapEntry entries[DE_LENGTH_OF_ARRAY(specData)] = {
        {0, (uint32_t)(sizeof(uint32_t) * 0), sizeof(uint32_t)},
        {1, (uint32_t)(sizeof(uint32_t) * 1), sizeof(uint32_t)},
    };
    const vk::VkSpecializationInfo specInfo = {
        DE_LENGTH_OF_ARRAY(entries), // mapEntryCount
        entries,                     // pMapEntries
        sizeof(specData),            // dataSize
        specData                     // pData
    };

    const VkPipelineShaderStageRequiredSubgroupSizeCreateInfoEXT subgroupSizeCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO_EXT, // VkStructureType sType;
        DE_NULL,                                                                        // void* pNext;
        m_subgroupSize // uint32_t requiredSubgroupSize;
    };

    const VkBool32 computeFullSubgroups =
        m_subgroupSize <= 64 && m_context.getSubgroupSizeControlFeatures().computeFullSubgroups;

    const void *shaderPNext = computeFullSubgroups ? &subgroupSizeCreateInfo : DE_NULL;
    VkPipelineShaderStageCreateFlags pipelineShaderStageCreateFlags =
        (VkPipelineShaderStageCreateFlags)(computeFullSubgroups ?
                                               VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT_EXT :
                                               0);

    const VkPipelineShaderStageCreateInfo shaderCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        shaderPNext,
        pipelineShaderStageCreateFlags,
        VK_SHADER_STAGE_COMPUTE_BIT, // stage
        computeShader,               // shader
        "main",
        &specInfo, // pSpecializationInfo
    };

    const VkComputePipelineCreateInfo pipelineCreateInfo = {
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        DE_NULL,
        0u,               // flags
        shaderCreateInfo, // cs
        pipelineLayout,   // layout
        VK_NULL_HANDLE,   // basePipelineHandle
        0u,               // basePipelineIndex
    };

    return vk::createComputePipeline(vk, device, VK_NULL_HANDLE, &pipelineCreateInfo, NULL);
}

typedef enum
{
    // store subgroupBallot().
    // For OP_BALLOT, OP::caseValue is initialized to zero, and then
    // set to 1 by simulate if the ballot is not workgroup- (or subgroup-_uniform.
    // Only workgroup-uniform ballots are validated for correctness in
    // WUCF modes.
    OP_BALLOT,

    // store literal constant
    OP_STORE,

    // if ((1ULL << gl_SubgroupInvocationID) & mask).
    // Special case if mask = ~0ULL, converted into "if (inputA.a[idx] == idx)"
    OP_IF_MASK,
    OP_ELSE_MASK,
    OP_ENDIF,

    // if (gl_SubgroupInvocationID == loopIdxN) (where N is most nested loop counter)
    OP_IF_LOOPCOUNT,
    OP_ELSE_LOOPCOUNT,

    // if (gl_LocalInvocationIndex >= inputA.a[N]) (where N is most nested loop counter)
    OP_IF_LOCAL_INVOCATION_INDEX,
    OP_ELSE_LOCAL_INVOCATION_INDEX,

    // break/continue
    OP_BREAK,
    OP_CONTINUE,

    // if (subgroupElect())
    OP_ELECT,

    // Loop with uniform number of iterations (read from a buffer)
    OP_BEGIN_FOR_UNIF,
    OP_END_FOR_UNIF,

    // for (int loopIdxN = 0; loopIdxN < gl_SubgroupInvocationID + 1; ++loopIdxN)
    OP_BEGIN_FOR_VAR,
    OP_END_FOR_VAR,

    // for (int loopIdxN = 0;; ++loopIdxN, OP_BALLOT)
    // Always has an "if (subgroupElect()) break;" inside.
    // Does the equivalent of OP_BALLOT in the continue construct
    OP_BEGIN_FOR_INF,
    OP_END_FOR_INF,

    // do { loopIdxN++; ... } while (loopIdxN < uniformValue);
    OP_BEGIN_DO_WHILE_UNIF,
    OP_END_DO_WHILE_UNIF,

    // do { ... } while (true);
    // Always has an "if (subgroupElect()) break;" inside
    OP_BEGIN_DO_WHILE_INF,
    OP_END_DO_WHILE_INF,

    // return;
    OP_RETURN,

    // function call (code bracketed by these is extracted into a separate function)
    OP_CALL_BEGIN,
    OP_CALL_END,

    // switch statement on uniform value
    OP_SWITCH_UNIF_BEGIN,
    // switch statement on gl_SubgroupInvocationID & 3 value
    OP_SWITCH_VAR_BEGIN,
    // switch statement on loopIdx value
    OP_SWITCH_LOOP_COUNT_BEGIN,

    // case statement with a (invocation mask, case mask) pair
    OP_CASE_MASK_BEGIN,
    // case statement used for loop counter switches, with a value and a mask of loop iterations
    OP_CASE_LOOP_COUNT_BEGIN,

    // end of switch/case statement
    OP_SWITCH_END,
    OP_CASE_END,

    // Extra code with no functional effect. Currently inculdes:
    // - value 0: while (!subgroupElect()) {}
    // - value 1: if (condition_that_is_false) { infinite loop }
    OP_NOISE,

    // do nothing, only markup
    OP_NOP
} OPType;

const char *OPtypeToStr(const OPType op)
{
#define MAKETEXT(s__) #s__
#define CASETEXT(e__) \
    case e__:         \
        return MAKETEXT(e__)
    switch (op)
    {
        CASETEXT(OP_BALLOT);
        CASETEXT(OP_STORE);
        CASETEXT(OP_IF_MASK);
        CASETEXT(OP_ELSE_MASK);
        CASETEXT(OP_ENDIF);
        CASETEXT(OP_IF_LOOPCOUNT);
        CASETEXT(OP_ELSE_LOOPCOUNT);
        CASETEXT(OP_IF_LOCAL_INVOCATION_INDEX);
        CASETEXT(OP_ELSE_LOCAL_INVOCATION_INDEX);
        CASETEXT(OP_BREAK);
        CASETEXT(OP_CONTINUE);
        CASETEXT(OP_ELECT);
        CASETEXT(OP_BEGIN_FOR_UNIF);
        CASETEXT(OP_END_FOR_UNIF);
        CASETEXT(OP_BEGIN_FOR_VAR);
        CASETEXT(OP_END_FOR_VAR);
        CASETEXT(OP_BEGIN_FOR_INF);
        CASETEXT(OP_END_FOR_INF);
        CASETEXT(OP_BEGIN_DO_WHILE_UNIF);
        CASETEXT(OP_END_DO_WHILE_UNIF);
        CASETEXT(OP_BEGIN_DO_WHILE_INF);
        CASETEXT(OP_END_DO_WHILE_INF);
        CASETEXT(OP_RETURN);
        CASETEXT(OP_CALL_BEGIN);
        CASETEXT(OP_CALL_END);
        CASETEXT(OP_SWITCH_UNIF_BEGIN);
        CASETEXT(OP_SWITCH_VAR_BEGIN);
        CASETEXT(OP_SWITCH_LOOP_COUNT_BEGIN);
        CASETEXT(OP_CASE_MASK_BEGIN);
        CASETEXT(OP_CASE_LOOP_COUNT_BEGIN);
        CASETEXT(OP_SWITCH_END);
        CASETEXT(OP_CASE_END);
        CASETEXT(OP_NOISE);
        CASETEXT(OP_NOP);
    }
    return "<Unknown>";
}

typedef enum
{
    // Different if test conditions
    IF_MASK,
    IF_UNIFORM,
    IF_LOOPCOUNT,
    IF_LOCAL_INVOCATION_INDEX,
} IFType;

class OP
{
public:
    OP(OPType _type, uint64_t _value, uint32_t _caseValue = 0)
        : type(_type)
        , value(_value)
        // by default, initialized only lower part with a repetition of _value
        , bvalue(tcu::UVec4(uint32_t(_value), uint32_t(_value >> 32), uint32_t(_value), uint32_t(_value >> 32)))
        , caseValue(_caseValue)
    {
    }

    // The type of operation and an optional value.
    // The value could be a mask for an if test, the index of the loop
    // header for an end of loop, or the constant value for a store instruction
    OPType type;
    uint64_t value;
    Ballot bvalue;
    uint32_t caseValue;
};

class RandomProgram
{

public:
    RandomProgram(const CaseDef &c, uint32_t invocationCount = 0u)
        : caseDef(c)
        , invocationStride(invocationCount ? invocationCount : (c.sizeX * c.sizeY))
        , rnd()
        , ops()
        , masks()
        , ballotMasks()
        , numMasks(5)
        , nesting(0)
        , maxNesting(c.maxNesting)
        , loopNesting(0)
        , loopNestingThisFunction(0)
        , callNesting(0)
        , minCount(30)
        , indent(0)
        , isLoopInf(100, false)
        , doneInfLoopBreak(100, false)
        , storeBase(0x10000)
    {
        deRandom_init(&rnd, caseDef.seed);
        for (int i = 0; i < numMasks; ++i)
        {
            const uint64_t lo = deRandom_getUint64(&rnd);
            const uint64_t hi = deRandom_getUint64(&rnd);
            const tcu::UVec4 v4(uint32_t(lo), uint32_t(lo >> 32), uint32_t(hi), uint32_t(hi >> 32));
            ballotMasks.emplace_back(v4);
            masks.push_back(lo);
        }
    }
    virtual ~RandomProgram() = default;

    const CaseDef caseDef;
    const uint32_t invocationStride;
    deRandom rnd;
    vector<OP> ops;
    vector<uint64_t> masks;
    vector<Ballot> ballotMasks;
    int32_t numMasks;
    int32_t nesting;
    int32_t maxNesting;
    int32_t loopNesting;
    int32_t loopNestingThisFunction;
    int32_t callNesting;
    int32_t minCount;
    int32_t indent;
    vector<bool> isLoopInf;
    vector<bool> doneInfLoopBreak;
    // Offset the value we use for OP_STORE, to avoid colliding with fully converged
    // active masks with small subgroup sizes (e.g. with subgroupSize == 4, the SUCF
    // tests need to know that 0xF is really an active mask).
    int32_t storeBase;

    virtual void genIf(IFType ifType, uint32_t maxLocalIndexCmp = 0u)
    {
        uint32_t maskIdx = deRandom_getUint32(&rnd) % numMasks;
        uint64_t mask    = masks[maskIdx];
        Ballot bmask     = ballotMasks[maskIdx];
        if (ifType == IF_UNIFORM)
        {
            mask = ~0ULL;
            bmask.set();
        }

        uint32_t localIndexCmp = deRandom_getUint32(&rnd) % (maxLocalIndexCmp ? maxLocalIndexCmp : invocationStride);
        if (ifType == IF_LOCAL_INVOCATION_INDEX)
            ops.push_back({OP_IF_LOCAL_INVOCATION_INDEX, localIndexCmp});
        else if (ifType == IF_LOOPCOUNT)
            ops.push_back({OP_IF_LOOPCOUNT, 0});
        else
        {
            ops.push_back({OP_IF_MASK, mask});
            ops.back().bvalue = bmask;
        }

        nesting++;

        size_t thenBegin = ops.size();
        pickOP(2);
        size_t thenEnd = ops.size();

        uint32_t randElse = (deRandom_getUint32(&rnd) % 100);
        if (randElse < 50)
        {
            if (ifType == IF_LOCAL_INVOCATION_INDEX)
                ops.push_back({OP_ELSE_LOCAL_INVOCATION_INDEX, localIndexCmp});
            else if (ifType == IF_LOOPCOUNT)
                ops.push_back({OP_ELSE_LOOPCOUNT, 0});
            else
                ops.push_back({OP_ELSE_MASK, 0});

            if (randElse < 10)
            {
                // Sometimes make the else block identical to the then block
                for (size_t i = thenBegin; i < thenEnd; ++i)
                    ops.push_back(ops[i]);
            }
            else
                pickOP(2);
        }
        ops.push_back({OP_ENDIF, 0});
        nesting--;
    }

    void genForUnif()
    {
        uint32_t iterCount = (deRandom_getUint32(&rnd) % 5) + 1;
        ops.push_back({OP_BEGIN_FOR_UNIF, iterCount});
        uint32_t loopheader = (uint32_t)ops.size() - 1;
        nesting++;
        loopNesting++;
        loopNestingThisFunction++;
        pickOP(2);
        ops.push_back({OP_END_FOR_UNIF, loopheader});
        loopNestingThisFunction--;
        loopNesting--;
        nesting--;
    }

    void genDoWhileUnif()
    {
        uint32_t iterCount = (deRandom_getUint32(&rnd) % 5) + 1;
        ops.push_back({OP_BEGIN_DO_WHILE_UNIF, iterCount});
        uint32_t loopheader = (uint32_t)ops.size() - 1;
        nesting++;
        loopNesting++;
        loopNestingThisFunction++;
        pickOP(2);
        ops.push_back({OP_END_DO_WHILE_UNIF, loopheader});
        loopNestingThisFunction--;
        loopNesting--;
        nesting--;
    }

    void genForVar()
    {
        ops.push_back({OP_BEGIN_FOR_VAR, 0});
        uint32_t loopheader = (uint32_t)ops.size() - 1;
        nesting++;
        loopNesting++;
        loopNestingThisFunction++;
        pickOP(2);
        ops.push_back({OP_END_FOR_VAR, loopheader});
        loopNestingThisFunction--;
        loopNesting--;
        nesting--;
    }

    void genForInf()
    {
        ops.push_back({OP_BEGIN_FOR_INF, 0});
        uint32_t loopheader = (uint32_t)ops.size() - 1;

        nesting++;
        loopNesting++;
        loopNestingThisFunction++;
        isLoopInf[loopNesting]        = true;
        doneInfLoopBreak[loopNesting] = false;

        pickOP(2);

        genElect(true);
        doneInfLoopBreak[loopNesting] = true;

        pickOP(2);

        ops.push_back({OP_END_FOR_INF, loopheader});

        isLoopInf[loopNesting]        = false;
        doneInfLoopBreak[loopNesting] = false;
        loopNestingThisFunction--;
        loopNesting--;
        nesting--;
    }

    void genDoWhileInf()
    {
        ops.push_back({OP_BEGIN_DO_WHILE_INF, 0});
        uint32_t loopheader = (uint32_t)ops.size() - 1;

        nesting++;
        loopNesting++;
        loopNestingThisFunction++;
        isLoopInf[loopNesting]        = true;
        doneInfLoopBreak[loopNesting] = false;

        pickOP(2);

        genElect(true);
        doneInfLoopBreak[loopNesting] = true;

        pickOP(2);

        ops.push_back({OP_END_DO_WHILE_INF, loopheader});

        isLoopInf[loopNesting]        = false;
        doneInfLoopBreak[loopNesting] = false;
        loopNestingThisFunction--;
        loopNesting--;
        nesting--;
    }

    void genBreak()
    {
        if (loopNestingThisFunction > 0)
        {
            // Sometimes put the break in a divergent if
            if ((deRandom_getUint32(&rnd) % 100) < 10)
            {
                ops.push_back({OP_IF_MASK, masks[0]});
                ops.back().bvalue = ballotMasks[0];
                ops.push_back({OP_BREAK, 0});
                ops.push_back({OP_ELSE_MASK, 0});
                ops.push_back({OP_BREAK, 0});
                ops.push_back({OP_ENDIF, 0});
            }
            else
                ops.push_back({OP_BREAK, 0});
        }
    }

    void genContinue()
    {
        // continues are allowed if we're in a loop and the loop is not infinite,
        // or if it is infinite and we've already done a subgroupElect+break.
        // However, adding more continues seems to reduce the failure rate, so
        // disable it for now
        if (loopNestingThisFunction > 0 && !(isLoopInf[loopNesting] /*&& !doneInfLoopBreak[loopNesting]*/))
        {
            // Sometimes put the continue in a divergent if
            if ((deRandom_getUint32(&rnd) % 100) < 10)
            {
                ops.push_back({OP_IF_MASK, masks[0]});
                ops.back().bvalue = ballotMasks[0];
                ops.push_back({OP_CONTINUE, 0});
                ops.push_back({OP_ELSE_MASK, 0});
                ops.push_back({OP_CONTINUE, 0});
                ops.push_back({OP_ENDIF, 0});
            }
            else
                ops.push_back({OP_CONTINUE, 0});
        }
    }

    // doBreak is used to generate "if (subgroupElect()) { ... break; }" inside infinite loops
    void genElect(bool doBreak)
    {
        ops.push_back({OP_ELECT, 0});
        nesting++;
        if (doBreak)
        {
            // Put something interestign before the break
            genBallot();
            genBallot();
            if ((deRandom_getUint32(&rnd) % 100) < 10)
                pickOP(1);

            // if we're in a function, sometimes  use return instead
            if (callNesting > 0 && (deRandom_getUint32(&rnd) % 100) < 30)
                ops.push_back({OP_RETURN, 0});
            else
                genBreak();
        }
        else
            pickOP(2);

        ops.push_back({OP_ENDIF, 0});
        nesting--;
    }

    void genReturn()
    {
        uint32_t r = deRandom_getUint32(&rnd) % 100;
        if (nesting > 0 &&
            // Use return rarely in main, 20% of the time in a singly nested loop in a function
            // and 50% of the time in a multiply nested loop in a function
            (r < 5 || (callNesting > 0 && loopNestingThisFunction > 0 && r < 20) ||
             (callNesting > 0 && loopNestingThisFunction > 1 && r < 50)))
        {
            genBallot();
            if ((deRandom_getUint32(&rnd) % 100) < 10)
            {
                ops.push_back({OP_IF_MASK, masks[0]});
                ops.back().bvalue = ballotMasks[0];
                ops.push_back({OP_RETURN, 0});
                ops.push_back({OP_ELSE_MASK, 0});
                ops.push_back({OP_RETURN, 0});
                ops.push_back({OP_ENDIF, 0});
            }
            else
                ops.push_back({OP_RETURN, 0});
        }
    }

    // Generate a function call. Save and restore some loop information, which is used to
    // determine when it's safe to use break/continue
    void genCall()
    {
        ops.push_back({OP_CALL_BEGIN, 0});
        callNesting++;
        nesting++;
        int32_t saveLoopNestingThisFunction = loopNestingThisFunction;
        loopNestingThisFunction             = 0;

        pickOP(2);

        loopNestingThisFunction = saveLoopNestingThisFunction;
        nesting--;
        callNesting--;
        ops.push_back({OP_CALL_END, 0});
    }

    // Generate switch on a uniform value:
    // switch (inputA.a[r]) {
    // case r+1: ... break; // should not execute
    // case r:   ... break; // should branch uniformly
    // case r+2: ... break; // should not execute
    // }
    void genSwitchUnif()
    {
        uint32_t r = deRandom_getUint32(&rnd) % 5;
        ops.push_back({OP_SWITCH_UNIF_BEGIN, r});
        nesting++;

        ops.push_back({OP_CASE_MASK_BEGIN, 0, 1u << (r + 1)});
        pickOP(1);
        ops.push_back({OP_CASE_END, 0});

        ops.push_back({OP_CASE_MASK_BEGIN, ~0ULL, 1u << r});
        ops.back().bvalue.set();
        pickOP(2);
        ops.push_back({OP_CASE_END, 0});

        ops.push_back({OP_CASE_MASK_BEGIN, 0, 1u << (r + 2)});
        pickOP(1);
        ops.push_back({OP_CASE_END, 0});

        ops.push_back({OP_SWITCH_END, 0});
        nesting--;
    }

    // switch (gl_SubgroupInvocationID & 3) with four unique targets
    void genSwitchVar()
    {
        ops.push_back({OP_SWITCH_VAR_BEGIN, 0});
        nesting++;

        ops.push_back({OP_CASE_MASK_BEGIN, 0x1111111111111111ULL, 1 << 0});
        ops.back().bvalue = tcu::UVec4(0x11111111);
        pickOP(1);
        ops.push_back({OP_CASE_END, 0});

        ops.push_back({OP_CASE_MASK_BEGIN, 0x2222222222222222ULL, 1 << 1});
        ops.back().bvalue = tcu::UVec4(0x22222222);
        pickOP(1);
        ops.push_back({OP_CASE_END, 0});

        ops.push_back({OP_CASE_MASK_BEGIN, 0x4444444444444444ULL, 1 << 2});
        ops.back().bvalue = tcu::UVec4(0x44444444);
        pickOP(1);
        ops.push_back({OP_CASE_END, 0});

        ops.push_back({OP_CASE_MASK_BEGIN, 0x8888888888888888ULL, 1 << 3});
        ops.back().bvalue = tcu::UVec4(0x88888888);
        pickOP(1);
        ops.push_back({OP_CASE_END, 0});

        ops.push_back({OP_SWITCH_END, 0});
        nesting--;
    }

    // switch (gl_SubgroupInvocationID & 3) with two shared targets.
    // XXX TODO: The test considers these two targets to remain converged,
    // though we haven't agreed to that behavior yet.
    void genSwitchMulticase()
    {
        ops.push_back({OP_SWITCH_VAR_BEGIN, 0});
        nesting++;

        ops.push_back({OP_CASE_MASK_BEGIN, 0x3333333333333333ULL, (1 << 0) | (1 << 1)});
        ops.back().bvalue = tcu::UVec4(0x33333333);
        pickOP(2);
        ops.push_back({OP_CASE_END, 0});

        ops.push_back({OP_CASE_MASK_BEGIN, 0xCCCCCCCCCCCCCCCCULL, (1 << 2) | (1 << 3)});
        ops.back().bvalue = tcu::UVec4(0xCCCCCCCC);
        pickOP(2);
        ops.push_back({OP_CASE_END, 0});

        ops.push_back({OP_SWITCH_END, 0});
        nesting--;
    }

    // switch (loopIdxN) {
    // case 1:  ... break;
    // case 2:  ... break;
    // default: ... break;
    // }
    void genSwitchLoopCount()
    {
        uint32_t r = deRandom_getUint32(&rnd) % loopNesting;
        ops.push_back({OP_SWITCH_LOOP_COUNT_BEGIN, r});
        nesting++;

        ops.push_back({OP_CASE_LOOP_COUNT_BEGIN, 1ULL << 1, 1});
        ops.back().bvalue = tcu::UVec4(1 << 1, 0, 0, 0);
        pickOP(1);
        ops.push_back({OP_CASE_END, 0});

        ops.push_back({OP_CASE_LOOP_COUNT_BEGIN, 1ULL << 2, 2});
        ops.back().bvalue = tcu::UVec4(1 << 2, 0, 0, 0);
        pickOP(1);
        ops.push_back({OP_CASE_END, 0});

        // default:
        ops.push_back({OP_CASE_LOOP_COUNT_BEGIN, ~6ULL, 0xFFFFFFFF});
        ops.back().bvalue = tcu::UVec4(~6u, ~0u, ~0u, ~0u);
        pickOP(1);
        ops.push_back({OP_CASE_END, 0});

        ops.push_back({OP_SWITCH_END, 0});
        nesting--;
    }

    void pickOP(uint32_t count)
    {
        // Pick "count" instructions. These can recursively insert more instructions,
        // so "count" is just a seed
        for (uint32_t i = 0; i < count; ++i)
        {
            genBallot();
            if (nesting < maxNesting)
            {
                uint32_t r = deRandom_getUint32(&rnd) % 11;
                switch (r)
                {
                default:
                    DE_ASSERT(0);
                    // fallthrough
                case 2:
                    if (loopNesting)
                    {
                        genIf(IF_LOOPCOUNT);
                        break;
                    }
                    // fallthrough
                case 10:
                    genIf(IF_LOCAL_INVOCATION_INDEX);
                    break;
                case 0:
                    genIf(IF_MASK);
                    break;
                case 1:
                    genIf(IF_UNIFORM);
                    break;
                case 3:
                {
                    // don't nest loops too deeply, to avoid extreme memory usage or timeouts
                    if (loopNesting <= 3)
                    {
                        uint32_t r2 = deRandom_getUint32(&rnd) % 3;
                        switch (r2)
                        {
                        default:
                            DE_ASSERT(0); // fallthrough
                        case 0:
                            genForUnif();
                            break;
                        case 1:
                            genForInf();
                            break;
                        case 2:
                            genForVar();
                            break;
                        }
                    }
                }
                break;
                case 4:
                    genBreak();
                    break;
                case 5:
                    genContinue();
                    break;
                case 6:
                    genElect(false);
                    break;
                case 7:
                {
                    uint32_t r2 = deRandom_getUint32(&rnd) % 5;
                    if (r2 == 0 && callNesting == 0 && nesting < maxNesting - 2)
                        genCall();
                    else
                        genReturn();
                    break;
                }
                case 8:
                {
                    // don't nest loops too deeply, to avoid extreme memory usage or timeouts
                    if (loopNesting <= 3)
                    {
                        uint32_t r2 = deRandom_getUint32(&rnd) % 2;
                        switch (r2)
                        {
                        default:
                            DE_ASSERT(0); // fallthrough
                        case 0:
                            genDoWhileUnif();
                            break;
                        case 1:
                            genDoWhileInf();
                            break;
                        }
                    }
                }
                break;
                case 9:
                {
                    uint32_t r2 = deRandom_getUint32(&rnd) % 4;
                    switch (r2)
                    {
                    default:
                        DE_ASSERT(0);
                        // fallthrough
                    case 0:
                        genSwitchUnif();
                        break;
                    case 1:
                        if (loopNesting > 0)
                        {
                            genSwitchLoopCount();
                            break;
                        }
                        // fallthrough
                    case 2:
                        if (caseDef.testType != TT_MAXIMAL)
                        {
                            // multicase doesn't have fully-defined behavior for MAXIMAL tests,
                            // but does for SUCF tests
                            genSwitchMulticase();
                            break;
                        }
                        // fallthrough
                    case 3:
                        genSwitchVar();
                        break;
                    }
                }
                break;
                }
            }
            genBallot();
        }
    }

    void genBallot()
    {
        // optionally insert ballots, stores, and noise. Ballots and stores are used to determine
        // correctness.
        if ((deRandom_getUint32(&rnd) % 100) < 20)
        {
            if (ops.size() < 2 || !(ops[ops.size() - 1].type == OP_BALLOT ||
                                    (ops[ops.size() - 1].type == OP_STORE && ops[ops.size() - 2].type == OP_BALLOT)))
            {
                // do a store along with each ballot, so we can correlate where
                // the ballot came from
                if (caseDef.testType != TT_MAXIMAL)
                    ops.push_back({OP_STORE, (uint32_t)ops.size() + storeBase});
                ops.push_back({OP_BALLOT, 0});
            }
        }

        if ((deRandom_getUint32(&rnd) % 100) < 10)
        {
            if (ops.size() < 2 || !(ops[ops.size() - 1].type == OP_STORE ||
                                    (ops[ops.size() - 1].type == OP_BALLOT && ops[ops.size() - 2].type == OP_STORE)))
            {
                // SUCF does a store with every ballot. Don't bloat the code by adding more.
                if (caseDef.testType == TT_MAXIMAL)
                    ops.push_back({OP_STORE, (uint32_t)ops.size() + storeBase});
            }
        }

        uint32_t r = deRandom_getUint32(&rnd) % 10000;
        if (r < 3)
            ops.push_back({OP_NOISE, 0});
        else if (r < 10)
            ops.push_back({OP_NOISE, 1});
    }

    void generateRandomProgram(qpWatchDog *watchDog, add_ref<tcu::TestLog> log)
    {
        std::vector<tcu::UVec4> ref;

        do
        {
            ops.clear();
            while ((int32_t)ops.size() < minCount)
                pickOP(1);

            // Retry until the program has some UCF results in it
            if (caseDef.isUCF())
            {
                // Simulate for all subgroup sizes, to determine whether OP_BALLOTs are nonuniform
                for (int32_t subgroupSize = 4; subgroupSize <= 128; subgroupSize *= 2)
                {
                    //simulate(true, subgroupSize, ref);
                    execute(watchDog, true, subgroupSize, 0u, invocationStride, ref, log);
                }
            }
        } while (caseDef.isUCF() && !hasUCF());
    }

    void printIndent(std::stringstream &css)
    {
        for (int32_t i = 0; i < indent; ++i)
            css << " ";
    }

    struct FlowState
    {
        add_cref<vector<OP>> ops;
        const int32_t opsIndex;
        const int32_t loopNesting;
        const int funcNum;
    };

    // State of the subgroup at each level of nesting
    struct SubgroupState
    {
        // Currently executing
        bitset_inv_t activeMask;
        // Have executed a continue instruction in this loop
        bitset_inv_t continueMask;
        // index of the current if test or loop header
        uint32_t header;
        // number of loop iterations performed
        uint32_t tripCount;
        // is this nesting a loop?
        uint32_t isLoop;
        // is this nesting a function call?
        uint32_t isCall;
        // is this nesting a switch?
        uint32_t isSwitch;
    };

    struct SubgroupState2
    {
        // Currently executing
        Ballots activeMask;
        // Have executed a continue instruction in this loop
        Ballots continueMask;
        // index of the current if test or loop header
        uint32_t header;
        // number of loop iterations performed
        uint32_t tripCount;
        // is this nesting a loop?
        uint32_t isLoop;
        // is this nesting a function call?
        uint32_t isCall;
        // is this nesting a switch?
        uint32_t isSwitch;
        virtual ~SubgroupState2() = default;
        SubgroupState2() : SubgroupState2(0)
        {
        }
        SubgroupState2(uint32_t subgroupCount)
            : activeMask(subgroupCount)
            , continueMask(subgroupCount)
            , header()
            , tripCount()
            , isLoop()
            , isCall()
            , isSwitch()
        {
        }
    };

    struct Prerequisites
    {
    };

    virtual std::string getPartitionBallotText()
    {
        return "subgroupBallot(true)";
    }

    virtual void printIfLocalInvocationIndex(std::stringstream &css, add_cref<FlowState> flow)
    {
        printIndent(css);
        css << "if (gl_LocalInvocationIndex >= inputA.a[0x" << std::hex << flow.ops[flow.opsIndex].value << "]) {\n";
    }

    virtual void printStore(std::stringstream &css, add_cref<FlowState> flow)
    {
        printIndent(css);
        css << "outputC.loc[gl_LocalInvocationIndex]++;\n";
        printIndent(css);
        css << "outputB.b[(outLoc++)*invocationStride + gl_LocalInvocationIndex].x = 0x" << std::hex
            << flow.ops[flow.opsIndex].value << ";\n";
    }

    virtual void printBallot(std::stringstream &css, add_cref<FlowState>, bool endWithSemicolon = false)
    {
        printIndent(css);

        css << "outputC.loc[gl_LocalInvocationIndex]++,";
        // When inside loop(s), use partitionBallot rather than subgroupBallot to compute
        // a ballot, to make sure the ballot is "diverged enough". Don't do this for
        // subgroup_uniform_control_flow, since we only validate results that must be fully
        // reconverged.
        if (loopNesting > 0 && caseDef.testType == TT_MAXIMAL)
        {
            css << "outputB.b[(outLoc++)*invocationStride + gl_LocalInvocationIndex] = " << getPartitionBallotText()
                << ".xy";
        }
        else if (caseDef.isElect())
        {
            css << "outputB.b[(outLoc++)*invocationStride + gl_LocalInvocationIndex].x = elect()";
        }
        else
        {
            css << "outputB.b[(outLoc++)*invocationStride + gl_LocalInvocationIndex] = subgroupBallot(true).xy";
        }
        if (endWithSemicolon)
        {
            css << ";\n";
        }
    }

    void printCode(std::stringstream &functions, std::stringstream &main)
    {
        std::stringstream *css = &main;
        indent                 = 4;
        loopNesting            = 0;
        int funcNum            = 0;
        int32_t i              = 0;

        auto makeFlowState = [&]() -> FlowState { return FlowState{ops, i, loopNesting, funcNum}; };

        for (; i < (int32_t)ops.size(); ++i)
        {
            switch (ops[i].type)
            {
            case OP_IF_MASK:
                printIndent(*css);
                if (ops[i].value == ~0ULL)
                {
                    // This equality test will always succeed, since inputA.a[i] == i
                    int idx = deRandom_getUint32(&rnd) % 4;
                    *css << "if (inputA.a[" << idx << "] == " << idx << ") {\n";
                }
                else
                {
                    const tcu::UVec4 v(ops[i].bvalue);
                    *css << std::hex << "if (testBit(uvec4("
                         << "0x" << v.x() << ", "
                         << "0x" << v.y() << ", "
                         << "0x" << v.z() << ", "
                         << "0x" << v.w() << std::dec << "), gl_SubgroupInvocationID)) {\n";
                }
                indent += 4;
                break;
            case OP_IF_LOOPCOUNT:
                printIndent(*css);
                *css << "if (gl_SubgroupInvocationID == loopIdx" << loopNesting - 1 << ") {\n";
                indent += 4;
                break;
            case OP_IF_LOCAL_INVOCATION_INDEX:
                printIfLocalInvocationIndex(*css, makeFlowState());
                indent += 4;
                break;
            case OP_ELSE_MASK:
            case OP_ELSE_LOOPCOUNT:
            case OP_ELSE_LOCAL_INVOCATION_INDEX:
                indent -= 4;
                printIndent(*css);
                *css << "} else {\n";
                indent += 4;
                break;
            case OP_ENDIF:
                indent -= 4;
                printIndent(*css);
                *css << "}\n";
                break;
            case OP_BALLOT:
                printBallot(*css, makeFlowState(), true);
                break;
            case OP_STORE:
                printStore(*css, makeFlowState());
                break;
            case OP_BEGIN_FOR_VAR:
                printIndent(*css);
                *css << "for (int loopIdx" << loopNesting << " = 0;\n";
                printIndent(*css);
                *css << "         loopIdx" << loopNesting << " < gl_SubgroupInvocationID + 1;\n";
                printIndent(*css);
                *css << "         loopIdx" << loopNesting << "++) {\n";
                indent += 4;
                loopNesting++;
                break;
            case OP_END_FOR_VAR:
                loopNesting--;
                indent -= 4;
                printIndent(*css);
                *css << "}\n";
                break;
            case OP_BEGIN_FOR_UNIF:
                printIndent(*css);
                *css << "for (int loopIdx" << loopNesting << " = 0;\n";
                printIndent(*css);
                *css << "         loopIdx" << loopNesting << " < inputA.a[" << ops[i].value << "];\n";
                printIndent(*css);
                *css << "         loopIdx" << loopNesting << "++) {\n";
                indent += 4;
                loopNesting++;
                break;
            case OP_END_FOR_UNIF:
                loopNesting--;
                indent -= 4;
                printIndent(*css);
                *css << "}\n";
                break;
            case OP_BEGIN_FOR_INF:
                printIndent(*css);
                *css << "for (int loopIdx" << loopNesting << " = 0;;loopIdx" << loopNesting << "++,";
                loopNesting++;
                printBallot(*css, makeFlowState());
                *css << ") {\n";
                indent += 4;
                break;
            case OP_END_FOR_INF:
                loopNesting--;
                indent -= 4;
                printIndent(*css);
                *css << "}\n";
                break;
            case OP_BEGIN_DO_WHILE_UNIF:
                printIndent(*css);
                *css << "{\n";
                indent += 4;
                printIndent(*css);
                *css << "int loopIdx" << loopNesting << " = 0;\n";
                printIndent(*css);
                *css << "do {\n";
                indent += 4;
                printIndent(*css);
                *css << "loopIdx" << loopNesting << "++;\n";
                loopNesting++;
                break;
            case OP_END_DO_WHILE_UNIF:
                loopNesting--;
                indent -= 4;
                printIndent(*css);
                *css << "} while (loopIdx" << loopNesting << " < inputA.a[" << ops[(uint32_t)ops[i].value].value
                     << "]);\n";
                indent -= 4;
                printIndent(*css);
                *css << "}\n";
                break;
            case OP_BEGIN_DO_WHILE_INF:
                printIndent(*css);
                *css << "{\n";
                indent += 4;
                printIndent(*css);
                *css << "int loopIdx" << loopNesting << " = 0;\n";
                printIndent(*css);
                *css << "do {\n";
                indent += 4;
                loopNesting++;
                break;
            case OP_END_DO_WHILE_INF:
                loopNesting--;
                printIndent(*css);
                *css << "loopIdx" << loopNesting << "++;\n";
                indent -= 4;
                printIndent(*css);
                *css << "} while (true);\n";
                indent -= 4;
                printIndent(*css);
                *css << "}\n";
                break;
            case OP_BREAK:
                printIndent(*css);
                *css << "break;\n";
                break;
            case OP_CONTINUE:
                printIndent(*css);
                *css << "continue;\n";
                break;
            case OP_ELECT:
                printIndent(*css);
                *css << "if (subgroupElect()) {\n";
                indent += 4;
                break;
            case OP_RETURN:
                printIndent(*css);
                *css << "return;\n";
                break;
            case OP_CALL_BEGIN:
                printIndent(*css);
                *css << "func" << funcNum << "(";
                for (int32_t n = 0; n < loopNesting; ++n)
                {
                    *css << "loopIdx" << n;
                    if (n != loopNesting - 1)
                        *css << ", ";
                }
                *css << ");\n";
                css = &functions;
                printIndent(*css);
                *css << "void func" << funcNum << "(";
                for (int32_t n = 0; n < loopNesting; ++n)
                {
                    *css << "int loopIdx" << n;
                    if (n != loopNesting - 1)
                        *css << ", ";
                }
                *css << ") {\n";
                indent += 4;
                funcNum++;
                break;
            case OP_CALL_END:
                indent -= 4;
                printIndent(*css);
                *css << "}\n";
                css = &main;
                break;
            case OP_NOISE:
                if (ops[i].value == 0)
                {
                    printIndent(*css);
                    *css << "while (!subgroupElect()) {}\n";
                }
                else
                {
                    printIndent(*css);
                    *css << "if (inputA.a[0] == 12345) {\n";
                    indent += 4;
                    printIndent(*css);
                    *css << "while (true) {\n";
                    indent += 4;
                    printBallot(*css, makeFlowState(), true);
                    indent -= 4;
                    printIndent(*css);
                    *css << "}\n";
                    indent -= 4;
                    printIndent(*css);
                    *css << "}\n";
                }
                break;
            case OP_SWITCH_UNIF_BEGIN:
                printIndent(*css);
                *css << "switch (inputA.a[" << ops[i].value << "]) {\n";
                indent += 4;
                break;
            case OP_SWITCH_VAR_BEGIN:
                printIndent(*css);
                *css << "switch (gl_SubgroupInvocationID & 3) {\n";
                indent += 4;
                break;
            case OP_SWITCH_LOOP_COUNT_BEGIN:
                printIndent(*css);
                *css << "switch (loopIdx" << ops[i].value << ") {\n";
                indent += 4;
                break;
            case OP_SWITCH_END:
                indent -= 4;
                printIndent(*css);
                *css << "}\n";
                break;
            case OP_CASE_MASK_BEGIN:
                for (int32_t b = 0; b < 32; ++b)
                {
                    if ((1u << b) & ops[i].caseValue)
                    {
                        printIndent(*css);
                        *css << "case " << b << ":\n";
                    }
                }
                printIndent(*css);
                *css << "{\n";
                indent += 4;
                break;
            case OP_CASE_LOOP_COUNT_BEGIN:
                if (ops[i].caseValue == 0xFFFFFFFF)
                {
                    printIndent(*css);
                    *css << "default: {\n";
                }
                else
                {
                    printIndent(*css);
                    *css << "case " << ops[i].caseValue << ": {\n";
                }
                indent += 4;
                break;
            case OP_CASE_END:
                printIndent(*css);
                *css << "break;\n";
                indent -= 4;
                printIndent(*css);
                *css << "}\n";
                break;
            default:
                DE_ASSERT(0);
                break;
            }
        }
    }

    // Simulate execution of the program. If countOnly is true, just return
    // the max number of outputs written. If it's false, store out the result
    // values to ref.
    virtual uint32_t simulate(bool countOnly, uint32_t subgroupSize, add_ref<std::vector<uint64_t>> ref) = 0;

    virtual uint32_t execute(qpWatchDog *watchDog, bool countOnly, const uint32_t subgroupSize,
                             const uint32_t fragmentStride, const uint32_t primitiveStride,
                             add_ref<std::vector<tcu::UVec4>> ref, add_ref<tcu::TestLog> log,
                             add_cref<std::vector<uint32_t>> outputP = {}, const tcu::UVec4 *cmp = nullptr,
                             const uint32_t primitiveID = (~0u))
    {
        // Per-invocation output location counters
        std::vector<uint32_t> outLoc;
        std::vector<SubgroupState2> stateStack;
        uint32_t subgroupCount;
        uint32_t logFailureCount;
        auto prerequisites = makePrerequisites(outputP, subgroupSize, fragmentStride, primitiveStride, stateStack,
                                               outLoc, subgroupCount);
        const Ballot fullSubgroupMask = subgroupSizeToMask(subgroupSize, subgroupCount);

        logFailureCount = 10u;
        nesting         = 0;
        loopNesting     = 0;

        int32_t i          = 0;
        uint32_t loopCount = 0;

        while (i < (int32_t)ops.size())
        {
            add_cref<Ballots> activeMask = stateStack[nesting].activeMask;

            if ((loopCount % 5000) == 0 && watchDog)
                qpWatchDog_touch(watchDog);

            switch (ops[i].type)
            {
            case OP_BALLOT:
                // Flag that this ballot is workgroup-nonuniform
                if (caseDef.isWUCF() && activeMask.any() && !activeMask.all())
                    ops[i].caseValue = 1;

                if (caseDef.isSUCF())
                {
                    for (uint32_t id = 0; id < invocationStride; id += subgroupSize)
                    {
                        const Ballot subgroupMask = bitsetToBallot(activeMask, subgroupSize, id);
                        // Flag that this ballot is subgroup-nonuniform
                        if (subgroupMask != 0 && subgroupMask != fullSubgroupMask)
                            ops[i].caseValue = 1;
                    }
                }

                simulateBallot(countOnly, activeMask, primitiveID, i, outLoc, ref, log, prerequisites, logFailureCount,
                               (i > 0 ? ops[i - 1].type : OP_BALLOT), cmp);
                break;
            case OP_STORE:
                simulateStore(countOnly, stateStack[nesting].activeMask, primitiveID, ops[i].value, outLoc, ref, log,
                              prerequisites, logFailureCount, (i > 0 ? ops[i - 1].type : OP_STORE), cmp);
                break;
            case OP_IF_MASK:
                nesting++;
                stateStack[nesting].activeMask =
                    stateStack[nesting - 1].activeMask & ballotsFromBallot(ops[i].bvalue, subgroupSize, subgroupCount);
                stateStack[nesting].header   = i;
                stateStack[nesting].isLoop   = 0;
                stateStack[nesting].isSwitch = 0;
                break;
            case OP_ELSE_MASK:
                stateStack[nesting].activeMask =
                    stateStack[nesting - 1].activeMask &
                    ~ballotsFromBallot(ops[stateStack[nesting].header].bvalue, subgroupSize, subgroupCount);
                break;
            case OP_IF_LOOPCOUNT:
            {
                uint32_t n = nesting;
                while (!stateStack[n].isLoop)
                    n--;
                const Ballot tripBallot = Ballot::withSetBit(stateStack[n].tripCount);

                nesting++;
                stateStack[nesting].activeMask =
                    stateStack[nesting - 1].activeMask & ballotsFromBallot(tripBallot, subgroupSize, subgroupCount);
                stateStack[nesting].header   = i;
                stateStack[nesting].isLoop   = 0;
                stateStack[nesting].isSwitch = 0;
                break;
            }
            case OP_ELSE_LOOPCOUNT:
            {
                uint32_t n = nesting;
                while (!stateStack[n].isLoop)
                    n--;
                const Ballot tripBallot = Ballot::withSetBit(stateStack[n].tripCount);

                stateStack[nesting].activeMask =
                    stateStack[nesting - 1].activeMask & ~ballotsFromBallot(tripBallot, subgroupSize, subgroupCount);
                break;
            }
            case OP_IF_LOCAL_INVOCATION_INDEX:
            {
                // all bits >= N
                Ballots mask(subgroupCount);
                const uint32_t maxID = subgroupCount * subgroupSize;
                for (uint32_t id = static_cast<uint32_t>(ops[i].value); id < maxID; ++id)
                {
                    mask.set(Ballots::findBit(id, subgroupSize));
                }

                nesting++;
                stateStack[nesting].activeMask = stateStack[nesting - 1].activeMask & mask;
                stateStack[nesting].header     = i;
                stateStack[nesting].isLoop     = 0;
                stateStack[nesting].isSwitch   = 0;
                break;
            }
            case OP_ELSE_LOCAL_INVOCATION_INDEX:
            {
                // all bits < N
                Ballots mask(subgroupCount);
                const uint32_t maxID = subgroupCount * subgroupSize;
                for (uint32_t id = 0u; id < static_cast<uint32_t>(ops[i].value) && id < maxID; ++id)
                {
                    mask.set(Ballots::findBit(id, subgroupSize));
                }

                stateStack[nesting].activeMask = stateStack[nesting - 1].activeMask & mask;
                break;
            }
            case OP_ENDIF:
                nesting--;
                break;
            case OP_BEGIN_FOR_UNIF:
                // XXX TODO: We don't handle a for loop with zero iterations
                nesting++;
                loopNesting++;
                stateStack[nesting].activeMask   = stateStack[nesting - 1].activeMask;
                stateStack[nesting].header       = i;
                stateStack[nesting].tripCount    = 0;
                stateStack[nesting].isLoop       = 1;
                stateStack[nesting].isSwitch     = 0;
                stateStack[nesting].continueMask = 0;
                break;
            case OP_END_FOR_UNIF:
                stateStack[nesting].tripCount++;
                stateStack[nesting].activeMask |= stateStack[nesting].continueMask;
                stateStack[nesting].continueMask = 0;
                if (stateStack[nesting].tripCount < ops[stateStack[nesting].header].value &&
                    stateStack[nesting].activeMask.any())
                {
                    i = stateStack[nesting].header + 1;
                    continue;
                }
                else
                {
                    loopNesting--;
                    nesting--;
                }
                break;
            case OP_BEGIN_DO_WHILE_UNIF:
                // XXX TODO: We don't handle a for loop with zero iterations
                nesting++;
                loopNesting++;
                stateStack[nesting].activeMask   = stateStack[nesting - 1].activeMask;
                stateStack[nesting].header       = i;
                stateStack[nesting].tripCount    = 1;
                stateStack[nesting].isLoop       = 1;
                stateStack[nesting].isSwitch     = 0;
                stateStack[nesting].continueMask = 0;
                break;
            case OP_END_DO_WHILE_UNIF:
                stateStack[nesting].activeMask |= stateStack[nesting].continueMask;
                stateStack[nesting].continueMask = 0;
                if (stateStack[nesting].tripCount < ops[stateStack[nesting].header].value &&
                    stateStack[nesting].activeMask.any())
                {
                    i = stateStack[nesting].header + 1;
                    stateStack[nesting].tripCount++;
                    continue;
                }
                else
                {
                    loopNesting--;
                    nesting--;
                }
                break;
            case OP_BEGIN_FOR_VAR:
                // XXX TODO: We don't handle a for loop with zero iterations
                nesting++;
                loopNesting++;
                stateStack[nesting].activeMask   = stateStack[nesting - 1].activeMask;
                stateStack[nesting].header       = i;
                stateStack[nesting].tripCount    = 0;
                stateStack[nesting].isLoop       = 1;
                stateStack[nesting].isSwitch     = 0;
                stateStack[nesting].continueMask = 0;
                break;
            case OP_END_FOR_VAR:
            {
                stateStack[nesting].tripCount++;
                stateStack[nesting].activeMask |= stateStack[nesting].continueMask;
                stateStack[nesting].continueMask = 0;
                Ballot tripBallot;
                if (subgroupSize != stateStack[nesting].tripCount)
                {
                    for (uint32_t bit = stateStack[nesting].tripCount; bit < tripBallot.size(); ++bit)
                        tripBallot.set(bit);
                }
                stateStack[nesting].activeMask &= ballotsFromBallot(tripBallot, subgroupSize, subgroupCount);

                if (stateStack[nesting].activeMask.any())
                {
                    i = stateStack[nesting].header + 1;
                    continue;
                }
                else
                {
                    loopNesting--;
                    nesting--;
                }
                break;
            }
            case OP_BEGIN_FOR_INF:
            case OP_BEGIN_DO_WHILE_INF:
                nesting++;
                loopNesting++;
                stateStack[nesting].activeMask   = stateStack[nesting - 1].activeMask;
                stateStack[nesting].header       = i;
                stateStack[nesting].tripCount    = 0;
                stateStack[nesting].isLoop       = 1;
                stateStack[nesting].isSwitch     = 0;
                stateStack[nesting].continueMask = 0;
                break;
            case OP_END_FOR_INF:
                stateStack[nesting].tripCount++;
                stateStack[nesting].activeMask |= stateStack[nesting].continueMask;
                stateStack[nesting].continueMask = 0;
                if (stateStack[nesting].activeMask.any())
                {
                    // output expected OP_BALLOT values
                    simulateBallot(countOnly, stateStack[nesting].activeMask, primitiveID, i, outLoc, ref, log,
                                   prerequisites, logFailureCount, (i > 0 ? ops[i - 1].type : OP_BALLOT), cmp);

                    i = stateStack[nesting].header + 1;
                    continue;
                }
                else
                {
                    loopNesting--;
                    nesting--;
                }
                break;
            case OP_END_DO_WHILE_INF:
                stateStack[nesting].tripCount++;
                stateStack[nesting].activeMask |= stateStack[nesting].continueMask;
                stateStack[nesting].continueMask = 0;
                if (stateStack[nesting].activeMask.any())
                {
                    i = stateStack[nesting].header + 1;
                    continue;
                }
                else
                {
                    loopNesting--;
                    nesting--;
                }
                break;
            case OP_BREAK:
            {
                uint32_t n         = nesting;
                const Ballots mask = stateStack[nesting].activeMask;
                while (true)
                {
                    stateStack[n].activeMask &= ~mask;
                    if (stateStack[n].isLoop || stateStack[n].isSwitch)
                        break;

                    n--;
                }
            }
            break;
            case OP_CONTINUE:
            {
                uint32_t n         = nesting;
                const Ballots mask = stateStack[nesting].activeMask;
                while (true)
                {
                    stateStack[n].activeMask &= ~mask;
                    if (stateStack[n].isLoop)
                    {
                        stateStack[n].continueMask |= mask;
                        break;
                    }
                    n--;
                }
            }
            break;
            case OP_ELECT:
            {
                nesting++;
                stateStack[nesting].activeMask = bitsetElect(stateStack[nesting - 1].activeMask);
                stateStack[nesting].header     = i;
                stateStack[nesting].isLoop     = 0;
                stateStack[nesting].isSwitch   = 0;
            }
            break;
            case OP_RETURN:
            {
                const Ballots mask = stateStack[nesting].activeMask;
                for (int32_t n = nesting; n >= 0; --n)
                {
                    stateStack[n].activeMask &= ~mask;
                    if (stateStack[n].isCall)
                        break;
                }
            }
            break;

            case OP_CALL_BEGIN:
                nesting++;
                stateStack[nesting].activeMask = stateStack[nesting - 1].activeMask;
                stateStack[nesting].isLoop     = 0;
                stateStack[nesting].isSwitch   = 0;
                stateStack[nesting].isCall     = 1;
                break;
            case OP_CALL_END:
                stateStack[nesting].isCall = 0;
                nesting--;
                break;
            case OP_NOISE:
                break;

            case OP_SWITCH_UNIF_BEGIN:
            case OP_SWITCH_VAR_BEGIN:
            case OP_SWITCH_LOOP_COUNT_BEGIN:
                nesting++;
                stateStack[nesting].activeMask = stateStack[nesting - 1].activeMask;
                stateStack[nesting].header     = i;
                stateStack[nesting].isLoop     = 0;
                stateStack[nesting].isSwitch   = 1;
                break;
            case OP_SWITCH_END:
                nesting--;
                break;
            case OP_CASE_MASK_BEGIN:
                stateStack[nesting].activeMask =
                    stateStack[nesting - 1].activeMask & ballotsFromBallot(ops[i].bvalue, subgroupSize, subgroupCount);
                break;
            case OP_CASE_LOOP_COUNT_BEGIN:
            {
                uint32_t n = nesting;
                uint32_t l = loopNesting;

                while (true)
                {
                    if (stateStack[n].isLoop)
                    {
                        l--;
                        if (l == ops[stateStack[nesting].header].value)
                            break;
                    }
                    n--;
                }

                if ((Ballot::withSetBit(stateStack[n].tripCount) & Ballot(ops[i].bvalue)).any())
                    stateStack[nesting].activeMask = stateStack[nesting - 1].activeMask;
                else
                    stateStack[nesting].activeMask = 0;
                break;
            }
            case OP_CASE_END:
                break;

            default:
                DE_ASSERT(0);
                break;
            }
            i++;
            loopCount++;
        }
        uint32_t maxLoc = 0;
        for (uint32_t id = 0; id < (uint32_t)outLoc.size(); ++id)
            maxLoc = de::max(maxLoc, outLoc[id]);

        return maxLoc;
    }

    bool hasUCF() const
    {
        for (int32_t i = 0; i < (int32_t)ops.size(); ++i)
        {
            if (ops[i].type == OP_BALLOT && ops[i].caseValue == 0)
                return true;
        }
        return false;
    }

protected:
    virtual std::shared_ptr<Prerequisites> makePrerequisites(add_cref<std::vector<uint32_t>> outputP,
                                                             const uint32_t subgroupSize, const uint32_t fragmentStride,
                                                             const uint32_t primitiveStride,
                                                             add_ref<std::vector<SubgroupState2>> stateStack,
                                                             add_ref<std::vector<uint32_t>> outLoc,
                                                             add_ref<uint32_t> subgroupCount)
    {
        DE_UNREF(outputP);
        DE_UNREF(subgroupSize);
        DE_UNREF(fragmentStride);
        DE_UNREF(primitiveStride);
        DE_UNREF(stateStack);
        DE_UNREF(outLoc);
        DE_UNREF(subgroupCount);
        return std::make_shared<Prerequisites>();
    }

    virtual void simulateBallot(const bool countOnly, add_cref<Ballots> activeMask, const uint32_t primitiveID,
                                const int32_t opsIndex, add_ref<std::vector<uint32_t>> outLoc,
                                add_ref<std::vector<tcu::UVec4>> ref, add_ref<tcu::TestLog> log,
                                std::shared_ptr<Prerequisites> prerequisites, add_ref<uint32_t> logFailureCount,
                                const OPType reason, const tcu::UVec4 *cmp)
    {
        DE_UNREF(countOnly);
        DE_UNREF(activeMask);
        DE_UNREF(primitiveID);
        DE_UNREF(opsIndex);
        DE_UNREF(outLoc);
        DE_UNREF(ref);
        DE_UNREF(log);
        DE_UNREF(prerequisites);
        DE_UNREF(logFailureCount);
        DE_UNREF(reason);
        DE_UNREF(cmp);
    }

    virtual void simulateStore(const bool countOnly, add_cref<Ballots> activeMask, const uint32_t primitiveID,
                               const uint64_t storeValue, add_ref<std::vector<uint32_t>> outLoc,
                               add_ref<std::vector<tcu::UVec4>> ref, add_ref<tcu::TestLog> log,
                               std::shared_ptr<Prerequisites> prerequisites, add_ref<uint32_t> logFailureCount,
                               const OPType reason, const tcu::UVec4 *cmp)
    {
        DE_UNREF(countOnly);
        DE_UNREF(activeMask);
        DE_UNREF(primitiveID);
        DE_UNREF(storeValue);
        DE_UNREF(outLoc);
        DE_UNREF(ref);
        DE_UNREF(log);
        DE_UNREF(prerequisites);
        DE_UNREF(logFailureCount);
        DE_UNREF(reason);
        DE_UNREF(cmp);
    }
};

class ComputeRandomProgram : public RandomProgram
{
public:
    ComputeRandomProgram(const CaseDef &c) : RandomProgram(c, uint32_t(c.sizeX * c.sizeY))
    {
        DE_ASSERT(c.shaderStage == VK_SHADER_STAGE_COMPUTE_BIT);
    }
    virtual ~ComputeRandomProgram() = default;

    virtual uint32_t simulate(bool countOnly, uint32_t subgroupSize, add_ref<std::vector<uint64_t>> ref) override
    {
        DE_ASSERT(false);
        // Do not use this method, to simulate generated program use simulate2 instead
        DE_UNREF(countOnly);
        DE_UNREF(subgroupSize);
        DE_UNREF(ref);
        return 0;
    }

    struct ComputePrerequisites : Prerequisites
    {
        const uint32_t m_subgroupSize;
        ComputePrerequisites(uint32_t subgroupSize) : m_subgroupSize(subgroupSize)
        {
        }
    };

    virtual void printBallot(add_ref<std::stringstream> css, add_cref<FlowState>,
                             bool endWithSemicolon = false) override
    {
        printIndent(css);

        css << "outputC.loc[gl_LocalInvocationIndex]++,";
        // When inside loop(s), use partitionBallot rather than subgroupBallot to compute
        // a ballot, to make sure the ballot is "diverged enough". Don't do this for
        // subgroup_uniform_control_flow, since we only validate results that must be fully
        // reconverged.
        if (loopNesting > 0 && caseDef.testType == TT_MAXIMAL)
        {
            css << "outputB.b[(outLoc++)*invocationStride + gl_LocalInvocationIndex] = " << getPartitionBallotText();
        }
        else if (caseDef.isElect())
        {
            css << "outputB.b[(outLoc++)*invocationStride + gl_LocalInvocationIndex].x = elect()";
        }
        else
        {
            css << "outputB.b[(outLoc++)*invocationStride + gl_LocalInvocationIndex] = subgroupBallot(true)";
        }
        if (endWithSemicolon)
        {
            css << ";\n";
        }
    }

protected:
    virtual void simulateBallot(const bool countOnly, add_cref<Ballots> activeMask, const uint32_t unusedPrimitiveID,
                                const int32_t opsIndex, add_ref<std::vector<uint32_t>> outLoc,
                                add_ref<std::vector<tcu::UVec4>> ref, add_ref<tcu::TestLog> log,
                                std::shared_ptr<Prerequisites> prerequisites, add_ref<uint32_t> logFailureCount,
                                const OPType reason, const tcu::UVec4 *cmp) override
    {
        DE_UNREF(unusedPrimitiveID);
        DE_UNREF(log);
        DE_UNREF(logFailureCount);
        DE_UNREF(reason);
        DE_UNREF(cmp);
        const uint32_t subgroupCount = activeMask.subgroupCount();
        const uint32_t subgroupSize  = static_pointer_cast<ComputePrerequisites>(prerequisites)->m_subgroupSize;

        for (uint32_t id = 0; id < invocationStride; ++id)
        {
            if (activeMask.test((Ballots::findBit(id, subgroupSize))))
            {
                if (countOnly)
                {
                    outLoc[id]++;
                }
                else
                {
                    if (ops[opsIndex].caseValue)
                    {
                        // Emit a magic value to indicate that we shouldn't validate this ballot
                        ref[(outLoc[id]++) * invocationStride + id] =
                            bitsetToBallot(0x12345678, subgroupCount, subgroupSize, id);
                    }
                    else
                        ref[(outLoc[id]++) * invocationStride + id] = bitsetToBallot(activeMask, subgroupSize, id);
                }
            }
        }
    }

    virtual void simulateStore(const bool countOnly, add_cref<Ballots> activeMask, const uint32_t unusedPrimitiveID,
                               const uint64_t storeValue, add_ref<std::vector<uint32_t>> outLoc,
                               add_ref<std::vector<tcu::UVec4>> ref, add_ref<tcu::TestLog> log,
                               std::shared_ptr<Prerequisites> prerequisites, add_ref<uint32_t> logFailureCount,
                               const OPType reason, const tcu::UVec4 *cmp) override
    {
        DE_UNREF(unusedPrimitiveID);
        DE_UNREF(log);
        DE_UNREF(logFailureCount);
        DE_UNREF(reason);
        DE_UNREF(cmp);
        const uint32_t subgroupSize = static_pointer_cast<ComputePrerequisites>(prerequisites)->m_subgroupSize;
        for (uint32_t id = 0; id < invocationStride; ++id)
        {
            if (activeMask.test(Ballots::findBit(id, subgroupSize)))
            {
                if (countOnly)
                    outLoc[id]++;
                else
                    ref[(outLoc[id]++) * invocationStride + id] =
                        Ballot(tcu::UVec4(uint32_t(storeValue & 0xFFFFFFFF), 0u, 0u, 0u));
            }
        }
    }

    virtual std::shared_ptr<Prerequisites> makePrerequisites(add_cref<std::vector<uint32_t>> outputP,
                                                             const uint32_t subgroupSize, const uint32_t fragmentStride,
                                                             const uint32_t primitiveStride,
                                                             add_ref<std::vector<SubgroupState2>> stateStack,
                                                             add_ref<std::vector<uint32_t>> outLoc,
                                                             add_ref<uint32_t> subgroupCount) override
    {
        DE_UNREF(outputP);
        DE_UNREF(fragmentStride);
        DE_ASSERT(invocationStride == primitiveStride);
        auto prerequisites = std::make_shared<ComputePrerequisites>(subgroupSize);
        subgroupCount      = ROUNDUP(invocationStride, subgroupSize) / subgroupSize;
        stateStack.resize(10u, SubgroupState2(subgroupCount));
        outLoc.resize(primitiveStride, 0u);
        add_ref<Ballots> activeMask(stateStack.at(0).activeMask);
        for (uint32_t id = 0; id < invocationStride; ++id)
        {
            activeMask.set(Ballots::findBit(id, subgroupSize));
        }
        return prerequisites;
    }
};

class FragmentRandomProgram : public RandomProgram
{
public:
#define BALLOT_STACK_SIZE_DEFVAL_LINE (__LINE__ + 1)
    static constexpr const uint32_t experimentalOutLocSize      = 16384;
    static constexpr const uint32_t conditionIfInvocationStride = 511u;
    FragmentRandomProgram(const CaseDef &c) : RandomProgram(c, conditionIfInvocationStride)
    {
        DE_ASSERT(caseDef.testType == TT_MAXIMAL);
        DE_ASSERT(c.shaderStage == VK_SHADER_STAGE_FRAGMENT_BIT);
    }
    virtual ~FragmentRandomProgram() = default;

    static de::MovePtr<FragmentRandomProgram> create(const CaseDef &c)
    {
        return de::MovePtr<FragmentRandomProgram>(new FragmentRandomProgram(c));
    }

    virtual void printIfLocalInvocationIndex(add_ref<std::stringstream> css, add_cref<FlowState> flow) override
    {
        printIndent(css);
        css << "if (invocationIndex() >= inputA.a[0x" << std::hex << flow.ops[flow.opsIndex].value << "]) {\n";
    }

    virtual void printStore(add_ref<std::stringstream> css, add_cref<FlowState> flow) override
    {
        printIndent(css);
        css << "storeValue(outLoc++, 0x" << std::hex << flow.ops[flow.opsIndex].value << ");\n";
    }

    virtual void printBallot(add_ref<std::stringstream> css, add_cref<FlowState>,
                             bool endWidthSemicolon = false) override
    {
        printIndent(css);
        // When inside loop(s), use partitionBallot rather than subgroupBallot to compute
        // a ballot, to make sure the ballot is "diverged enough". Don't do this for
        // subgroup_uniform_control_flow, since we only validate results that must be fully
        // reconverged.
        if (loopNesting > 0)
        {
            css << "storeBallot(outLoc++)";
        }
        else
        {
            css << getPartitionBallotText();
        }
        if (endWidthSemicolon)
        {
            css << ";\n";
        }
    }

    virtual std::string getPartitionBallotText() override
    {
        return "storeBallot(outLoc++)";
    }

    virtual void genIf(IFType ifType, uint32_t maxLocalIndexCmp = 0u) override
    {
        DE_UNREF(maxLocalIndexCmp);
        RandomProgram::genIf(ifType, conditionIfInvocationStride);
    }

    struct Arrangement : Prerequisites, ReconvergenceTestFragmentInstance::Arrangement
    {
        const uint32_t m_width;
        const uint32_t m_height;
        const uint32_t m_subgroupSize;
        const uint32_t m_fragmentStride;
        const uint32_t m_primitiveStride;
        const uint32_t m_subgroupCount;
        const Ballots m_initialBallots;
        const Ballots m_nonHelperInitialBallots;
        const uint32_t m_invocationStride;
        const std::vector<std::vector<uint32_t>> m_fragmentSubgroups;
        Arrangement(add_cref<std::vector<uint32_t>> info, uint32_t width, uint32_t height, uint32_t subgroupSize,
                    uint32_t primitiveStride)
            : m_width(width)
            , m_height(height)
            , m_subgroupSize(subgroupSize)
            , m_fragmentStride(width * height)
            , m_primitiveStride(primitiveStride)
            , m_subgroupCount(calcSubgroupCount(info, primitiveStride, m_fragmentStride))
            , m_initialBallots(makeInitialBallots(info, primitiveStride, m_fragmentStride, false))
            , m_nonHelperInitialBallots(makeInitialBallots(info, primitiveStride, m_fragmentStride, true))
            , m_invocationStride(calcInvocationStride(info, subgroupSize, primitiveStride, m_fragmentStride))
            , m_fragmentSubgroups(makeFragmentSubgroups(info, subgroupSize, primitiveStride, m_fragmentStride))
        {
        }
        static uint32_t calcSubgroupCount(add_cref<std::vector<uint32_t>> info, const uint32_t primitiveStride,
                                          const uint32_t fragmentStride)
        {
            const uint32_t cc = fragmentStride * primitiveStride;
            std::set<uint32_t> s;
            uint32_t subgroupID;
            uint32_t subgroupInvocationID;
            uint32_t isHelperInvocation;
            for (uint32_t c = 0u; c < cc; ++c)
            {
                if (validID(info.at(c), subgroupID, subgroupInvocationID, isHelperInvocation))
                    s.insert(subgroupID);
            }
            const uint32_t gMin = *s.begin();
            DE_UNREF(gMin);
            const uint32_t gMax = *std::next(s.begin(), (s.size() - 1u));
            DE_UNREF(gMax);
            DE_ASSERT(gMin == 0u);
            DE_ASSERT(gMax == (s.size() - 1u));
            return static_cast<uint32_t>(s.size());
        }
        static uint32_t calcInvocationStride(add_cref<std::vector<uint32_t>> info, const uint32_t subgroupSize,
                                             const uint32_t primitiveStride, const uint32_t fragmentStride)
        {
            return calcSubgroupCount(info, fragmentStride, primitiveStride) * subgroupSize;
        }
        static Ballots makeInitialBallots(add_cref<std::vector<uint32_t>> info, const uint32_t primitiveStride,
                                          const uint32_t fragmentStride, bool excludeHelpers)
        {
            uint32_t subgroupID;
            uint32_t subgroupInvocationID;
            uint32_t isHelperInvocation;
            Ballots b(calcSubgroupCount(info, fragmentStride, primitiveStride));
            const uint32_t cc = fragmentStride * primitiveStride;
            for (uint32_t c = 0u; c < cc; ++c)
            {
                if (validID(info.at(c), subgroupID, subgroupInvocationID, isHelperInvocation))
                {
                    if (!(excludeHelpers && (isHelperInvocation != 0)))
                        b.at(subgroupID).set(subgroupInvocationID);
                }
            }
            return b;
        }
        // Fully Qualified Invocation Name
        static uint32_t fqin(uint32_t maybeHelperFragmentFQIN, add_ref<uint32_t> isHelperInvocation)
        {
            isHelperInvocation = maybeHelperFragmentFQIN >> 31;
            return (maybeHelperFragmentFQIN & 0x7FFFFFFF);
        }
        static auto makeFragmentSubgroups(add_cref<std::vector<uint32_t>> info, const uint32_t subgroupSize,
                                          const uint32_t primitiveStride, const uint32_t fragmentStride)
            -> std::vector<std::vector<uint32_t>>
        {
            const uint32_t subgroupCount = calcSubgroupCount(info, fragmentStride, primitiveStride);
            std::vector<std::vector<uint32_t>> map(primitiveStride);
            for (uint32_t p = 0u; p < primitiveStride; ++p)
                map[p].resize(fragmentStride, (subgroupCount * subgroupSize));

            uint32_t subgroupID;
            uint32_t subgroupInvocationID;
            uint32_t isHelperInvocation;
            for (uint32_t p = 0u; p < primitiveStride; ++p)
                for (uint32_t f = 0u; f < fragmentStride; ++f)
                {
                    const uint32_t sgid = info.at(f * primitiveStride + p);
                    if (validID(sgid, subgroupID, subgroupInvocationID, isHelperInvocation))
                        map.at(p).at(f) =
                            (subgroupID * subgroupSize + subgroupInvocationID) | (isHelperInvocation << 31);
                }
            return map;
        }
        static uint32_t calcRealInvocationCount(add_cref<std::vector<uint32_t>> info, uint32_t primitiveStride,
                                                uint32_t fragmentStride)
        {
            const uint32_t cc = fragmentStride * primitiveStride;
            uint32_t n        = 0u;
            for (uint32_t c = 0u; c < cc; ++c)
            {
                if (info[c])
                    ++n;
            }
            return n;
        }

    private:
        static bool validID(const uint32_t id)
        {
            uint32_t subgroupID;
            DE_UNREF(subgroupID);
            uint32_t subgroupInvocationID;
            DE_UNREF(subgroupInvocationID);
            uint32_t isHelperInvocation;
            DE_UNREF(isHelperInvocation);
            return validID(id, subgroupID, subgroupInvocationID, isHelperInvocation);
        }
        static bool validID(const uint32_t id, add_ref<uint32_t> subgroupID, add_ref<uint32_t> subgroupInvocationID,
                            add_ref<uint32_t> isHelperInvocation)
        {
            if (id != 0u)
            {
                subgroupInvocationID = (id & 0xFFFF);
                subgroupID           = ((id >> 16) & 0x7FFF) - 1u;
                isHelperInvocation   = (id >> 31);
                return true;
            }
            return false;
        }
    };

    virtual uint32_t simulate(bool countOnly, uint32_t subgroupSize, add_ref<std::vector<uint64_t>> ref) override
    {
        DE_ASSERT(false); // use overloaded version of simulate() instead
        DE_UNREF(countOnly);
        DE_UNREF(subgroupSize);
        DE_UNREF(ref);
        return 0;
    }

    // Simulate execution of the program. If countOnly is true, just return
    // the max number of outputs written. If it's false, store out the result
    // values to ref.
    virtual uint32_t execute(qpWatchDog *watchDog, bool countOnly, const uint32_t subgroupSize,
                             const uint32_t fragmentStride, const uint32_t primitiveStride,
                             add_ref<std::vector<tcu::UVec4>> ref, add_ref<tcu::TestLog> log,
                             add_cref<std::vector<uint32_t>> outputP, const tcu::UVec4 *cmp = nullptr,
                             const uint32_t reserved = (~0u)) override
    {
        DE_UNREF(reserved);
        uint32_t outLocs    = 0u;
        uint32_t maxOutLocs = 0u;
        for (uint32_t primitiveID = 0u; primitiveID < primitiveStride; ++primitiveID)
        {
            outLocs    = RandomProgram::execute(watchDog, countOnly, subgroupSize, fragmentStride, primitiveStride, ref,
                                                log, outputP, cmp, primitiveID);
            maxOutLocs = std::max(outLocs, maxOutLocs);
        }
        return maxOutLocs;
    }

protected:
    virtual void simulateStore(const bool countOnly, add_cref<Ballots> activeMask, const uint32_t primitiveID,
                               const uint64_t storeValue, add_ref<std::vector<uint32_t>> outLoc,
                               add_ref<std::vector<tcu::UVec4>> ref, add_ref<tcu::TestLog> log,
                               std::shared_ptr<Prerequisites> prerequisites, add_ref<uint32_t> logFailureCount,
                               const OPType reason, const tcu::UVec4 *cmp) override
    {
        uint32_t isHelperInvocation;
        add_cref<Arrangement> a(*std::static_pointer_cast<Arrangement>(prerequisites));
        for (const uint32_t id : a.m_fragmentSubgroups.at(primitiveID))
        {
            const uint32_t sgid = a.fqin(id, isHelperInvocation);
            if (sgid >= (a.m_subgroupCount * a.m_subgroupSize))
                continue;
            if (false == activeMask.test(Ballots::findBit(sgid, a.m_subgroupSize)))
                continue;
            const uint32_t loc   = primitiveID * a.m_subgroupCount * 128 + sgid;
            const uint32_t index = ((outLoc.at(loc)++) * (a.m_primitiveStride * a.m_subgroupCount * 128) +
                                    (primitiveID * a.m_subgroupCount * 128) + sgid);
            if (false == countOnly)
            {
                ref.at(index) = tcu::UVec4(uint32_t(storeValue & 0xFFFFFFFF), 0u, 0u, 0u);
                if (cmp && logFailureCount > 0u && cmp[index] != ref.at(index))
                {
                    logFailureCount -= 1u;
                    log << tcu::TestLog::Message << logFailureCount << ": stored value mismatch from "
                        << OPtypeToStr(reason) << tcu::TestLog::EndMessage;
                }
            }
        }
    }

    virtual void simulateBallot(const bool countOnly, add_cref<Ballots> activeMask, const uint32_t primitiveID,
                                const int32_t opsIndex, add_ref<std::vector<uint32_t>> outLoc,
                                add_ref<std::vector<tcu::UVec4>> ref, add_ref<tcu::TestLog> log,
                                std::shared_ptr<Prerequisites> prerequisites, add_ref<uint32_t> logFailureCount,
                                const OPType reason, const tcu::UVec4 *cmp) override
    {
        DE_UNREF(opsIndex);
        uint32_t isHelperInvocation;
        add_cref<Arrangement> a(*std::static_pointer_cast<Arrangement>(prerequisites));
        for (const uint32_t id : a.m_fragmentSubgroups.at(primitiveID))
        {
            const uint32_t sgid = a.fqin(id, isHelperInvocation);
            if (sgid >= (a.m_subgroupCount * a.m_subgroupSize))
                continue;
            if (false == activeMask.test(Ballots::findBit(sgid, a.m_subgroupSize)))
                continue;
            const uint32_t loc   = primitiveID * a.m_subgroupCount * 128 + sgid;
            const uint32_t index = ((outLoc.at(loc)++) * (a.m_primitiveStride * a.m_subgroupCount * 128) +
                                    (primitiveID * a.m_subgroupCount * 128) + sgid);
            if (false == countOnly)
            {
                ref.at(index) = Ballot(activeMask.at(sgid / a.m_subgroupSize));
                if (cmp && logFailureCount > 0u && cmp[index] != ref.at(index))
                {
                    logFailureCount -= 1u;
                    log << tcu::TestLog::Message << logFailureCount << ": ballot mismatch from " << OPtypeToStr(reason)
                        << tcu::TestLog::EndMessage;
                }
            }
        }
    }

    virtual std::shared_ptr<Prerequisites> makePrerequisites(add_cref<std::vector<uint32_t>> outputP,
                                                             const uint32_t subgroupSize, const uint32_t fragmentStride,
                                                             const uint32_t primitiveStride,
                                                             add_ref<std::vector<SubgroupState2>> stateStack,
                                                             add_ref<std::vector<uint32_t>> outLoc,
                                                             add_ref<uint32_t> subgroupCount) override
    {
        auto prerequisites = std::make_shared<Arrangement>(outputP, fragmentStride, 1u, subgroupSize, primitiveStride);
        subgroupCount      = prerequisites->m_subgroupCount;
        stateStack.resize(10u, SubgroupState2(subgroupCount));
        outLoc.resize((subgroupCount * 128u * fragmentStride), 0u);
        stateStack.at(0).activeMask = prerequisites->m_initialBallots;
        return prerequisites;
    }
};

class VertexRandomProgram : public RandomProgram
{
public:
    static const constexpr uint32_t fillPercentage = 73u;
    VertexRandomProgram(add_cref<CaseDef> c)
        : RandomProgram(c,
                        static_cast<uint32_t>(Arrangement::generatePrimitives(c.sizeX, c.sizeY, fillPercentage).size()))
    {
        DE_ASSERT(c.shaderStage == VK_SHADER_STAGE_VERTEX_BIT);
    }
    virtual ~VertexRandomProgram() = default;

    struct Arrangement : Prerequisites
    {
        static constexpr uint32_t NUM_SUBGROUPS_OFFSET      = 0u;
        static constexpr uint32_t SUBGROUP_SIZE_OFFSET      = 1u;
        static constexpr uint32_t INVOCATION_COUNT_OFFSET   = 2u;
        static constexpr uint32_t INVOCATION_ENTRIES_OFFSET = 3u;

        const uint32_t m_subgroupSize;
        const uint32_t m_primitiveStride;
        const uint32_t m_subgroupCount;
        const Ballots m_initialBallots;
        const uint32_t m_invocationStride;
        const std::vector<uint32_t> m_primitiveSubgroups;
        Arrangement(add_cref<std::vector<uint32_t>> outputP, uint32_t subgroupSize, uint32_t primitiveStride)
            : m_subgroupSize(subgroupSize)
            , m_primitiveStride(primitiveStride)
            , m_subgroupCount(calcSubgroupCount(outputP))
            , m_initialBallots(makeInitialBallots(subgroupSize, primitiveStride, outputP))
            , m_invocationStride(primitiveStride)
            , m_primitiveSubgroups(makePrimitiveSubgroups(subgroupSize, primitiveStride, outputP))
        {
        }
        static uint32_t calcSubgroupCount(add_cref<std::vector<uint32_t>> outputP)
        {
            return outputP.at(NUM_SUBGROUPS_OFFSET);
        }
        static uint32_t calcSubgroupSize(add_cref<std::vector<uint32_t>> outputP)
        {
            return outputP.at(SUBGROUP_SIZE_OFFSET);
        }
        static uint32_t calcSubgroupInvocationStride(add_cref<std::vector<uint32_t>> outputP)
        {
            return outputP.at(INVOCATION_COUNT_OFFSET);
        }
        static Ballots makeInitialBallots(uint32_t subgroupSize, uint32_t primitiveStride,
                                          add_cref<std::vector<uint32_t>> outputP)
        {
            DE_UNREF(subgroupSize);
            const uint32_t subgroupCount = calcSubgroupCount(outputP);
            Ballots initialBallots(subgroupCount);
            for (uint32_t primitiveID = 0u; primitiveID < primitiveStride; ++primitiveID)
            {
                const uint32_t id = outputP.at(primitiveID + INVOCATION_ENTRIES_OFFSET);
                if (id)
                {
                    const uint32_t subgroupID           = (id >> 16) - 1u;
                    const uint32_t subgroupInvocationID = id & 0xFFFF;
                    DE_ASSERT(subgroupID < subgroupCount);
                    DE_ASSERT(subgroupInvocationID < subgroupSize);
                    initialBallots.at(subgroupID).set(subgroupInvocationID);
                }
            }
            return initialBallots;
        }
        static std::vector<uint32_t> makePrimitiveSubgroups(uint32_t subgroupSize, uint32_t primitiveStride,
                                                            add_cref<std::vector<uint32_t>> outputP)
        {
            std::vector<uint32_t> map(primitiveStride);
            for (uint32_t primitiveID = 0u; primitiveID < primitiveStride; ++primitiveID)
            {
                const uint32_t id = outputP.at(primitiveID + INVOCATION_ENTRIES_OFFSET);
                if (id)
                {
                    const uint32_t subgroupID           = (id >> 16) - 1u;
                    const uint32_t subgroupInvocationID = id & 0xFFFF;
                    DE_ASSERT(subgroupInvocationID < subgroupSize);
                    map.at(primitiveID) = subgroupID * subgroupSize + subgroupInvocationID;
                }
            }
            return map;
        }
        static std::vector<tcu::Vec4> generatePrimitives(uint32_t width, uint32_t height, uint32_t fillPercent)
        {
            deRandom rnd;
            std::map<uint32_t, int> map;
            std::vector<tcu::Vec4> points;
            const uint32_t frags = (width * height);
            const uint32_t total = (frags * fillPercent) / 100u;

            deRandom_init(&rnd, (width * height));

            for (uint32_t i = 0u; i < total; ++i)
            {
                const uint32_t r = deRandom_getUint32(&rnd) % frags;
                if (map[r] != 0)
                {
                    i -= 1;
                    continue;
                }
                map[r] = 1;

                uint32_t y = r / width;
                uint32_t x = r % width;
                float xx   = (float(x) + float(x + 1)) / (2.0f * float(width));
                float yy   = (float(y) + float(y + 1)) / (2.0f * float(height));
                float xxx  = xx * 2.0f - 1.0f;
                float yyy  = yy * 2.0f - 1.0f;
                points.emplace_back(tcu::Vec4(xxx, yyy, 0u, 0u));
            }
            return points;
        }
        static std::vector<uint32_t> generateOutputPvector(uint32_t subgroupSize, uint32_t vertexCount)
        {
            const uint32_t subgroupCount = ROUNDUP(vertexCount, subgroupSize) / subgroupSize;
            std::vector<uint32_t> outputP(vertexCount + INVOCATION_ENTRIES_OFFSET);
            outputP.at(NUM_SUBGROUPS_OFFSET)    = subgroupCount;
            outputP.at(SUBGROUP_SIZE_OFFSET)    = subgroupSize;
            outputP.at(INVOCATION_COUNT_OFFSET) = vertexCount;
            for (uint32_t vertexID = 0u; vertexID < vertexCount; ++vertexID)
            {
                const uint32_t subgroupID                        = vertexID / subgroupSize;
                const uint32_t subgroupInvocationID              = vertexID % subgroupSize;
                outputP.at(vertexID + INVOCATION_ENTRIES_OFFSET) = ((subgroupID + 1u) << 16) | subgroupInvocationID;
            }
            return outputP;
        }
    };

    virtual uint32_t simulate(bool countOnly, uint32_t subgroupSize, add_ref<std::vector<uint64_t>> ref) override
    {
        DE_ASSERT(false); // use overloaded version of simulate() instead
        DE_UNREF(countOnly);
        DE_UNREF(subgroupSize);
        DE_UNREF(ref);
        return 0;
    }

protected:
    virtual void genIf(IFType ifType, uint32_t /*maxLocalIndexCmp*/) override
    {
        RandomProgram::genIf(ifType, RandomProgram::invocationStride);
    }

    virtual std::string getPartitionBallotText() override
    {
        return "storeValue(outLoc++, subgroupBallot(true))";
    }

    virtual void printIfLocalInvocationIndex(add_ref<std::stringstream> css, add_cref<FlowState> flow) override
    {
        printIndent(css);
        css << "if (invocationIndex() >= inputA.a[0x" << std::hex << flow.ops[flow.opsIndex].value << "]) {\n";
    }

    virtual void printStore(add_ref<std::stringstream> css, add_cref<FlowState> flow) override
    {
        printIndent(css);
        css << "storeValue(outLoc++, 0x" << std::hex << flow.ops[flow.opsIndex].value << std::dec << ");\n";
    }

    virtual void printBallot(add_ref<std::stringstream> css, add_cref<FlowState>,
                             bool endWithSemicolon = false) override
    {
        printIndent(css);
        // When inside loop(s), use partitionBallot rather than subgroupBallot to compute
        // a ballot, to make sure the ballot is "diverged enough". Don't do this for
        // subgroup_uniform_control_flow, since we only validate results that must be fully
        // reconverged.
        if (loopNesting > 0 && caseDef.testType == TT_MAXIMAL)
        {
            css << getPartitionBallotText();
        }
        else
        {
            css << "storeValue(outLoc++, subgroupBallot(true))";
        }
        if (endWithSemicolon)
        {
            css << ";\n";
        }
    }

    virtual void simulateBallot(const bool countOnly, add_cref<Ballots> activeMask, const uint32_t unusedPrimitiveID,
                                const int32_t opsIndex, add_ref<std::vector<uint32_t>> outLoc,
                                add_ref<std::vector<tcu::UVec4>> ref, add_ref<tcu::TestLog> log,
                                std::shared_ptr<Prerequisites> prerequisites, add_ref<uint32_t> logFailureCount,
                                const OPType reason, const tcu::UVec4 *cmp) override
    {
        DE_UNREF(unusedPrimitiveID);
        DE_UNREF(opsIndex);
        add_cref<Arrangement> a(*std::static_pointer_cast<Arrangement>(prerequisites));
        for (uint32_t primitiveID = 0u; primitiveID < a.m_primitiveStride; ++primitiveID)
        {
            const uint32_t sgid = a.m_primitiveSubgroups.at(primitiveID);
            DE_ASSERT(sgid < (a.m_subgroupCount * a.m_subgroupSize));
            if (false == activeMask.test(Ballots::findBit(sgid, a.m_subgroupSize)))
                continue;
            const uint32_t index = (outLoc.at(primitiveID)++) * a.m_invocationStride + primitiveID;
            if (false == countOnly)
            {
                ref.at(index) = Ballot(activeMask.at(sgid / a.m_subgroupSize));
                if (cmp && logFailureCount > 0u && cmp[index] != ref.at(index))
                {
                    logFailureCount -= 1u;
                    log << tcu::TestLog::Message << logFailureCount << ": stored value mismatch from "
                        << OPtypeToStr(reason) << tcu::TestLog::EndMessage;
                }
            }
        }
    }

    virtual void simulateStore(const bool countOnly, add_cref<Ballots> activeMask, const uint32_t unusedPrimitiveID,
                               const uint64_t storeValue, add_ref<std::vector<uint32_t>> outLoc,
                               add_ref<std::vector<tcu::UVec4>> ref, add_ref<tcu::TestLog> log,
                               std::shared_ptr<Prerequisites> prerequisites, add_ref<uint32_t> logFailureCount,
                               const OPType reason, const tcu::UVec4 *cmp) override
    {
        DE_UNREF(unusedPrimitiveID);
        add_cref<Arrangement> a(*std::static_pointer_cast<Arrangement>(prerequisites));
        for (uint32_t primitiveID = 0u; primitiveID < a.m_primitiveStride; ++primitiveID)
        {
            const uint32_t sgid = a.m_primitiveSubgroups.at(primitiveID);
            DE_ASSERT(sgid < (a.m_subgroupCount * a.m_subgroupSize));
            if (false == activeMask.test(Ballots::findBit(sgid, a.m_subgroupSize)))
                continue;
            const uint32_t index = (outLoc.at(primitiveID)++) * a.m_invocationStride + primitiveID;
            if (false == countOnly)
            {
                ref.at(index) = Ballot(tcu::UVec4(uint32_t(storeValue & 0xFFFFFFFF), 0u, 0u, 0u));
                if (cmp && logFailureCount > 0u && cmp[index] != ref.at(index))
                {
                    logFailureCount -= 1u;
                    log << tcu::TestLog::Message << logFailureCount << ": stored value mismatch from "
                        << OPtypeToStr(reason) << tcu::TestLog::EndMessage;
                }
            }
        }
    }

    virtual std::shared_ptr<Prerequisites> makePrerequisites(add_cref<std::vector<uint32_t>> outputP,
                                                             const uint32_t subgroupSize, const uint32_t fragmentStride,
                                                             const uint32_t primitiveStride,
                                                             add_ref<std::vector<SubgroupState2>> stateStack,
                                                             add_ref<std::vector<uint32_t>> outLoc,
                                                             add_ref<uint32_t> subgroupCount) override
    {
        DE_UNREF(fragmentStride);
        auto prerequisites = std::make_shared<Arrangement>(outputP, subgroupSize, primitiveStride);
        subgroupCount      = prerequisites->m_subgroupCount;
        stateStack.resize(10u, SubgroupState2(subgroupCount));
        outLoc.resize(primitiveStride, 0u);
        stateStack.at(0).activeMask = prerequisites->m_initialBallots;
        return prerequisites;
    }
};

class TessCtrlRandomProgram : public RandomProgram
{
public:
    TessCtrlRandomProgram(add_cref<CaseDef> c, uint32_t invocationCount) : RandomProgram(c, invocationCount)
    {
        DE_ASSERT(c.shaderStage == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
    }
    virtual ~TessCtrlRandomProgram() = default;

    static const uint32_t minSubgroupSize = 4;

    virtual void genIf(IFType ifType, uint32_t /*maxLocalIndexCmp*/) override
    {
        RandomProgram::genIf(ifType, std::min((minSubgroupSize * caseDef.sizeX), 64u));
    }

    virtual void printIfLocalInvocationIndex(add_ref<std::stringstream> css, add_cref<FlowState> flow) override
    {
        printIndent(css);
        css << "if (";
        css << "((((gl_PrimitiveID * width) / gl_SubgroupSize) * gl_SubgroupSize) + gl_SubgroupInvocationID)";
        css << " >= inputA.a[0x" << std::hex << flow.ops[flow.opsIndex].value << "]) {\n";
    }

    virtual void printStore(add_ref<std::stringstream> css, add_cref<FlowState> flow) override
    {
        printIndent(css);
        css << "outputC.loc[invocationIndex()]++;\n";
        printIndent(css);
        css << "outputB.b[(outLoc++) * invocationStride + invocationIndex()].x = 0x" << std::hex
            << flow.ops[flow.opsIndex].value << ";\n";
    }

    virtual void printBallot(add_ref<std::stringstream> css, add_cref<FlowState>,
                             bool endWithSemicolon = false) override
    {
        printIndent(css);

        css << "outputC.loc[invocationIndex()]++,";
        // When inside loop(s), use partitionBallot rather than subgroupBallot to compute
        // a ballot, to make sure the ballot is "diverged enough". Don't do this for
        // subgroup_uniform_control_flow, since we only validate results that must be fully
        // reconverged.
        if (loopNesting > 0 && caseDef.testType == TT_MAXIMAL)
        {
            css << "outputB.b[(outLoc++) * invocationStride + invocationIndex()] = " << getPartitionBallotText()
                << ".xy";
        }
        else
        {
            css << "outputB.b[(outLoc++) * invocationStride + invocationIndex()] = subgroupBallot(true).xy";
        }
        if (endWithSemicolon)
        {
            css << ";\n";
        }
    }

    void simulateStoreToChange(bool countOnly, uint32_t /*subgroupSize*/, const SubgroupState (&stateStack)[10],
                               int32_t opsIndex, add_ref<std::vector<uint32_t>> outLoc,
                               add_ref<std::vector<uint64_t>> ref)
    {
        for (uint32_t id = 0; id < invocationStride; ++id)
        {
            if (stateStack[nesting].activeMask.test(id))
            {
                if (countOnly)
                    outLoc[id]++;
                else
                    ref[(outLoc[id]++) * invocationStride + id] = ops[opsIndex].value;
            }
        }
    }

    void simulateBallotToChange(bool countOnly, uint32_t subgroupSize, const SubgroupState (&stateStack)[10],
                                uint32_t /*opsIndex*/, add_ref<std::vector<uint32_t>> outLoc,
                                add_ref<std::vector<uint64_t>> ref)
    {
        for (uint32_t id = 0; id < invocationStride; ++id)
        {
            if (stateStack[nesting].activeMask.test(id))
            {
                if (countOnly)
                    outLoc[id]++;
                else
                    ref[(outLoc[id]++) * invocationStride + id] =
                        bitsetToU64(stateStack[nesting].activeMask, subgroupSize, id);
            }
        }
    }

    // Simulate execution of the program. If countOnly is true, just return
    // the max number of outputs written. If it's false, store out the result
    // values to ref.
    virtual uint32_t simulate(bool countOnly, uint32_t subgroupSize, add_ref<std::vector<uint64_t>> ref) override
    {
        SubgroupState stateStack[10];
        deMemset(&stateStack, 0, sizeof(stateStack));

        // Per-invocation output location counters
        std::vector<uint32_t> outLoc(invocationStride, 0u);

        nesting     = 0;
        loopNesting = 0;

        for (uint32_t k = 0; k < invocationStride; ++k)
            stateStack[nesting].activeMask.set(k);

        int32_t i = 0;
        while (i < (int32_t)ops.size())
        {
            switch (ops[i].type)
            {
            case OP_BALLOT:
                simulateBallotToChange(countOnly, subgroupSize, stateStack, i, outLoc, ref);
                break;
            case OP_STORE:
                simulateStoreToChange(countOnly, subgroupSize, stateStack, i, outLoc, ref);
                break;
            case OP_IF_MASK:
                nesting++;
                stateStack[nesting].activeMask =
                    stateStack[nesting - 1].activeMask & bitsetFromU64(ops[i].value, subgroupSize);
                stateStack[nesting].header   = i;
                stateStack[nesting].isLoop   = 0;
                stateStack[nesting].isSwitch = 0;
                break;
            case OP_ELSE_MASK:
                stateStack[nesting].activeMask = stateStack[nesting - 1].activeMask &
                                                 ~bitsetFromU64(ops[stateStack[nesting].header].value, subgroupSize);
                break;
            case OP_IF_LOOPCOUNT:
            {
                uint32_t n = nesting;
                while (!stateStack[n].isLoop)
                    n--;

                nesting++;
                stateStack[nesting].activeMask =
                    stateStack[nesting - 1].activeMask & bitsetFromU64((1ULL << stateStack[n].tripCount), subgroupSize);
                stateStack[nesting].header   = i;
                stateStack[nesting].isLoop   = 0;
                stateStack[nesting].isSwitch = 0;
                break;
            }
            case OP_ELSE_LOOPCOUNT:
            {
                uint32_t n = nesting;
                while (!stateStack[n].isLoop)
                    n--;

                stateStack[nesting].activeMask = stateStack[nesting - 1].activeMask &
                                                 ~bitsetFromU64((1ULL << stateStack[n].tripCount), subgroupSize);
                break;
            }
            case OP_IF_LOCAL_INVOCATION_INDEX: // TessCtrlRandomProgram
            {
                // all bits >= N
                bitset_inv_t mask;
                for (uint32_t j = static_cast<uint32_t>(ops[i].value); j < invocationStride; ++j)
                    mask.set(j);

                nesting++;
                stateStack[nesting].activeMask = stateStack[nesting - 1].activeMask & mask;
                stateStack[nesting].header     = i;
                stateStack[nesting].isLoop     = 0;
                stateStack[nesting].isSwitch   = 0;
                break;
            }
            case OP_ELSE_LOCAL_INVOCATION_INDEX: // TessCtrlRandomProgram
            {
                // all bits < N
                bitset_inv_t mask;
                for (uint32_t j = 0; j < static_cast<uint32_t>(ops[i].value); ++j)
                    mask.set(j);

                stateStack[nesting].activeMask = stateStack[nesting - 1].activeMask & mask;
                break;
            }
            case OP_ENDIF:
                nesting--;
                break;
            case OP_BEGIN_FOR_UNIF:
                // XXX TODO: We don't handle a for loop with zero iterations
                nesting++;
                loopNesting++;
                stateStack[nesting].activeMask   = stateStack[nesting - 1].activeMask;
                stateStack[nesting].header       = i;
                stateStack[nesting].tripCount    = 0;
                stateStack[nesting].isLoop       = 1;
                stateStack[nesting].isSwitch     = 0;
                stateStack[nesting].continueMask = 0;
                break;
            case OP_END_FOR_UNIF:
                stateStack[nesting].tripCount++;
                stateStack[nesting].activeMask |= stateStack[nesting].continueMask;
                stateStack[nesting].continueMask = 0;
                if (stateStack[nesting].tripCount < ops[stateStack[nesting].header].value &&
                    stateStack[nesting].activeMask.any())
                {
                    i = stateStack[nesting].header + 1;
                    continue;
                }
                else
                {
                    loopNesting--;
                    nesting--;
                }
                break;
            case OP_BEGIN_DO_WHILE_UNIF:
                // XXX TODO: We don't handle a for loop with zero iterations
                nesting++;
                loopNesting++;
                stateStack[nesting].activeMask   = stateStack[nesting - 1].activeMask;
                stateStack[nesting].header       = i;
                stateStack[nesting].tripCount    = 1;
                stateStack[nesting].isLoop       = 1;
                stateStack[nesting].isSwitch     = 0;
                stateStack[nesting].continueMask = 0;
                break;
            case OP_END_DO_WHILE_UNIF:
                stateStack[nesting].activeMask |= stateStack[nesting].continueMask;
                stateStack[nesting].continueMask = 0;
                if (stateStack[nesting].tripCount < ops[stateStack[nesting].header].value &&
                    stateStack[nesting].activeMask.any())
                {
                    i = stateStack[nesting].header + 1;
                    stateStack[nesting].tripCount++;
                    continue;
                }
                else
                {
                    loopNesting--;
                    nesting--;
                }
                break;
            case OP_BEGIN_FOR_VAR:
                // XXX TODO: We don't handle a for loop with zero iterations
                nesting++;
                loopNesting++;
                stateStack[nesting].activeMask   = stateStack[nesting - 1].activeMask;
                stateStack[nesting].header       = i;
                stateStack[nesting].tripCount    = 0;
                stateStack[nesting].isLoop       = 1;
                stateStack[nesting].isSwitch     = 0;
                stateStack[nesting].continueMask = 0;
                break;
            case OP_END_FOR_VAR:
                stateStack[nesting].tripCount++;
                stateStack[nesting].activeMask |= stateStack[nesting].continueMask;
                stateStack[nesting].continueMask = 0;
                stateStack[nesting].activeMask &= bitsetFromU64(stateStack[nesting].tripCount == subgroupSize ?
                                                                    0 :
                                                                    ~((1ULL << (stateStack[nesting].tripCount)) - 1),
                                                                subgroupSize);
                if (stateStack[nesting].activeMask.any())
                {
                    i = stateStack[nesting].header + 1;
                    continue;
                }
                else
                {
                    loopNesting--;
                    nesting--;
                }
                break;
            case OP_BEGIN_FOR_INF:
            case OP_BEGIN_DO_WHILE_INF:
                nesting++;
                loopNesting++;
                stateStack[nesting].activeMask   = stateStack[nesting - 1].activeMask;
                stateStack[nesting].header       = i;
                stateStack[nesting].tripCount    = 0;
                stateStack[nesting].isLoop       = 1;
                stateStack[nesting].isSwitch     = 0;
                stateStack[nesting].continueMask = 0;
                break;
            case OP_END_FOR_INF:
                stateStack[nesting].tripCount++;
                stateStack[nesting].activeMask |= stateStack[nesting].continueMask;
                stateStack[nesting].continueMask = 0;
                if (stateStack[nesting].activeMask.any())
                {
                    // output expected OP_BALLOT values
                    simulateBallotToChange(countOnly, subgroupSize, stateStack, i, outLoc, ref);

                    i = stateStack[nesting].header + 1;
                    continue;
                }
                else
                {
                    loopNesting--;
                    nesting--;
                }
                break;
            case OP_END_DO_WHILE_INF:
                stateStack[nesting].tripCount++;
                stateStack[nesting].activeMask |= stateStack[nesting].continueMask;
                stateStack[nesting].continueMask = 0;
                if (stateStack[nesting].activeMask.any())
                {
                    i = stateStack[nesting].header + 1;
                    continue;
                }
                else
                {
                    loopNesting--;
                    nesting--;
                }
                break;
            case OP_BREAK:
            {
                uint32_t n        = nesting;
                bitset_inv_t mask = stateStack[nesting].activeMask;
                while (true)
                {
                    stateStack[n].activeMask &= ~mask;
                    if (stateStack[n].isLoop || stateStack[n].isSwitch)
                        break;

                    n--;
                }
            }
            break;
            case OP_CONTINUE:
            {
                uint32_t n        = nesting;
                bitset_inv_t mask = stateStack[nesting].activeMask;
                while (true)
                {
                    stateStack[n].activeMask &= ~mask;
                    if (stateStack[n].isLoop)
                    {
                        stateStack[n].continueMask |= mask;
                        break;
                    }
                    n--;
                }
            }
            break;
            case OP_ELECT:
            {
                nesting++;
                stateStack[nesting].activeMask = bitsetElect(stateStack[nesting - 1].activeMask, subgroupSize);
                stateStack[nesting].header     = i;
                stateStack[nesting].isLoop     = 0;
                stateStack[nesting].isSwitch   = 0;
            }
            break;
            case OP_RETURN:
            {
                bitset_inv_t mask = stateStack[nesting].activeMask;
                for (int32_t n = nesting; n >= 0; --n)
                {
                    stateStack[n].activeMask &= ~mask;
                    if (stateStack[n].isCall)
                        break;
                }
            }
            break;

            case OP_CALL_BEGIN:
                nesting++;
                stateStack[nesting].activeMask = stateStack[nesting - 1].activeMask;
                stateStack[nesting].isLoop     = 0;
                stateStack[nesting].isSwitch   = 0;
                stateStack[nesting].isCall     = 1;
                break;
            case OP_CALL_END:
                stateStack[nesting].isCall = 0;
                nesting--;
                break;
            case OP_NOISE:
                break;

            case OP_SWITCH_UNIF_BEGIN:
            case OP_SWITCH_VAR_BEGIN:
            case OP_SWITCH_LOOP_COUNT_BEGIN:
                nesting++;
                stateStack[nesting].activeMask = stateStack[nesting - 1].activeMask;
                stateStack[nesting].header     = i;
                stateStack[nesting].isLoop     = 0;
                stateStack[nesting].isSwitch   = 1;
                break;
            case OP_SWITCH_END:
                nesting--;
                break;
            case OP_CASE_MASK_BEGIN:
                stateStack[nesting].activeMask =
                    stateStack[nesting - 1].activeMask & bitsetFromU64(ops[i].value, subgroupSize);
                break;
            case OP_CASE_LOOP_COUNT_BEGIN:
            {
                uint32_t n = nesting;
                uint32_t l = loopNesting;

                while (true)
                {
                    if (stateStack[n].isLoop)
                    {
                        l--;
                        if (l == ops[stateStack[nesting].header].value)
                            break;
                    }
                    n--;
                }

                if ((1ULL << stateStack[n].tripCount) & ops[i].value)
                    stateStack[nesting].activeMask = stateStack[nesting - 1].activeMask;
                else
                    stateStack[nesting].activeMask = 0;
                break;
            }
            case OP_CASE_END:
                break;

            default:
                DE_ASSERT(0);
                break;
            }
            i++;
        }
        uint32_t maxLoc = 0;
        for (uint32_t id = 0; id < (uint32_t)outLoc.size(); ++id)
            maxLoc = de::max(maxLoc, outLoc[id]);

        return maxLoc;
    }
};

class TessEvalRandomProgram : public RandomProgram
{
public:
    TessEvalRandomProgram(add_cref<CaseDef> c, uint32_t invocationCount = 0)
        : RandomProgram(c, (invocationCount ? invocationCount : 64))
        , ifLocalInvocationIndexAsSubgroupInvocationID(false)
    {
        DE_ASSERT(c.shaderStage == VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
    }
    virtual ~TessEvalRandomProgram() = default;

    const bool ifLocalInvocationIndexAsSubgroupInvocationID;
    static const uint32_t quadInvocationCount = 4;

    // Simulate execution of the program. If countOnly is true, just return
    // the max number of outputs written. If it's false, store out the result
    // values to ref.
    virtual uint32_t simulate(bool countOnly, uint32_t subgroupSize, add_ref<std::vector<uint64_t>> ref) override
    {
        SubgroupState stateStack[10];
        deMemset(&stateStack, 0, sizeof(stateStack));

        // Per-invocation output location counters
        std::vector<uint32_t> outLoc(invocationStride, 0u);

        nesting     = 0;
        loopNesting = 0;

        for (uint32_t k = 0; k < invocationStride; ++k)
            stateStack[nesting].activeMask.set(k);

        int32_t i = 0;
        while (i < (int32_t)ops.size())
        {
            switch (ops[i].type)
            {
            case OP_BALLOT:
                simulateBallotToChange(countOnly, subgroupSize, stateStack, i, outLoc, ref);
                break;
            case OP_STORE:
                simulateStoreToChange(countOnly, subgroupSize, stateStack, i, outLoc, ref);
                break;
            case OP_IF_MASK:
                nesting++;
                stateStack[nesting].activeMask =
                    stateStack[nesting - 1].activeMask & bitsetFromU64(ops[i].value, subgroupSize);
                stateStack[nesting].header   = i;
                stateStack[nesting].isLoop   = 0;
                stateStack[nesting].isSwitch = 0;
                break;
            case OP_ELSE_MASK:
                stateStack[nesting].activeMask = stateStack[nesting - 1].activeMask &
                                                 ~bitsetFromU64(ops[stateStack[nesting].header].value, subgroupSize);
                break;
            case OP_IF_LOOPCOUNT:
            {
                uint32_t n = nesting;
                while (!stateStack[n].isLoop)
                    n--;

                nesting++;
                stateStack[nesting].activeMask =
                    stateStack[nesting - 1].activeMask & bitsetFromU64((1ULL << stateStack[n].tripCount), subgroupSize);
                stateStack[nesting].header   = i;
                stateStack[nesting].isLoop   = 0;
                stateStack[nesting].isSwitch = 0;
                break;
            }
            case OP_ELSE_LOOPCOUNT:
            {
                uint32_t n = nesting;
                while (!stateStack[n].isLoop)
                    n--;

                stateStack[nesting].activeMask = stateStack[nesting - 1].activeMask &
                                                 ~bitsetFromU64((1ULL << stateStack[n].tripCount), subgroupSize);
                break;
            }
            case OP_IF_LOCAL_INVOCATION_INDEX: // TessEvalRandomProgram
            {
                bitset_inv_t mask;
                if (ifLocalInvocationIndexAsSubgroupInvocationID)
                {
                    // if (gl_SubgroupInvocationID >= value), all bits >= N
                    for (uint32_t j = static_cast<uint32_t>(ops[i].value); j < subgroupSize; ++j)
                        mask.set(j);
                    mask = bitsetFromU64(mask.to_ullong(), subgroupSize);
                }
                else
                {
                    // all bits >= N
                    for (uint32_t j = (uint32_t)ops[i].value; j < invocationStride; ++j)
                        mask.set(j);
                }

                nesting++;
                stateStack[nesting].activeMask = stateStack[nesting - 1].activeMask & mask;
                stateStack[nesting].header     = i;
                stateStack[nesting].isLoop     = 0;
                stateStack[nesting].isSwitch   = 0;
                break;
            }
            case OP_ELSE_LOCAL_INVOCATION_INDEX: // TessEvalRandomProgram
            {
                // all bits < N
                bitset_inv_t mask;
                for (uint32_t j = 0; j < static_cast<uint32_t>(ops[i].value); ++j)
                    mask.set(j);

                if (ifLocalInvocationIndexAsSubgroupInvocationID)
                {
                    // else (gl_SubgroupInvocationID >= value), all bits < N
                    mask = bitsetFromU64(mask.to_ullong(), subgroupSize);
                }

                stateStack[nesting].activeMask = stateStack[nesting - 1].activeMask & mask;
                break;
            }
            case OP_ENDIF:
                nesting--;
                break;
            case OP_BEGIN_FOR_UNIF:
                // XXX TODO: We don't handle a for loop with zero iterations
                nesting++;
                loopNesting++;
                stateStack[nesting].activeMask   = stateStack[nesting - 1].activeMask;
                stateStack[nesting].header       = i;
                stateStack[nesting].tripCount    = 0;
                stateStack[nesting].isLoop       = 1;
                stateStack[nesting].isSwitch     = 0;
                stateStack[nesting].continueMask = 0;
                break;
            case OP_END_FOR_UNIF:
                stateStack[nesting].tripCount++;
                stateStack[nesting].activeMask |= stateStack[nesting].continueMask;
                stateStack[nesting].continueMask = 0;
                if (stateStack[nesting].tripCount < ops[stateStack[nesting].header].value &&
                    stateStack[nesting].activeMask.any())
                {
                    i = stateStack[nesting].header + 1;
                    continue;
                }
                else
                {
                    loopNesting--;
                    nesting--;
                }
                break;
            case OP_BEGIN_DO_WHILE_UNIF:
                // XXX TODO: We don't handle a for loop with zero iterations
                nesting++;
                loopNesting++;
                stateStack[nesting].activeMask   = stateStack[nesting - 1].activeMask;
                stateStack[nesting].header       = i;
                stateStack[nesting].tripCount    = 1;
                stateStack[nesting].isLoop       = 1;
                stateStack[nesting].isSwitch     = 0;
                stateStack[nesting].continueMask = 0;
                break;
            case OP_END_DO_WHILE_UNIF:
                stateStack[nesting].activeMask |= stateStack[nesting].continueMask;
                stateStack[nesting].continueMask = 0;
                if (stateStack[nesting].tripCount < ops[stateStack[nesting].header].value &&
                    stateStack[nesting].activeMask.any())
                {
                    i = stateStack[nesting].header + 1;
                    stateStack[nesting].tripCount++;
                    continue;
                }
                else
                {
                    loopNesting--;
                    nesting--;
                }
                break;
            case OP_BEGIN_FOR_VAR:
                // XXX TODO: We don't handle a for loop with zero iterations
                nesting++;
                loopNesting++;
                stateStack[nesting].activeMask   = stateStack[nesting - 1].activeMask;
                stateStack[nesting].header       = i;
                stateStack[nesting].tripCount    = 0;
                stateStack[nesting].isLoop       = 1;
                stateStack[nesting].isSwitch     = 0;
                stateStack[nesting].continueMask = 0;
                break;
            case OP_END_FOR_VAR:
                stateStack[nesting].tripCount++;
                stateStack[nesting].activeMask |= stateStack[nesting].continueMask;
                stateStack[nesting].continueMask = 0;
                stateStack[nesting].activeMask &= bitsetFromU64(stateStack[nesting].tripCount == subgroupSize ?
                                                                    0 :
                                                                    ~((1ULL << (stateStack[nesting].tripCount)) - 1),
                                                                subgroupSize);
                if (stateStack[nesting].activeMask.any())
                {
                    i = stateStack[nesting].header + 1;
                    continue;
                }
                else
                {
                    loopNesting--;
                    nesting--;
                }
                break;
            case OP_BEGIN_FOR_INF:
            case OP_BEGIN_DO_WHILE_INF:
                nesting++;
                loopNesting++;
                stateStack[nesting].activeMask   = stateStack[nesting - 1].activeMask;
                stateStack[nesting].header       = i;
                stateStack[nesting].tripCount    = 0;
                stateStack[nesting].isLoop       = 1;
                stateStack[nesting].isSwitch     = 0;
                stateStack[nesting].continueMask = 0;
                break;
            case OP_END_FOR_INF:
                stateStack[nesting].tripCount++;
                stateStack[nesting].activeMask |= stateStack[nesting].continueMask;
                stateStack[nesting].continueMask = 0;
                if (stateStack[nesting].activeMask.any())
                {
                    // output expected OP_BALLOT values
                    simulateBallotToChange(countOnly, subgroupSize, stateStack, i, outLoc, ref);

                    i = stateStack[nesting].header + 1;
                    continue;
                }
                else
                {
                    loopNesting--;
                    nesting--;
                }
                break;
            case OP_END_DO_WHILE_INF:
                stateStack[nesting].tripCount++;
                stateStack[nesting].activeMask |= stateStack[nesting].continueMask;
                stateStack[nesting].continueMask = 0;
                if (stateStack[nesting].activeMask.any())
                {
                    i = stateStack[nesting].header + 1;
                    continue;
                }
                else
                {
                    loopNesting--;
                    nesting--;
                }
                break;
            case OP_BREAK:
            {
                uint32_t n        = nesting;
                bitset_inv_t mask = stateStack[nesting].activeMask;
                while (true)
                {
                    stateStack[n].activeMask &= ~mask;
                    if (stateStack[n].isLoop || stateStack[n].isSwitch)
                        break;

                    n--;
                }
            }
            break;
            case OP_CONTINUE:
            {
                uint32_t n        = nesting;
                bitset_inv_t mask = stateStack[nesting].activeMask;
                while (true)
                {
                    stateStack[n].activeMask &= ~mask;
                    if (stateStack[n].isLoop)
                    {
                        stateStack[n].continueMask |= mask;
                        break;
                    }
                    n--;
                }
            }
            break;
            case OP_ELECT:
            {
                nesting++;
                stateStack[nesting].activeMask = bitsetElect(stateStack[nesting - 1].activeMask, subgroupSize);
                stateStack[nesting].header     = i;
                stateStack[nesting].isLoop     = 0;
                stateStack[nesting].isSwitch   = 0;
            }
            break;
            case OP_RETURN:
            {
                bitset_inv_t mask = stateStack[nesting].activeMask;
                for (int32_t n = nesting; n >= 0; --n)
                {
                    stateStack[n].activeMask &= ~mask;
                    if (stateStack[n].isCall)
                        break;
                }
            }
            break;

            case OP_CALL_BEGIN:
                nesting++;
                stateStack[nesting].activeMask = stateStack[nesting - 1].activeMask;
                stateStack[nesting].isLoop     = 0;
                stateStack[nesting].isSwitch   = 0;
                stateStack[nesting].isCall     = 1;
                break;
            case OP_CALL_END:
                stateStack[nesting].isCall = 0;
                nesting--;
                break;
            case OP_NOISE:
                break;

            case OP_SWITCH_UNIF_BEGIN:
            case OP_SWITCH_VAR_BEGIN:
            case OP_SWITCH_LOOP_COUNT_BEGIN:
                nesting++;
                stateStack[nesting].activeMask = stateStack[nesting - 1].activeMask;
                stateStack[nesting].header     = i;
                stateStack[nesting].isLoop     = 0;
                stateStack[nesting].isSwitch   = 1;
                break;
            case OP_SWITCH_END:
                nesting--;
                break;
            case OP_CASE_MASK_BEGIN:
                stateStack[nesting].activeMask =
                    stateStack[nesting - 1].activeMask & bitsetFromU64(ops[i].value, subgroupSize);
                break;
            case OP_CASE_LOOP_COUNT_BEGIN:
            {
                uint32_t n = nesting;
                uint32_t l = loopNesting;

                while (true)
                {
                    if (stateStack[n].isLoop)
                    {
                        l--;
                        if (l == ops[stateStack[nesting].header].value)
                            break;
                    }
                    n--;
                }

                if ((1ULL << stateStack[n].tripCount) & ops[i].value)
                    stateStack[nesting].activeMask = stateStack[nesting - 1].activeMask;
                else
                    stateStack[nesting].activeMask = 0;
                break;
            }
            case OP_CASE_END:
                break;

            default:
                DE_ASSERT(0);
                break;
            }
            i++;
        }
        uint32_t maxLoc = 0;
        for (uint32_t id = 0; id < (uint32_t)outLoc.size(); ++id)
            maxLoc = de::max(maxLoc, outLoc[id]);

        return maxLoc;
    }

protected:
    virtual void genIf(IFType ifType, uint32_t /*maxLocalIndexCmp*/) override
    {
        RandomProgram::genIf(ifType, std::min(64u, (caseDef.sizeX * quadInvocationCount - 1)));
    }

    virtual void printIfLocalInvocationIndex(add_ref<std::stringstream> css, add_cref<FlowState> flow) override
    {
        // uint invocationIndex() { return gl_PrimitiveID * width + gl_SubgroupInvocationID; }
        printIndent(css);
        css << "if (";
        if (ifLocalInvocationIndexAsSubgroupInvocationID)
            css << "gl_SubgroupInvocationID";
        else
            css << "((((gl_PrimitiveID * width) / gl_SubgroupSize) * gl_SubgroupSize) + gl_SubgroupInvocationID)";
        css << " >= inputA.a[0x" << std::hex << flow.ops[flow.opsIndex].value << "]) {\n";
    }

    virtual void printStore(add_ref<std::stringstream> css, add_cref<FlowState> flow) override
    {
        printIndent(css);
        css << "outputC.loc[invocationIndex()]++;\n";
        printIndent(css);
        css << "outputB.b[(outLoc++)*invocationStride + invocationIndex()].x = 0x" << std::hex
            << flow.ops[flow.opsIndex].value << ";\n";
    }

    virtual void printBallot(add_ref<std::stringstream> css, add_cref<FlowState>,
                             bool endWithSemicolon = false) override
    {
        printIndent(css);

        css << "outputC.loc[invocationIndex()]++,";
        // When inside loop(s), use partitionBallot rather than subgroupBallot to compute
        // a ballot, to make sure the ballot is "diverged enough". Don't do this for
        // subgroup_uniform_control_flow, since we only validate results that must be fully
        // reconverged.
        if (loopNesting > 0 && caseDef.testType == TT_MAXIMAL)
        {
            css << "outputB.b[(outLoc++)*invocationStride + invocationIndex()] = " << getPartitionBallotText() << ".xy";
        }
        else
        {
            css << "outputB.b[(outLoc++)*invocationStride + invocationIndex()] = subgroupBallot(true).xy";
        }
        if (endWithSemicolon)
        {
            css << ";\n";
        }
    }

    void simulateStoreToChange(bool countOnly, uint32_t /*subgroupSize*/, const SubgroupState (&stateStack)[10],
                               int32_t opsIndex, add_ref<std::vector<uint32_t>> outLoc,
                               add_ref<std::vector<uint64_t>> ref)
    {
        for (uint32_t id = 0; id < invocationStride; ++id)
        {
            if (stateStack[nesting].activeMask.test(id))
            {
                if (countOnly)
                    outLoc[id]++;
                else
                    ref[(outLoc[id]++) * invocationStride + id] = ops[opsIndex].value;
            }
        }
    }

    void simulateBallotToChange(bool countOnly, uint32_t subgroupSize, const SubgroupState (&stateStack)[10],
                                uint32_t /*opsIndex*/, add_ref<std::vector<uint32_t>> outLoc,
                                add_ref<std::vector<uint64_t>> ref)
    {
        for (uint32_t id = 0; id < invocationStride; ++id)
        {
            if (stateStack[nesting].activeMask.test(id))
            {
                if (countOnly)
                    outLoc[id]++;
                else
                    ref[(outLoc[id]++) * invocationStride + id] =
                        bitsetToU64(stateStack[nesting].activeMask, subgroupSize, id);
            }
        }
    }
};

class GeometryRandomProgram : public RandomProgram
{
public:
    static const constexpr uint32_t fillPercentage = 71u;
    GeometryRandomProgram(add_cref<CaseDef> c)
        : RandomProgram(c, Arrangement::calculatePrimitiveCount(c.sizeX, c.sizeY, fillPercentage))
    {
        DE_ASSERT(c.shaderStage == VK_SHADER_STAGE_GEOMETRY_BIT);
    }
    virtual ~GeometryRandomProgram() = default;

    struct Arrangement : Prerequisites
    {
        static constexpr uint32_t NUM_SUBGROUPS_OFFSET    = 0u;
        static constexpr uint32_t SUBGROUP_SIZE_OFFSET    = 1u;
        static constexpr uint32_t INVOCATION_COUNT_OFFSET = 2u;
        static constexpr uint32_t MAX_LOC_OFFSET          = 3u;
        static constexpr uint32_t MAX_IDENTITY_OFFSET     = 4u;
        static constexpr uint32_t INVOCATION_ENTRY_OFFSET = 5u;

        const uint32_t m_shaderSubgroupSize;
        const uint32_t m_shaderSubgroupCount;
        const uint32_t m_shaderInvocationCount;
        const uint32_t m_shaderMaxLoc;
        const uint32_t m_shaderMaxIdentity;

        const uint32_t m_subgroupSize;
        const uint32_t m_primitiveStride;
        const uint32_t m_invocationStride;
        const uint32_t m_subgroupCount;
        const Ballots m_initialBallots;
        const std::vector<uint32_t> m_primitiveSubgroups;

        Arrangement(add_cref<std::vector<uint32_t>> outputP, uint32_t subgroupSize, uint32_t primitiveStride)
            : m_shaderSubgroupSize(outputP.at(SUBGROUP_SIZE_OFFSET))
            , m_shaderSubgroupCount(outputP.at(NUM_SUBGROUPS_OFFSET))
            , m_shaderInvocationCount(outputP.at(INVOCATION_COUNT_OFFSET))
            , m_shaderMaxLoc(outputP.at(MAX_LOC_OFFSET))
            , m_shaderMaxIdentity(outputP.at(MAX_IDENTITY_OFFSET))
            , m_subgroupSize(subgroupSize)
            , m_primitiveStride(primitiveStride)
            , m_invocationStride(primitiveStride)
            , m_subgroupCount(ROUNDUP(primitiveStride, subgroupSize) / subgroupSize)
            , m_initialBallots(makeInitialBallots(outputP))
            , m_primitiveSubgroups(makePrimitiveSubgroups(outputP))
        {
        }
        static Ballots makeInitialBallots(add_cref<std::vector<uint32_t>> outputP)
        {
            const uint32_t subgroupCount = outputP.at(NUM_SUBGROUPS_OFFSET);
            const uint32_t subgroupSize  = outputP.at(SUBGROUP_SIZE_OFFSET);
            DE_UNREF(subgroupSize);
            const uint32_t primitiveStride = outputP.at(INVOCATION_COUNT_OFFSET);
            Ballots b(subgroupCount);
            for (uint32_t primitiveID = 0u; primitiveID < primitiveStride; ++primitiveID)
            {
                const uint32_t id = outputP.at(primitiveID + INVOCATION_ENTRY_OFFSET);
                if (id)
                {
                    const uint32_t subgroupID           = (id >> 16) - 1u;
                    const uint32_t subgroupInvocationID = id & 0xFFFF;
                    DE_ASSERT(subgroupID < subgroupCount);
                    DE_ASSERT(subgroupInvocationID < subgroupSize);
                    b.at(subgroupID).set(subgroupInvocationID);
                }
            }
            return b;
        }
        static std::vector<uint32_t> makePrimitiveSubgroups(add_cref<std::vector<uint32_t>> outputP)
        {
            const uint32_t subgroupSize    = outputP.at(SUBGROUP_SIZE_OFFSET);
            const uint32_t primitiveStride = outputP.at(INVOCATION_COUNT_OFFSET);
            std::vector<uint32_t> map(primitiveStride);
            for (uint32_t primitiveID = 0u; primitiveID < primitiveStride; ++primitiveID)
            {
                const uint32_t id = outputP.at(primitiveID + INVOCATION_ENTRY_OFFSET);
                if (id)
                {
                    const uint32_t subgroupID           = (id >> 16) - 1u;
                    const uint32_t subgroupInvocationID = id & 0xFFFF;
                    DE_ASSERT(subgroupInvocationID < subgroupSize);
                    map.at(primitiveID) = subgroupID * subgroupSize + subgroupInvocationID;
                }
            }
            return map;
        }
        static uint32_t calculatePrimitiveCount(uint32_t width, uint32_t height, uint32_t fillPercent)
        {
            deRandom rnd;
            std::map<uint32_t, int> map;
            std::vector<tcu::Vec4> points;
            const uint32_t frags = (width * height);
            const uint32_t total = (frags * fillPercent) / 100u;

            deRandom_init(&rnd, (width * height));

            for (uint32_t i = 0u; i < total; ++i)
            {
                const uint32_t r = deRandom_getUint32(&rnd) % frags;
                if (map[r] != 0)
                {
                    i -= 1;
                    continue;
                }
                map[r] = 1;
            }

            return static_cast<uint32_t>(map.size());
        }
        static std::vector<tcu::Vec4> generatePrimitives(uint32_t width, uint32_t height, uint32_t fillPercent)
        {
            deRandom rnd;
            std::map<uint32_t, int> map;
            std::vector<tcu::Vec4> points;
            const uint32_t frags = (width * height);
            const uint32_t total = (frags * fillPercent) / 100u;

            deRandom_init(&rnd, (width * height));

            for (uint32_t i = 0u; i < total; ++i)
            {
                const uint32_t r = deRandom_getUint32(&rnd) % frags;
                if (map[r] != 0)
                {
                    i -= 1;
                    continue;
                }
                map[r] = 1;

                uint32_t y = r / width;
                uint32_t x = r % width;
                float xx   = (float(x) + float(x + 1)) / (2.0f * float(width));
                float yy   = (float(y) + float(y + 1)) / (2.0f * float(height));
                float xxx  = xx * 2.0f - 1.0f;
                float yyy  = yy * 2.0f - 1.0f;
                points.emplace_back(tcu::Vec4(xxx, yyy, 0u, 0u));
            }
            return points;
        }
        static std::vector<uint32_t> generateVectorOutputP(uint32_t subgroupSize, uint32_t primitiveStride)
        {
            const uint32_t subgroupCount = ROUNDUP(primitiveStride, subgroupSize) / subgroupSize;
            std::vector<uint32_t> outputP(primitiveStride + INVOCATION_ENTRY_OFFSET);
            outputP.at(NUM_SUBGROUPS_OFFSET)    = subgroupCount;
            outputP.at(SUBGROUP_SIZE_OFFSET)    = subgroupSize;
            outputP.at(INVOCATION_COUNT_OFFSET) = primitiveStride;
            outputP.at(MAX_LOC_OFFSET)          = 0u;
            outputP.at(MAX_IDENTITY_OFFSET)     = 0u;
            for (uint32_t vertexID = 0u; vertexID < primitiveStride; ++vertexID)
            {
                const uint32_t subgroupID                      = vertexID / subgroupSize;
                const uint32_t subgroupInvocationID            = vertexID % subgroupSize;
                outputP.at(vertexID + INVOCATION_ENTRY_OFFSET) = ((subgroupID + 1u) << 16) | subgroupInvocationID;
            }
            return outputP;
        }
        static std::vector<uint32_t> generateVectorOutputP(uint32_t subgroupSize, uint32_t width, uint32_t height,
                                                           uint32_t percent)
        {
            const uint32_t primitiveStride = calculatePrimitiveCount(width, height, percent);
            return generateVectorOutputP(subgroupSize, primitiveStride);
        }
    };

    virtual uint32_t simulate(bool countOnly, uint32_t subgroupSize, add_ref<std::vector<uint64_t>> ref) override
    {
        DE_ASSERT(false); // use overloaded version of simulate() instead
        DE_UNREF(countOnly);
        DE_UNREF(subgroupSize);
        DE_UNREF(ref);
        return 0;
    }

protected:
    virtual void genIf(IFType ifType, uint32_t /*maxLocalIndexCmp*/) override
    {
        RandomProgram::genIf(ifType, RandomProgram::invocationStride);
    }

    virtual std::string getPartitionBallotText() override
    {
        return "storeValue(outLoc++, subgroupBallot(true))";
    }

    virtual void printIfLocalInvocationIndex(add_ref<std::stringstream> css, add_cref<FlowState> flow) override
    {
        printIndent(css);
        css << "if (invocationIndex() >= inputA.a[0x" << std::hex << flow.ops[flow.opsIndex].value << "]) {\n";
    }

    virtual void printStore(add_ref<std::stringstream> css, add_cref<FlowState> flow) override
    {
        printIndent(css);
        css << "storeValue(outLoc++, 0x" << std::hex << flow.ops[flow.opsIndex].value << std::dec << ");\n";
    }

    virtual void printBallot(add_ref<std::stringstream> css, add_cref<FlowState>,
                             bool endWithSemicolon = false) override
    {
        printIndent(css);
        // When inside loop(s), use partitionBallot rather than subgroupBallot to compute
        // a ballot, to make sure the ballot is "diverged enough". Don't do this for
        // subgroup_uniform_control_flow, since we only validate results that must be fully
        // reconverged.
        if (loopNesting > 0 && caseDef.testType == TT_MAXIMAL)
        {
            css << getPartitionBallotText();
        }
        else
        {
            css << "storeValue(outLoc++, subgroupBallot(true))";
        }
        if (endWithSemicolon)
        {
            css << ";\n";
        }
    }

    virtual void simulateBallot(const bool countOnly, add_cref<Ballots> activeMask, const uint32_t unusedPrimitiveID,
                                const int32_t opsIndex, add_ref<std::vector<uint32_t>> outLoc,
                                add_ref<std::vector<tcu::UVec4>> ref, add_ref<tcu::TestLog> log,
                                std::shared_ptr<Prerequisites> prerequisites, add_ref<uint32_t> logFailureCount,
                                const OPType reason, const tcu::UVec4 *cmp) override
    {
        DE_UNREF(unusedPrimitiveID);
        DE_UNREF(opsIndex);
        add_cref<Arrangement> a(*std::static_pointer_cast<Arrangement>(prerequisites));
        for (uint32_t primitiveID = 0u; primitiveID < a.m_primitiveStride; ++primitiveID)
        {
            const uint32_t sgid = a.m_primitiveSubgroups.at(primitiveID);
            DE_ASSERT(sgid < (a.m_subgroupCount * a.m_subgroupSize));
            if (false == activeMask.test(Ballots::findBit(sgid, a.m_subgroupSize)))
                continue;
            const uint32_t index = (outLoc.at(primitiveID)++) * a.m_invocationStride + primitiveID;
            if (false == countOnly)
            {
                ref.at(index) = Ballot(activeMask.at(sgid / a.m_subgroupSize));
                if (cmp && logFailureCount > 0u && cmp[index] != ref.at(index))
                {
                    logFailureCount -= 1u;
                    log << tcu::TestLog::Message << logFailureCount << ": stored value mismatch from "
                        << OPtypeToStr(reason) << tcu::TestLog::EndMessage;
                }
            }
        }
    }

    virtual void simulateStore(const bool countOnly, add_cref<Ballots> activeMask, const uint32_t unusedPrimitiveID,
                               const uint64_t storeValue, add_ref<std::vector<uint32_t>> outLoc,
                               add_ref<std::vector<tcu::UVec4>> ref, add_ref<tcu::TestLog> log,
                               std::shared_ptr<Prerequisites> prerequisites, add_ref<uint32_t> logFailureCount,
                               const OPType reason, const tcu::UVec4 *cmp) override
    {
        DE_UNREF(unusedPrimitiveID);
        add_cref<Arrangement> a(*std::static_pointer_cast<Arrangement>(prerequisites));
        for (uint32_t primitiveID = 0u; primitiveID < a.m_primitiveStride; ++primitiveID)
        {
            const uint32_t sgid = a.m_primitiveSubgroups.at(primitiveID);
            DE_ASSERT(sgid < (a.m_subgroupCount * a.m_subgroupSize));
            if (false == activeMask.test(Ballots::findBit(sgid, a.m_subgroupSize)))
                continue;
            const uint32_t index = (outLoc.at(primitiveID)++) * a.m_invocationStride + primitiveID;
            if (false == countOnly)
            {
                ref.at(index) = Ballot(tcu::UVec4(uint32_t(storeValue & 0xFFFFFFFF), 0u, 0u, 0u));
                if (cmp && logFailureCount > 0u && cmp[index] != ref.at(index))
                {
                    logFailureCount -= 1u;
                    log << tcu::TestLog::Message << logFailureCount << ": stored value mismatch from "
                        << OPtypeToStr(reason) << tcu::TestLog::EndMessage;
                }
            }
        }
    }

    virtual std::shared_ptr<Prerequisites> makePrerequisites(add_cref<std::vector<uint32_t>> outputP,
                                                             const uint32_t subgroupSize, const uint32_t fragmentStride,
                                                             const uint32_t primitiveStride,
                                                             add_ref<std::vector<SubgroupState2>> stateStack,
                                                             add_ref<std::vector<uint32_t>> outLoc,
                                                             add_ref<uint32_t> subgroupCount) override
    {
        DE_UNREF(fragmentStride);
        auto prerequisites = std::make_shared<Arrangement>(outputP, subgroupSize, primitiveStride);
        subgroupCount      = prerequisites->m_subgroupCount;
        stateStack.resize(10u, SubgroupState2(subgroupCount));
        outLoc.resize(primitiveStride, 0u);
        stateStack.at(0).activeMask = prerequisites->m_initialBallots;
        return prerequisites;
    }
};

class ReconvergenceTestCase : public TestCase
{
public:
    ReconvergenceTestCase(tcu::TestContext &context, const std::string &name, const CaseDef data)
        : TestCase(context, name)
        , m_data(data)
    {
    }
    ~ReconvergenceTestCase(void) = default;
    virtual void checkSupport(Context &context) const override;
    virtual void initPrograms(SourceCollections &programCollection) const override;
    virtual TestInstance *createInstance(Context &context) const override;
    de::MovePtr<RandomProgram> selectProgram() const;

private:
    CaseDef m_data;
};

void ReconvergenceTestCase::checkSupport(Context &context) const
{
    if (!context.contextSupports(vk::ApiVersion(0u, 1u, 1u, 0u)))
        TCU_THROW(NotSupportedError, "Vulkan 1.1 not supported");

    const auto properties                                            = getSubgroupProperties(context);
    const vk::VkPhysicalDeviceSubgroupProperties &subgroupProperties = properties.first;
    const VkPhysicalDeviceLimits &limits                             = properties.second.properties.limits;

    if (m_data.isElect() && !(subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_BASIC_BIT))
        TCU_THROW(NotSupportedError, "VK_SUBGROUP_FEATURE_BASIC_BIT not supported");

    if (!m_data.isElect() && !(subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_BALLOT_BIT))
        TCU_THROW(NotSupportedError, "VK_SUBGROUP_FEATURE_BALLOT_BIT not supported");

    if (m_data.shaderStage == VK_SHADER_STAGE_COMPUTE_BIT)
    {
        if ((m_data.sizeX > limits.maxComputeWorkGroupSize[0]) || (m_data.sizeY > limits.maxComputeWorkGroupSize[1]) ||
            ((m_data.sizeX * m_data.sizeY) > limits.maxComputeWorkGroupInvocations))
        {
            TCU_THROW(NotSupportedError, "compute workgroup count exceeds device limit");
        }
    }

    if (!(subgroupProperties.supportedStages & m_data.shaderStage))
    {
        std::stringstream ss;
        ss << getShaderStageFlagsStr(m_data.shaderStage);
        ss << " does not support subgroup operations";
        ss.flush();
        TCU_THROW(NotSupportedError, ss.str());
    }

    // Both subgroup- AND workgroup-uniform tests are enabled by shaderSubgroupUniformControlFlow.
    if (m_data.isUCF() && !context.getShaderSubgroupUniformControlFlowFeatures().shaderSubgroupUniformControlFlow)
        TCU_THROW(NotSupportedError, "shaderSubgroupUniformControlFlow not supported");

    if (m_data.testType == TT_MAXIMAL && !context.getShaderMaximalReconvergenceFeatures().shaderMaximalReconvergence)
        TCU_THROW(NotSupportedError, "shaderMaximalReconvergence not supported");
}

de::MovePtr<RandomProgram> ReconvergenceTestCase::selectProgram() const
{
    RandomProgram *programPtr(nullptr);
    switch (m_data.shaderStage)
    {
    case VK_SHADER_STAGE_COMPUTE_BIT:
        programPtr = new ComputeRandomProgram(m_data);
        break;
    case VK_SHADER_STAGE_FRAGMENT_BIT:
        programPtr = new FragmentRandomProgram(m_data);
        break;
    case VK_SHADER_STAGE_VERTEX_BIT:
        programPtr = new VertexRandomProgram(m_data);
        break;
    case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
        programPtr = new TessCtrlRandomProgram(m_data, 0);
        break;
    case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
        programPtr = new TessEvalRandomProgram(m_data);
        break;
    case VK_SHADER_STAGE_GEOMETRY_BIT:
        programPtr = new GeometryRandomProgram(m_data);
        break;
    default:
        DE_ASSERT(0);
    }
    DE_ASSERT(programPtr);
    return de::MovePtr<RandomProgram>(programPtr);
}

std::string genPassThroughFragmentSource()
{
    std::stringstream str;
    str << "#version 450 core\n";
    str << "layout(location = 0) out vec4 color;\n";
    str << "void main() {\n";
    str << "  color = vec4(1.0);\n";
    str << "}\n";
    str.flush();
    return str.str();
}

std::string genPassThroughVertexSource()
{
    std::stringstream str;
    str << "#version 450 core\n";
    str << "layout(location = 0) in vec4 pos;\n";
    str << "void main() {\n";
    str << "   gl_Position = vec4(pos.xy, 0.0, 1.0);\n";
    str << "}\n";
    str.flush();
    return str.str();
}

std::string genPassThroughTessCtrlSource()
{
    std::stringstream str;
    str << "#version 450 core\n";
    str << "#extension GL_EXT_tessellation_shader : require\n";
    str << "layout(vertices = 3) out;\n";
    str << "void main() {\n";
    str << "   gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n";
    str << "   gl_TessLevelOuter[0] = 1.0;\n";
    str << "   gl_TessLevelOuter[1] = 1.0;\n";
    str << "   gl_TessLevelOuter[2] = 1.0;\n";
    str << "   gl_TessLevelOuter[3] = 1.0;\n";
    str << "   gl_TessLevelInner[0] = 1.0;\n";
    str << "   gl_TessLevelInner[1] = 1.0;\n";
    str << "}\n";
    str.flush();
    return str.str();
}

std::string genPassThroughTessEvalSource()
{
    std::stringstream str;
    str << "#version 450 core\n";
    str << "#extension GL_EXT_tessellation_shader : require\n";
    str << "layout(equal_spacing, triangles) in;\n";
    str << "void main() {\n";
    str << "   float u = gl_TessCoord.x;\n";
    str << "   float v = gl_TessCoord.y;\n";
    str << "   float w = gl_TessCoord.z;\n";
    str << "   vec4 p0 = vec4(gl_in[0].gl_Position.xy, 0.0, 1.0);\n";
    str << "   vec4 p1 = vec4(gl_in[1].gl_Position.xy, 0.0, 1.0);\n";
    str << "   vec4 p2 = vec4(gl_in[2].gl_Position.xy, 0.0, 1.0);\n";
    str << "   gl_Position = u * p0 + v * p1 + w * p2;\n";
    str << "}\n";
    str.flush();
    return str.str();
}

void ReconvergenceTestCase::initPrograms(SourceCollections &programCollection) const
{
    de::MovePtr<RandomProgram> program = selectProgram();

    program->generateRandomProgram(m_testCtx.getWatchDog(), m_testCtx.getLog());

    std::stringstream header, layout, globals, prologue, epilogue, aux;

    header << "#version 450 core\n";
    header << "#extension GL_KHR_shader_subgroup_ballot : enable\n";
    header << "#extension GL_KHR_shader_subgroup_vote : enable\n";
    header << "#extension GL_NV_shader_subgroup_partitioned : enable\n";
    header << "#extension GL_EXT_subgroup_uniform_control_flow : enable\n";
    if (m_data.testType == TT_MAXIMAL)
    {
        header << "#extension GL_EXT_maximal_reconvergence : require\n";
    }
    switch (m_data.shaderStage)
    {
    case VK_SHADER_STAGE_COMPUTE_BIT:
        layout << "layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z = 1) in;\n";
        layout << "layout(set=0, binding=2) coherent buffer OutputC { uint loc[]; } outputC;\n";
        layout << "layout(set=0, binding=1) coherent buffer OutputB { uvec4 b[]; } outputB;\n";
        layout << "layout(set=0, binding=0) coherent buffer InputA  { uint  a[]; } inputA;\n";
        break;
    case VK_SHADER_STAGE_FRAGMENT_BIT:
        layout << "// NOTE: A fragment can belong to more than one primitive, and the shader processes each\n";
        layout << "//       fragment primitive by primitive, so the number of invocation does not have to be\n";
        layout << "//       equal to the number of fragments of the rendering area. Another important thing\n";
        layout << "//       is that the Implementation is free to change the order of draving primitives\n";
        layout << "//       between subsequent application calls.\n";

        layout << "// inputA.a[ invocationStride ] = { 0, 1, ..., (invocationStride - 1) }\n";
        layout << "layout(set=0, binding=0) coherent buffer InputA  { uint  a[]; } inputA;\n";

        layout << "// outputB.b[ max(loc[]) * invocationStride * primitiveStride ]\n";
        layout << "layout(set=0, binding=1) coherent buffer OutputB { uvec4 b[]; } outputB;\n";

        layout << "// outputC.c[invocationStride * primitiveStride ], incremented per primitive\n";
        layout << "layout(set=0, binding=2) coherent buffer OutputC { uint  loc[]; } outputC;\n";

        layout << "// outputP.p[ width * height * primitiveStride + 1 ], one more for calculating subgroupID\n";
        layout << "layout(set=0, binding=3) coherent buffer OutputP { uint  p[]; } outputP;\n";

        layout << "layout(location = 0) out vec4 dEQP_FragColor;\n";
        break;
    case VK_SHADER_STAGE_VERTEX_BIT:
        layout << "layout(location = 0) in vec4 pos;\n";
        layout << "layout(set=0, binding=3) coherent buffer OutputP { uint  p[]; } outputP;\n";
        layout << "layout(set=0, binding=2) coherent buffer OutputC { uint loc[]; } outputC;\n";
        layout << "layout(set=0, binding=1) coherent buffer OutputB { uvec4 b[]; } outputB;\n";
        layout << "layout(set=0, binding=0) coherent buffer InputA  { uint  a[]; } inputA;\n";
        break;
    case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
        layout << "#extension GL_EXT_tessellation_shader : require\n";
        layout << "layout(vertices = " << TessCtrlRandomProgram::minSubgroupSize << ") out;\n";
        layout << "layout(set=0, binding=2) coherent buffer OutputC { uint loc[]; } outputC;\n";
        layout << "layout(set=0, binding=1) coherent buffer OutputB { uvec2 b[]; } outputB;\n";
        layout << "layout(set=0, binding=0) coherent buffer InputA  { uint  a[]; } inputA;\n";
        break;
    case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
        layout << "#extension GL_EXT_tessellation_shader : require\n";
        layout << "layout(equal_spacing, quads) in;\n";
        layout << "layout(set=0, binding=2) coherent buffer OutputC { uint loc[]; } outputC;\n";
        layout << "layout(set=0, binding=1) coherent buffer OutputB { uvec2 b[]; } outputB;\n";
        layout << "layout(set=0, binding=0) coherent buffer InputA  { uint  a[]; } inputA;\n";
        break;
    case VK_SHADER_STAGE_GEOMETRY_BIT:
        layout << "#extension GL_EXT_geometry_shader : require\n";
        layout << "layout(points) in;\n";
        layout << "layout(points, max_vertices = 1) out;\n";
        layout << "layout(set=0, binding=3) coherent buffer OutputP { uint  p[]; } outputP;\n";
        layout << "layout(set=0, binding=2) coherent buffer OutputC { uint loc[]; } outputC;\n";
        layout << "layout(set=0, binding=1) coherent buffer OutputB { uvec4 b[]; } outputB;\n";
        layout << "layout(set=0, binding=0) coherent buffer InputA  { uint  a[]; } inputA;\n";
        break;
    default:
        DE_ASSERT(0);
    }

    std::stringstream pushConstantLayout;
    pushConstantLayout
        << "layout(push_constant) uniform PC {\n"
           "   // set to the real stride when writing out ballots, or zero when just counting\n"
           "   int  invocationStride;\n"
           "   // wildcard fields, for an example the dimensions of rendered area in the case of graphics shaders\n"
           "   int  width;\n"
           "   int  height;\n"
           "   uint primitiveStride;\n"
           "   uint subgroupStride;\n"
           "   uint enableInvocationIndex;\n"
           "};\n";
    pushConstantLayout.flush();
    layout << pushConstantLayout.str();

    globals << "int outLoc = 0;\n";
    globals << "bool testBit(uvec4 mask, uint bit) { return ((mask[bit / 32] >> (bit % 32)) & 1) != 0; }\n";
    globals << "uint elect() { return int(subgroupElect()) + 1; }\n";
    if (m_data.shaderStage == VK_SHADER_STAGE_FRAGMENT_BIT)
    {
        static const std::string helperRoutinesCode(R"glsl(
        void setBit(uint bit, in out uvec4 ballot) {
            uint c = bit / 32;
            switch (c) {
                case 0: ballot.x |= (1u << (bit % 32)); break;
                case 1: ballot.y |= (1u << (bit % 32)); break;
                case 2: ballot.z |= (1u << (bit % 32)); break;
                case 3: ballot.w |= (1u << (bit % 32)); break;
            }
        }
        void resetBit(uint bit, in out uvec4 ballot) {
            uint c = bit / 32;
            uint mask = 0xFFFFFFFF ^ (1u << (bit % 32));
            switch (c) {
                case 0: ballot.x &= mask; break;
                case 1: ballot.y &= mask; break;
                case 2: ballot.z &= mask; break;
                case 3: ballot.w &= mask; break;
            }
        }
        uint fragmentIndex() { return (uint(gl_FragCoord.y) * width + uint(gl_FragCoord.x)); }
        uint invocationIndex() { return subgroupID * gl_SubgroupSize + gl_SubgroupInvocationID; }
        uvec4 invocationElectBallot() {
            uvec4 ballot = uvec4(0);
            ballot[gl_SubgroupInvocationID / 32] = (1 << (gl_SubgroupInvocationID % 32));
            return ballot;
        }
        uint next(uint hint) {
            return gl_HelperInvocation
                ? (hint * enableInvocationIndex)
                : outputC.loc[(gl_PrimitiveID * (subgroupStride * 128) + invocationIndex()) * enableInvocationIndex]++;
        }
        uint index(uint hint) {
            return ((
                next(hint) * (subgroupStride * 128 * primitiveStride)
                + (gl_PrimitiveID * subgroupStride * 128) + invocationIndex()) * enableInvocationIndex);
        }
        void storeValue(uint hintIndex, uvec4 value)
        {
            if (gl_HelperInvocation) {
                if (hintIndex < BALLOT_STACK_SIZE)
                    ballotStack[hintIndex] = value;
            }
            else {
                outputB.b[index(hintIndex)] = value;
            }
        }
        void storeValue(uint hintIndex, uint value) { storeValue(hintIndex, uvec4(value, 0, 0, 0)); }
        void storeBallot(uint hintIndex) { storeValue(hintIndex, subgroupBallot(true)); }
        )glsl");

        static const std::string prologueCode(R"glsl(
        uint helperInvocationCount = 0u;
        uint nonHelperInvocationCount = 0u;
        uvec4 helperInvocationsBits = uvec4(0, 0, 0, 0);
        uvec4 nonHelperInvocationsBits = uvec4(0, 0, 0, 0);
        if (gl_HelperInvocation)
        {
            helperInvocationsBits = subgroupBallot(true);
            helperInvocationCount = 1u;
        }
        else
        {
            nonHelperInvocationsBits = subgroupBallot(true);
            nonHelperInvocationCount = 1u;
        }

        helperInvocationsBits = subgroupOr(helperInvocationsBits);
        nonHelperInvocationsBits = subgroupOr(nonHelperInvocationsBits);
        uint helperBitCount = subgroupBallotBitCount(helperInvocationsBits);
        uint nonHelperBitCount = subgroupBallotBitCount(nonHelperInvocationsBits);
        helperInvocationCount = subgroupAdd(helperInvocationCount);
        nonHelperInvocationCount = subgroupAdd(nonHelperInvocationCount);

        const uint nonHelperElectBit = subgroupBallotFindLSB(nonHelperInvocationsBits);
        if (gl_SubgroupInvocationID == nonHelperElectBit)
        {
            subgroupID = atomicAdd(outputP.p[width * height * primitiveStride + 0], 1);
            outputP.p[width * height * primitiveStride + 1] = gl_SubgroupSize;
            atomicAdd(outputP.p[width * height * primitiveStride + 2], nonHelperInvocationCount);
            atomicAdd(outputP.p[width * height * primitiveStride + 3], helperInvocationCount);
        }

        subgroupID = subgroupShuffle(subgroupID, nonHelperElectBit);

        const uint localPrimitiveID = gl_PrimitiveID;
        const uint localFragmentID = fragmentIndex();

        if (!gl_HelperInvocation)
        {
            outputP.p[localFragmentID * primitiveStride + localPrimitiveID] =
                ((subgroupID + 1) << 16) | gl_SubgroupInvocationID;
        }

        // Maping helper invocations block
        {
            uvec4 tmpHelperBits = helperInvocationsBits;
            uint helperSubgroupInvocationID = subgroupBallotFindLSB(tmpHelperBits);
            while (subgroupBallotBitExtract(tmpHelperBits, helperSubgroupInvocationID))
            {
                uint helperSubgroupID = subgroupShuffle(subgroupID, helperSubgroupInvocationID);
                uint helperFragmentID = subgroupShuffle(localFragmentID, helperSubgroupInvocationID);
                uint helperPrimitiveID = subgroupShuffle(localPrimitiveID, helperSubgroupInvocationID);
                if (gl_SubgroupInvocationID == nonHelperElectBit)
                {
                    outputP.p[helperFragmentID * primitiveStride + helperPrimitiveID] =
                        (((helperSubgroupID + 1) | 0x8000) << 16) | helperSubgroupInvocationID;
                }
                resetBit(helperSubgroupInvocationID, tmpHelperBits);
                helperSubgroupInvocationID = subgroupBallotFindLSB(tmpHelperBits);
            }
        }
        )glsl");

        static const std::string epilogueCode(R"glsl(
        // Save helper invocations entries block
        {
            uvec4 tmpHelperBits = subgroupOr(helperInvocationsBits);
            uint helperSubgroupInvocationID = subgroupBallotFindLSB(tmpHelperBits);
            while (helperSubgroupInvocationID < gl_SubgroupSize)
            {
                const uint maxOutLoc = subgroupShuffle(outLoc, helperSubgroupInvocationID);
                if (maxOutLoc == 0)
                {
                    resetBit(helperSubgroupInvocationID, tmpHelperBits);
                    helperSubgroupInvocationID = subgroupBallotFindLSB(tmpHelperBits);
                    continue;
                }

                uvec4 helperBallotStack[BALLOT_STACK_SIZE];
                uint helperSubgroupID = subgroupShuffle(subgroupID, helperSubgroupInvocationID);
                uint helperFragmentID = subgroupShuffle(localFragmentID, helperSubgroupInvocationID);
                uint helperPrimitiveID = subgroupShuffle(localPrimitiveID, helperSubgroupInvocationID);
                for (uint i = 0; i < maxOutLoc && i < BALLOT_STACK_SIZE; i++) {
                    helperBallotStack[i] = subgroupShuffle(ballotStack[i], helperSubgroupInvocationID);
                }

                if (gl_SubgroupInvocationID == nonHelperElectBit)
                {
                    uint helperInvocationIndex = helperSubgroupID * gl_SubgroupSize + helperSubgroupInvocationID;
                    uint helperPrimitiveInvocationIndex = helperInvocationIndex * primitiveStride + helperPrimitiveID;

                    outputC.loc[(helperInvocationIndex * primitiveStride + helperPrimitiveID) * enableInvocationIndex] = maxOutLoc;

                    for (uint j = 0; j < maxOutLoc; j++)
                    {
                        uint outputIndex = ((j * (subgroupStride * 128u * primitiveStride)
                            + (helperPrimitiveID * subgroupStride * 128u) + helperInvocationIndex) * enableInvocationIndex);
                        uvec4 outputValue = (j < BALLOT_STACK_SIZE) ? helperBallotStack[j] : uvec4(0,0,0,0);
                        outputB.b[outputIndex] = outputValue;
                    }
                }
                resetBit(helperSubgroupInvocationID, tmpHelperBits);
                helperSubgroupInvocationID = subgroupBallotFindLSB(tmpHelperBits);
            } // wend
        }

        dEQP_FragColor = vec4(1.0);
        )glsl");

        header << "#extension GL_KHR_shader_subgroup_shuffle : enable\n";
        header << "#extension GL_KHR_shader_subgroup_arithmetic : enable\n";
        header << "#define BALLOT_STACK_SIZE " << FragmentRandomProgram::experimentalOutLocSize << '\n';

        {
            aux << header.str();
            aux << pushConstantLayout.str();
            aux << "uint outLoc = 0;\n";
            aux << "struct OutputC { uint loc[1]; };\n";
            aux << "struct OutputB { uvec4 b[1]; };\n";
            aux << "uint subgroupID = 11111;\n";
            aux << "uvec4 ballotStack[BALLOT_STACK_SIZE];\n";
            aux << "OutputC outputC;\n";
            aux << "OutputB outputB;\n";
            aux << "// OutputP.p[ width * height * primitiveStride + 4 ], few more for calculating subgroupID, "
                   "subgroupSize, non-helper and helper invocations\n";
            aux << "layout(set = 0, binding = 0) coherent buffer OutputP { uint p[]; } outputP;\n";
            aux << "layout(location = 0) out vec4 dEQP_FragColor;\n";
            aux << helperRoutinesCode;
            aux << "void main() {\n"
                << prologueCode << epilogueCode << "   \n"
                << "}\n";
        }

        globals << "uint subgroupID = 22222;\n";
        globals << "uvec4 ballotStack[BALLOT_STACK_SIZE];\n";
        globals << helperRoutinesCode;

        prologue << prologueCode;
        epilogue << epilogueCode;
    }
    else if (m_data.shaderStage == VK_SHADER_STAGE_VERTEX_BIT)
    {
        static const std::string helperRoutinesCode(R"glsl(
        uint invocationIndex() { return subgroupID * gl_SubgroupSize + gl_SubgroupInvocationID; }
        uvec4 invocationElectBallot() {
            uvec4 ballot = uvec4(0);
            ballot[gl_SubgroupInvocationID / 32] = (1 << (gl_SubgroupInvocationID % 32));
            return ballot;
        }
        void storeValue(uint loc, uvec4 value) {
            outputC.loc[gl_VertexIndex] = loc + 1u;
            outputB.b[(loc * invocationStride + gl_VertexIndex) * enableInvocationIndex] = value;
        }
        void storeValue(uint loc, uint value) { storeValue(loc, uvec4(value, 0, 0, 0)); }
        )glsl");

        static const std::string prologueCode(R"glsl(
        uint invocationCount = 1u;
        invocationCount = subgroupAdd(invocationCount);

        if (subgroupElect())
        {
            subgroupID = atomicAdd(outputP.p[NUM_SUBGROUPS_OFFSET], 1u);    // [+0]    subgroupID
            outputP.p[SUBGROUP_SIZE_OFFSET] = gl_SubgroupSize;                // [+1]    subgroupSize
            atomicAdd(outputP.p[INVOCATION_COUNT_OFFSET], invocationCount);    // [+2]    invocationCount
        }
        subgroupID = subgroupBroadcastFirst(subgroupID);

        outputP.p[gl_VertexIndex + INVOCATION_ENTRIES_OFFSET] = ((subgroupID + 1) << 16) | gl_SubgroupInvocationID;
        )glsl");

        static const std::string epilogueCode(R"glsl(
        gl_Position = vec4(pos.xy, 0.0, 1.0);
        gl_PointSize = 1.0;
        )glsl");

        header << "#extension GL_KHR_shader_subgroup_arithmetic : enable\n";
        header << "#define NUM_SUBGROUPS_OFFSET            0\n";
        header << "#define SUBGROUP_SIZE_OFFSET            1\n";
        header << "#define INVOCATION_COUNT_OFFSET        2\n";
        header << "#define INVOCATION_ENTRIES_OFFSET    3\n";

        globals << "uint subgroupID = 33333;\n";
        globals << helperRoutinesCode;

        prologue << prologueCode;
        epilogue << epilogueCode;
    }
    else if (m_data.shaderStage == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
    {
        // push_constant::width holds the smallest subgroup size defined in TessCtrlRandomProgram::minSubgroupSize
        globals << "// push_constant::width is the smallest subgroup size which this shader is run on\n";
        globals << "uint invocationIndex() { return ((((gl_PrimitiveID * width) / gl_SubgroupSize) * gl_SubgroupSize) "
                   "+ gl_SubgroupInvocationID); }\n";

        epilogue
            << "   gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID % gl_PatchVerticesIn].gl_Position;\n";
        epilogue << "   gl_TessLevelOuter[0] = 1.0;\n";
        epilogue << "   gl_TessLevelOuter[1] = 1.0;\n";
        epilogue << "   gl_TessLevelOuter[2] = 1.0;\n";
        epilogue << "   gl_TessLevelOuter[3] = 1.0;\n";
        epilogue << "   gl_TessLevelInner[0] = 1.0;\n";
        epilogue << "   gl_TessLevelInner[1] = 1.0;\n";
    }
    else if (m_data.shaderStage == VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
    {
        globals << "// push_constant::width is an invocation count when processing a quad for a single patch\n";
        globals << "uint invocationIndex() { return ((((gl_PrimitiveID * width) / gl_SubgroupSize) * gl_SubgroupSize) "
                   "+ gl_SubgroupInvocationID); }\n";

        epilogue << "   float u = gl_TessCoord.x;\n";
        epilogue << "   float v = gl_TessCoord.y;\n";
        epilogue << "   float w = gl_TessCoord.z;\n";
        epilogue << "   vec4 p0 = vec4(gl_in[0].gl_Position.xy, 0.0, 1.0);\n";
        epilogue << "   vec4 p1 = vec4(gl_in[1].gl_Position.xy, 0.0, 1.0);\n";
        epilogue << "   vec4 p2 = vec4(gl_in[2].gl_Position.xy, 0.0, 1.0);\n";
        epilogue << "   gl_Position = u * p0 + v * p1 + w * p2;\n";
    }
    else if (m_data.shaderStage == VK_SHADER_STAGE_GEOMETRY_BIT)
    {
        static const std::string helperRoutinesCode(R"glsl(
        uint invocationIndex() { return subgroupID * gl_SubgroupSize + gl_SubgroupInvocationID; }
        void storeValue(uint loc, uvec4 value) {
            outputC.loc[gl_PrimitiveIDIn] = loc + 1u;
            outputB.b[(loc * invocationStride + gl_PrimitiveIDIn) * enableInvocationIndex] = value;
        }
        void storeValue(uint loc, uint value) { storeValue(loc, uvec4(value, 0, 0, 0)); }
        void storeBallot(uint loc) { storeValue(loc, subgroupBallot(true)); }
        uvec4 invocationElectBallot() {
            uvec4 ballot = uvec4(0);
            ballot[gl_SubgroupInvocationID / 32] = (1 << (gl_SubgroupInvocationID % 32));
            return ballot;
        }
        )glsl");

        static const std::string prologueCode(R"glsl(
        uint invocationCount = 1u;
        invocationCount = subgroupAdd(invocationCount);
        uint identity = gl_PrimitiveIDIn + 1u;
        uint maxIdentity = subgroupMax(identity);

        if (subgroupElect()) {
            subgroupID = atomicAdd(outputP.p[SUBGROUP_ID_OFFSET], 1u);            // [+0]    subgroupID
            outputP.p[SUBGROUP_SIZE_OFFSET] = gl_SubgroupSize;                    // [+1]    subgroupSize
            atomicAdd(outputP.p[INVOCATION_COUNT_OFFSET], invocationCount);        // [+2]    invocationCount
            atomicMax(outputP.p[MAX_IDENTITY_OFFSET], maxIdentity);
        }
        subgroupID = subgroupBroadcastFirst(subgroupID);

        outputP.p[gl_PrimitiveIDIn + INVOCATION_ENTRY_OFFSET] = ((subgroupID + 1) << 16) | gl_SubgroupInvocationID;

        )glsl");

        static const std::string epilogueCode(R"glsl(
        uint maxLoc = subgroupMax(outLoc);
        atomicMax(outputP.p[MAX_LOC_OFFSET], maxLoc);

        gl_Position = gl_in[gl_PrimitiveIDIn].gl_Position;
        gl_PrimitiveID = gl_PrimitiveIDIn;

        EmitVertex();
        EndPrimitive();
        )glsl");

        header << "#extension GL_KHR_shader_subgroup_arithmetic : enable\n";
        header << "#define SUBGROUP_ID_OFFSET       0\n";
        header << "#define SUBGROUP_SIZE_OFFSET     1\n";
        header << "#define INVOCATION_COUNT_OFFSET  2\n";
        header << "#define MAX_LOC_OFFSET           3\n";
        header << "#define MAX_IDENTITY_OFFSET      4\n";
        header << "#define INVOCATION_ENTRY_OFFSET  5\n";

        globals << "uint subgroupID;\n";
        globals << "uint numSubgroups;\n";
        globals << helperRoutinesCode;

        prologue << prologueCode;
        epilogue << epilogueCode;
    }

    std::stringstream css, functions, main;
    program->printCode(functions, main);

    css << header.str();
    css << layout.str();
    css << globals.str();

    css << functions.str() << "\n\n";

    css << "void main()\n"
        << (m_data.isSUCF() ? "[[subgroup_uniform_control_flow]]\n" : "")
        << (m_data.testType == TT_MAXIMAL ? "[[maximally_reconverges]]\n" : "") << "{\n";

    css << prologue.str() << "\n";
    css << main.str() << "\n\n";
    css << epilogue.str() << "\n";

    css << "}\n";

    const vk::ShaderBuildOptions buildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);

    auto &testingShader = programCollection.glslSources.add("test");
    switch (m_data.shaderStage)
    {
    case VK_SHADER_STAGE_COMPUTE_BIT:
        testingShader << glu::ComputeSource(css.str()) << buildOptions;
        break;
    case VK_SHADER_STAGE_FRAGMENT_BIT:
        testingShader << glu::FragmentSource(css.str()) << buildOptions;
        programCollection.glslSources.add("vert") << glu::VertexSource(genPassThroughVertexSource()) << buildOptions;
        programCollection.glslSources.add("aux") << glu::FragmentSource(aux.str()) << buildOptions;
        break;
    case VK_SHADER_STAGE_VERTEX_BIT:
        testingShader << glu::VertexSource(css.str()) << buildOptions;
        programCollection.glslSources.add("frag") << glu::FragmentSource(genPassThroughFragmentSource());
        break;
    case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
        testingShader << glu::TessellationControlSource(css.str()) << buildOptions;
        programCollection.glslSources.add("vert") << glu::VertexSource(genPassThroughVertexSource());
        programCollection.glslSources.add("frag") << glu::FragmentSource(genPassThroughFragmentSource());
        programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(genPassThroughTessEvalSource());
        break;
    case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
        testingShader << glu::TessellationEvaluationSource(css.str()) << buildOptions;
        programCollection.glslSources.add("vert") << glu::VertexSource(genPassThroughVertexSource());
        programCollection.glslSources.add("frag") << glu::FragmentSource(genPassThroughFragmentSource());
        programCollection.glslSources.add("tesc") << glu::TessellationControlSource(genPassThroughTessCtrlSource());
        break;
    case VK_SHADER_STAGE_GEOMETRY_BIT:
        testingShader << glu::GeometrySource(css.str()) << buildOptions;
        programCollection.glslSources.add("vert") << glu::VertexSource(genPassThroughVertexSource());
        programCollection.glslSources.add("frag") << glu::FragmentSource(genPassThroughFragmentSource());
        break;
    default:
        DE_ASSERT(0);
    }
}

TestInstance *ReconvergenceTestCase::createInstance(Context &context) const
{
    switch (m_data.shaderStage)
    {
    case VK_SHADER_STAGE_COMPUTE_BIT:
        return new ReconvergenceTestComputeInstance(context, m_data);
    case VK_SHADER_STAGE_FRAGMENT_BIT:
        return new ReconvergenceTestFragmentInstance(context, m_data);
    case VK_SHADER_STAGE_VERTEX_BIT:
        return new ReconvergenceTestVertexInstance(context, m_data);
    case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
        return new ReconvergenceTestTessCtrlInstance(context, m_data);
    case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
        return new ReconvergenceTestTessEvalInstance(context, m_data);
    case VK_SHADER_STAGE_GEOMETRY_BIT:
        return new ReconvergenceTestGeometryInstance(context, m_data);
    default:
        DE_ASSERT(false);
    }
    return nullptr;
}

tcu::TestStatus ReconvergenceTestComputeInstance::iterate(void)
{
    const DeviceInterface &vk            = m_context.getDeviceInterface();
    const VkDevice device                = m_context.getDevice();
    Allocator &allocator                 = m_context.getDefaultAllocator();
    tcu::TestLog &log                    = m_context.getTestContext().getLog();
    const VkPhysicalDeviceLimits &limits = m_context.getDeviceProperties().limits;

    //const uint32_t invocationCount = (ROUNDUP(invocationCount, m_subgroupSize) / m_subgroupSize) * 128u;
    const uint32_t invocationStride = m_data.sizeX * m_data.sizeY;

    std::vector<tcu::UVec4> ref;
    ComputeRandomProgram program(m_data);
    program.generateRandomProgram(m_context.getTestContext().getWatchDog(), log);

    uint32_t maxLoc =
        program.execute(m_context.getTestContext().getWatchDog(), true, m_subgroupSize, 0u, invocationStride, ref, log);
    uint32_t shaderMaxLoc = maxLoc;

    // maxLoc is per-invocation. Add one (to make sure no additional writes are done) and multiply by
    // the number of invocations
    maxLoc++;
    maxLoc *= invocationStride;

    // buffer[0] is an input filled with a[i] == i
    // buffer[1] is the output
    // buffer[2] is the location counts
    de::MovePtr<BufferWithMemory> buffers[3];
    vk::VkDescriptorBufferInfo bufferDescriptors[3];

    VkDeviceSize sizes[3] = {
        invocationStride * sizeof(uint32_t),
        maxLoc * sizeof(tcu::UVec4),
        invocationStride * sizeof(uint32_t),
    };

    for (uint32_t i = 0; i < 3; ++i)
    {
        if (sizes[i] > limits.maxStorageBufferRange)
            TCU_THROW(NotSupportedError, "Storage buffer size larger than device limits");

        try
        {
            buffers[i] = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
                vk, device, allocator,
                makeBufferCreateInfo(sizes[i], VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
                MemoryRequirement::HostVisible | MemoryRequirement::Cached));
        }
        catch (tcu::ResourceError &)
        {
            // Allocation size is unpredictable and can be too large for some systems. Don't treat allocation failure as a test failure.
            return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING,
                                   "Failed device memory allocation " + de::toString(sizes[i]) + " bytes");
        }
        bufferDescriptors[i] = makeDescriptorBufferInfo(**buffers[i], 0, sizes[i]);
    }

    void *ptrs[3];
    for (uint32_t i = 0; i < 3; ++i)
    {
        ptrs[i] = buffers[i]->getAllocation().getHostPtr();
    }
    for (uint32_t i = 0; i < sizes[0] / sizeof(uint32_t); ++i)
    {
        ((uint32_t *)ptrs[0])[i] = i;
    }
    deMemset(ptrs[1], 0, (size_t)sizes[1]);
    deMemset(ptrs[2], 0, (size_t)sizes[2]);

    vk::DescriptorSetLayoutBuilder layoutBuilder;

    layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, m_data.shaderStage);
    layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, m_data.shaderStage);
    layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, m_data.shaderStage);

    vk::Unique<vk::VkDescriptorSetLayout> descriptorSetLayout(layoutBuilder.build(vk, device));

    vk::Unique<vk::VkDescriptorPool> descriptorPool(
        vk::DescriptorPoolBuilder()
            .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3u)
            .build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));
    vk::Unique<vk::VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

    const VkPushConstantRange pushConstantRange = {
        (VkShaderStageFlags)m_data.shaderStage, // VkShaderStageFlags stageFlags;
        0u,                                     // uint32_t offset;
        sizeof(PushConstant)                    // uint32_t size;
    };

    const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // sType
        DE_NULL,                                       // pNext
        (VkPipelineLayoutCreateFlags)0,
        1,                          // setLayoutCount
        &descriptorSetLayout.get(), // pSetLayouts
        1u,                         // pushConstantRangeCount
        &pushConstantRange,         // pPushConstantRanges
    };

    flushAlloc(vk, device, buffers[0]->getAllocation());
    flushAlloc(vk, device, buffers[1]->getAllocation());
    flushAlloc(vk, device, buffers[2]->getAllocation());

    const VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
    const Unique<VkShaderModule> shader(createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0));
    Move<VkPipelineLayout> pipelineLayout = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo, NULL);
    Move<VkPipeline> pipeline             = createComputePipeline(*pipelineLayout, *shader);
    const VkQueue queue                   = m_context.getUniversalQueue();
    Move<VkCommandPool> cmdPool     = createCommandPool(vk, device, vk::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                                        m_context.getUniversalQueueFamilyIndex());
    Move<VkCommandBuffer> cmdBuffer = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    vk::DescriptorSetUpdateBuilder setUpdateBuilder;
    setUpdateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0),
                                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptors[0]);
    setUpdateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(1),
                                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptors[1]);
    setUpdateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(2),
                                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptors[2]);
    setUpdateBuilder.update(vk, device);

    PushConstant pc{/* pcinvocationStride is initialized with 0, the rest of fields as well */};

    // compute "maxLoc", the maximum number of locations written
    beginCommandBuffer(vk, *cmdBuffer, 0u);
    vk.cmdBindDescriptorSets(*cmdBuffer, bindPoint, *pipelineLayout, 0u, 1, &*descriptorSet, 0u, DE_NULL);
    vk.cmdBindPipeline(*cmdBuffer, bindPoint, *pipeline);
    vk.cmdPushConstants(*cmdBuffer, *pipelineLayout, m_data.shaderStage, 0, sizeof(pc), &pc);
    vk.cmdDispatch(*cmdBuffer, 1, 1, 1);
    endCommandBuffer(vk, *cmdBuffer);

    submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

    invalidateAlloc(vk, device, buffers[1]->getAllocation());
    invalidateAlloc(vk, device, buffers[2]->getAllocation());

    // Take the max over all invocations. Add one (to make sure no additional writes are done) and multiply by
    // the number of invocations
    uint32_t newMaxLoc = 0;
    for (uint32_t id = 0; id < invocationStride; ++id)
        newMaxLoc = de::max(newMaxLoc, ((uint32_t *)ptrs[2])[id]);
    shaderMaxLoc = newMaxLoc;
    newMaxLoc++;
    newMaxLoc *= invocationStride;

    // If we need more space, reallocate buffers[1]
    if (newMaxLoc > maxLoc)
    {
        maxLoc   = newMaxLoc;
        sizes[1] = maxLoc * sizeof(tcu::UVec4);

        if (sizes[1] > limits.maxStorageBufferRange)
            TCU_THROW(NotSupportedError, "Storage buffer size larger than device limits");

        try
        {
            buffers[1] = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
                vk, device, allocator,
                makeBufferCreateInfo(sizes[1], VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
                MemoryRequirement::HostVisible | MemoryRequirement::Cached));
        }
        catch (tcu::ResourceError &)
        {
            // Allocation size is unpredictable and can be too large for some systems. Don't treat allocation failure as a test failure.
            return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING,
                                   "Failed device memory allocation " + de::toString(sizes[1]) + " bytes");
        }
        bufferDescriptors[1] = makeDescriptorBufferInfo(**buffers[1], 0, sizes[1]);
        ptrs[1]              = buffers[1]->getAllocation().getHostPtr();

        vk::DescriptorSetUpdateBuilder setUpdateBuilder2;
        setUpdateBuilder2.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(1),
                                      VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptors[1]);
        setUpdateBuilder2.update(vk, device);
    }

    // Clear any writes to buffer[1] during the counting pass
    deMemset(ptrs[1], 0, (size_t)sizes[1]);
    flushAlloc(vk, device, buffers[1]->getAllocation());
    // Clear any writes to buffer[2] during the counting pass
    deMemset(ptrs[2], 0, (size_t)sizes[2]);
    flushAlloc(vk, device, buffers[2]->getAllocation());

    // change invocationStride value in shader
    pc.invocationStride = invocationStride;

    // run the actual shader
    beginCommandBuffer(vk, *cmdBuffer, 0u);
    vk.cmdBindDescriptorSets(*cmdBuffer, bindPoint, *pipelineLayout, 0u, 1, &*descriptorSet, 0u, DE_NULL);
    vk.cmdBindPipeline(*cmdBuffer, bindPoint, *pipeline);
    vk.cmdPushConstants(*cmdBuffer, *pipelineLayout, m_data.shaderStage, 0, sizeof(pc), &pc);
    vk.cmdDispatch(*cmdBuffer, 1, 1, 1);
    endCommandBuffer(vk, *cmdBuffer);

    submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

    invalidateAlloc(vk, device, buffers[1]->getAllocation());

    // Simulate execution on the CPU, and compare against the GPU result
    try
    {
        ref.resize(maxLoc, tcu::UVec4());
    }
    catch (const std::bad_alloc &)
    {
        // Allocation size is unpredictable and can be too large for some systems. Don't treat allocation failure as a test failure.
        return tcu::TestStatus(QP_TEST_RESULT_NOT_SUPPORTED,
                               "Failed system memory allocation " + de::toString(maxLoc * sizeof(uint64_t)) + " bytes");
    }

    program.execute(m_context.getTestContext().getWatchDog(), false, m_subgroupSize, 0u, invocationStride, ref, log);

    const tcu::UVec4 *result = (const tcu::UVec4 *)ptrs[1];

    qpTestResult res = calculateAndLogResult(result, ref, invocationStride, m_subgroupSize, shaderMaxLoc);

    return tcu::TestStatus(res, qpGetTestResultName(res));
}

qpTestResult_e ReconvergenceTestComputeInstance::calculateAndLogResult(const tcu::UVec4 *result,
                                                                       const std::vector<tcu::UVec4> &ref,
                                                                       uint32_t invocationStride, uint32_t subgroupSize,
                                                                       uint32_t shaderMaxLoc)
{
    const uint32_t maxLoc = static_cast<uint32_t>(ref.size());
    tcu::TestLog &log     = m_context.getTestContext().getLog();
    qpTestResult res      = QP_TEST_RESULT_PASS;
    DE_ASSERT(subgroupSize * shaderMaxLoc <= maxLoc);
    DE_UNREF(shaderMaxLoc);

    uint32_t mismatchCount            = 0u;
    const uint32_t printMismatchCount = 5u;
    if (m_data.testType == TT_MAXIMAL)
    {
        // With maximal reconvergence, we should expect the output to exactly match
        // the reference.
        for (uint32_t i = 0; i < maxLoc; ++i)
        {
            const Ballot resultVal(result[i], subgroupSize);
            const Ballot refVal(ref[i], subgroupSize);
            if (resultVal != refVal)
            {
                res = QP_TEST_RESULT_FAIL;
                if (mismatchCount++ < printMismatchCount)
                {
                    log << tcu::TestLog::Message << "Mismatch at " << i << "\nexpected: " << resultVal
                        << "\n     got: " << refVal << tcu::TestLog::EndMessage;
                }
                else
                    break;
            }
        }

#if 0 // This log can be large and slow, ifdef it out by default
        log << tcu::TestLog::Message << "subgroupSize:" << subgroupSize << ", invocationStride:" << invocationStride << ", maxLoc:" << shaderMaxLoc << tcu::TestLog::EndMessage;
        uint32_t invMax = std::min(invocationStride, 50u);
        for (uint32_t inv = 0; inv < invMax; ++inv)
        {
            auto ll = log << tcu::TestLog::Message;
            ll << inv << ": ";
            for (uint32_t loc = 0; loc < shaderMaxLoc; ++loc)
            {
                uint64_t entry = result[loc * invocationStride + inv];
                ll << de::toString(loc) << ":" << tcu::toHex(entry) << ' ';
            }
            ll << tcu::TestLog::EndMessage;
        }
#endif

        if (res != QP_TEST_RESULT_PASS)
        {
            for (uint32_t i = 0; i < maxLoc; ++i)
            {
#if 0
                // This log can be large and slow, ifdef it out by default
                const Ballot resultVal(result[i], subgroupSize);
                const Ballot refVal(ref[i], subgroupSize);
                log << tcu::TestLog::Message << "result " << i << "(" << (i / invocationStride) << ", " << (i % invocationStride) << "): " << resultVal << " ref " << refVal << (resultVal != refVal ? " different" : "") << tcu::TestLog::EndMessage;
#endif
            }
        }
    }
    else
    {
        DE_ASSERT(subgroupSize != 0);

        Ballot fullMask = subgroupSizeToMask(subgroupSize, 0 /* ignored */);
        // For subgroup_uniform_control_flow, we expect any fully converged outputs in the reference
        // to have a corresponding fully converged output in the result. So walk through each lane's
        // results, and for each reference value of fullMask, find a corresponding result value of
        // fullMask where the previous value (OP_STORE) matches. That means these came from the same
        // source location.
        vector<uint32_t> firstFail(invocationStride, 0);
        for (uint32_t lane = 0; lane < invocationStride; ++lane)
        {
            uint32_t resLoc = lane + invocationStride, refLoc = lane + invocationStride;
            while (refLoc < maxLoc)
            {
                while (refLoc < maxLoc && ref[refLoc] != fullMask)
                    refLoc += invocationStride;
                if (refLoc >= maxLoc)
                    break;

                // For TT_SUCF_ELECT, when the reference result has a full mask, we expect lane 0 to be elected
                // (a value of 2) and all other lanes to be not elected (a value of 1). For TT_SUCF_BALLOT, we
                // expect a full mask. Search until we find the expected result with a matching store value in
                // the previous result.
                Ballot expectedResult = m_data.isElect() ? Ballot((lane % m_subgroupSize) == 0 ? 2 : 1) : fullMask;

                while (resLoc < maxLoc && !(result[resLoc] == expectedResult &&
                                            result[resLoc - invocationStride] == ref[refLoc - invocationStride]))
                    resLoc += invocationStride;

                // If we didn't find this output in the result, flag it as an error.
                if (resLoc >= maxLoc)
                {
                    firstFail[lane] = refLoc;
                    log << tcu::TestLog::Message << "lane " << lane << " first mismatch at " << firstFail[lane]
                        << tcu::TestLog::EndMessage;
                    res = QP_TEST_RESULT_FAIL;
                    break;
                }
                refLoc += invocationStride;
                resLoc += invocationStride;
            }
        }

        if (res != QP_TEST_RESULT_PASS)
        {
            for (uint32_t i = 0; i < maxLoc; ++i)
            {
                // This log can be large and slow, ifdef it out by default
#if 0
                log << tcu::TestLog::Message << "result " << i << "(" << (i / invocationStride) << ", " << (i % invocationStride) << "): " << tcu::toHex(result[i]) << " ref " << tcu::toHex(ref[i]) << (i == firstFail[i % invocationStride] ? " first fail" : "") << tcu::TestLog::EndMessage;
#endif
            }
        }
    }

    return res;
}

VkRenderPassBeginInfo ReconvergenceTestGraphicsInstance::makeRenderPassBeginInfo(const VkRenderPass renderPass,
                                                                                 const VkFramebuffer framebuffer)
{
    static const VkClearValue clearValue{{{0u, 0u, 0u, 0u}}};
    return {
        VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, // VkStructureType sType;
        nullptr,                                  // const void* pNext;
        renderPass,                               // VkRenderPass renderPass;
        framebuffer,                              // VkFramebuffer framebuffer;
        makeRect2D(m_data.sizeX, m_data.sizeY),   // VkRect2D renderArea;
        1u,                                       // uint32_t clearValueCount;
        &clearValue                               // const VkClearValue* pClearValues;
    };
}

de::MovePtr<BufferWithMemory> ReconvergenceTestGraphicsInstance::createVertexBufferAndFlush(
    uint32_t cellsHorz, uint32_t cellsVert, VkPrimitiveTopology topology)
{
    uint32_t vertexCount   = cellsHorz * cellsVert;
    uint32_t triangleCount = cellsHorz * cellsVert;
    switch (topology)
    {
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
        vertexCount = triangleCount * 3;
        break;
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
        vertexCount = triangleCount - 1 + 3;
        break;
    case VK_PRIMITIVE_TOPOLOGY_PATCH_LIST:
    case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
        triangleCount = vertexCount - 3 + 1;
        break;
    default:
        DE_ASSERT(0);
    }

    const DeviceInterface &vk            = m_context.getDeviceInterface();
    const VkDevice device                = m_context.getDevice();
    Allocator &allocator                 = m_context.getDefaultAllocator();
    const VkDeviceSize bufferSize        = VkDeviceSize(vertexCount) * sizeof(Vertex);
    const VkBufferUsageFlags bufferUsage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    const VkBufferCreateInfo createInfo  = makeBufferCreateInfo(bufferSize, bufferUsage);
    const MemoryRequirement memoryReqs   = (MemoryRequirement::HostVisible | MemoryRequirement::Coherent);
    de::MovePtr<BufferWithMemory> buffer(new BufferWithMemory(vk, device, allocator, createInfo, memoryReqs));
    Allocation &allocation = buffer->getAllocation();
    Vertex *vertices       = static_cast<Vertex *>(allocation.getHostPtr());

    if (VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST == topology)
    {
        const float stepX = 2.0f / float(cellsHorz);
        const float stepY = 2.0f / float(cellsVert);

        uint32_t t = 0;
        float y    = -1.0f;
        for (uint32_t h = 0; h < cellsVert; ++h)
        {
            float x        = -1.0f;
            const float yy = y + stepY;
            for (uint32_t w = 0; w < cellsHorz; ++w)
            {
                const float xx = x + stepX;

                vertices[t++] = {x, yy, 0.f, 0.f};
                vertices[t++] = {((xx + x) / 2.f), y, 0.f, 0.f};
                vertices[t++] = {xx, ((yy + y) / 2.f), 0.f, 0.f};

                x = xx;
            }
            y = yy;
        }
        DE_ASSERT(vertexCount == t);
    }
    else
    {
        const uint32_t div = static_cast<uint32_t>(ROUNDUP(triangleCount, 2) / 2);
        const float step   = 2.0f / static_cast<float>(div);

        uint32_t t = 0;
        float x    = -1.0f;
        for (uint32_t i = 0; i < div; ++i)
        {
            const bool last   = ((div - i) == 1u);
            const float xNext = last ? +1.0f : (x + step);

            const Vertex v0{x, +1.0f, 0.0f, 0.0f};
            const Vertex v1{xNext, +1.0f, 0.0f, 0.0f};
            const Vertex v2{xNext, -1.0f, 0.0f, 0.0f};
            const Vertex v3{x, -1.0f, 0.0f, 0.0f};

            if (t == 0)
            {
                vertices[0] = v0;
                vertices[1] = v3;
                vertices[2] = v1;

                t = 3;
            }
            else
            {
                vertices[t++] = v1;
            }

            if (!last || !(triangleCount % 2))
            {
                vertices[t++] = v2;
            }

            x += step;
        }
        DE_ASSERT(vertexCount == t);
    }

    flushAlloc(vk, device, allocation);
    return buffer;
}
std::vector<tcu::Vec4> ReconvergenceTestGraphicsInstance::generateVertices(const uint32_t primitiveCount,
                                                                           const VkPrimitiveTopology topology,
                                                                           const uint32_t patchSize)
{
    auto cast     = [](const float f) -> float { return ((f * 2.0f) - 1.0f); };
    auto bestRect = [](const uint32_t c) -> std::pair<uint32_t, uint32_t>
    {
        uint32_t a = 1;
        uint32_t b = 1;
        do
        {
            a = a + 1;
            b = (c / a) + ((c % a) ? 1 : 0);
        } while (a < b);
        return {a, b};
    };

    uint32_t triangleCount = 0;
    uint32_t vertexCount   = 0;
    switch (topology)
    {
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
        triangleCount = primitiveCount;
        vertexCount   = triangleCount + 3 - 1;
        break;
    case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
        triangleCount = primitiveCount;
        vertexCount   = triangleCount * 3;
        break;
    case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
        vertexCount = primitiveCount;
        break;
    case VK_PRIMITIVE_TOPOLOGY_PATCH_LIST:
        vertexCount   = primitiveCount * patchSize;
        triangleCount = ROUNDUP(vertexCount, 3) / 3;
        break;
    default:
        DE_ASSERT(false);
    }

    if (3 == vertexCount)
    {
        return {{-1.0f, +1.0f, 0.0f, 1.0f}, {0.0f, -1.0f, 0.0f, 1.0f}, {+1.0f, +1.0f, 0.0f, 1.0f}};
    }

    std::vector<tcu::Vec4> vertices(vertexCount);

    if (VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP == topology)
    {
        uint32_t v         = 0;
        const uint32_t div = ROUNDUP(triangleCount, 2) / 2;

        for (uint32_t i = 0; i < triangleCount && v < vertexCount; ++i)
        {
            const float xx = cast(float((i / 2) + 1) / float(div));
            if (0 == i)
            {
                const float x = cast(float(i / 2) / float(div));
                vertices[v++] = {x, +1.0f, 0.0f, 1.0f};
                vertices[v++] = {x, -1.0f, 0.0f, 1.0f};
                vertices[v++] = {xx, +1.0f, 0.0f, 1.0f};
            }
            else
            {
                if (i % 2)
                    vertices[v++] = {xx, -1.0f, 0.0f, 1.0f};
                else
                    vertices[v++] = {xx, +1.0f, 0.0f, 1.0f};
            }
        }
        DE_ASSERT(vertexCount == v);
    }
    else if (VK_PRIMITIVE_TOPOLOGY_POINT_LIST == topology)
    {
        uint32_t v      = 0;
        const auto rect = bestRect(vertexCount);

        float y = -1.0f;
        for (uint32_t h = 0; h < rect.second; ++h)
        {
            const float yy = cast(float(h + 1) / float(rect.second));
            float x        = -1.0f;
            for (uint32_t w = 0; w < rect.first && v < vertexCount; ++w)
            {
                const float xx = cast(float(w + 1) / float(rect.first));
                vertices[v++]  = {((xx - x) / 2.0f), ((yy - y) / 2.0f), 0.0f, 1.0f};
                x              = xx;
            }
            y = yy;
        }
        DE_ASSERT(vertexCount == v);
    }
    else if (VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST == topology || VK_PRIMITIVE_TOPOLOGY_PATCH_LIST == topology)
    {
        uint32_t v      = 0;
        const auto rect = bestRect(triangleCount);

        float y = -1.0f;
        for (uint32_t h = 0; h < rect.second && v < vertexCount; ++h)
        {
            const float yy = cast(float(h + 1) / float(rect.second));
            float x        = -1.0f;
            for (uint32_t w = 0; w < rect.first && v < vertexCount; ++w)
            {
                const float xx = cast(float(w + 1) / float(rect.first));
                if (v < vertexCount)
                    vertices[v++] = {x, yy, 0.f, 0.f};
                if (v < vertexCount)
                    vertices[v++] = {((xx + x) / 2.f), y, 0.f, 0.f};
                if (v < vertexCount)
                    vertices[v++] = {xx, ((yy + y) / 2.f), 0.f, 0.f};
                x = xx;
            }
            y = yy;
        }
        DE_ASSERT(vertexCount == v);
    }

    return vertices;
}

de::MovePtr<BufferWithMemory> ReconvergenceTestGraphicsInstance::createVertexBufferAndFlush(
    const std::vector<tcu::Vec4> &vertices)
{
    const DeviceInterface &vk            = m_context.getDeviceInterface();
    const VkDevice device                = m_context.getDevice();
    Allocator &allocator                 = m_context.getDefaultAllocator();
    const VkDeviceSize bufferSize        = VkDeviceSize(vertices.size()) * sizeof(tcu::Vec4);
    const VkBufferUsageFlags bufferUsage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    const VkBufferCreateInfo createInfo  = makeBufferCreateInfo(bufferSize, bufferUsage);
    const MemoryRequirement memoryReqs   = (MemoryRequirement::HostVisible | MemoryRequirement::Coherent);
    de::MovePtr<BufferWithMemory> buffer(new BufferWithMemory(vk, device, allocator, createInfo, memoryReqs));
    Allocation &allocation = buffer->getAllocation();
    auto bufferRange       = makeStdBeginEnd<tcu::Vec4>(allocation.getHostPtr(), (uint32_t)vertices.size());
    std::copy(vertices.begin(), vertices.end(), bufferRange.first);
    flushAlloc(vk, device, allocation);
    return buffer;
}

void ReconvergenceTestGraphicsInstance::recordDrawingAndSubmit(
    const VkCommandBuffer cmdBuffer, const VkPipelineLayout pipelineLayout, const VkPipeline pipeline,
    const VkDescriptorSet descriptorSet, const PushConstant &pushConstant, const VkRenderPassBeginInfo &renderPassInfo,
    const VkBuffer vertexBuffer, const uint32_t vertexCount, const VkImage image)
{
    DE_UNREF(image);
    const DeviceInterface &vk           = m_context.getDeviceInterface();
    const VkDevice device               = m_context.getDevice();
    const VkQueue queue                 = m_context.getUniversalQueue();
    const VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    beginCommandBuffer(vk, cmdBuffer, 0u);
    vk.cmdBindDescriptorSets(cmdBuffer, bindPoint, pipelineLayout, 0u, 1u, &descriptorSet, 0u, DE_NULL);
    vk.cmdBindPipeline(cmdBuffer, bindPoint, pipeline);
    vk.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &static_cast<const VkBuffer &>(vertexBuffer),
                            &static_cast<const VkDeviceSize &>(0u));
    vk.cmdPushConstants(cmdBuffer, pipelineLayout, m_data.shaderStage, 0, sizeof(PushConstant), &pushConstant);
    vk.cmdBeginRenderPass(cmdBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vk.cmdDraw(cmdBuffer, vertexCount, 1u, 0u, 0u);
    vk.cmdEndRenderPass(cmdBuffer);
    endCommandBuffer(vk, cmdBuffer);

    submitCommandsAndWait(vk, device, queue, cmdBuffer);
}

std::vector<Move<VkShaderModule>> ReconvergenceTestFragmentInstance::createShaders(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();

    Move<VkShaderModule> vertex   = createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0);
    Move<VkShaderModule> fragment = createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0);

    // { #vert, #frag, tesc, tese, geom }; if any
    std::vector<Move<VkShaderModule>> shaders;
    shaders.emplace_back(vertex);
    shaders.emplace_back(fragment);

    return shaders;
}

qpTestResult_e ReconvergenceTestGraphicsInstance::calculateAndLogResult(const uint64_t *result,
                                                                        const std::vector<uint64_t> &ref,
                                                                        uint32_t invocationStride,
                                                                        uint32_t subgroupSize, uint32_t shaderMaxLocs,
                                                                        uint32_t primitiveCount, PrintMode printMode)
{
    DE_ASSERT(m_data.testType == TT_MAXIMAL);

    const uint32_t maxLoc  = static_cast<uint32_t>(ref.size());
    tcu::TestLog &log      = m_context.getTestContext().getLog();
    qpTestResult res       = QP_TEST_RESULT_PASS;
    uint32_t mismatchCount = 0;

    DE_ASSERT(shaderMaxLocs * invocationStride <= maxLoc);

    // With maximal reconvergence, we should expect the output to exactly match
    // the reference.
    for (uint32_t i = 0; i < maxLoc; ++i)
    {
        const uint64_t resultVal = result[i];
        const uint64_t refVal    = ref[i];
        if (resultVal != refVal)
        {
            if (1 > mismatchCount++)
            {
                log << tcu::TestLog::Message << mismatchCount << ": Mismatch at " << i
                    << ", res: " << tcu::toHex(resultVal) << ", ref: " << tcu::toHex(refVal)
                    << tcu::TestLog::EndMessage;
            }
        }
    }

    if (PrintMode::None != printMode)
    {
        log << tcu::TestLog::Message << "deviceSubgroupSize: " << m_subgroupSize
            << ", testSubgroupSize: " << subgroupSize << ", invocationStride: " << invocationStride
            << ", shaderMaxLocs: " << shaderMaxLocs << "\n\t, framebuffer: " << m_data.sizeX << 'x' << m_data.sizeY
            << ", primitiveCount: " << primitiveCount << ", PRINT_MODE: "
            << ((PrintMode::ThreadsInColumns == printMode) ?
                    "\"ouLocs in rows & threads in columns\"" :
                    ((PrintMode::OutLocsInColumns == printMode) ? "\"threads in rows & outLocs in columns\"" : ""))
            << " { id:res,ref }\n"
            << tcu::TestLog::EndMessage;
    }

    uint32_t invMax = std::min(invocationStride, 80u);

    if (PrintMode::ThreadsInColumns == printMode)
    {
        for (uint32_t loc = 0; loc < shaderMaxLocs; ++loc)
        {
            auto l1 = log << tcu::TestLog::Message;
            l1 << "loc " << std::setw(3) << loc << ": ";
            for (uint32_t inv = 0; inv < invMax; ++inv)
            {
                uint32_t idx = loc * invocationStride + inv;
                DE_ASSERT(idx < maxLoc);
                uint64_t resEntry = result[idx];
                uint64_t refEntry = ref[idx];
                //l1 << de::toString(inv) << ':' << tcu::toHex(resEntry) << ',' << tcu::toHex(refEntry) << ' ';
                l1 << std::dec << inv << ':' << std::setw(subgroupSize / 4) << std::hex << resEntry << ','
                   << std::setw(subgroupSize / 4) << std::hex << refEntry << std::dec << ' ';
            }
            l1 << std::setw(0) << tcu::TestLog::EndMessage;
        }
    }
    else if (PrintMode::OutLocsInColumns == printMode)
    {
        for (uint32_t inv = 0; inv < invMax; ++inv)
        {
            auto l1 = log << tcu::TestLog::Message;
            l1 << "res " << std::setw(3) << inv << ": ";
            for (uint32_t loc = 0; loc < shaderMaxLocs; ++loc)
            {
                uint32_t idx = loc * invocationStride + inv;
                DE_ASSERT(idx < maxLoc);
                uint64_t entry = result[idx];
                l1 << de::toString(loc) << ':' << tcu::toHex(entry) << ' ';
            }
            l1 << std::setw(0) << tcu::TestLog::EndMessage;

            auto l2 = log << tcu::TestLog::Message;
            l2 << "ref " << std::setw(3) << inv << ": ";
            for (uint32_t loc = 0; loc < shaderMaxLocs; ++loc)
            {
                uint32_t idx = loc * invocationStride + inv;
                DE_ASSERT(idx < maxLoc);
                uint64_t entry = ref[idx];
                l2 << de::toString(loc) << ':' << tcu::toHex(entry) << ' ';
            }
            l2 << std::setw(0) << tcu::TestLog::EndMessage;
        }
    }

    if (mismatchCount)
    {
        double mismatchPercentage = 0.0;
        std::modf((double)(mismatchCount * 100) / (double)maxLoc, &mismatchPercentage);
        log << tcu::TestLog::Message << "Mismatch count " << mismatchCount << " from " << maxLoc << " ("
            << mismatchPercentage << "%)" << tcu::TestLog::EndMessage;
        res = QP_TEST_RESULT_FAIL;
    }

    if (res != QP_TEST_RESULT_PASS)
    {
        for (uint32_t i = 0; i < maxLoc; ++i)
        {
            // This log can be large and slow, ifdef it out by default
#if 0
            log << tcu::TestLog::Message << "result " << i << "(" << (i / invocationStride) << ", " << (i % invocationStride) << "): " << tcu::toHex(result[i]) << " ref " << tcu::toHex(ref[i]) << (result[i] != ref[i] ? " different" : "") << tcu::TestLog::EndMessage;
#endif
        }
    }

    return res;
}

qpTestResult_e ReconvergenceTestFragmentInstance::calculateAndLogResultEx(tcu::TestLog &log, const tcu::UVec4 *result,
                                                                          const std::vector<tcu::UVec4> &ref,
                                                                          const uint32_t maxLoc, const Arrangement &a,
                                                                          const PrintMode printMode)
{
    DE_UNREF(printMode);

    qpTestResult res                             = QP_TEST_RESULT_PASS;
    uint32_t mismatchCount                       = 0u;
    const uint32_t printMismatchCount            = 5u;
    const FragmentRandomProgram::Arrangement &aa = static_cast<const FragmentRandomProgram::Arrangement &>(a);

    // With maximal reconvergence, we should expect the output to exactly match
    // the reference.
    const uint32_t ballotStoreCount = maxLoc * aa.m_invocationStride * aa.m_primitiveStride;
    for (uint32_t i = 0; i < ballotStoreCount; ++i)
    {
        const Ballot resultVal(result[i], aa.m_subgroupSize);
        ;
        const Ballot refVal(ref[i], aa.m_subgroupSize);
        if (resultVal != refVal)
        {
            if (mismatchCount++ < printMismatchCount)
            {
                res = QP_TEST_RESULT_FAIL;
                log << tcu::TestLog::Message << "Mismatch at " << i << "\nexpected: " << resultVal
                    << "\n     got: " << refVal << tcu::TestLog::EndMessage;
                if (printMode == PrintMode::Console)
                {
                    std::cout << "Mismatch at " << i << "\nexpected: " << resultVal << "\n     got: " << refVal
                              << std::endl;
                }
            }
        }
    }

    log << tcu::TestLog::Message << "Mismatch count: " << mismatchCount << " from " << ballotStoreCount
        << tcu::TestLog::EndMessage;
    if (printMode == PrintMode::Console)
    {
        std::cout << "Mismatch count: " << mismatchCount << " from " << ballotStoreCount << std::endl;
    }

    return res;
}

VkImageCreateInfo ReconvergenceTestFragmentInstance::makeImageCreateInfo(VkFormat format) const
{
    return {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        VkImageCreateFlags(0),               // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
        format,                              // VkFormat format;
        {m_data.sizeX, m_data.sizeY, 1u},    // VkExtent3D extent;
        1u,                                  // uint32_t mipLevels;
        1u,                                  // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        0u,                                  // uint32_t queueFamilyIndexCount;
        0u,                                  // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED            // VkImageLayout initialLayout;
    };
}

de::MovePtr<BufferWithMemory> ReconvergenceTestFragmentInstance::createVertexBufferAndFlush(
    uint32_t cellsHorz, uint32_t cellsVert, VkPrimitiveTopology topology)
{
    // DE_ASSERT(cellsHorz == 2u);
    // DE_ASSERT((cellsHorz * 3) == cellsVert);
    DE_UNREF(cellsHorz);
    DE_UNREF(cellsVert);
    DE_UNREF(topology);
    DE_ASSERT(topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    const std::vector<tcu::Vec4> vertices{{-1.0f, 0.0f, 0.0f, 0.0f},  {-0.5f, -1.0f, 0.0f, 0.0f},
                                          {+1.0f, +1.0f, 0.0f, 0.0f}, {+0.5f, -1.0f, 0.0f, 0.0f},
                                          {+1.0f, 0.0f, 0.0f, 0.0f},  {-1.0f, +1.0f, 0.0f, 0.0f}};
    return ReconvergenceTestGraphicsInstance::createVertexBufferAndFlush(vertices);
}

std::vector<uint32_t> ReconvergenceTestFragmentInstance::callAuxiliaryShader(tcu::TestStatus &status,
                                                                             uint32_t triangleCount)
{
    const DeviceInterface &vk    = m_context.getDeviceInterface();
    const VkDevice device        = m_context.getDevice();
    add_ref<Allocator> allocator = m_context.getDefaultAllocator();
    const uint32_t queueIndex    = m_context.getUniversalQueueFamilyIndex();
    //add_ref<tcu::TestLog> log = m_context.getTestContext().getLog();
    const uint32_t bufferElems    = m_data.sizeX * m_data.sizeY * triangleCount + 3u;
    const VkDeviceSize bufferSize = bufferElems * sizeof(uint32_t);

    if (bufferSize > m_context.getDeviceProperties().limits.maxStorageBufferRange)
        TCU_THROW(NotSupportedError, "Storage buffer size larger than device limits");

    const VkBufferCreateInfo createInfo =
        vk::makeBufferCreateInfo(bufferSize, (VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                              VK_BUFFER_USAGE_TRANSFER_SRC_BIT));
    de::MovePtr<BufferWithMemory> buffer;
    try
    {
        buffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
            vk, device, allocator, createInfo, (MemoryRequirement::HostVisible | MemoryRequirement::Coherent)));
    }
    catch (tcu::ResourceError &)
    {
        // Allocation size is unpredictable and can be too large for some systems. Don't treat allocation failure as a test failure.
        status = tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING,
                                 "Failed device memory allocation " + de::toString(bufferSize) + " bytes");
        return {};
    }

    const VkDescriptorBufferInfo bufferInfo = makeDescriptorBufferInfo(**buffer, 0, bufferSize);

    vk::DescriptorSetLayoutBuilder layoutBuilder;
    layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
    vk::Unique<vk::VkDescriptorSetLayout> descriptorSetLayout(layoutBuilder.build(vk, device));

    vk::DescriptorPoolBuilder poolBuilder;
    poolBuilder.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u);
    vk::Unique<vk::VkDescriptorPool> descriptorPool(
        poolBuilder.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

    vk::Unique<vk::VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

    vk::DescriptorSetUpdateBuilder setUpdateBuilder;
    setUpdateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0),
                                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferInfo);
    setUpdateBuilder.update(vk, device);

    const VkPushConstantRange pushConstantRange{
        VK_SHADER_STAGE_FRAGMENT_BIT, // VkShaderStageFlags stageFlags;
        0u,                           // uint32_t offset;
        sizeof(PushConstant)          // uint32_t size;
    };
    const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // sType
        DE_NULL,                                       // pNext
        (VkPipelineLayoutCreateFlags)0,                // flags
        1u,                                            // setLayoutCount
        &descriptorSetLayout.get(),                    // pSetLayouts
        1u,                                            // pushConstantRangeCount
        &pushConstantRange,                            // pPushConstantRanges
    };

    const VkFormat format                   = VK_FORMAT_R8G8B8A8_UNORM;
    const VkImageCreateInfo imageCreateInfo = makeImageCreateInfo(format);
    const VkImageSubresourceRange rscRange  = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    de::MovePtr<ImageWithMemory> image(
        new ImageWithMemory(vk, device, allocator, imageCreateInfo, vk::MemoryRequirement::Any));
    Move<VkImageView> view        = makeImageView(vk, device, **image, VK_IMAGE_VIEW_TYPE_2D, format, rscRange);
    Move<VkRenderPass> renderPass = makeRenderPass(vk, device, format);
    Move<VkFramebuffer> framebuffer =
        makeFramebuffer(vk, device, *renderPass, *view, m_data.sizeX, m_data.sizeY, rscRange.layerCount);
    const VkRenderPassBeginInfo renderBeginInfo = makeRenderPassBeginInfo(*renderPass, *framebuffer);
    auto createAuxShaders                       = [&]()
    {
        Shaders shaders;
        auto vert = createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0);
        auto frag = createShaderModule(vk, device, m_context.getBinaryCollection().get("aux"), 0);
        shaders.emplace_back(vert);
        shaders.emplace_back(frag);
        return shaders;
    };
    const Shaders shaders      = createAuxShaders();
    const uint32_t vertexCount = triangleCount * 3u;
    de::MovePtr<BufferWithMemory> vertexBuffer =
        createVertexBufferAndFlush(triangleCount, vertexCount, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    Move<VkPipelineLayout> pipelineLayout = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo, NULL);
    Move<VkPipeline> pipeline = createGraphicsPipeline(*pipelineLayout, *renderPass, m_data.sizeX, m_data.sizeY,
                                                       shaders, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0U);
    Move<VkCommandPool> cmdPool =
        createCommandPool(vk, device, vk::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueIndex);
    Move<VkCommandBuffer> cmdBuffer = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    PushConstant pc{};
    pc.invocationStride = 0u;
    pc.width            = m_data.sizeX;
    pc.height           = m_data.sizeY;
    pc.primitiveStride  = triangleCount;

    void *ptr        = buffer->getAllocation().getHostPtr();
    auto bufferRange = makeStdBeginEnd<uint32_t>(ptr, bufferElems);
    std::fill(bufferRange.first, bufferRange.second, 0u);

    std::bind(&ReconvergenceTestGraphicsInstance::recordDrawingAndSubmit, this, *cmdBuffer, *pipelineLayout, *pipeline,
              *descriptorSet, std::cref(pc), std::cref(renderBeginInfo), **vertexBuffer, vertexCount, **image)();

    status = tcu::TestStatus::pass(std::string());
    return std::vector<uint32_t>(bufferRange.first, bufferRange.second);
}

tcu::TestStatus ReconvergenceTestFragmentInstance::iterate(void)
{
    const DeviceInterface &vk            = m_context.getDeviceInterface();
    const VkDevice device                = m_context.getDevice();
    add_ref<Allocator> allocator         = m_context.getDefaultAllocator();
    const uint32_t queueIndex            = m_context.getUniversalQueueFamilyIndex();
    add_ref<tcu::TestLog> log            = m_context.getTestContext().getLog();
    const VkPhysicalDeviceLimits &limits = m_context.getDeviceProperties().limits;
    const uint32_t fragmentStride        = m_data.sizeX * m_data.sizeY;
    const uint32_t primitiveStride       = 2;

    if (sizeof(PushConstant) > limits.maxPushConstantsSize)
    {
        return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING,
                               "PushConstant size " + std::to_string(sizeof(PushConstant)) + " exceeds device limit " +
                                   std::to_string(limits.maxPushConstantsSize));
    }

    tcu::TestStatus auxStatus(QP_TEST_RESULT_FAIL, std::string());
    std::vector<uint32_t> primitiveMap = callAuxiliaryShader(auxStatus, primitiveStride);
    if (auxStatus.isFail())
        return auxStatus;

    const uint32_t shaderSubgroupSize = primitiveMap.at(fragmentStride * primitiveStride + 1u);
    if (shaderSubgroupSize != m_subgroupSize)
    {
        return tcu::TestStatus(QP_TEST_RESULT_FAIL,
                               "The size of the subgroup from the shader (" + std::to_string(shaderSubgroupSize) +
                                   ") is different from the size of the subgroup from the device (" +
                                   std::to_string(m_subgroupSize) + ")");
    }
    const uint32_t shaderSubgroupStride = primitiveMap.at(fragmentStride * primitiveStride + 0u);
    const uint32_t hostSubgroupStride =
        FragmentRandomProgram::Arrangement::calcSubgroupCount(primitiveMap, primitiveStride, fragmentStride);
    if (shaderSubgroupStride != hostSubgroupStride)
    {
        return tcu::TestStatus(QP_TEST_RESULT_FAIL,
                               "The number of subgroups from the shader (" + std::to_string(shaderSubgroupStride) +
                                   ") is different from the number of subgroups calculated manually (" +
                                   std::to_string(hostSubgroupStride) + ")");
    }

    log << tcu::TestLog::Message << "Subgroup count: " << hostSubgroupStride << tcu::TestLog::EndMessage;
    log << tcu::TestLog::Message << "Subgroup size: " << m_subgroupSize << tcu::TestLog::EndMessage;

    const VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    de::MovePtr<BufferWithMemory> vertexBuffer =
        createVertexBufferAndFlush(primitiveStride, (primitiveStride * 3u), topology);

    std::vector<tcu::UVec4> ref;
    de::MovePtr<FragmentRandomProgram> program = FragmentRandomProgram::create(m_data);
    program->generateRandomProgram(m_context.getTestContext().getWatchDog(), log);

    const uint32_t simulationMaxLoc = program->execute(m_context.getTestContext().getWatchDog(), true, m_subgroupSize,
                                                       fragmentStride, primitiveStride, ref, log, primitiveMap);
    log << tcu::TestLog::Message << "simulated maxLoc: " << simulationMaxLoc << tcu::TestLog::EndMessage;
    // maxLoc is per-invocation. Add one (to make sure no additional writes are done)
    uint32_t maxLoc = simulationMaxLoc;
    maxLoc += 1;
    maxLoc *= (hostSubgroupStride * 128u * primitiveStride);

    constexpr uint32_t bufferCount = 4;
    enum Bindings
    {
        InputA,
        OutputBallots,
        OutputCounts,
        OutputPriMap
    };

    de::MovePtr<BufferWithMemory> buffers[bufferCount];
    vk::VkDescriptorBufferInfo bufferDescriptors[bufferCount];

    VkDeviceSize sizes[bufferCount]{
        // InputA  { uint    a[]; } inputA;  filled with a[i] := i
        (FragmentRandomProgram::conditionIfInvocationStride + 2) * sizeof(uint32_t),

        // OutputB { uvec4   b[]; } outputB;
        maxLoc * sizeof(tcu::UVec4),

        // OutputC { uint loc[]; } outputC;
        (hostSubgroupStride * 128u * primitiveStride) * sizeof(uint32_t),

        // OutputP { uvec   p[]; } outputP; few more for calculating subgroupID, subgroupSize, non-helper and helperinvocations
        (fragmentStride * primitiveStride + 16u) * sizeof(uint32_t)};

    VkBufferUsageFlags usages[bufferCount]{
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
    };

    // allocate buffers
    for (uint32_t i = 0; i < bufferCount; ++i)
    {
        if (sizes[i] > limits.maxStorageBufferRange)
            TCU_THROW(NotSupportedError, "Storage buffer size larger than device limits");

        try
        {
            buffers[i] = de::MovePtr<BufferWithMemory>(
                new BufferWithMemory(vk, device, allocator,
                                     makeBufferCreateInfo(sizes[i], usages[i] | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                                        VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
                                     MemoryRequirement::HostVisible | MemoryRequirement::Cached));
        }
        catch (tcu::ResourceError &)
        {
            // Allocation size is unpredictable and can be too large for some systems. Don't treat allocation failure as a test failure.
            return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING,
                                   "Failed device memory allocation " + de::toString(sizes[i]) + " bytes");
        }
        bufferDescriptors[i] = makeDescriptorBufferInfo(**buffers[i], 0, sizes[i]);
    }

    // get raw pointers to previously allocated buffers
    void *ptrs[bufferCount];
    for (uint32_t i = 0; i < bufferCount; ++i)
    {
        ptrs[i] = buffers[i]->getAllocation().getHostPtr();
    }

    // populate buffers with their destination
    {
        auto rangeBufferA =
            makeStdBeginEnd<uint32_t>(ptrs[InputA], static_cast<uint32_t>(sizes[InputA] / sizeof(uint32_t)));
        std::iota(rangeBufferA.first, rangeBufferA.second, 0u);
    }
    deMemset(ptrs[OutputBallots], 0, (size_t)sizes[OutputBallots]);
    deMemset(ptrs[OutputCounts], 0, (size_t)sizes[OutputCounts]);
    deMemset(ptrs[OutputPriMap], 0, (size_t)sizes[OutputPriMap]);

    // (...) and flush them to the GPU
    for (uint32_t i = 0; i < bufferCount; ++i)
    {
        flushAlloc(vk, device, buffers[i]->getAllocation());
    }

    VkDescriptorType descTypes[bufferCount]{
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    };

    vk::DescriptorSetLayoutBuilder layoutBuilder;
    for (uint32_t i = 0; i < bufferCount; ++i)
    {
        layoutBuilder.addSingleBinding(descTypes[i], m_data.shaderStage);
    }
    vk::Unique<vk::VkDescriptorSetLayout> descriptorSetLayout(layoutBuilder.build(vk, device));

    vk::DescriptorPoolBuilder poolBuilder;
    for (uint32_t i = 0; i < bufferCount; ++i)
    {
        poolBuilder.addType(descTypes[i], 1);
    }
    vk::Unique<vk::VkDescriptorPool> descriptorPool(
        poolBuilder.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));
    vk::Unique<vk::VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

    vk::DescriptorSetUpdateBuilder setUpdateBuilder;
    for (uint32_t i = 0; i < bufferCount; ++i)
    {
        setUpdateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(i), descTypes[i],
                                     &bufferDescriptors[i]);
    }
    setUpdateBuilder.update(vk, device);

    const VkPushConstantRange pushConstantRange{
        (VkShaderStageFlags)m_data.shaderStage, // VkShaderStageFlags stageFlags;
        0u,                                     // uint32_t offset;
        sizeof(PushConstant)                    // uint32_t size;
    };

    const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // sType
        DE_NULL,                                       // pNext
        (VkPipelineLayoutCreateFlags)0,                // flags
        1u,                                            // setLayoutCount
        &descriptorSetLayout.get(),                    // pSetLayouts
        1u,                                            // pushConstantRangeCount
        &pushConstantRange,                            // pPushConstantRanges
    };

    const VkFormat format                   = VK_FORMAT_R8G8B8A8_UNORM;
    const VkImageCreateInfo imageCreateInfo = makeImageCreateInfo(format);
    const VkImageSubresourceRange rscRange  = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    de::MovePtr<ImageWithMemory> image(
        new ImageWithMemory(vk, device, allocator, imageCreateInfo, vk::MemoryRequirement::Any));
    Move<VkImageView> view        = makeImageView(vk, device, **image, VK_IMAGE_VIEW_TYPE_2D, format, rscRange);
    Move<VkRenderPass> renderPass = makeRenderPass(vk, device, format);
    Move<VkFramebuffer> framebuffer =
        makeFramebuffer(vk, device, *renderPass, *view, m_data.sizeX, m_data.sizeY, rscRange.layerCount);
    const VkRenderPassBeginInfo renderBeginInfo = makeRenderPassBeginInfo(*renderPass, *framebuffer);
    const Shaders shaders                       = createShaders();
    Move<VkPipelineLayout> pipelineLayout       = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo, NULL);
    Move<VkPipeline> pipeline =
        createGraphicsPipeline(*pipelineLayout, *renderPass, m_data.sizeX, m_data.sizeY, shaders, topology, 0U);
    Move<VkCommandPool> cmdPool =
        createCommandPool(vk, device, vk::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueIndex);
    Move<VkCommandBuffer> cmdBuffer = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    PushConstant pc{};
    pc.width                 = m_data.sizeX;
    pc.height                = m_data.sizeY;
    pc.primitiveStride       = primitiveStride;
    pc.invocationStride      = 0u;
    pc.subgroupStride        = hostSubgroupStride;
    pc.enableInvocationIndex = VK_FALSE;

    auto callRecordDrawingAndSubmit = std::bind(
        &ReconvergenceTestGraphicsInstance::recordDrawingAndSubmit, this, *cmdBuffer, *pipelineLayout, *pipeline,
        *descriptorSet, std::cref(pc), std::cref(renderBeginInfo), **vertexBuffer, (primitiveStride * 3u), **image);

    // compute "maxLoc", which is a potential maximum number of locations written
    callRecordDrawingAndSubmit();

    // Take the maximum of "maxLoc" over all invocations.
    invalidateAlloc(vk, device, buffers[OutputCounts]->getAllocation());
    auto rangeLoc = makeStdBeginEnd<const uint32_t>(ptrs[OutputCounts], (hostSubgroupStride * 128u * primitiveStride));
    const uint32_t computedShaderMaxLoc = *max_element(rangeLoc.first, rangeLoc.second);
    log << tcu::TestLog::Message << "Computed maxLoc in the shader: " << computedShaderMaxLoc
        << tcu::TestLog::EndMessage;

    if (computedShaderMaxLoc >= FragmentRandomProgram::experimentalOutLocSize)
    {
        return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING,
                               "Calculated maxLoc from a shader (which is " + de::toString(computedShaderMaxLoc) +
                                   ") "
                                   "exceeds BALLOT_STACK_SIZE (which is " +
                                   de::toString(FragmentRandomProgram::experimentalOutLocSize) +
                                   ").\n"
                                   "To repair this just increment slightly a " MAKETEXT(
                                       FragmentRandomProgram::experimentalOutLocSize) " "
                                                                                      "in line " +
                                   de::toString(BALLOT_STACK_SIZE_DEFVAL_LINE));
    }

    // If we need more space, reallocate OutputB::b[]
    if (computedShaderMaxLoc != simulationMaxLoc)
    {
        // Add one (to make sure no additional writes are done) and multiply by
        // the number of invocations and current primitive count
        maxLoc = (std::max(computedShaderMaxLoc, simulationMaxLoc) + 1) * (hostSubgroupStride * 128u * primitiveStride);
        sizes[OutputBallots] = maxLoc * sizeof(tcu::UVec4);

        if (sizes[OutputBallots] > limits.maxStorageBufferRange)
            TCU_THROW(NotSupportedError, "Storage buffer size larger than device limits");

        try
        {
            buffers[OutputBallots] = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
                vk, device, allocator,
                makeBufferCreateInfo(sizes[OutputBallots], usages[OutputBallots] | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                               VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
                MemoryRequirement::HostVisible | MemoryRequirement::Cached));
        }
        catch (tcu::ResourceError &)
        {
            // Allocation size is unpredictable and can be too large for some systems. Don't treat allocation failure as a test failure.
            return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING,
                                   "Failed device memory allocation " + de::toString(sizes[OutputBallots]) + " bytes");
        }
        bufferDescriptors[OutputBallots] = makeDescriptorBufferInfo(**buffers[OutputBallots], 0, sizes[OutputBallots]);
        ptrs[OutputBallots]              = buffers[OutputBallots]->getAllocation().getHostPtr();

        vk::DescriptorSetUpdateBuilder setUpdateBuilder2;
        setUpdateBuilder2.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(OutputBallots),
                                      VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptors[OutputBallots]);
        setUpdateBuilder2.update(vk, device);
    }

    // Clear any writes to ballots/stores OutputB::b[] aka buffer[OutputBallots] during the counting pass
    // Note that its size would may change since the first memory allocation
    deMemset(ptrs[OutputBallots], 0, (size_t)sizes[OutputBallots]);
    // Clear any writes to counting OutputC::loc[] aka buffer[OutputCounts] during the counting pass
    deMemset(ptrs[OutputCounts], 0, (size_t)sizes[OutputCounts]);
    // Clear any writes to counting OutputP::p[] aka buffer[OutputPriMap] during the counting pass
    deMemset(ptrs[OutputPriMap], 0, (size_t)sizes[OutputPriMap]);

    // flush them all to the GPU
    flushAlloc(vk, device, buffers[OutputBallots]->getAllocation());
    flushAlloc(vk, device, buffers[OutputCounts]->getAllocation());
    flushAlloc(vk, device, buffers[OutputPriMap]->getAllocation());

    // run the actual shader with updated PushConstant
    pc.enableInvocationIndex = VK_TRUE;
    callRecordDrawingAndSubmit();

    invalidateAlloc(vk, device, buffers[OutputCounts]->getAllocation());
    invalidateAlloc(vk, device, buffers[OutputBallots]->getAllocation());
    invalidateAlloc(vk, device, buffers[OutputPriMap]->getAllocation());

    // Simulate execution on the CPU, and compare against the GPU result
    try
    {
        ref.resize(maxLoc, tcu::UVec4());
    }
    catch (const std::bad_alloc &)
    {
        // Allocation size is unpredictable and can be too large for some systems. Don't treat allocation failure as a test failure.
        return tcu::TestStatus(QP_TEST_RESULT_NOT_SUPPORTED,
                               "Failed system memory allocation " + de::toString(maxLoc * sizeof(uint64_t)) + " bytes");
    }

    std::fill(primitiveMap.begin(), primitiveMap.end(), 0);
    auto primitiveMapRange = makeStdBeginEnd<const uint32_t>(ptrs[OutputPriMap], (fragmentStride * primitiveStride));
    std::copy(primitiveMapRange.first, primitiveMapRange.second, primitiveMap.begin());

    const FragmentRandomProgram::Arrangement a(primitiveMap, m_data.sizeX, m_data.sizeY, m_subgroupSize,
                                               primitiveStride);
    const tcu::UVec4 *ballots = static_cast<tcu::UVec4 *>(ptrs[OutputBallots]);

    program->execute(m_context.getTestContext().getWatchDog(), false, m_subgroupSize, fragmentStride, primitiveStride,
                     ref, log, primitiveMap, ballots);

    const uint32_t finalMaxLoc = std::max(computedShaderMaxLoc, simulationMaxLoc);
    const qpTestResult res     = calculateAndLogResultEx(log, ballots, ref, finalMaxLoc, a, PrintMode::None);

    return tcu::TestStatus(res, qpGetTestResultName(res));
}

de::MovePtr<BufferWithMemory> ReconvergenceTestVertexInstance::createVertexBufferAndFlush(uint32_t cellsHorz,
                                                                                          uint32_t cellsVert,
                                                                                          VkPrimitiveTopology topology)
{
    DE_UNREF(topology);
    DE_ASSERT(VK_PRIMITIVE_TOPOLOGY_POINT_LIST == topology);
    const std::vector<tcu::Vec4> vertices =
        VertexRandomProgram::Arrangement::generatePrimitives(cellsHorz, cellsVert, VertexRandomProgram::fillPercentage);
    return ReconvergenceTestGraphicsInstance::createVertexBufferAndFlush(vertices);
}

std::vector<Move<VkShaderModule>> ReconvergenceTestVertexInstance::createShaders(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();

    Move<VkShaderModule> vertex   = createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0);
    Move<VkShaderModule> fragment = createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"), 0);

    // { #vert, #frag, #tesc, tese, geom }; if any
    std::vector<Move<VkShaderModule>> shaders;
    shaders.emplace_back(vertex);
    shaders.emplace_back(fragment);

    return shaders;
}

tcu::TestStatus ReconvergenceTestVertexInstance::iterate(void)
{
    const VkPhysicalDeviceLimits &limits = m_context.getDeviceProperties().limits;
    if (sizeof(PushConstant) > limits.maxPushConstantsSize)
    {
        return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING,
                               "PushConstant size " + std::to_string(sizeof(PushConstant)) + " exceeds device limit " +
                                   std::to_string(limits.maxPushConstantsSize));
    }

    const DeviceInterface &vk          = m_context.getDeviceInterface();
    const VkDevice device              = m_context.getDevice();
    Allocator &allocator               = m_context.getDefaultAllocator();
    const uint32_t queueIndex          = m_context.getUniversalQueueFamilyIndex();
    add_ref<tcu::TestLog> log          = m_context.getTestContext().getLog();
    const VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    const uint32_t fragmentStride      = uint32_t(m_data.sizeX * m_data.sizeY);
    const uint32_t invocationStride =
        static_cast<uint32_t>(VertexRandomProgram::Arrangement::generatePrimitives(m_data.sizeX, m_data.sizeY,
                                                                                   VertexRandomProgram::fillPercentage)
                                  .size());

    de::MovePtr<VertexRandomProgram> program(new VertexRandomProgram(m_data));
    program->generateRandomProgram(m_context.getTestContext().getWatchDog(), log);

    // simulate content of outputP buffer
    std::vector<uint32_t> outputP =
        VertexRandomProgram::Arrangement::generateOutputPvector(m_subgroupSize, invocationStride);

    std::vector<tcu::UVec4> ref;
    const uint32_t hostMaxLoc = program->execute(m_context.getTestContext().getWatchDog(), true, m_subgroupSize,
                                                 fragmentStride, invocationStride, ref, log, outputP, nullptr);
    log << tcu::TestLog::Message << "Rendering area  : " << tcu::UVec2(m_data.sizeX, m_data.sizeY)
        << tcu::TestLog::EndMessage;
    log << tcu::TestLog::Message << "invocationStride: " << invocationStride << tcu::TestLog::EndMessage;
    log << tcu::TestLog::Message << "Simulated maxLoc: " << hostMaxLoc << tcu::TestLog::EndMessage;
    // maxLoc is per-invocation. Add one (to make sure no additional writes are done).
    uint32_t maxLoc = hostMaxLoc;
    maxLoc += 1;
    maxLoc *= invocationStride;

    constexpr uint32_t bufferCount = 4u;
    enum Bindings
    {
        InputA,
        OutputBallots,
        OutputCounts,
        OutputPrimitives
    };

    de::MovePtr<BufferWithMemory> buffers[bufferCount];
    vk::VkDescriptorBufferInfo bufferDescriptors[bufferCount];

    uint32_t counts[bufferCount]{// InputA  { uint    a[]; } inputA;
                                 uint32_t(m_data.sizeX * m_data.sizeY),
                                 // OutputB { uvec2   b[]; } outputB;
                                 maxLoc,
                                 // OutputC { uint loc[]; } outputC;
                                 invocationStride,
                                 // OutputP { uint p[]; } outputP;
                                 uint32_t(outputP.size())};

    VkDeviceSize sizes[bufferCount]{// InputA  { uint    a[]; } inputA;
                                    counts[InputA] * sizeof(uint32_t),
                                    // OutputB { uvec2   b[]; } outputB;
                                    counts[OutputBallots] * sizeof(tcu::UVec4),
                                    // OutputC { uint loc[]; } outputC;
                                    counts[OutputCounts] * sizeof(uint32_t),
                                    // OutputP { uint p[]; } outputP;
                                    counts[OutputPrimitives] * sizeof(uint32_t)};

    const VkBufferUsageFlags cmnUsages = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VkBufferUsageFlags usages[bufferCount]{
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
    };

    // allocate buffers
    for (uint32_t i = 0; i < bufferCount; ++i)
    {
        if (sizes[i] > limits.maxStorageBufferRange)
            TCU_THROW(NotSupportedError, "Storage buffer size larger than device limits");

        try
        {
            buffers[i] = de::MovePtr<BufferWithMemory>(
                new BufferWithMemory(vk, device, allocator, makeBufferCreateInfo(sizes[i], usages[i] | cmnUsages),
                                     MemoryRequirement::HostVisible | MemoryRequirement::Cached));
        }
        catch (tcu::ResourceError &)
        {
            // Allocation size is unpredictable and can be too large for some systems. Don't treat allocation failure as a test failure.
            return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING,
                                   "Failed device memory allocation " + de::toString(sizes[i]) + " bytes");
        }
        bufferDescriptors[i] = makeDescriptorBufferInfo(**buffers[i], 0, sizes[i]);
    }

    // get raw pointers to previously allocated buffers
    void *ptrs[bufferCount];
    for (uint32_t i = 0; i < bufferCount; ++i)
    {
        ptrs[i] = buffers[i]->getAllocation().getHostPtr();
    }

    // populate buffers with their destination
    {
        auto rangeBufferA = makeStdBeginEnd<uint32_t>(ptrs[InputA], counts[InputA]);
        std::iota(rangeBufferA.first, rangeBufferA.second, 0u);
    }
    deMemset(ptrs[OutputBallots], 0, (size_t)sizes[OutputBallots]);
    deMemset(ptrs[OutputCounts], 0, (size_t)sizes[OutputCounts]);
    deMemset(ptrs[OutputPrimitives], 0, (size_t)sizes[OutputPrimitives]);

    // (...) and flush them to the GPU
    for (uint32_t i = 0; i < bufferCount; ++i)
    {
        flushAlloc(vk, device, buffers[i]->getAllocation());
    }

    VkDescriptorType descTypes[bufferCount]{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER};

    vk::DescriptorSetLayoutBuilder layoutBuilder;
    for (uint32_t i = 0; i < bufferCount; ++i)
    {
        layoutBuilder.addSingleBinding(descTypes[i], m_data.shaderStage);
    }
    vk::Unique<vk::VkDescriptorSetLayout> descriptorSetLayout(layoutBuilder.build(vk, device));

    vk::DescriptorPoolBuilder poolBuilder;
    for (uint32_t i = 0; i < bufferCount; ++i)
    {
        poolBuilder.addType(descTypes[i], 1);
    }
    vk::Unique<vk::VkDescriptorPool> descriptorPool(
        poolBuilder.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));
    vk::Unique<vk::VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

    vk::DescriptorSetUpdateBuilder setUpdateBuilder;
    for (uint32_t i = 0; i < bufferCount; ++i)
    {
        setUpdateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(i), descTypes[i],
                                     &bufferDescriptors[i]);
    }
    setUpdateBuilder.update(vk, device);

    const VkPushConstantRange pushConstantRange{
        (VkShaderStageFlags)m_data.shaderStage, // VkShaderStageFlags stageFlags;
        0u,                                     // uint32_t offset;
        sizeof(PushConstant)                    // uint32_t size;
    };

    const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // sType
        DE_NULL,                                       // pNext
        (VkPipelineLayoutCreateFlags)0,                // flags
        1u,                                            // setLayoutCount
        &descriptorSetLayout.get(),                    // pSetLayouts
        1u,                                            // pushConstantRangeCount
        &pushConstantRange,                            // pPushConstantRanges
    };

    const uint32_t imageWidth  = m_data.sizeX;
    const uint32_t imageHeight = m_data.sizeY;
    const VkFormat format      = VK_FORMAT_R8G8B8A8_UNORM;
    const VkImageCreateInfo imageCreateInfo{
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        VkImageCreateFlags(0),               // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
        format,                              // VkFormat format;
        {imageWidth, imageHeight, 1u},       // VkExtent3D extent;
        1u,                                  // uint32_t mipLevels;
        1u,                                  // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        0u,                                  // uint32_t queueFamilyIndexCount;
        0u,                                  // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED            // VkImageLayout initialLayout;
    };
    const VkImageSubresourceRange rscRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    de::MovePtr<ImageWithMemory> image(
        new ImageWithMemory(vk, device, allocator, imageCreateInfo, vk::MemoryRequirement::Any));
    Move<VkImageView> view        = makeImageView(vk, device, **image, VK_IMAGE_VIEW_TYPE_2D, format, rscRange);
    Move<VkRenderPass> renderPass = makeRenderPass(vk, device, format);
    Move<VkFramebuffer> framebuffer =
        makeFramebuffer(vk, device, *renderPass, *view, m_data.sizeX, m_data.sizeY, rscRange.layerCount);
    de::MovePtr<BufferWithMemory> vertexBuffer  = createVertexBufferAndFlush(m_data.sizeX, m_data.sizeY, topology);
    const VkRenderPassBeginInfo renderBeginInfo = makeRenderPassBeginInfo(*renderPass, *framebuffer);
    const Shaders shaders                       = createShaders();
    Move<VkPipelineLayout> pipelineLayout       = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo, NULL);
    Move<VkPipeline> pipeline =
        createGraphicsPipeline(*pipelineLayout, *renderPass, imageWidth, imageHeight, shaders, topology, 0u);
    Move<VkCommandPool> cmdPool =
        createCommandPool(vk, device, vk::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueIndex);
    Move<VkCommandBuffer> cmdBuffer = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    PushConstant pc{};
    pc.invocationStride      = invocationStride;
    pc.width                 = m_data.sizeX;
    pc.height                = m_data.sizeY;
    pc.enableInvocationIndex = VK_FALSE;

    auto callRecordDrawingAndSubmit = std::bind(&ReconvergenceTestGraphicsInstance::recordDrawingAndSubmit, this,
                                                *cmdBuffer, *pipelineLayout, *pipeline, *descriptorSet, std::cref(pc),
                                                std::cref(renderBeginInfo), **vertexBuffer, invocationStride, **image);

    // compute "maxLoc", which is a potential maximum number of locations written
    callRecordDrawingAndSubmit();

    // Take the maximum of "maxLoc" over all invocations.
    invalidateAlloc(vk, device, buffers[OutputCounts]->getAllocation());
    auto rangeLoc               = makeStdBeginEnd<const uint32_t>(ptrs[OutputCounts], counts[OutputCounts]);
    const uint32_t shaderMaxLoc = (*max_element(rangeLoc.first, rangeLoc.second));
    log << tcu::TestLog::Message << "Computed maxLoc in shader: " << shaderMaxLoc << tcu::TestLog::EndMessage;

    // If we need more space, reallocate OutputB::b[] aka buffers[1]
    if (shaderMaxLoc != hostMaxLoc)
    {
        // Add one (to make sure no additional writes are done) and multiply by
        // the number of invocations and current primitive count
        maxLoc                = (std::max(shaderMaxLoc, hostMaxLoc) + 1u) * invocationStride;
        counts[OutputBallots] = maxLoc;
        sizes[OutputBallots]  = counts[OutputBallots] * sizeof(tcu::UVec4);

        if (sizes[OutputBallots] > limits.maxStorageBufferRange)
            TCU_THROW(NotSupportedError, "Storage buffer size larger than device limits");

        try
        {
            buffers[OutputBallots] = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
                vk, device, allocator, makeBufferCreateInfo(sizes[OutputBallots], usages[OutputBallots] | cmnUsages),
                MemoryRequirement::HostVisible | MemoryRequirement::Cached));
        }
        catch (tcu::ResourceError &)
        {
            // Allocation size is unpredictable and can be too large for some systems. Don't treat allocation failure as a test failure.
            return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING,
                                   "Failed device memory allocation " + de::toString(sizes[OutputBallots]) + " bytes");
        }
        bufferDescriptors[OutputBallots] = makeDescriptorBufferInfo(**buffers[OutputBallots], 0, sizes[OutputBallots]);
        ptrs[OutputBallots]              = buffers[OutputBallots]->getAllocation().getHostPtr();

        vk::DescriptorSetUpdateBuilder setUpdateBuilder2;
        setUpdateBuilder2.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(OutputBallots),
                                      VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptors[OutputBallots]);
        setUpdateBuilder2.update(vk, device);
    }

    // Clear any writes to ballots/stores OutputB::b[] aka buffer[1] during the counting pass
    // Note that its size would may change since the first memory allocation
    deMemset(ptrs[OutputBallots], 0, (size_t)sizes[OutputBallots]);
    deMemset(ptrs[OutputCounts], 0, (size_t)sizes[OutputCounts]);
    deMemset(ptrs[OutputPrimitives], 0, (size_t)sizes[OutputPrimitives]);

    // flush them all to the GPU
    flushAlloc(vk, device, buffers[OutputBallots]->getAllocation());
    flushAlloc(vk, device, buffers[OutputCounts]->getAllocation());
    flushAlloc(vk, device, buffers[OutputPrimitives]->getAllocation());

    // run the actual shader with updated PushConstant
    pc.enableInvocationIndex = VK_TRUE;
    callRecordDrawingAndSubmit();

    invalidateAlloc(vk, device, buffers[OutputCounts]->getAllocation());
    const uint32_t finalShaderMaxLoc = (*max_element(rangeLoc.first, rangeLoc.second));
    log << tcu::TestLog::Message << "Final maxLoc from shader: " << finalShaderMaxLoc << tcu::TestLog::EndMessage;
    if (finalShaderMaxLoc != shaderMaxLoc)
    {
        return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING,
                               "maxLoc differs across shader invocations, expected: " + de::toString(shaderMaxLoc) +
                                   " got: " + de::toString(finalShaderMaxLoc));
    }

    invalidateAlloc(vk, device, buffers[OutputBallots]->getAllocation());
    const tcu::UVec4 *ballots = static_cast<tcu::UVec4 *>(ptrs[OutputBallots]);

    invalidateAlloc(vk, device, buffers[OutputPrimitives]->getAllocation());
    auto outputPrange = makeStdBeginEnd<uint32_t>(ptrs[OutputPrimitives], counts[OutputPrimitives]);
    std::copy(outputPrange.first, outputPrange.second, outputP.begin());

    try
    {
        ref.resize(counts[OutputBallots], tcu::UVec4(0u, 0u, 0u, 0u));
    }
    catch (const std::bad_alloc &)
    {
        // Allocation size is unpredictable and can be too large for some systems. Don't treat allocation failure as a test failure.
        return tcu::TestStatus(QP_TEST_RESULT_NOT_SUPPORTED,
                               "Failed system memory allocation " + de::toString(sizes[OutputBallots]) + " bytes");
    }

    // Simulate execution on the CPU, and compare against the GPU result
    const uint32_t finalHostMaxLoc = program->execute(m_context.getTestContext().getWatchDog(), false, m_subgroupSize,
                                                      fragmentStride, invocationStride, ref, log, outputP, ballots);

    const qpTestResult res = calculateAndLogResultEx(log, ballots, ref, finalHostMaxLoc, PrintMode::None);

    return tcu::TestStatus(res, qpGetTestResultName(res));
}

qpTestResult_e ReconvergenceTestVertexInstance::calculateAndLogResultEx(add_ref<tcu::TestLog> log,
                                                                        const tcu::UVec4 *result,
                                                                        const std::vector<tcu::UVec4> &ref,
                                                                        const uint32_t maxLoc,
                                                                        const PrintMode printMode)
{
    DE_UNREF(maxLoc);
    DE_UNREF(printMode);

    qpTestResult res                  = QP_TEST_RESULT_PASS;
    uint32_t mismatchCount            = 0u;
    const uint32_t printMismatchCount = 5u;

    // With maximal reconvergence, we should expect the output to exactly match the reference.
    const uint32_t ballotStoreCount = static_cast<uint32_t>(ref.size());
    for (uint32_t i = 0; i < ballotStoreCount; ++i)
    {
        const Ballot resultVal(result[i], m_subgroupSize);
        const Ballot refVal(ref.at(i), m_subgroupSize);
        if (resultVal != refVal)
        {
            if (mismatchCount++ < printMismatchCount)
            {
                res = QP_TEST_RESULT_FAIL;
                log << tcu::TestLog::Message << "Mismatch at " << i << "\nexpected: " << resultVal
                    << "\n     got: " << refVal << tcu::TestLog::EndMessage;
                if (printMode == PrintMode::Console)
                {
                    std::cout << "Mismatch at " << i << "\nexpected: " << resultVal << "\n     got: " << refVal
                              << std::endl;
                }
            }
        }
    }

    log << tcu::TestLog::Message << "Mismatch count: " << mismatchCount << " from " << ballotStoreCount
        << tcu::TestLog::EndMessage;
    if (printMode == PrintMode::Console)
    {
        std::cout << "Mismatch count: " << mismatchCount << " from " << ballotStoreCount << std::endl;
    }

    return res;
}

std::vector<Move<VkShaderModule>> ReconvergenceTestTessCtrlInstance::createShaders(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();

    Move<VkShaderModule> vertex     = createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"));
    Move<VkShaderModule> fragment   = createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"));
    Move<VkShaderModule> control    = createShaderModule(vk, device, m_context.getBinaryCollection().get("test"));
    Move<VkShaderModule> evaluation = createShaderModule(vk, device, m_context.getBinaryCollection().get("tese"));

    // { #vert, #frag, #tesc, #tese, geom }; if any
    std::vector<Move<VkShaderModule>> shaders;
    shaders.emplace_back(vertex);
    shaders.emplace_back(fragment);
    shaders.emplace_back(control);
    shaders.emplace_back(evaluation);

    return shaders;
}

tcu::TestStatus ReconvergenceTestTessCtrlInstance::iterate(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();
    Allocator &allocator      = m_context.getDefaultAllocator();
    const uint32_t queueIndex = m_context.getUniversalQueueFamilyIndex();
    add_ref<tcu::TestLog> log = m_context.getTestContext().getLog();

    if (m_subgroupSize < TessCtrlRandomProgram::minSubgroupSize || m_subgroupSize > 64)
    {
        std::stringstream str;
        str << "Subgroup size less than " << TessCtrlRandomProgram::minSubgroupSize
            << " or greater than 64 not handled.";
        str.flush();
        TCU_THROW(TestError, str.str());
    }

    deRandom rnd;
    deRandom_init(&rnd, m_data.seed);

    vk::VkPhysicalDeviceProperties2 properties2;
    deMemset(&properties2, 0, sizeof(properties2));
    properties2.sType = vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    m_context.getInstanceInterface().getPhysicalDeviceProperties2(m_context.getPhysicalDevice(), &properties2);
    const VkPhysicalDeviceLimits &limits = properties2.properties.limits;

    const uint32_t patchControlPoints = 1;
    const uint32_t vertexCount =
        (m_subgroupSize / TessCtrlRandomProgram::minSubgroupSize) * patchControlPoints * m_data.sizeX;
    const uint32_t primitiveStride = vertexCount / patchControlPoints;
    de::MovePtr<BufferWithMemory> vertexBuffer =
        createVertexBufferAndFlush(vertexCount, 1u, VK_PRIMITIVE_TOPOLOGY_PATCH_LIST);
    const uint32_t invocationStride = vertexCount * TessCtrlRandomProgram::minSubgroupSize;
    DE_ASSERT(invocationStride < MAX_INVOCATIONS_ALL_TESTS);

    log << tcu::TestLog::Message << "LayoutVertexOut:    " << (uint32_t)TessCtrlRandomProgram::minSubgroupSize
        << tcu::TestLog::EndMessage;
    log << tcu::TestLog::Message << "patchControlPoints: " << patchControlPoints << tcu::TestLog::EndMessage;
    log << tcu::TestLog::Message << "primitiveStride:    " << primitiveStride << tcu::TestLog::EndMessage;
    log << tcu::TestLog::Message << "invocationStride:   " << invocationStride << tcu::TestLog::EndMessage;
    log << tcu::TestLog::Message << "usedSubgroupCount:  " << m_data.sizeX << tcu::TestLog::EndMessage;

    de::MovePtr<TessCtrlRandomProgram> program(new TessCtrlRandomProgram(m_data, invocationStride));
    program->generateRandomProgram(m_context.getTestContext().getWatchDog(), log);

    std::vector<uint64_t> ref;
    const uint32_t simulationMaxLoc = program->simulate(true, m_subgroupSize, ref);
    log << tcu::TestLog::Message << "simulated maxLoc: " << simulationMaxLoc << tcu::TestLog::EndMessage;
    // maxLoc is per-invocation. Add one (to make sure no additional writes are done)
    uint32_t maxLoc = simulationMaxLoc;
    maxLoc += 1;
    maxLoc *= invocationStride;

    constexpr uint32_t bufferCount = 3;
    enum Bindings
    {
        InputA,
        OutputBallots,
        OutputCounts,
    };

    de::MovePtr<BufferWithMemory> buffers[bufferCount];
    vk::VkDescriptorBufferInfo bufferDescriptors[bufferCount];

    VkDeviceSize sizes[bufferCount]{
        // InputA  { uint    a[]; } inputA;  filled with a[i] == i
        invocationStride * sizeof(uint32_t),
        // OutputB { uvec2   b[]; } outputB;
        maxLoc * sizeof(uint64_t),
        // OutputC { uint loc[]; } outputC;
        invocationStride * sizeof(uint32_t),
    };

    VkBufferUsageFlags usages[bufferCount]{
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
    };

    // allocate buffers
    for (uint32_t i = 0; i < bufferCount; ++i)
    {
        if (sizes[i] > limits.maxStorageBufferRange)
            TCU_THROW(NotSupportedError, "Storage buffer size larger than device limits");

        try
        {
            buffers[i] = de::MovePtr<BufferWithMemory>(
                new BufferWithMemory(vk, device, allocator,
                                     makeBufferCreateInfo(sizes[i], usages[i] | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                                        VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
                                     MemoryRequirement::HostVisible | MemoryRequirement::Cached));
        }
        catch (tcu::ResourceError &)
        {
            // Allocation size is unpredictable and can be too large for some systems. Don't treat allocation failure as a test failure.
            return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING,
                                   "Failed device memory allocation " + de::toString(sizes[i]) + " bytes");
        }
        bufferDescriptors[i] = makeDescriptorBufferInfo(**buffers[i], 0, sizes[i]);
    }

    // get raw pointers to previously allocated buffers
    void *ptrs[bufferCount];
    for (uint32_t i = 0; i < bufferCount; ++i)
    {
        ptrs[i] = (uint32_t *)buffers[i]->getAllocation().getHostPtr();
    }

    // populate buffers with their destination
    {
        auto rangeBufferA = makeStdBeginEnd<uint32_t>(ptrs[InputA], invocationStride);
        std::iota(rangeBufferA.first, rangeBufferA.second, 0u);
    }
    deMemset(ptrs[OutputBallots], 0, (size_t)sizes[OutputBallots]);
    deMemset(ptrs[OutputCounts], 0, (size_t)sizes[OutputCounts]);

    // (...) and flush them to the GPU
    for (uint32_t i = 0; i < bufferCount; ++i)
    {
        flushAlloc(vk, device, buffers[i]->getAllocation());
    }

    VkDescriptorType descTypes[bufferCount]{
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    };

    vk::DescriptorSetLayoutBuilder layoutBuilder;
    for (uint32_t i = 0; i < bufferCount; ++i)
    {
        layoutBuilder.addSingleBinding(descTypes[i], m_data.shaderStage);
    }
    vk::Unique<vk::VkDescriptorSetLayout> descriptorSetLayout(layoutBuilder.build(vk, device));

    vk::DescriptorPoolBuilder poolBuilder;
    for (uint32_t i = 0; i < bufferCount; ++i)
    {
        poolBuilder.addType(descTypes[i], 1);
    }
    vk::Unique<vk::VkDescriptorPool> descriptorPool(
        poolBuilder.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));
    vk::Unique<vk::VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

    vk::DescriptorSetUpdateBuilder setUpdateBuilder;
    for (uint32_t i = 0; i < bufferCount; ++i)
    {
        setUpdateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(i), descTypes[i],
                                     &bufferDescriptors[i]);
    }
    setUpdateBuilder.update(vk, device);

    const VkPushConstantRange pushConstantRange{
        (VkShaderStageFlags)m_data.shaderStage, // VkShaderStageFlags stageFlags;
        0u,                                     // uint32_t offset;
        sizeof(PushConstant)                    // uint32_t size;
    };

    // TODO: verify that PushConstant is available on running machine

    const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // sType
        DE_NULL,                                       // pNext
        (VkPipelineLayoutCreateFlags)0,                // flags
        1u,                                            // setLayoutCount
        &descriptorSetLayout.get(),                    // pSetLayouts
        1u,                                            // pushConstantRangeCount
        &pushConstantRange,                            // pPushConstantRanges
    };

    const uint32_t imageWidth  = 256;
    const uint32_t imageHeight = 256;
    const VkFormat format      = VK_FORMAT_R8G8B8A8_UNORM;
    const VkImageCreateInfo imageCreateInfo{
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        VkImageCreateFlags(0),               // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
        format,                              // VkFormat format;
        {imageWidth, imageHeight, 1u},       // VkExtent3D extent;
        1u,                                  // uint32_t mipLevels;
        1u,                                  // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        0u,                                  // uint32_t queueFamilyIndexCount;
        0u,                                  // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED            // VkImageLayout initialLayout;
    };
    const VkImageSubresourceRange rscRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    de::MovePtr<ImageWithMemory> image(
        new ImageWithMemory(vk, device, allocator, imageCreateInfo, vk::MemoryRequirement::Any));
    Move<VkImageView> view        = makeImageView(vk, device, **image, VK_IMAGE_VIEW_TYPE_2D, format, rscRange);
    Move<VkRenderPass> renderPass = makeRenderPass(vk, device, format);
    Move<VkFramebuffer> framebuffer =
        makeFramebuffer(vk, device, *renderPass, *view, m_data.sizeX, m_data.sizeY, rscRange.layerCount);
    const VkRenderPassBeginInfo renderBeginInfo = makeRenderPassBeginInfo(*renderPass, *framebuffer);
    const Shaders shaders                       = createShaders();
    Move<VkPipelineLayout> pipelineLayout       = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo, NULL);
    Move<VkPipeline> pipeline = createGraphicsPipeline(*pipelineLayout, *renderPass, imageWidth, imageHeight, shaders,
                                                       VK_PRIMITIVE_TOPOLOGY_PATCH_LIST, patchControlPoints);
    Move<VkCommandPool> cmdPool =
        createCommandPool(vk, device, vk::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueIndex);
    Move<VkCommandBuffer> cmdBuffer = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    PushConstant pc{};
    pc.invocationStride = 0u;
    pc.width            = TessCtrlRandomProgram::minSubgroupSize;
    pc.height           = patchControlPoints;
    pc.primitiveStride  = primitiveStride;

    auto callRecordDrawingAndSubmit = std::bind(&ReconvergenceTestGraphicsInstance::recordDrawingAndSubmit, this,
                                                *cmdBuffer, *pipelineLayout, *pipeline, *descriptorSet, std::cref(pc),
                                                std::cref(renderBeginInfo), **vertexBuffer, vertexCount, **image);

    // compute "maxLoc", which is a potential maximum number of locations written
    callRecordDrawingAndSubmit();

    // Take the maximum of "maxLoc" over all invocations.
    invalidateAlloc(vk, device, buffers[OutputCounts]->getAllocation());
    auto rangeLoc                       = makeStdBeginEnd<const uint32_t>(ptrs[OutputCounts], invocationStride);
    const uint32_t computedShaderMaxLoc = (*max_element(rangeLoc.first, rangeLoc.second));
    log << tcu::TestLog::Message << "computed shaderMaxLoc: " << computedShaderMaxLoc << tcu::TestLog::EndMessage;

    // If we need more space, reallocate OutputB::b[] aka buffers[1]
    if (computedShaderMaxLoc > simulationMaxLoc)
    {
        // Add one (to make sure no additional writes are done) and multiply by
        // the number of invocations and current primitive count
        maxLoc               = (computedShaderMaxLoc + 1) * invocationStride;
        sizes[OutputBallots] = maxLoc * sizeof(uint64_t);

        if (sizes[OutputBallots] > limits.maxStorageBufferRange)
            TCU_THROW(NotSupportedError, "Storage buffer size larger than device limits");

        try
        {
            buffers[OutputBallots] = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
                vk, device, allocator,
                makeBufferCreateInfo(sizes[1], VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
                MemoryRequirement::HostVisible | MemoryRequirement::Cached));
        }
        catch (tcu::ResourceError &)
        {
            // Allocation size is unpredictable and can be too large for some systems. Don't treat allocation failure as a test failure.
            return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING,
                                   "Failed device memory allocation " + de::toString(sizes[OutputBallots]) + " bytes");
        }
        bufferDescriptors[OutputBallots] = makeDescriptorBufferInfo(**buffers[OutputBallots], 0, sizes[OutputBallots]);
        ptrs[OutputBallots]              = buffers[OutputBallots]->getAllocation().getHostPtr();

        vk::DescriptorSetUpdateBuilder setUpdateBuilder2;
        setUpdateBuilder2.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(OutputBallots),
                                      VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptors[OutputBallots]);
        setUpdateBuilder2.update(vk, device);
    }

    // Clear any writes to ballots/stores OutputB::b[] aka buffer[1] during the counting pass
    // Note that its size would may change since the first memory allocation
    deMemset(ptrs[OutputBallots], 0, (size_t)sizes[OutputBallots]);
    // Clear any writes to counting OutputC::loc[] aka buffer[2] during the counting pass
    deMemset(ptrs[OutputCounts], 0, (size_t)sizes[OutputCounts]);

    // flush them all to the GPU
    flushAlloc(vk, device, buffers[OutputBallots]->getAllocation());
    flushAlloc(vk, device, buffers[OutputCounts]->getAllocation());

    // run the actual shader with updated PushConstant
    pc.invocationStride = invocationStride;
    pc.width            = TessCtrlRandomProgram::minSubgroupSize;
    pc.height           = patchControlPoints;
    pc.primitiveStride  = primitiveStride;
    callRecordDrawingAndSubmit();

    invalidateAlloc(vk, device, buffers[OutputCounts]->getAllocation());
    const uint32_t finalShaderMaxLoc = (*max_element(rangeLoc.first, rangeLoc.second));
    log << tcu::TestLog::Message << "final shaderMaxLoc: " << finalShaderMaxLoc << tcu::TestLog::EndMessage;
    if (finalShaderMaxLoc > computedShaderMaxLoc)
    {
        return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, "maxLoc differs across shader invocations");
    }

    invalidateAlloc(vk, device, buffers[OutputCounts]->getAllocation());
    invalidateAlloc(vk, device, buffers[OutputBallots]->getAllocation());

    // Simulate execution on the CPU, and compare against the GPU result
    try
    {
        ref.resize(maxLoc, 0ull);
    }
    catch (const std::bad_alloc &)
    {
        // Allocation size is unpredictable and can be too large for some systems. Don't treat allocation failure as a test failure.
        return tcu::TestStatus(QP_TEST_RESULT_NOT_SUPPORTED,
                               "Failed system memory allocation " + de::toString(maxLoc * sizeof(uint64_t)) + " bytes");
    }

    program->simulate(false, m_subgroupSize, ref);

    const uint64_t *ballots = static_cast<uint64_t *>(ptrs[OutputBallots]);
    qpTestResult res        = calculateAndLogResult(ballots, ref, invocationStride, m_subgroupSize, finalShaderMaxLoc,
                                                    (invocationStride / 3), PrintMode::None);

    return tcu::TestStatus(res, qpGetTestResultName(res));
}

std::vector<Move<VkShaderModule>> ReconvergenceTestTessEvalInstance::createShaders(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();

    Move<VkShaderModule> vertex     = createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"));
    Move<VkShaderModule> fragment   = createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"));
    Move<VkShaderModule> control    = createShaderModule(vk, device, m_context.getBinaryCollection().get("tesc"));
    Move<VkShaderModule> evaluation = createShaderModule(vk, device, m_context.getBinaryCollection().get("test"));

    // { #vert, #frag, #tesc, #tese, geom }; if any
    std::vector<Move<VkShaderModule>> shaders;
    shaders.emplace_back(vertex);
    shaders.emplace_back(fragment);
    shaders.emplace_back(control);
    shaders.emplace_back(evaluation);

    return shaders;
}

tcu::TestStatus ReconvergenceTestTessEvalInstance::iterate(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();
    Allocator &allocator      = m_context.getDefaultAllocator();
    const uint32_t queueIndex = m_context.getUniversalQueueFamilyIndex();
    add_ref<tcu::TestLog> log = m_context.getTestContext().getLog();

    if (m_subgroupSize < TessEvalRandomProgram::quadInvocationCount || m_subgroupSize > 64)
    {
        std::stringstream str;
        str << "Subgroup size less than " << TessEvalRandomProgram::quadInvocationCount
            << " or greater than 64 not handled.";
        str.flush();
        TCU_THROW(TestError, str.str());
    }

    deRandom rnd;
    deRandom_init(&rnd, m_data.seed);

    vk::VkPhysicalDeviceProperties2 properties2;
    deMemset(&properties2, 0, sizeof(properties2));
    properties2.sType = vk::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    m_context.getInstanceInterface().getPhysicalDeviceProperties2(m_context.getPhysicalDevice(), &properties2);
    const VkPhysicalDeviceLimits &limits = properties2.properties.limits;

    const uint32_t patchesPerGroup             = m_subgroupSize / TessEvalRandomProgram::quadInvocationCount;
    const uint32_t primitiveStride             = patchesPerGroup * m_data.sizeX;
    const uint32_t invocationStride            = primitiveStride * TessEvalRandomProgram::quadInvocationCount;
    const std::vector<tcu::Vec4> vertices      = generateVertices(invocationStride, VK_PRIMITIVE_TOPOLOGY_POINT_LIST);
    const uint32_t vertexCount                 = uint32_t(vertices.size());
    de::MovePtr<BufferWithMemory> vertexBuffer = createVertexBufferAndFlush(vertices);

    DE_ASSERT(invocationStride <= MAX_INVOCATIONS_ALL_TESTS);

    de::MovePtr<TessEvalRandomProgram> program(new TessEvalRandomProgram(m_data, invocationStride));
    program->generateRandomProgram(m_context.getTestContext().getWatchDog(), log);

    std::vector<uint64_t> ref;
    const uint32_t simulationMaxLoc = program->simulate(true, m_subgroupSize, ref);
    log << tcu::TestLog::Message << "simulated maxLoc:       " << simulationMaxLoc << tcu::TestLog::EndMessage;
    log << tcu::TestLog::Message << "effective patch size:   " << m_data.sizeY << tcu::TestLog::EndMessage;
    log << tcu::TestLog::Message << "effective patch count:  " << primitiveStride << tcu::TestLog::EndMessage;
    log << tcu::TestLog::Message << "total invocation count: " << invocationStride << tcu::TestLog::EndMessage;

    // maxLoc is per-invocation. Add one (to make sure no additional writes are done).
    uint32_t maxLoc = simulationMaxLoc;
    maxLoc += 1;
    maxLoc *= invocationStride;

    constexpr uint32_t bufferCount = 3;
    enum Bindings
    {
        InputA,
        OutputBallots,
        OutputCounts,
    };

    de::MovePtr<BufferWithMemory> buffers[bufferCount];
    vk::VkDescriptorBufferInfo bufferDescriptors[bufferCount];

    VkDeviceSize sizes[bufferCount]{
        // InputA  { uint    a[]; } inputA;  filled with a[i] == i
        invocationStride * sizeof(uint32_t),
        // OutputB { uvec2   b[]; } outputB;
        maxLoc * sizeof(uint64_t),
        // OutputC { uint loc[]; } outputC;
        invocationStride * sizeof(uint32_t),
    };

    VkBufferUsageFlags usages[bufferCount]{
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
    };

    // allocate buffers
    for (uint32_t i = 0; i < bufferCount; ++i)
    {
        if (sizes[i] > limits.maxStorageBufferRange)
            TCU_THROW(NotSupportedError, "Storage buffer size larger than device limits");

        try
        {
            buffers[i] = de::MovePtr<BufferWithMemory>(
                new BufferWithMemory(vk, device, allocator,
                                     makeBufferCreateInfo(sizes[i], usages[i] | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                                        VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
                                     MemoryRequirement::HostVisible | MemoryRequirement::Cached));
        }
        catch (tcu::ResourceError &)
        {
            // Allocation size is unpredictable and can be too large for some systems. Don't treat allocation failure as a test failure.
            return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING,
                                   "Failed device memory allocation " + de::toString(sizes[i]) + " bytes");
        }
        bufferDescriptors[i] = makeDescriptorBufferInfo(**buffers[i], 0, sizes[i]);
    }

    // get raw pointers to previously allocated buffers
    void *ptrs[bufferCount];
    for (uint32_t i = 0; i < bufferCount; ++i)
    {
        ptrs[i] = (uint32_t *)buffers[i]->getAllocation().getHostPtr();
    }

    // populate buffers with their destination
    {
        auto rangeBufferA = makeStdBeginEnd<uint32_t>(ptrs[InputA], invocationStride);
        std::iota(rangeBufferA.first, rangeBufferA.second, 0u);
    }
    deMemset(ptrs[OutputBallots], 0, (size_t)sizes[OutputBallots]);
    deMemset(ptrs[OutputCounts], 0, (size_t)sizes[OutputCounts]);

    // (...) and flush them to the GPU
    for (uint32_t i = 0; i < bufferCount; ++i)
    {
        flushAlloc(vk, device, buffers[i]->getAllocation());
    }

    VkDescriptorType descTypes[bufferCount]{
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    };

    vk::DescriptorSetLayoutBuilder layoutBuilder;
    for (uint32_t i = 0; i < bufferCount; ++i)
    {
        layoutBuilder.addSingleBinding(descTypes[i], m_data.shaderStage);
    }
    vk::Unique<vk::VkDescriptorSetLayout> descriptorSetLayout(layoutBuilder.build(vk, device));

    vk::DescriptorPoolBuilder poolBuilder;
    for (uint32_t i = 0; i < bufferCount; ++i)
    {
        poolBuilder.addType(descTypes[i], 1);
    }
    vk::Unique<vk::VkDescriptorPool> descriptorPool(
        poolBuilder.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));
    vk::Unique<vk::VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

    vk::DescriptorSetUpdateBuilder setUpdateBuilder;
    for (uint32_t i = 0; i < bufferCount; ++i)
    {
        setUpdateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(i), descTypes[i],
                                     &bufferDescriptors[i]);
    }
    setUpdateBuilder.update(vk, device);

    const VkPushConstantRange pushConstantRange{
        (VkShaderStageFlags)m_data.shaderStage, // VkShaderStageFlags stageFlags;
        0u,                                     // uint32_t offset;
        sizeof(PushConstant)                    // uint32_t size;
    };

    // TODO: verify that PushConstant is available on running machine

    const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // sType
        DE_NULL,                                       // pNext
        (VkPipelineLayoutCreateFlags)0,                // flags
        1u,                                            // setLayoutCount
        &descriptorSetLayout.get(),                    // pSetLayouts
        1u,                                            // pushConstantRangeCount
        &pushConstantRange,                            // pPushConstantRanges
    };

    const uint32_t imageWidth  = 256;
    const uint32_t imageHeight = 256;
    const VkFormat format      = VK_FORMAT_R8G8B8A8_UNORM;
    const VkImageCreateInfo imageCreateInfo{
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        VkImageCreateFlags(0),               // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
        format,                              // VkFormat format;
        {imageWidth, imageHeight, 1u},       // VkExtent3D extent;
        1u,                                  // uint32_t mipLevels;
        1u,                                  // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        0u,                                  // uint32_t queueFamilyIndexCount;
        0u,                                  // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED            // VkImageLayout initialLayout;
    };
    const VkImageSubresourceRange rscRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    de::MovePtr<ImageWithMemory> image(
        new ImageWithMemory(vk, device, allocator, imageCreateInfo, vk::MemoryRequirement::Any));
    Move<VkImageView> view        = makeImageView(vk, device, **image, VK_IMAGE_VIEW_TYPE_2D, format, rscRange);
    Move<VkRenderPass> renderPass = makeRenderPass(vk, device, format);
    Move<VkFramebuffer> framebuffer =
        makeFramebuffer(vk, device, *renderPass, *view, m_data.sizeX, m_data.sizeY, rscRange.layerCount);
    const VkRenderPassBeginInfo renderBeginInfo = makeRenderPassBeginInfo(*renderPass, *framebuffer);
    const Shaders shaders                       = createShaders();
    Move<VkPipelineLayout> pipelineLayout       = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo, NULL);
    Move<VkPipeline> pipeline =
        createGraphicsPipeline(*pipelineLayout, *renderPass, imageWidth, imageHeight, shaders,
                               VK_PRIMITIVE_TOPOLOGY_PATCH_LIST, TessEvalRandomProgram::quadInvocationCount);
    Move<VkCommandPool> cmdPool =
        createCommandPool(vk, device, vk::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueIndex);
    Move<VkCommandBuffer> cmdBuffer = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    PushConstant pc{};
    pc.invocationStride = 0u;
    pc.width            = TessEvalRandomProgram::quadInvocationCount;

    auto callRecordDrawingAndSubmit = std::bind(&ReconvergenceTestGraphicsInstance::recordDrawingAndSubmit, this,
                                                *cmdBuffer, *pipelineLayout, *pipeline, *descriptorSet, std::cref(pc),
                                                std::cref(renderBeginInfo), **vertexBuffer, vertexCount, **image);

    // compute "maxLoc", which is a potential maximum number of locations written
    callRecordDrawingAndSubmit();

    // Take the maximum of "maxLoc" over all invocations.
    invalidateAlloc(vk, device, buffers[OutputCounts]->getAllocation());
    auto rangeLoc                       = makeStdBeginEnd<const uint32_t>(ptrs[OutputCounts], invocationStride);
    const uint32_t computedShaderMaxLoc = (*max_element(rangeLoc.first, rangeLoc.second));
    log << tcu::TestLog::Message << "computed shaderMaxLoc: " << computedShaderMaxLoc << tcu::TestLog::EndMessage;

    // If we need more space, reallocate OutputB::b[] aka buffers[1]
    if (computedShaderMaxLoc > simulationMaxLoc)
    {
        // Add one (to make sure no additional writes are done) and multiply by
        // the number of invocations and current primitive count
        maxLoc               = (computedShaderMaxLoc + 1) * invocationStride;
        sizes[OutputBallots] = maxLoc * sizeof(uint64_t);

        if (sizes[OutputBallots] > limits.maxStorageBufferRange)
            TCU_THROW(NotSupportedError, "Storage buffer size larger than device limits");

        try
        {
            buffers[OutputBallots] = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
                vk, device, allocator,
                makeBufferCreateInfo(sizes[1], VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
                MemoryRequirement::HostVisible | MemoryRequirement::Cached));
        }
        catch (tcu::ResourceError &)
        {
            // Allocation size is unpredictable and can be too large for some systems. Don't treat allocation failure as a test failure.
            return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING,
                                   "Failed device memory allocation " + de::toString(sizes[OutputBallots]) + " bytes");
        }
        bufferDescriptors[OutputBallots] = makeDescriptorBufferInfo(**buffers[OutputBallots], 0, sizes[OutputBallots]);
        ptrs[OutputBallots]              = buffers[OutputBallots]->getAllocation().getHostPtr();

        vk::DescriptorSetUpdateBuilder setUpdateBuilder2;
        setUpdateBuilder2.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(OutputBallots),
                                      VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptors[OutputBallots]);
        setUpdateBuilder2.update(vk, device);
    }

    // Clear any writes to ballots/stores OutputB::b[] aka buffer[1] during the counting pass
    // Note that its size would may change since the first memory allocation
    deMemset(ptrs[OutputBallots], 0, (size_t)sizes[OutputBallots]);
    // Clear any writes to counting OutputC::loc[] aka buffer[2] during the counting pass
    deMemset(ptrs[OutputCounts], 0, (size_t)sizes[OutputCounts]);

    // flush them all to the GPU
    flushAlloc(vk, device, buffers[OutputBallots]->getAllocation());
    flushAlloc(vk, device, buffers[OutputCounts]->getAllocation());

    // run the actual shader with updated PushConstant
    pc.invocationStride = invocationStride;
    pc.width            = TessEvalRandomProgram::quadInvocationCount;
    callRecordDrawingAndSubmit();

    invalidateAlloc(vk, device, buffers[OutputCounts]->getAllocation());
    const uint32_t finalShaderMaxLoc = (*max_element(rangeLoc.first, rangeLoc.second));
    log << tcu::TestLog::Message << "final shaderMaxLoc: " << finalShaderMaxLoc << tcu::TestLog::EndMessage;
    if (finalShaderMaxLoc > computedShaderMaxLoc)
    {
        std::stringstream s;
        s << "maxLoc differs across shader invocations: " << finalShaderMaxLoc << " and " << computedShaderMaxLoc;
        return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING, s.str());
    }

    invalidateAlloc(vk, device, buffers[OutputCounts]->getAllocation());
    invalidateAlloc(vk, device, buffers[OutputBallots]->getAllocation());

    // Simulate execution on the CPU, and compare against the GPU result
    try
    {
        ref.resize(maxLoc, 0ull);
    }
    catch (const std::bad_alloc &)
    {
        // Allocation size is unpredictable and can be too large for some systems. Don't treat allocation failure as a test failure.
        return tcu::TestStatus(QP_TEST_RESULT_NOT_SUPPORTED,
                               "Failed system memory allocation " + de::toString(maxLoc * sizeof(uint64_t)) + " bytes");
    }

    program->simulate(false, m_subgroupSize, ref);

    const uint64_t *ballots = static_cast<uint64_t *>(ptrs[OutputBallots]);
    qpTestResult res        = calculateAndLogResult(ballots, ref, invocationStride, m_subgroupSize, finalShaderMaxLoc,
                                                    (invocationStride / 3), PrintMode::None);

    return tcu::TestStatus(res, qpGetTestResultName(res));
}

de::MovePtr<BufferWithMemory> ReconvergenceTestGeometryInstance::createVertexBufferAndFlush(
    uint32_t cellsHorz, uint32_t cellsVert, VkPrimitiveTopology topology)
{
    DE_UNREF(topology);
    DE_ASSERT(VK_PRIMITIVE_TOPOLOGY_POINT_LIST == topology);
    const std::vector<tcu::Vec4> vertices = GeometryRandomProgram::Arrangement::generatePrimitives(
        cellsHorz, cellsVert, GeometryRandomProgram::fillPercentage);
    return ReconvergenceTestGraphicsInstance::createVertexBufferAndFlush(vertices);
}

std::vector<Move<VkShaderModule>> ReconvergenceTestGeometryInstance::createShaders(void)
{
    const DeviceInterface &vk = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();

    Move<VkShaderModule> vertex   = createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"));
    Move<VkShaderModule> fragment = createShaderModule(vk, device, m_context.getBinaryCollection().get("frag"));
    Move<VkShaderModule> geometry = createShaderModule(vk, device, m_context.getBinaryCollection().get("test"));

    // { #vert, #frag, tesc, tese, #geom }; if any
    std::vector<Move<VkShaderModule>> shaders;
    shaders.emplace_back(vertex);
    shaders.emplace_back(fragment);
    shaders.emplace_back();
    shaders.emplace_back();
    shaders.emplace_back(geometry);

    return shaders;
}

tcu::TestStatus ReconvergenceTestGeometryInstance::iterate(void)
{
    const VkPhysicalDeviceLimits &limits = m_context.getDeviceProperties().limits;
    if (sizeof(PushConstant) > limits.maxPushConstantsSize)
    {
        return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING,
                               "PushConstant size " + std::to_string(sizeof(PushConstant)) + " exceeds device limit " +
                                   std::to_string(limits.maxPushConstantsSize));
    }

    const DeviceInterface &vk          = m_context.getDeviceInterface();
    const VkDevice device              = m_context.getDevice();
    Allocator &allocator               = m_context.getDefaultAllocator();
    const uint32_t queueIndex          = m_context.getUniversalQueueFamilyIndex();
    add_ref<tcu::TestLog> log          = m_context.getTestContext().getLog();
    const VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    const uint32_t fragmentStride      = uint32_t(m_data.sizeX * m_data.sizeY);
    const uint32_t invocationStride    = GeometryRandomProgram::Arrangement::calculatePrimitiveCount(
        m_data.sizeX, m_data.sizeY, GeometryRandomProgram::fillPercentage);

    de::MovePtr<GeometryRandomProgram> program(new GeometryRandomProgram(m_data));
    program->generateRandomProgram(m_context.getTestContext().getWatchDog(), log);

    // simulate content of outputP buffer
    std::vector<uint32_t> outputP =
        GeometryRandomProgram::Arrangement::generateVectorOutputP(m_subgroupSize, invocationStride);

    std::vector<tcu::UVec4> ref;
    const uint32_t hostMaxLoc = program->execute(m_context.getTestContext().getWatchDog(), true, m_subgroupSize,
                                                 fragmentStride, invocationStride, ref, log, outputP, nullptr);
    log << tcu::TestLog::Message << "Rendering area  : " << tcu::UVec2(m_data.sizeX, m_data.sizeY)
        << tcu::TestLog::EndMessage;
    log << tcu::TestLog::Message << "invocationStride: " << invocationStride << tcu::TestLog::EndMessage;
    log << tcu::TestLog::Message << "Simulated maxLoc: " << hostMaxLoc << tcu::TestLog::EndMessage;
    // maxLoc is per-invocation. Add one (to make sure no additional writes are done).
    uint32_t maxLoc = hostMaxLoc;
    maxLoc += 1;
    maxLoc *= invocationStride;

    constexpr uint32_t bufferCount = 4u;
    enum Bindings
    {
        InputA,
        OutputBallots,
        OutputCounts,
        OutputPrimitives
    };

    de::MovePtr<BufferWithMemory> buffers[bufferCount];
    vk::VkDescriptorBufferInfo bufferDescriptors[bufferCount];

    uint32_t counts[bufferCount]{// InputA  { uint    a[]; } inputA;
                                 uint32_t(m_data.sizeX * m_data.sizeY),
                                 // OutputB { uvec2   b[]; } outputB;
                                 maxLoc,
                                 // OutputC { uint loc[]; } outputC;
                                 invocationStride,
                                 // OutputP { uint p[]; } outputP;
                                 uint32_t(outputP.size())};

    VkDeviceSize sizes[bufferCount]{// InputA  { uint    a[]; } inputA;
                                    counts[InputA] * sizeof(uint32_t),
                                    // OutputB { uvec2   b[]; } outputB;
                                    counts[OutputBallots] * sizeof(tcu::UVec4),
                                    // OutputC { uint loc[]; } outputC;
                                    counts[OutputCounts] * sizeof(uint32_t),
                                    // OutputP { uint p[]; } outputP;
                                    counts[OutputPrimitives] * sizeof(uint32_t)};

    const VkBufferUsageFlags cmnUsages = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VkBufferUsageFlags usages[bufferCount]{
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
    };

    // allocate buffers
    for (uint32_t i = 0; i < bufferCount; ++i)
    {
        if (sizes[i] > limits.maxStorageBufferRange)
            TCU_THROW(NotSupportedError, "Storage buffer size larger than device limits");
        try
        {
            buffers[i] = de::MovePtr<BufferWithMemory>(
                new BufferWithMemory(vk, device, allocator, makeBufferCreateInfo(sizes[i], usages[i] | cmnUsages),
                                     MemoryRequirement::HostVisible | MemoryRequirement::Cached));
        }
        catch (tcu::ResourceError &)
        {
            // Allocation size is unpredictable and can be too large for some systems. Don't treat allocation failure as a test failure.
            return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING,
                                   "Failed device memory allocation " + de::toString(sizes[i]) + " bytes");
        }
        bufferDescriptors[i] = makeDescriptorBufferInfo(**buffers[i], 0, sizes[i]);
    }

    // get raw pointers to previously allocated buffers
    void *ptrs[bufferCount];
    for (uint32_t i = 0; i < bufferCount; ++i)
    {
        ptrs[i] = (uint32_t *)buffers[i]->getAllocation().getHostPtr();
    }

    // populate buffers with their destination
    {
        auto rangeBufferA = makeStdBeginEnd<uint32_t>(ptrs[InputA], counts[InputA]);
        std::iota(rangeBufferA.first, rangeBufferA.second, 0u);
    }
    deMemset(ptrs[OutputBallots], 0, (size_t)sizes[OutputBallots]);
    deMemset(ptrs[OutputCounts], 0, (size_t)sizes[OutputCounts]);
    deMemset(ptrs[OutputPrimitives], 0, (size_t)sizes[OutputPrimitives]);

    // (...) and flush them to the GPU
    for (uint32_t i = 0; i < bufferCount; ++i)
    {
        flushAlloc(vk, device, buffers[i]->getAllocation());
    }

    VkDescriptorType descTypes[bufferCount]{
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    };

    vk::DescriptorSetLayoutBuilder layoutBuilder;
    for (uint32_t i = 0; i < bufferCount; ++i)
    {
        layoutBuilder.addSingleBinding(descTypes[i], m_data.shaderStage);
    }
    vk::Unique<vk::VkDescriptorSetLayout> descriptorSetLayout(layoutBuilder.build(vk, device));

    vk::DescriptorPoolBuilder poolBuilder;
    for (uint32_t i = 0; i < bufferCount; ++i)
    {
        poolBuilder.addType(descTypes[i], 1);
    }
    vk::Unique<vk::VkDescriptorPool> descriptorPool(
        poolBuilder.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));
    vk::Unique<vk::VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

    vk::DescriptorSetUpdateBuilder setUpdateBuilder;
    for (uint32_t i = 0; i < bufferCount; ++i)
    {
        setUpdateBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(i), descTypes[i],
                                     &bufferDescriptors[i]);
    }
    setUpdateBuilder.update(vk, device);

    const VkPushConstantRange pushConstantRange{
        (VkShaderStageFlags)m_data.shaderStage, // VkShaderStageFlags stageFlags;
        0u,                                     // uint32_t offset;
        sizeof(PushConstant)                    // uint32_t size;
    };

    const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // sType
        DE_NULL,                                       // pNext
        (VkPipelineLayoutCreateFlags)0,                // flags
        1u,                                            // setLayoutCount
        &descriptorSetLayout.get(),                    // pSetLayouts
        1u,                                            // pushConstantRangeCount
        &pushConstantRange,                            // pPushConstantRanges
    };

    const uint32_t imageWidth  = m_data.sizeX;
    const uint32_t imageHeight = m_data.sizeY;
    const VkFormat format      = VK_FORMAT_R8G8B8A8_UNORM;
    const VkImageCreateInfo imageCreateInfo{
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        VkImageCreateFlags(0),               // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
        format,                              // VkFormat format;
        {imageWidth, imageHeight, 1u},       // VkExtent3D extent;
        1u,                                  // uint32_t mipLevels;
        1u,                                  // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        0u,                                  // uint32_t queueFamilyIndexCount;
        0u,                                  // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED            // VkImageLayout initialLayout;
    };
    const VkImageSubresourceRange rscRange = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    de::MovePtr<ImageWithMemory> image(
        new ImageWithMemory(vk, device, allocator, imageCreateInfo, vk::MemoryRequirement::Any));
    Move<VkImageView> view        = makeImageView(vk, device, **image, VK_IMAGE_VIEW_TYPE_2D, format, rscRange);
    Move<VkRenderPass> renderPass = makeRenderPass(vk, device, format);
    Move<VkFramebuffer> framebuffer =
        makeFramebuffer(vk, device, *renderPass, *view, m_data.sizeX, m_data.sizeY, rscRange.layerCount);
    de::MovePtr<BufferWithMemory> vertexBuffer  = createVertexBufferAndFlush(m_data.sizeX, m_data.sizeY, topology);
    const VkRenderPassBeginInfo renderBeginInfo = makeRenderPassBeginInfo(*renderPass, *framebuffer);
    const Shaders shaders                       = createShaders();
    Move<VkPipelineLayout> pipelineLayout       = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo, NULL);
    Move<VkPipeline> pipeline = createGraphicsPipeline(*pipelineLayout, *renderPass, imageWidth, imageHeight, shaders,
                                                       VK_PRIMITIVE_TOPOLOGY_POINT_LIST);
    Move<VkCommandPool> cmdPool =
        createCommandPool(vk, device, vk::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueIndex);
    Move<VkCommandBuffer> cmdBuffer = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    PushConstant pc{};
    pc.invocationStride      = invocationStride;
    pc.width                 = m_data.sizeX;
    pc.height                = m_data.sizeY;
    pc.enableInvocationIndex = VK_FALSE;

    auto callRecordDrawingAndSubmit = std::bind(&ReconvergenceTestGraphicsInstance::recordDrawingAndSubmit, this,
                                                *cmdBuffer, *pipelineLayout, *pipeline, *descriptorSet, std::cref(pc),
                                                std::cref(renderBeginInfo), **vertexBuffer, invocationStride, **image);

    // compute "maxLoc", which is a potential maximum number of locations written
    callRecordDrawingAndSubmit();

    // Take the maximum of "maxLoc" over all invocations.
    invalidateAlloc(vk, device, buffers[OutputCounts]->getAllocation());
    auto rangeLoc               = makeStdBeginEnd<const uint32_t>(ptrs[OutputCounts], invocationStride);
    const uint32_t shaderMaxLoc = (*max_element(rangeLoc.first, rangeLoc.second));
    log << tcu::TestLog::Message << "computed maxLoc in shader: " << shaderMaxLoc << tcu::TestLog::EndMessage;

    // If we need more space, reallocate OutputB::b[] aka buffers[1]
    if (shaderMaxLoc > hostMaxLoc)
    {
        // Add one (to make sure no additional writes are done) and multiply by
        // the number of invocations and current primitive count
        maxLoc                = (std::max(shaderMaxLoc, hostMaxLoc) + 1u) * invocationStride;
        counts[OutputBallots] = maxLoc;
        sizes[OutputBallots]  = counts[OutputBallots] * sizeof(tcu::UVec4);

        if (sizes[OutputBallots] > limits.maxStorageBufferRange)
            TCU_THROW(NotSupportedError, "Storage buffer size larger than device limits");

        try
        {
            buffers[OutputBallots] = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
                vk, device, allocator, makeBufferCreateInfo(sizes[1], VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | cmnUsages),
                MemoryRequirement::HostVisible | MemoryRequirement::Cached));
        }
        catch (tcu::ResourceError &)
        {
            // Allocation size is unpredictable and can be too large for some systems. Don't treat allocation failure as a test failure.
            return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING,
                                   "Failed device memory allocation " + de::toString(sizes[OutputBallots]) + " bytes");
        }
        bufferDescriptors[OutputBallots] = makeDescriptorBufferInfo(**buffers[OutputBallots], 0, sizes[OutputBallots]);
        ptrs[OutputBallots]              = buffers[OutputBallots]->getAllocation().getHostPtr();

        vk::DescriptorSetUpdateBuilder setUpdateBuilder2;
        setUpdateBuilder2.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(OutputBallots),
                                      VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptors[OutputBallots]);
        setUpdateBuilder2.update(vk, device);
    }

    // Clear any writes to ballots/stores OutputB::b[] aka buffer[1] during the counting pass
    // Note that its size would may change since the first memory allocation
    deMemset(ptrs[OutputBallots], 0, (size_t)sizes[OutputBallots]);
    deMemset(ptrs[OutputCounts], 0, (size_t)sizes[OutputCounts]);
    deMemset(ptrs[OutputPrimitives], 0, (size_t)sizes[OutputPrimitives]);

    // flush them all to the GPU
    flushAlloc(vk, device, buffers[OutputBallots]->getAllocation());
    flushAlloc(vk, device, buffers[OutputCounts]->getAllocation());
    flushAlloc(vk, device, buffers[OutputPrimitives]->getAllocation());

    // run the actual shader with updated PushConstant
    pc.enableInvocationIndex = VK_TRUE;
    callRecordDrawingAndSubmit();

    invalidateAlloc(vk, device, buffers[OutputCounts]->getAllocation());
    const uint32_t finalShaderMaxLoc = (*max_element(rangeLoc.first, rangeLoc.second));
    log << tcu::TestLog::Message << "final shaderMaxLoc: " << finalShaderMaxLoc << tcu::TestLog::EndMessage;
    if (finalShaderMaxLoc != shaderMaxLoc)
    {
        return tcu::TestStatus(QP_TEST_RESULT_QUALITY_WARNING,
                               "maxLoc differs across shader invocations, expected: " + de::toString(shaderMaxLoc) +
                                   " got: " + de::toString(finalShaderMaxLoc));
    }

    invalidateAlloc(vk, device, buffers[OutputBallots]->getAllocation());
    const tcu::UVec4 *ballots = static_cast<tcu::UVec4 *>(ptrs[OutputBallots]);

    invalidateAlloc(vk, device, buffers[OutputPrimitives]->getAllocation());
    auto outputPrange = makeStdBeginEnd<uint32_t>(ptrs[OutputPrimitives], counts[OutputPrimitives]);
    std::copy(outputPrange.first, outputPrange.second, outputP.begin());

    try
    {
        ref.resize(counts[OutputBallots], tcu::UVec4(0u, 0u, 0u, 0u));
    }
    catch (const std::bad_alloc &)
    {
        // Allocation size is unpredictable and can be too large for some systems. Don't treat allocation failure as a test failure.
        return tcu::TestStatus(QP_TEST_RESULT_NOT_SUPPORTED,
                               "Failed system memory allocation " + de::toString(maxLoc * sizeof(uint64_t)) + " bytes");
    }

    // Simulate execution on the CPU, and compare against the GPU result
    const uint32_t finalHostMaxLoc = program->execute(m_context.getTestContext().getWatchDog(), false, m_subgroupSize,
                                                      fragmentStride, invocationStride, ref, log, outputP, ballots);

    const qpTestResult res = calculateAndLogResultEx(log, ballots, ref, finalHostMaxLoc, PrintMode::None);

    return tcu::TestStatus(res, qpGetTestResultName(res));
}

qpTestResult_e ReconvergenceTestGeometryInstance::calculateAndLogResultEx(add_ref<tcu::TestLog> log,
                                                                          const tcu::UVec4 *result,
                                                                          const std::vector<tcu::UVec4> &ref,
                                                                          const uint32_t maxLoc,
                                                                          const PrintMode printMode)
{
    DE_UNREF(maxLoc);
    DE_UNREF(printMode);

    qpTestResult res                  = QP_TEST_RESULT_PASS;
    uint32_t mismatchCount            = 0u;
    const uint32_t printMismatchCount = 5u;

    // With maximal reconvergence, we should expect the output to exactly match the reference.
    const uint32_t ballotStoreCount = static_cast<uint32_t>(ref.size());
    for (uint32_t i = 0; i < ballotStoreCount; ++i)
    {
        const Ballot resultVal(result[i], m_subgroupSize);
        const Ballot refVal(ref.at(i), m_subgroupSize);
        if (resultVal != refVal)
        {
            if (mismatchCount++ < printMismatchCount)
            {
                res = QP_TEST_RESULT_FAIL;
                log << tcu::TestLog::Message << "Mismatch at " << i << "\nexpected: " << resultVal
                    << "\n     got: " << refVal << tcu::TestLog::EndMessage;
                if (printMode == PrintMode::Console)
                {
                    std::cout << "Mismatch at " << i << "\nexpected: " << resultVal << "\n     got: " << refVal
                              << std::endl;
                }
            }
        }
    }

    log << tcu::TestLog::Message << "Mismatch count: " << mismatchCount << " from " << ballotStoreCount
        << tcu::TestLog::EndMessage;
    if (printMode == PrintMode::Console)
    {
        std::cout << "Mismatch count: " << mismatchCount << " from " << ballotStoreCount << std::endl;
    }

    return res;
}

void createAmberFragmentTestCases(add_ref<tcu::TestContext> testCtx, add_ptr<tcu::TestCaseGroup> group);

tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name, bool createExperimental)
{
    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, name.c_str(), "reconvergence tests"));

    typedef struct
    {
        uint32_t value;
        const char *name;
        const char *description;
    } TestGroupCase;

    TestGroupCase ttCases[] = {
        {TT_SUCF_ELECT, "subgroup_uniform_control_flow_elect", "subgroup_uniform_control_flow_elect"},
        {TT_SUCF_BALLOT, "subgroup_uniform_control_flow_ballot", "subgroup_uniform_control_flow_ballot"},
        {TT_WUCF_ELECT, "workgroup_uniform_control_flow_elect", "workgroup_uniform_control_flow_elect"},
        {TT_WUCF_BALLOT, "workgroup_uniform_control_flow_ballot", "workgroup_uniform_control_flow_ballot"},
        {TT_MAXIMAL, "maximal", "maximal"},
    };

    std::pair<VkShaderStageFlagBits, const char *> const stTypes[]{
        {VK_SHADER_STAGE_COMPUTE_BIT, "compute"},
        {VK_SHADER_STAGE_FRAGMENT_BIT, "fragment"},
#ifdef INCLUDE_GRAPHICS_TESTS
        {VK_SHADER_STAGE_VERTEX_BIT, "vertex"},
        {VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, "tessctrl"},
        {VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, "tesseval"},
        {VK_SHADER_STAGE_GEOMETRY_BIT, "geometry"},
#endif
    };

    for (int ttNdx = 0; ttNdx < DE_LENGTH_OF_ARRAY(ttCases); ttNdx++)
    {
        de::MovePtr<tcu::TestCaseGroup> ttGroup(
            new tcu::TestCaseGroup(testCtx, ttCases[ttNdx].name, ttCases[ttNdx].description));

        for (int stNdx = 0; stNdx < DE_LENGTH_OF_ARRAY(stTypes); ++stNdx)
        {
            // Only 'maximal' tests can process this loop when we are dealing with various kind of shaders,
            if (stTypes[stNdx].first != VK_SHADER_STAGE_COMPUTE_BIT && ttCases[ttNdx].value != TT_MAXIMAL)
                continue;

            de::MovePtr<tcu::TestCaseGroup> shaderGroup(new tcu::TestCaseGroup(testCtx, stTypes[stNdx].second, ""));

            uint32_t nNdx = 2;

            if (stTypes[stNdx].first == VK_SHADER_STAGE_FRAGMENT_BIT)
            {
                nNdx = 7;
                createAmberFragmentTestCases(testCtx, shaderGroup.get());
            }

            for (/*uint32_t nNdx = 2*/; nNdx <= 6; nNdx++)
            {
                de::MovePtr<tcu::TestCaseGroup> nestGroup(
                    new tcu::TestCaseGroup(testCtx, ("nesting" + de::toString(nNdx)).c_str(), ""));

                uint32_t seed = 0;

                for (int sNdx = 0; sNdx < 8; sNdx++)
                {
                    de::MovePtr<tcu::TestCaseGroup> seedGroup(
                        new tcu::TestCaseGroup(testCtx, de::toString(sNdx).c_str(), ""));

                    uint32_t numTests = 0;
                    switch (nNdx)
                    {
                    default:
                        DE_ASSERT(0);
                        // fallthrough
                    case 2:
                    case 3:
                    case 4:
                        numTests = 250;
                        break;
                    case 5:
                        numTests = 100;
                        break;
                    case 6:
                        numTests = 50;
                        break;
                    }

                    if (ttCases[ttNdx].value != TT_MAXIMAL)
                    {
                        if (nNdx >= 5)
                            continue;
                    }

                    for (uint32_t ndx = 0; ndx < numTests; ndx++)
                    {
                        uint32_t dim = 0u;
                        DE_UNREF(dim);
                        uint32_t sizeX = 0u;
                        uint32_t sizeY = 0u;
                        switch (stTypes[stNdx].first)
                        {
                        case VK_SHADER_STAGE_COMPUTE_BIT:
                            // we want to test at least full subgroup
                            // both are primary numbers
                            sizeX = 13u;
                            sizeY = 19u;
                            break;
                        case VK_SHADER_STAGE_FRAGMENT_BIT:
                            sizeX = 32;
                            sizeY = 32;
                            break;
                        case VK_SHADER_STAGE_VERTEX_BIT:
                            // we want to test at least full subgroup
                            dim   = uint32_t(std::ceil(
                                std::sqrt((double)(((128u + 31u) * 100u) / VertexRandomProgram::fillPercentage))));
                            sizeX = dim;
                            sizeY = dim;
                            break;
                        case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
                            sizeX = 19; // positive number of desired subgroups
                            sizeY = 1;  // used only for framebuffer extent in TCS test
                            break;
                        case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
                            sizeX = 23; // positive number of desired subgroups
                            sizeY = 1;  // used only for framebuffer extent in TES test
                            break;
                        case VK_SHADER_STAGE_GEOMETRY_BIT:
                            // we want to test at least full subgroup
                            dim   = uint32_t(std::ceil(
                                std::sqrt((double)(((128u + 29u) * 100u) / GeometryRandomProgram::fillPercentage))));
                            sizeX = dim;
                            sizeY = dim;
                            break;
                        default:
                            DE_ASSERT(0);
                        }
                        CaseDef c = {
                            stTypes[stNdx].first,           // VkShaderStageFlagBits    shaderStage
                            (TestType)ttCases[ttNdx].value, // TestType testType;
                            nNdx,                           // uint32_t maxNesting;
                            seed,                           // uint32_t seed;
                            sizeX,                          // uint32_t sizeX;
                            sizeY                           // uint32_t sizeY;
                        };
                        // product of sizeX and sizeY must not exceed MAX_INVOCATIONS_ALL_TESTS
                        DE_ASSERT(c.verify());
                        seed++;

                        bool isExperimentalTest = (ndx >= numTests / 5);

                        if (createExperimental == isExperimentalTest)
                            seedGroup->addChild(new ReconvergenceTestCase(testCtx, de::toString(ndx).c_str(), c));
                    }
                    if (!seedGroup->empty())
                        nestGroup->addChild(seedGroup.release());
                }
                if (!nestGroup->empty())
                    shaderGroup->addChild(nestGroup.release());
            }
            if (!shaderGroup->empty())
                ttGroup->addChild(shaderGroup.release());
        }
        group->addChild(ttGroup.release());
    }

    return group.release();
}

void createAmberFragmentTestCases(add_ref<tcu::TestContext> testCtx, add_ptr<tcu::TestCaseGroup> group)
{
    using namespace cts_amber;

    enum Tests
    {
        TERMINATE_INVOCATION,
        DEMOTE_INVOCATION,
        DEMOTE_ENTIRE_QUAD,
        DEMOTE_HALF_QUAD_TOP,
        DEMOTE_HALF_QUAD_RIGHT,
        DEMOTE_HALF_QUAD_BOTTOM,
        DEMOTE_HALF_QUAD_LEFT,
        DEMOTE_HALF_QUAD_SLASH,
        DEMOTE_HALF_QUAD_BACKSLASH
    };

    struct Case
    {
        Tests test;
        add_cptr<char> name;
        add_cptr<char> desc;
        std::size_t hname;
        Case(Tests aTest, add_cptr<char> aName, add_cptr<char> aDesc)
            : test(aTest)
            , name(aName)
            , desc(aDesc)
            , hname(std::hash<std::string>()(std::string(aName)))
        {
        }
        bool matches(add_cref<std::string> aName) const
        {
            return hname == std::hash<std::string>()(aName);
        }
        static bool matches(add_cref<std::string> aName, std::initializer_list<Case> aList)
        {
            for (auto i = aList.begin(); i != aList.end(); ++i)
            {
                if (i->matches(aName))
                    return true;
            }
            return false;
        }
        std::string makeFileName() const
        {
            return (std::string(name) + ".amber");
        }
    } static const cases[]{
        Case(TERMINATE_INVOCATION, "terminate_invocation",
             "Verifies that terminated invocation is no longer included in the ballot"),
        Case(DEMOTE_INVOCATION, "demote_invocation",
             "Verifies that the demoted invocation is not present in the ballot"),
        Case(DEMOTE_ENTIRE_QUAD, "demote_entire_quad", "Verifies that the demoted quad is not present in the ballot"),
        Case(DEMOTE_HALF_QUAD_TOP, "demote_half_quad_top",
             "Verifies that the demoted part of the quad is not present in the ballot"),
        Case(DEMOTE_HALF_QUAD_RIGHT, "demote_half_quad_right",
             "Verifies that the demoted part of the quad is not present in the ballot"),
        Case(DEMOTE_HALF_QUAD_BOTTOM, "demote_half_quad_bottom",
             "Verifies that the demoted part of the quad is not present in the ballot"),
        Case(DEMOTE_HALF_QUAD_LEFT, "demote_half_quad_left",
             "Verifies that the demoted part of the quad is not present in the ballot"),
        Case(DEMOTE_HALF_QUAD_SLASH, "demote_half_quad_slash",
             "Verifies that the demoted part of the quad is not present in the ballot"),
        Case(DEMOTE_HALF_QUAD_BACKSLASH, "demote_half_quad_backslash",
             "Verifies that the demoted part of the quad is not present in the ballot"),
    };

    auto testSupports = [](Context &context, std::string testName) -> void
    {
        if (!(context.getSubgroupProperties().supportedStages & VK_SHADER_STAGE_FRAGMENT_BIT))
            TCU_THROW(NotSupportedError, "Subgroup operations not supported in fragment stage");

        if (!context.getShaderMaximalReconvergenceFeatures().shaderMaximalReconvergence)
            TCU_THROW(NotSupportedError, "shaderMaximalReconvergence not supported");

        if (!(context.getSubgroupProperties().supportedOperations & VK_SUBGROUP_FEATURE_BALLOT_BIT))
            TCU_THROW(NotSupportedError, "VK_SUBGROUP_FEATURE_BALLOT_BIT not supported");

        if (Case::matches(testName, {cases[DEMOTE_ENTIRE_QUAD]}))
        {
            if (!(context.getSubgroupProperties().subgroupSize > 4))
                TCU_THROW(NotSupportedError, "subgroupSize is less than or equal to 4");
        }
        else
        {
            if (!(context.getSubgroupProperties().subgroupSize >= 4))
                TCU_THROW(NotSupportedError, "subgroupSize is less than 4");
        }

        if (Case::matches(testName, {cases[TERMINATE_INVOCATION]}))
        {
            if (!context.getShaderTerminateInvocationFeatures().shaderTerminateInvocation)
                TCU_THROW(NotSupportedError, "shaderTerminateInvocation not supported.");
        }
        else
        {
#ifndef CTS_USES_VULKANSC
            if (!context.getShaderDemoteToHelperInvocationFeatures().shaderDemoteToHelperInvocation)
                TCU_THROW(NotSupportedError, "demoteToHelperInvocation not supported.");
#else
            if (!context.getShaderDemoteToHelperInvocationFeaturesEXT().shaderDemoteToHelperInvocation)
                TCU_THROW(NotSupportedError, "demoteToHelperInvocation not supported.");
#endif
        }
    };

    auto updateTest = [&](add_ptr<AmberTestCase> theTest) -> add_ptr<AmberTestCase>
    {
        theTest->setCheckSupportCallback(testSupports);
        return theTest;
    };

    const std::string testsFolder(std::string("reconvergence/maximal/") + group->getName());

    for (add_cref<Case> aCase : cases)
    {
        group->addChild(updateTest(
            createAmberTestCase(testCtx, aCase.name, aCase.desc, testsFolder.c_str(), aCase.makeFileName())));
    }
}

} // namespace

tcu::TestCaseGroup *createTests(tcu::TestContext &testCtx, const std::string &name)
{
    return createTests(testCtx, name, false);
}

tcu::TestCaseGroup *createTestsExperimental(tcu::TestContext &testCtx, const std::string &name)
{
    return createTests(testCtx, name, true);
}

} // namespace Reconvergence
} // namespace vkt
