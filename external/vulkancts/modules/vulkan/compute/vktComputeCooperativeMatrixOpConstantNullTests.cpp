/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
 * Copyright (c) 2018-2024 NVIDIA Corporation
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
 * \brief Vulkan Cooperative Matrix OpConstantNull tests
 *//*--------------------------------------------------------------------*/

#include "vkComputePipelineConstructionUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktTestCase.hpp"
#include "tcuFloat.hpp"
#include "tcuStringTemplate.hpp"
#include "vkStrUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBuilderUtil.hpp"

#include <algorithm>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <map>
#include <mutex>
#include <numeric>
#include <set>
#include <unordered_map>
#include <variant>
#include <vector>

namespace vkt
{
namespace compute
{
using namespace vk;

namespace
{

enum class Matrices : uint32_t
{
    All = 10u,
    A,
    B,
    C,
    R
};

static const std::set<VkComponentTypeKHR> PossiobleTypes{
    VK_COMPONENT_TYPE_FLOAT16_KHR,     VK_COMPONENT_TYPE_FLOAT32_KHR,     VK_COMPONENT_TYPE_FLOAT64_KHR,
    VK_COMPONENT_TYPE_SINT8_KHR,       VK_COMPONENT_TYPE_SINT16_KHR,      VK_COMPONENT_TYPE_SINT32_KHR,
    VK_COMPONENT_TYPE_SINT64_KHR,      VK_COMPONENT_TYPE_UINT8_KHR,       VK_COMPONENT_TYPE_UINT16_KHR,
    VK_COMPONENT_TYPE_UINT32_KHR,      VK_COMPONENT_TYPE_UINT64_KHR,      VK_COMPONENT_TYPE_BFLOAT16_KHR,
    VK_COMPONENT_TYPE_FLOAT8_E4M3_EXT, VK_COMPONENT_TYPE_FLOAT8_E5M2_EXT,
};

bool inPossibleTypes(VkComponentTypeKHR type)
{
    return PossiobleTypes.find(type) != PossiobleTypes.end();
}

bool isPossibleConfiguration(const VkCooperativeMatrixPropertiesKHR &p)
{
    return inPossibleTypes(p.AType) && inPossibleTypes(p.BType) && inPossibleTypes(p.CType) &&
           inPossibleTypes(p.ResultType) && p.scope == VK_SCOPE_SUBGROUP_KHR;
}

bool anyComponentOf(const VkCooperativeMatrixPropertiesKHR &p, std::initializer_list<VkComponentTypeKHR> components)
{
    for (const VkComponentTypeKHR &c : components)
    {
        if (p.AType == c || p.BType == c || p.CType == c || p.ResultType == c)
            return true;
    }
    return false;
}

bool anyComponentOf(const std::vector<VkCooperativeMatrixPropertiesKHR> &p,
                    std::initializer_list<VkComponentTypeKHR> components)
{
    for (const VkCooperativeMatrixPropertiesKHR &conf : p)
    {
        if (anyComponentOf(conf, components))
            return true;
    }
    return false;
}

bool has16BitTypes(const std::vector<VkCooperativeMatrixPropertiesKHR> &p)
{
    return anyComponentOf(p, {VK_COMPONENT_TYPE_SINT16_KHR, VK_COMPONENT_TYPE_UINT16_KHR,
                              VK_COMPONENT_TYPE_BFLOAT16_KHR, VK_COMPONENT_TYPE_FLOAT16_KHR});
}

bool hasInt8BitTypes(const std::vector<VkCooperativeMatrixPropertiesKHR> &p)
{
    return anyComponentOf(p, {VK_COMPONENT_TYPE_SINT8_KHR, VK_COMPONENT_TYPE_UINT8_KHR});
}

bool hasFloat8BitTypes(const std::vector<VkCooperativeMatrixPropertiesKHR> &p)
{
    return anyComponentOf(p, {VK_COMPONENT_TYPE_FLOAT8_E4M3_EXT, VK_COMPONENT_TYPE_FLOAT8_E5M2_EXT});
}

bool hasBFloat16Types(const std::vector<VkCooperativeMatrixPropertiesKHR> &p)
{
    return anyComponentOf(p, {VK_COMPONENT_TYPE_BFLOAT16_KHR});
}

std::vector<VkCooperativeMatrixPropertiesKHR> getPossibleConfigurations(const InstanceInterface &vki,
                                                                        VkPhysicalDevice device)
{
    uint32_t propertyCount                      = 0u;
    const VkCooperativeMatrixPropertiesKHR temp = initVulkanStructure();
    std::vector<VkCooperativeMatrixPropertiesKHR> available, possible;
    VK_CHECK(vki.getPhysicalDeviceCooperativeMatrixPropertiesKHR(device, &propertyCount, nullptr));
    available.resize(propertyCount, temp);
    VK_CHECK(vki.getPhysicalDeviceCooperativeMatrixPropertiesKHR(device, &propertyCount, available.data()));
    for (const VkCooperativeMatrixPropertiesKHR &p : available)
    {
        if (isPossibleConfiguration(p))
            possible.push_back(p);
    }
    return possible;
}

struct Params
{
    ComputePipelineConstructionType pipelineConstructionType;
    Matrices matrix;
};

class CoopMtxOpConstantNullInstance : public TestInstance
{
    const Params m_params;
    uint32_t m_iteration;
    uint32_t m_failCount;

public:
    struct Executor
    {
        Executor(Context &context, const VkCooperativeMatrixPropertiesKHR &conf, const Params &params);
        void execute(Matrices targetMatrix);
        std::vector<float> getMatrix(Matrices m) const;
        const VkCooperativeMatrixPropertiesKHR &getConfiguration() const
        {
            return m_configuration;
        }
        template <class Stream>
        void dumpMatrices(Stream &str, bool includeReference) const;

    private:
        template <class Stream>
        void dumpMatrix(Stream &str, const std::vector<float> matrix, uint32_t rows, uint32_t cols,
                        const std::string &name, VkComponentTypeKHR type) const;

        Context &m_context;
        const VkCooperativeMatrixPropertiesKHR m_configuration;
        de::MovePtr<BufferWithMemory> m_bufferA;
        de::MovePtr<BufferWithMemory> m_bufferB;
        de::MovePtr<BufferWithMemory> m_bufferC;
        de::MovePtr<BufferWithMemory> m_bufferR;
        Move<VkDescriptorSetLayout> m_descriptorSetLayout;
        Move<VkDescriptorPool> m_descriptorPool;
        Move<VkDescriptorSet> m_descriptorSet;
        de::MovePtr<ComputePipelineWrapper> m_pipeline;
        VkQueue m_queue;
        Move<VkCommandPool> m_commandPool;
        Move<VkCommandBuffer> m_commandBuffer;
    };

public:
    CoopMtxOpConstantNullInstance(Context &context, const Params &params)
        : TestInstance(context)
        , m_params(params)
        , m_iteration(0u)
        , m_failCount(0u)
    {
    }
    virtual tcu::TestStatus iterate() override;
    void logConfiguration(const VkCooperativeMatrixPropertiesKHR &conf, uint32_t number, tcu::TestLog &log) const;
    bool verifyResult(const Executor &executor, Matrices targetMatrix, std::string &errorMessage) const;
};

class CoopMtxOpConstantNullCase : public TestCase
{
    const Params m_params;
    inline static std::mutex m_configurationsMutex;
    inline static std::vector<VkCooperativeMatrixPropertiesKHR> m_configurations;

public:
    CoopMtxOpConstantNullCase(tcu::TestContext &testCtx, const std::string &name, const Params &params)
        : TestCase(testCtx, name)
        , m_params(params)
    {
    }
    virtual TestInstance *createInstance(Context &context) const override
    {
        Params params(m_params);
        return new CoopMtxOpConstantNullInstance(context, params);
    }
    virtual std::string getRequiredCapabilitiesId() const override
    {
        return std::type_index(typeid(this)).name();
    }
    static std::vector<VkCooperativeMatrixPropertiesKHR> &getConfigurations()
    {
        return m_configurations;
    }
    virtual void initDeviceCapabilities(DevCaps &caps) override;
    virtual void initPrograms(SourceCollections &programCollection) const override;
};

template <VkComponentTypeKHR>
struct vK_component_type_to_ctype_impl;
#define VKCOMPONENTTOTYPEIMPL(comp_, cpptype_)    \
    template <>                                   \
    struct vK_component_type_to_ctype_impl<comp_> \
    {                                             \
        typedef cpptype_ type;                    \
    }

VKCOMPONENTTOTYPEIMPL(VK_COMPONENT_TYPE_FLOAT16_KHR, tcu::Float16);
VKCOMPONENTTOTYPEIMPL(VK_COMPONENT_TYPE_BFLOAT16_KHR, tcu::BrainFloat16);
VKCOMPONENTTOTYPEIMPL(VK_COMPONENT_TYPE_FLOAT8_E4M3_EXT, tcu::FloatE4M3);
VKCOMPONENTTOTYPEIMPL(VK_COMPONENT_TYPE_FLOAT8_E5M2_EXT, tcu::FloatE5M2);
VKCOMPONENTTOTYPEIMPL(VK_COMPONENT_TYPE_FLOAT32_KHR, float);
VKCOMPONENTTOTYPEIMPL(VK_COMPONENT_TYPE_FLOAT64_KHR, double);
VKCOMPONENTTOTYPEIMPL(VK_COMPONENT_TYPE_SINT8_KHR, int8_t);
VKCOMPONENTTOTYPEIMPL(VK_COMPONENT_TYPE_SINT16_KHR, int16_t);
VKCOMPONENTTOTYPEIMPL(VK_COMPONENT_TYPE_SINT32_KHR, int32_t);
VKCOMPONENTTOTYPEIMPL(VK_COMPONENT_TYPE_SINT64_KHR, int64_t);
VKCOMPONENTTOTYPEIMPL(VK_COMPONENT_TYPE_UINT8_KHR, uint8_t);
VKCOMPONENTTOTYPEIMPL(VK_COMPONENT_TYPE_UINT16_KHR, uint16_t);
VKCOMPONENTTOTYPEIMPL(VK_COMPONENT_TYPE_UINT32_KHR, uint32_t);
VKCOMPONENTTOTYPEIMPL(VK_COMPONENT_TYPE_UINT64_KHR, uint32_t);

template <VkComponentTypeKHR comp_>
using vK_component_type_to_ctype = typename vK_component_type_to_ctype_impl<comp_>::type;

template <typename T, typename = void>
struct has_asFloat : std::false_type
{
};
template <typename T>
struct has_asFloat<T, std::void_t<decltype(std::declval<const T>().asFloat())>> : std::true_type
{
};
template <class ValueType, bool>
struct AsFloatAdapter
{
    float convert(const ValueType &v) const
    {
        return static_cast<float>(v);
    }
    ValueType construct(float f) const
    {
        return static_cast<ValueType>(f);
    }
};

template <class ValueType>
struct AsFloatAdapter<ValueType, true>
{
    float convert(const ValueType &v) const
    {
        return v.asFloat();
    }
    ValueType construct(float f) const
    {
        return ValueType(f);
    }
};

template <VkComponentTypeKHR comp_type>
struct ValueImpl
{
    typedef vK_component_type_to_ctype<comp_type> type;
    const VkComponentTypeKHR component = comp_type;
    type value{};
    uint32_t size() const
    {
        return uint32_t(sizeof(type));
    }
    std::vector<float> readBuffer(const BufferWithMemory &buffer, uint32_t elemCount) const
    {
        std::vector<float> result(elemCount);
        const type *data = reinterpret_cast<type *>(buffer.getAllocation().getHostPtr());
        const AsFloatAdapter<type, has_asFloat<type>::value> adapter;
        for (uint32_t i = 0; i < elemCount; ++i)
            result[i] = adapter.convert(data[i]);
        return result;
    }
    void writeBuffer(BufferWithMemory &buffer, const std::vector<float> &data) const
    {
        const uint32_t elemCount = uint32_t(data.size());
        type *access             = reinterpret_cast<type *>(buffer.getAllocation().getHostPtr());
        const AsFloatAdapter<type, has_asFloat<type>::value> adapter;
        for (uint32_t i = 0; i < elemCount; ++i)
            access[i] = adapter.construct(data[i]);
    }
};

static_assert(std::is_same_v<uint32_t, vK_component_type_to_ctype<VK_COMPONENT_TYPE_UINT32_KHR>>, "???");
static_assert(std::is_same_v<tcu::BrainFloat16, vK_component_type_to_ctype<VK_COMPONENT_TYPE_BFLOAT16_KHR>>, "???");

class ValueGenerator
{
    std::vector<float> m_values;
    uint32_t m_current;
    inline static uint32_t m_seed;

public:
    ValueGenerator(VkComponentTypeKHR type) : m_values(), m_current(0)
    {
        switch (type)
        {
        case VK_COMPONENT_TYPE_FLOAT8_E4M3_EXT:
        case VK_COMPONENT_TYPE_FLOAT8_E5M2_EXT:
        case VK_COMPONENT_TYPE_FLOAT16_KHR:
        case VK_COMPONENT_TYPE_BFLOAT16_KHR:
        case VK_COMPONENT_TYPE_FLOAT32_KHR:
        case VK_COMPONENT_TYPE_FLOAT64_KHR:
            m_values = {-1.0f, -0.25f, 0.0f, +0.25f, +1.0f, +0.5f};
            break;
        case VK_COMPONENT_TYPE_SINT8_KHR:
        case VK_COMPONENT_TYPE_SINT16_KHR:
        case VK_COMPONENT_TYPE_SINT32_KHR:
        case VK_COMPONENT_TYPE_SINT64_KHR:
            m_values = {0.0f, -1.0f, +1.0f, +1.0f};
            break;
        case VK_COMPONENT_TYPE_UINT8_KHR:
        case VK_COMPONENT_TYPE_UINT16_KHR:
        case VK_COMPONENT_TYPE_UINT32_KHR:
        case VK_COMPONENT_TYPE_UINT64_KHR:
            m_values = {1.0f, 0.0f, 1.0f, 1.0f};
            break;
        default:
            DE_ASSERT(false);
        }

        m_seed    = m_seed + 1u;
        m_current = uint32_t(m_seed % m_values.size());
    }
    float next()
    {
        const float val = m_values[m_current];
        m_current       = uint32_t((m_current + 1u) % m_values.size());
        return val;
    }
};

struct Value
{
    std::variant<ValueImpl<VK_COMPONENT_TYPE_UINT8_KHR>, ValueImpl<VK_COMPONENT_TYPE_SINT8_KHR>,
                 ValueImpl<VK_COMPONENT_TYPE_UINT16_KHR>, ValueImpl<VK_COMPONENT_TYPE_SINT16_KHR>,
                 ValueImpl<VK_COMPONENT_TYPE_UINT32_KHR>, ValueImpl<VK_COMPONENT_TYPE_SINT32_KHR>,
                 ValueImpl<VK_COMPONENT_TYPE_UINT64_KHR>, ValueImpl<VK_COMPONENT_TYPE_SINT64_KHR>,
                 ValueImpl<VK_COMPONENT_TYPE_FLOAT8_E4M3_EXT>, ValueImpl<VK_COMPONENT_TYPE_FLOAT8_E5M2_EXT>,
                 ValueImpl<VK_COMPONENT_TYPE_BFLOAT16_KHR>, ValueImpl<VK_COMPONENT_TYPE_FLOAT16_KHR>,
                 ValueImpl<VK_COMPONENT_TYPE_FLOAT32_KHR>, ValueImpl<VK_COMPONENT_TYPE_FLOAT64_KHR>>
        value;
    uint32_t size() const
    {
        return std::visit([](const auto &x) { return x.size(); }, value);
    }
    template <VkComponentTypeKHR type>
    void create()
    {
        value.emplace<ValueImpl<type>>();
    }
    Value(VkComponentTypeKHR type)
    {
        static std::unordered_map<VkComponentTypeKHR, void (Value::*)()> factory{
            {VK_COMPONENT_TYPE_UINT8_KHR, &Value::create<VK_COMPONENT_TYPE_UINT8_KHR>},
            {VK_COMPONENT_TYPE_SINT8_KHR, &Value::create<VK_COMPONENT_TYPE_SINT8_KHR>},
            {VK_COMPONENT_TYPE_UINT16_KHR, &Value::create<VK_COMPONENT_TYPE_UINT16_KHR>},
            {VK_COMPONENT_TYPE_SINT16_KHR, &Value::create<VK_COMPONENT_TYPE_SINT16_KHR>},
            {VK_COMPONENT_TYPE_UINT32_KHR, &Value::create<VK_COMPONENT_TYPE_UINT32_KHR>},
            {VK_COMPONENT_TYPE_SINT32_KHR, &Value::create<VK_COMPONENT_TYPE_SINT32_KHR>},
            {VK_COMPONENT_TYPE_UINT64_KHR, &Value::create<VK_COMPONENT_TYPE_UINT64_KHR>},
            {VK_COMPONENT_TYPE_SINT64_KHR, &Value::create<VK_COMPONENT_TYPE_SINT64_KHR>},
            {VK_COMPONENT_TYPE_FLOAT8_E4M3_EXT, &Value::create<VK_COMPONENT_TYPE_FLOAT8_E4M3_EXT>},
            {VK_COMPONENT_TYPE_FLOAT8_E5M2_EXT, &Value::create<VK_COMPONENT_TYPE_FLOAT8_E5M2_EXT>},
            {VK_COMPONENT_TYPE_BFLOAT16_KHR, &Value::create<VK_COMPONENT_TYPE_BFLOAT16_KHR>},
            {VK_COMPONENT_TYPE_FLOAT16_KHR, &Value::create<VK_COMPONENT_TYPE_FLOAT16_KHR>},
            {VK_COMPONENT_TYPE_FLOAT32_KHR, &Value::create<VK_COMPONENT_TYPE_FLOAT32_KHR>},
            {VK_COMPONENT_TYPE_FLOAT64_KHR, &Value::create<VK_COMPONENT_TYPE_FLOAT64_KHR>},
        };
        (this->*factory[type])();
    }
    std::vector<float> readBuffer(const BufferWithMemory &buffer, uint32_t elemCount) const
    {
        return std::visit([&](const auto &x) { return x.readBuffer(buffer, elemCount); }, value);
    }
    void writeBuffer(BufferWithMemory &buffer, const std::vector<float> &data) const
    {
        return std::visit([&](const auto &x) { x.writeBuffer(buffer, data); }, value);
    }
    std::vector<std::string> getSpirvExtensions() const
    {
        return makeSpirvExtensions(std::visit([&](const auto &x) { return x.component; }, value));
    }
    std::vector<std::string> getSpirvCapabilities() const
    {
        return makeSpirvCapabilities(std::visit([&](const auto &x) { return x.component; }, value));
    }
    std::pair<std::string, std::string> getSpirvNames() const
    {
        return makeSpirvNames(std::visit([&](const auto &x) { return x.component; }, value));
    }
    static std::vector<std::string> makeSpirvExtensions(VkComponentTypeKHR type)
    {
        std::vector<std::string> exts;
        switch (type)
        {
        case VK_COMPONENT_TYPE_BFLOAT16_KHR:
            exts.push_back("SPV_KHR_bfloat16");
            exts.push_back("SPV_KHR_16bit_storage");
            break;
        case VK_COMPONENT_TYPE_FLOAT16_KHR:
            exts.push_back("SPV_KHR_16bit_storage");
            break;
        case VK_COMPONENT_TYPE_SINT16_KHR:
        case VK_COMPONENT_TYPE_UINT16_KHR:
            exts.push_back("SPV_KHR_16bit_storage");
            break;
        case VK_COMPONENT_TYPE_FLOAT8_E4M3_EXT:
        case VK_COMPONENT_TYPE_FLOAT8_E5M2_EXT:
            exts.push_back("SPV_EXT_float8");
            exts.push_back("SPV_KHR_8bit_storage");
            break;
        case VK_COMPONENT_TYPE_SINT8_KHR:
        case VK_COMPONENT_TYPE_UINT8_KHR:
            exts.push_back("SPV_KHR_8bit_storage");
            break;
        default:
            break;
        }
        return exts;
    }
    static std::vector<std::string> makeSpirvCapabilities(VkComponentTypeKHR type)
    {
        std::vector<std::string> caps;

        switch (type)
        {
        case VK_COMPONENT_TYPE_UINT8_KHR:
        case VK_COMPONENT_TYPE_SINT8_KHR:
            caps.push_back("Int8");
            caps.push_back("StorageBuffer8BitAccess");
            break;
        case VK_COMPONENT_TYPE_UINT16_KHR:
        case VK_COMPONENT_TYPE_SINT16_KHR:
            caps.push_back("Int16");
            caps.push_back("StorageBuffer16BitAccess");
            break;
        case VK_COMPONENT_TYPE_UINT64_KHR:
        case VK_COMPONENT_TYPE_SINT64_KHR:
            caps.push_back("Int64");
            break;
        case VK_COMPONENT_TYPE_FLOAT8_E4M3_EXT:
        case VK_COMPONENT_TYPE_FLOAT8_E5M2_EXT:
            caps.push_back("Float8EXT");
            caps.push_back("StorageBuffer8BitAccess");
            caps.push_back("Float8CooperativeMatrixEXT");
            break;
        case VK_COMPONENT_TYPE_BFLOAT16_KHR:
            caps.push_back("BFloat16TypeKHR");
            caps.push_back("StorageBuffer16BitAccess");
            caps.push_back("BFloat16CooperativeMatrixKHR");
            break;
        case VK_COMPONENT_TYPE_FLOAT16_KHR:
            caps.push_back("Float16");
            caps.push_back("StorageBuffer16BitAccess");
        default:
            break;
        }
        return caps;
    }
    static std::pair<std::string, std::string> makeSpirvNames(VkComponentTypeKHR type)
    {
        std::pair<std::string, std::string> names;
        switch (type)
        {
        case VK_COMPONENT_TYPE_FLOAT8_E4M3_EXT:
            names.first  = "%e4m3";
            names.second = "OpTypeFloat 8 Float8E4M3EXT";
            break;
        case VK_COMPONENT_TYPE_FLOAT8_E5M2_EXT:
            names.first  = "%e5m2";
            names.second = "OpTypeFloat 8 Float8E5M2EXT";
            break;
        case VK_COMPONENT_TYPE_BFLOAT16_KHR:
            names.first  = "%brainfloat";
            names.second = "OpTypeFloat 16 BFloat16KHR";
            break;
        case VK_COMPONENT_TYPE_FLOAT16_KHR:
            names.first  = "%half";
            names.second = "OpTypeFloat 16";
            break;
        case VK_COMPONENT_TYPE_FLOAT32_KHR:
            names.first  = "%float";
            names.second = "OpTypeFloat 32";
            break;
        case VK_COMPONENT_TYPE_FLOAT64_KHR:
            names.first  = "%double";
            names.second = "OpTypeFloat 64";
            break;
        case VK_COMPONENT_TYPE_SINT8_KHR:
            names.first  = "%char";
            names.second = "OpTypeInt 8 1";
            break;
        case VK_COMPONENT_TYPE_SINT16_KHR:
            names.first  = "%short";
            names.second = "OpTypeInt 16 1";
            break;
        case VK_COMPONENT_TYPE_SINT32_KHR:
            names.first  = "%int";
            names.second = "OpTypeInt 32 1";
            break;
        case VK_COMPONENT_TYPE_SINT64_KHR:
            names.first  = "%long";
            names.second = "OpTypeInt 64 1";
            break;
        case VK_COMPONENT_TYPE_UINT8_KHR:
            names.first  = "%uchar";
            names.second = "OpTypeInt 8 0";
            break;
        case VK_COMPONENT_TYPE_UINT16_KHR:
            names.first  = "%ushort";
            names.second = "OpTypeInt 16 0";
            break;
        case VK_COMPONENT_TYPE_UINT32_KHR:
            names.first  = "%uint";
            names.second = "OpTypeInt 32 0";
            break;
        case VK_COMPONENT_TYPE_UINT64_KHR:
            names.first  = "%ulong";
            names.second = "OpTypeInt 64 0";
            break;
        default:
            break;
        }
        return names;
    }
    std::string getMatrixOperand(Matrices m)
    {
        return makeMatrixOperand(std::visit([&](const auto &x) { return x.component; }, value), m);
    }
    static std::string makeMatrixOperand(VkComponentTypeKHR type, Matrices m)
    {
        bool hasSign = false;

        switch (type)
        {
        case VK_COMPONENT_TYPE_FLOAT16_KHR:
        case VK_COMPONENT_TYPE_FLOAT32_KHR:
        case VK_COMPONENT_TYPE_FLOAT64_KHR:
        case VK_COMPONENT_TYPE_SINT8_KHR:
        case VK_COMPONENT_TYPE_SINT16_KHR:
        case VK_COMPONENT_TYPE_SINT32_KHR:
        case VK_COMPONENT_TYPE_SINT64_KHR:
        case VK_COMPONENT_TYPE_BFLOAT16_KHR:
        case VK_COMPONENT_TYPE_SINT8_PACKED_NV:
        case VK_COMPONENT_TYPE_FLOAT_E4M3_NV:
        case VK_COMPONENT_TYPE_FLOAT_E5M2_NV:
            hasSign = true;
            break;
        default:
            break;
        }

        if (false == hasSign)
        {
            return std::string();
        }

        static const std::map<Matrices, std::string> names{
            {Matrices::A, "A"}, {Matrices::B, "B"}, {Matrices::C, "C"}, {Matrices::R, "Result"}};
        DE_ASSERT(names.find(m) != names.end());

        return "Matrix" + names.at(m) + "SignedComponentsKHR";
    }
};

std::vector<float> mulMatrices(const std::vector<float> &A, const std::vector<float> &B, const uint32_t rowCountOfA,
                               const uint32_t colCountOfB)
{
    std::vector<float> R(rowCountOfA * colCountOfB);
    const uint32_t K           = uint32_t(A.size()) / rowCountOfA;
    const uint32_t rowCountOfB = uint32_t(B.size()) / colCountOfB;
    DE_ASSERT(K == rowCountOfB);
    DE_UNREF(rowCountOfB);

    for (uint32_t row = 0; row < rowCountOfA; ++row)
    {
        for (uint32_t col = 0; col < colCountOfB; ++col)
        {
            float sum = 0.0f;
            for (uint32_t k = 0; k < K; ++k)
            {
                float a = A[row * K + k];
                float b = B[k * colCountOfB + col];
                sum += a * b;
            }
            R[row * colCountOfB + col] = sum;
        }
    }
    return R;
}

std::vector<float> addMatrices(const std::vector<float> &A, const std::vector<float> &B)
{
    const uint32_t N = uint32_t(A.size());
    DE_ASSERT(N == B.size());
    std::vector<float> R(N);
    for (uint32_t i = 0u; i < N; ++i)
        R[i] = A[i] + B[i];
    return R;
}

bool isNullMatrix(const std::vector<float> &mat)
{
    DE_ASSERT(mat.size());
    return std::all_of(mat.begin(), mat.end(), [](const float x) { return x == 0.0f; });
}

std::string genShaderName(const VkCooperativeMatrixPropertiesKHR &p)
{
    int i = 0;
    std::ostringstream os;
    std::map<VkComponentTypeKHR, int> m;
    for (VkComponentTypeKHR c : PossiobleTypes)
        m.insert(std::make_pair(c, i++));
    os << m[p.AType];
    os << '-' << m[p.BType];
    os << '-' << m[p.CType];
    os << '-' << m[p.ResultType];
    os << '-' << int(p.scope);
    return os.str();
}

std::string genShaderCode(const VkCooperativeMatrixPropertiesKHR &conf)
{
    /*
    #version 450

    #pragma use_vulkan_memory_model
    #extension GL_KHR_memory_scope_semantics : require
    #extension GL_KHR_cooperative_matrix : require
    #extension GL_KHR_shader_subgroup_basic : require
    #extension GL_EXT_shader_explicit_arithmetic_types : require
    #extension GL_EXT_buffer_reference : require

    layout(local_size_x_id = 0, local_size_y = 1, local_size_z = 1) in;
    layout(push_constant) uniform PC { uint REQUESTED_MATRIX; };
    layout(constant_id = 1) const int M = 1;
    layout(constant_id = 2) const int K = 1;
    layout(constant_id = 3) const int N = 1;
    layout(constant_id = 4) const int V = 1;

    layout(set = 0, binding = 0) coherent buffer AData { float16_t a[]; };
    layout(set = 0, binding = 1) coherent buffer BData { float16_t b[]; };
    layout(set = 0, binding = 2) coherent buffer CData { float16_t c[]; };
    layout(set = 0, binding = 3) coherent buffer RData { float16_t r[]; };

    coopmat<float16_t, gl_ScopeSubgroup, M, K, gl_MatrixUseA> A;
    coopmat<float16_t, gl_ScopeSubgroup, K, N, gl_MatrixUseB> B;
    coopmat<float16_t, gl_ScopeSubgroup, M, N, gl_MatrixUseAccumulator> C;
    coopmat<float16_t, gl_ScopeSubgroup, M, N, gl_MatrixUseAccumulator> R;

    void loadMatrix(out coopmat<float16_t, gl_ScopeSubgroup, M, K, gl_MatrixUseA> mtx) {
        if (REQUESTED_MATRIX == 11)
            mtx = null;
        else
            coopMatLoad(mtx, a, 0, 2, gl_CooperativeMatrixLayoutRowMajor);
    }
    void loadMatrix(out coopmat<float16_t, gl_ScopeSubgroup, K, N, gl_MatrixUseB> mtx) {
        if (REQUESTED_MATRIX == 12)
            mtx = null;
        else
            coopMatLoad(mtx, b, 0, 2, gl_CooperativeMatrixLayoutRowMajor);
    }
    void loadMatrix(out coopmat<float16_t, gl_ScopeSubgroup, M, N, gl_MatrixUseAccumulator> mtx) {
        if (REQUESTED_MATRIX == 13)
            mtx = null
        else
            coopMatLoad(mtx, c, 0, 2, gl_CooperativeMatrixLayoutRowMajor);
    }
    coopmat<float16_t, gl_ScopeSubgroup, M, N, gl_MatrixUseAccumulator> genOutputMatrix() {
        coopmat<float16_t, gl_ScopeSubgroup, M, N, gl_MatrixUseAccumulator> res;
        if (REQUESTED_MATRIX == 14)
            res = null;
        else
            res = coopMatMulAdd(A, B, C);
        return res;
    }
    void main() {
        loadMatrix(A);
        loadMatrix(B);
        loadMatrix(C);
        R = genOutputMatrix();
        coopMatStore(R, r, 0, N, gl_CooperativeMatrixLayoutRowMajor);
    }
    */
    const tcu::StringTemplate code(R"spirv(
; SPIR-V
; Version: 1.3
; Generator: Khronos Glslang Reference Front End; 11
; Bound: 131
; Schema: 0
               OpCapability Shader
${Capabilities}
               OpCapability VulkanMemoryModel
               OpCapability CooperativeMatrixKHR
${Extensions}
               OpExtension "SPV_KHR_cooperative_matrix"
               OpExtension "SPV_KHR_vulkan_memory_model"
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical Vulkan
               OpEntryPoint GLCompute %main "main"
               OpExecutionMode %main LocalSize 1 1 1

               ; Annotations
               OpDecorate %M SpecId 1
               OpDecorate %K SpecId 2
               OpDecorate %N SpecId 3
               OpDecorate %V SpecId 4
               OpDecorate %PC Block
               OpMemberDecorate %PC 0 Offset 0
               ;
               OpDecorate %_runtimearr_adata ArrayStride ${AStride}
               OpDecorate %AData Block
               OpMemberDecorate %AData 0 Offset 0
               OpDecorate %_ Binding 0
               OpDecorate %_ DescriptorSet 0
               ;
               OpDecorate %_runtimearr_bdata ArrayStride ${BStride}
               OpDecorate %BData Block
               OpMemberDecorate %BData 0 Offset 0
               OpDecorate %__0 Binding 1
               OpDecorate %__0 DescriptorSet 0
               ;
               OpDecorate %_runtimearr_cdata ArrayStride ${CStride}
               OpDecorate %CData Block
               OpMemberDecorate %CData 0 Offset 0
               OpDecorate %__1 Binding 2
               OpDecorate %__1 DescriptorSet 0
               ;
               OpDecorate %_runtimearr_rdata ArrayStride ${RStride}
               OpDecorate %RData Block
               OpMemberDecorate %RData 0 Offset 0
               OpDecorate %__2 Binding 3
               OpDecorate %__2 DescriptorSet 0
               ;
               OpDecorate %128 SpecId 0
               OpDecorate %gl_WorkGroupSize BuiltIn WorkgroupSize

               ; Types, variables and constants
       %void = OpTypeVoid
       %bool = OpTypeBool
          %3 = OpTypeFunction %void
       %uint = OpTypeInt 32 0
  %uint_vec2 = OpTypeVector %uint 2
  %uint_vec4 = OpTypeVector %uint 4
        %int = OpTypeInt 32 1
   %int_vec2 = OpTypeVector %int 2
   %int_vec4 = OpTypeVector %int 4

         ${TypeList}
          %M = OpSpecConstant %int 1                ; SpecId 1
          %K = OpSpecConstant %int 1                ; SpecId 2
          %N = OpSpecConstant %int 1                ; SpecId 3
          %V = OpSpecConstant %int 1                ; SpecId 4
     %uint_0 = OpConstant %uint 0
     %uint_1 = OpConstant %uint 1
     %uint_2 = OpConstant %uint 2
     %uint_3 = OpConstant %uint 3
         %PC = OpTypeStruct %uint
         %ptr_PC = OpTypePointer PushConstant %PC
         %var_PC = OpVariable %ptr_PC PushConstant
         %ptr_PC_uint = OpTypePointer PushConstant %uint
         %matA_type = OpTypeCooperativeMatrixKHR ${AType} %uint_3 %M %K %uint_0
         %matA_null = OpConstantNull %matA_type
%ptr_fun_matA = OpTypePointer Function %matA_type
         %15 = OpTypeFunction %void %ptr_fun_matA
         %matB_type = OpTypeCooperativeMatrixKHR ${BType} %uint_3 %K %N %uint_1
         %matB_null = OpConstantNull %matB_type
%ptr_fun_matB = OpTypePointer Function %matB_type
         %23 = OpTypeFunction %void %ptr_fun_matB
         %matCR_type = OpTypeCooperativeMatrixKHR ${CRType} %uint_3 %M %N %uint_2
         %matCR_null = OpConstantNull %matCR_type
%ptr_fun_matCR = OpTypePointer Function %matCR_type
         %30 = OpTypeFunction %void %ptr_fun_matCR
         %34 = OpTypeFunction %matCR_type
    %uint_11 = OpConstant %uint 11
%_runtimearr_adata = OpTypeRuntimeArray ${AType}
      %AData = OpTypeStruct %_runtimearr_adata       ; Block
%ptr_sb_AData = OpTypePointer StorageBuffer %AData
          %_ = OpVariable %ptr_sb_AData StorageBuffer   ; Binding 0, DescriptorSet 0
      %int_0 = OpConstant %int 0
     %uint_5 = OpConstant %uint 5
         %51 = OpSpecConstantOp %uint IAdd %K %uint_0
         %55 = OpSpecConstantOp %uint IAdd %K %uint_0
    %uint_12 = OpConstant %uint 12
%_runtimearr_bdata = OpTypeRuntimeArray ${BType}
      %BData = OpTypeStruct %_runtimearr_bdata     ; Block
%ptr_sb_BData = OpTypePointer StorageBuffer %BData
        %__0 = OpVariable %ptr_sb_BData StorageBuffer   ; Binding 1, DescriptorSet 0
         %66 = OpSpecConstantOp %uint IAdd %N %uint_0
         %70 = OpSpecConstantOp %uint IAdd %N %uint_0
    %uint_13 = OpConstant %uint 13
%_runtimearr_cdata = OpTypeRuntimeArray ${CRType}
      %CData = OpTypeStruct %_runtimearr_cdata     ; Block
%ptr_sb_CData = OpTypePointer StorageBuffer %CData
        %__1 = OpVariable %ptr_sb_CData StorageBuffer   ; Binding 2, DescriptorSet 0
         %81 = OpSpecConstantOp %uint IAdd %N %uint_0
         %85 = OpSpecConstantOp %uint IAdd %N %uint_0
    %uint_14 = OpConstant %uint 14
%_ptr_Private_13 = OpTypePointer Private %matA_type
          %A = OpVariable %_ptr_Private_13 Private
%_ptr_Private_21 = OpTypePointer Private %matB_type
          %B = OpVariable %_ptr_Private_21 Private
%_ptr_Private_28 = OpTypePointer Private %matCR_type
          %C = OpVariable %_ptr_Private_28 Private
          %D = OpVariable %_ptr_Private_28 Private
%_runtimearr_rdata = OpTypeRuntimeArray ${CRType}
      %RData = OpTypeStruct %_runtimearr_rdata     ; Block
%ptr_sb_RData = OpTypePointer StorageBuffer %RData
        %__2 = OpVariable %ptr_sb_RData StorageBuffer   ; Binding 3, DescriptorSet 0
        %127 = OpSpecConstantOp %uint IAdd %N %uint_0
        %128 = OpSpecConstant %uint 1               ; SpecId 0
     %v3uint = OpTypeVector %uint 3
%gl_WorkGroupSize = OpSpecConstantComposite %v3uint %128 %uint_1 %uint_1    ; BuiltIn WorkgroupSize

               ; Function main
       %main = OpFunction %void None %3
          %5 = OpLabel
      %param = OpVariable %ptr_fun_matA Function
    %param_0 = OpVariable %ptr_fun_matB Function
    %param_1 = OpVariable %ptr_fun_matCR Function
        %111 = OpFunctionCall %void %loadMatrix_A %param
        %112 = OpLoad %matA_type %param
               OpStore %A %112
        %114 = OpFunctionCall %void %loadMatrix_B %param_0
        %115 = OpLoad %matB_type %param_0
               OpStore %B %115
        %117 = OpFunctionCall %void %loadMatrix_C %param_1
        %118 = OpLoad %matCR_type %param_1
               OpStore %C %118
        %120 = OpFunctionCall %matCR_type %genOutputMatrix_
               OpStore %D %120

         %ld = OpLoad %matCR_type %D
        %ddd = OpAccessChain ${CRTypePtr} %__2 %int_0 %uint_0
               OpCooperativeMatrixStoreKHR %ddd %ld %int_0 %127 MakePointerAvailable|NonPrivatePointer %uint_5

         %lc = OpLoad %matCR_type %C
        %ccc = OpAccessChain ${CRTypePtr} %__1 %int_0 %uint_0
               OpCooperativeMatrixStoreKHR %ccc %lc %int_0 %85 MakePointerAvailable|NonPrivatePointer %uint_5

         %lb = OpLoad %matB_type %B
        %bbb = OpAccessChain ${BTypePtr} %__0 %int_0 %uint_0
               OpCooperativeMatrixStoreKHR %bbb %lb %int_0 %70 MakePointerAvailable|NonPrivatePointer %uint_5

         %la = OpLoad %matA_type %A
        %aaa = OpAccessChain ${ATypePtr} %_ %int_0 %uint_0
               OpCooperativeMatrixStoreKHR %aaa %la %int_0 %55 MakePointerAvailable|NonPrivatePointer %uint_5

               OpReturn
               OpFunctionEnd

               ; Function loadMatrix_A
%loadMatrix_A = OpFunction %void None %15
        %mtx = OpFunctionParameter %ptr_fun_matA
         %18 = OpLabel
     %p_PC_A = OpAccessChain %ptr_PC_uint %var_PC %int_0
     %v_PC_A = OpLoad %uint %p_PC_A
         %40 = OpIEqual %bool %v_PC_A %uint_11
               OpSelectionMerge %42 None
               OpBranchConditional %40 %41 %53
         %41 = OpLabel
               OpStore %mtx %matA_null
               OpBranch %42
         %53 = OpLabel
         %54 = OpAccessChain ${ATypePtr} %_ %int_0 %uint_0
         %56 = OpCooperativeMatrixLoadKHR %matA_type %54 %int_0 %55 MakePointerVisible|NonPrivatePointer %uint_5
               OpStore %mtx %56
               OpBranch %42
         %42 = OpLabel
               OpReturn
               OpFunctionEnd

               ; Function loadMatrix_B
%loadMatrix_B = OpFunction %void None %23
      %mtx_0 = OpFunctionParameter %ptr_fun_matB
         %26 = OpLabel
     %p_PC_B = OpAccessChain %ptr_PC_uint %var_PC %int_0
     %v_PC_B = OpLoad %uint %p_PC_B
         %58 = OpIEqual %bool %v_PC_B %uint_12
               OpSelectionMerge %60 None
               OpBranchConditional %58 %59 %68
         %59 = OpLabel
               OpStore %mtx_0 %matB_null
               OpBranch %60
         %68 = OpLabel
         %69 = OpAccessChain ${BTypePtr} %__0 %int_0 %uint_0
         %71 = OpCooperativeMatrixLoadKHR %matB_type %69 %int_0 %70 MakePointerVisible|NonPrivatePointer %uint_5
               OpStore %mtx_0 %71
               OpBranch %60
         %60 = OpLabel
               OpReturn
               OpFunctionEnd

               ; Function loadMatrix_C
%loadMatrix_C = OpFunction %void None %30
      %mtx_1 = OpFunctionParameter %ptr_fun_matCR
         %33 = OpLabel
     %p_PC_C = OpAccessChain %ptr_PC_uint %var_PC %int_0
     %v_PC_C = OpLoad %uint %p_PC_C
         %73 = OpIEqual %bool %v_PC_C %uint_13
               OpSelectionMerge %75 None
               OpBranchConditional %73 %74 %83
         %74 = OpLabel
               OpStore %mtx_1 %matCR_null
               OpBranch %75
         %83 = OpLabel
         %84 = OpAccessChain ${CRTypePtr} %__1 %int_0 %uint_0
         %86 = OpCooperativeMatrixLoadKHR %matCR_type %84 %int_0 %85 MakePointerVisible|NonPrivatePointer %uint_5
               OpStore %mtx_1 %86
               OpBranch %75
         %75 = OpLabel
               OpReturn
               OpFunctionEnd

               ; Function genOutputMatrix_
%genOutputMatrix_ = OpFunction %matCR_type None %34
         %36 = OpLabel
        %res = OpVariable %ptr_fun_matCR Function
     %p_PC_R = OpAccessChain %ptr_PC_uint %var_PC %int_0
     %v_PC_R = OpLoad %uint %p_PC_R
         %88 = OpIEqual %bool %v_PC_R %uint_14
               OpSelectionMerge %90 None
               OpBranchConditional %88 %89 %102
         %89 = OpLabel
               OpStore %res %matCR_null
               OpBranch %90
        %102 = OpLabel
        %103 = OpLoad %matA_type %A
        %104 = OpLoad %matB_type %B
        %105 = OpLoad %matCR_type %C
        %106 = OpCooperativeMatrixMulAddKHR %matCR_type %103 %104 %105 ${Operands}
               OpStore %res %106
               OpBranch %90
         %90 = OpLabel
        %107 = OpLoad %matCR_type %res
               OpReturnValue %107
               OpFunctionEnd
    )spirv");

    std::string AType, ATypePtr, ATypePtrDef;
    std::string BType, BTypePtr, BTypePtrDef;
    std::string CRType, CRTypePtr, CRTypePtrDef;
    std::set<std::string> capabilityList;
    std::set<std::string> extensionList;

    ATypePtr  = "%ptr_sb_A";
    BTypePtr  = "%ptr_sb_B";
    CRTypePtr = "%ptr_sb_CR";

    std::ostringstream typeList;
    std::vector<VkComponentTypeKHR> types{VK_COMPONENT_TYPE_UINT32_KHR, VK_COMPONENT_TYPE_SINT32_KHR};
    for (const VkComponentTypeKHR matType : {conf.AType, conf.BType, conf.CType, conf.ResultType})
    {
        if (auto typePtr = std::find(types.begin(), types.end(), matType); typePtr == types.end())
        {
            types.push_back(matType);
            const Value v(matType);
            const auto [typeName, typeDef] = v.getSpirvNames();
            typeList << typeName << " = " << typeDef << " ; generated" << std::endl;

            const std::vector<std::string> caps = v.getSpirvCapabilities();
            for (const std::string &cap : caps)
                capabilityList.insert(cap);

            const std::vector<std::string> exts = v.getSpirvExtensions();
            for (const std::string &ext : exts)
                extensionList.insert(ext);
        }
    }

    const VkComponentTypeKHR matList[]{conf.AType, conf.BType, conf.CType};
    std::string *matTypes[]{&AType, &BType, &CRType};
    std::string *const matStorages[]{&ATypePtr, &BTypePtr, &CRTypePtr};

    for (uint32_t i = 0u; i < 3u; ++i)
    {
        const Value v(matList[i]);
        const auto [typeName, typeDef] = v.getSpirvNames();
        matTypes[i]->assign(typeName);
        typeList << *matStorages[i] << " = OpTypePointer StorageBuffer " << typeName << " ; generated" << std::endl;
    }

    std::ostringstream capabilities;
    for (const std::string &cap : capabilityList)
        capabilities << "OpCapability " << cap << " ; generated" << std::endl;
    capabilities.flush();

    std::ostringstream extensions;
    for (const std::string &ext : extensionList)
        extensions << "OpExtension \"" << ext << "\" ; generated" << std::endl;
    extensions.flush();

    std::ostringstream operands;
    for (const std::pair<VkComponentTypeKHR, Matrices> &matType :
         {std::make_pair(conf.AType, Matrices::A), std::make_pair(conf.BType, Matrices::B),
          std::make_pair(conf.CType, Matrices::C), std::make_pair(conf.ResultType, Matrices::R)})
    {
        const std::string op = Value(matType.first).getMatrixOperand(matType.second);
        if (!op.empty())
        {
            if (operands.tellp())
                operands << '|';
            operands << op;
        }
    }

    const std::map<std::string, std::string> variables{{"TypeList", typeList.str()},
                                                       {"AStride", std::to_string(Value(conf.AType).size())},
                                                       {"BStride", std::to_string(Value(conf.BType).size())},
                                                       {"CStride", std::to_string(Value(conf.CType).size())},
                                                       {"RStride", std::to_string(Value(conf.ResultType).size())},
                                                       {"AType", AType},
                                                       {"BType", BType},
                                                       {"CRType", CRType},
                                                       {"ATypePtr", ATypePtr},
                                                       {"BTypePtr", BTypePtr},
                                                       {"CRTypePtr", CRTypePtr},
                                                       {"Capabilities", capabilities.str()},
                                                       {"Extensions", extensions.str()},
                                                       {"Operands", operands.str()}};

    return code.specialize(variables);
}

void CoopMtxOpConstantNullCase::initDeviceCapabilities(DevCaps &caps)
{
    if (!(caps.addFeature(&VkPhysicalDeviceCooperativeMatrixFeaturesKHR::cooperativeMatrix)))
        TCU_THROW(NotSupportedError, "cooperativeMatrix is not supported");
    caps.addExtension(VK_KHR_COOPERATIVE_MATRIX_EXTENSION_NAME);

    uint32_t configurationCount = 0u;
    {
        std::lock_guard<std::mutex> lock(m_configurationsMutex);
        if (m_configurations.empty())
            m_configurations = getPossibleConfigurations(caps.getContextManager().getInstanceInterface(),
                                                         caps.getContextManager().getPhysicalDevice());
        configurationCount = uint32_t(m_configurations.size());
    }

    if (0u == configurationCount)
        TCU_THROW(NotSupportedError, "No configurations to perform test");

    if (!(caps.addFeature(&VkPhysicalDeviceVulkan12Features::vulkanMemoryModel)))
        TCU_THROW(NotSupportedError, "vulkanMemoryModel is not supported");

    if (has16BitTypes(m_configurations))
    {
        if (!caps.addFeature(&VkPhysicalDevice16BitStorageFeatures::storageBuffer16BitAccess))
            TCU_THROW(NotSupportedError, "storageBuffer16BitAccess not supported");

        if (!caps.addFeature(&VkPhysicalDeviceVulkan12Features::shaderFloat16))
            TCU_THROW(NotSupportedError, "shaderFloat16 not supported");
    }

    if (hasInt8BitTypes(m_configurations))
    {
        if (!caps.addFeature(&VkPhysicalDeviceVulkan12Features::shaderInt8))
            TCU_THROW(NotSupportedError, "shaderInt8 not supported");

        if (!caps.addFeature(&VkPhysicalDeviceVulkan12Features::storageBuffer8BitAccess))
            TCU_THROW(NotSupportedError, "storageBuffer8BitAccess not supported");
    }

    if (hasFloat8BitTypes(m_configurations))
    {
        if (!caps.addFeature(&VkPhysicalDeviceShaderFloat8FeaturesEXT::shaderFloat8CooperativeMatrix))
            TCU_THROW(NotSupportedError, "shaderFloat8CooperativeMatrix not supported");

        if (!caps.addFeature(&VkPhysicalDeviceShaderFloat8FeaturesEXT::shaderFloat8))
            TCU_THROW(NotSupportedError, "shaderFloat8 not supported");

        if (!caps.addFeature(&VkPhysicalDeviceVulkan12Features::storageBuffer8BitAccess))
            TCU_THROW(NotSupportedError, "storageBuffer8BitAccess not supported");

        caps.addExtension("VK_EXT_shader_float8");
    }

    if (hasBFloat16Types(m_configurations))
    {
        if (!caps.addFeature(&VkPhysicalDeviceShaderBfloat16FeaturesKHR::shaderBFloat16CooperativeMatrix))
            TCU_THROW(NotSupportedError, "shaderBFloat16CooperativeMatrix not supported");

        if (!caps.addFeature(&VkPhysicalDeviceShaderBfloat16FeaturesKHR::shaderBFloat16Type))
            TCU_THROW(NotSupportedError, "shaderBFloat16Type not supported");

        caps.addExtension(VK_KHR_SHADER_BFLOAT16_EXTENSION_NAME);
    }
}

void CoopMtxOpConstantNullCase::initPrograms(SourceCollections &programCollection) const
{
    std::vector<std::string> shaderNames;
    const SpirVAsmBuildOptions buildOptions(programCollection.usedVulkanVersion, SPIRV_VERSION_1_3);

    for (const VkCooperativeMatrixPropertiesKHR &conf : CoopMtxOpConstantNullCase::m_configurations)
    {
        const std::string shaderName = genShaderName(conf);
        if (auto exists = std::find(shaderNames.begin(), shaderNames.end(), shaderName); exists != shaderNames.end())
        {
            continue;
        }
        shaderNames.push_back(shaderName);
        programCollection.spirvAsmSources.add(shaderName) << genShaderCode(conf) << buildOptions;
    }

    const std::string code(R"glsl(
    #version 450
    layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
    void main() {
    })glsl");
    programCollection.glslSources.add("test") << glu::ComputeSource(code);
}

void CoopMtxOpConstantNullInstance::logConfiguration(const VkCooperativeMatrixPropertiesKHR &conf, uint32_t number,
                                                     tcu::TestLog &log) const
{
    const char *comma = ", ";
    log << tcu::TestLog::Message << "Configuration: " << number << " A=" << getComponentTypeKHRName(conf.AType) << comma
        << "B=" << getComponentTypeKHRName(conf.BType) << comma << "C=" << getComponentTypeKHRName(conf.CType) << comma
        << "R=" << getComponentTypeKHRName(conf.ResultType) << comma << "Scope=" << getScopeKHRName(conf.scope) << comma
        << "M=" << conf.MSize << comma << "K=" << conf.KSize << comma << "N=" << conf.NSize << tcu::TestLog::EndMessage;
}

std::vector<float> CoopMtxOpConstantNullInstance::Executor::getMatrix(Matrices m) const
{
    const DeviceInterface &di = m_context.getDeviceInterface();
    const VkDevice device     = m_context.getDevice();

    if (Matrices::A == m)
    {
        invalidateAlloc(di, device, m_bufferA->getAllocation());
        const uint32_t count = m_configuration.MSize * m_configuration.KSize;
        return Value(m_configuration.AType).readBuffer(*m_bufferA, count);
    }
    else if (Matrices::B == m)
    {
        invalidateAlloc(di, device, m_bufferB->getAllocation());
        const uint32_t count = m_configuration.KSize * m_configuration.NSize;
        return Value(m_configuration.BType).readBuffer(*m_bufferB, count);
    }
    else if (Matrices::C == m)
    {
        invalidateAlloc(di, device, m_bufferC->getAllocation());
        const uint32_t count = m_configuration.MSize * m_configuration.NSize;
        return Value(m_configuration.CType).readBuffer(*m_bufferC, count);
    }
    else if (Matrices::R == m)
    {
        invalidateAlloc(di, device, m_bufferR->getAllocation());
        const uint32_t count = m_configuration.MSize * m_configuration.NSize;
        return Value(m_configuration.ResultType).readBuffer(*m_bufferR, count);
    }

    DE_ASSERT(false);
    return {};
}

CoopMtxOpConstantNullInstance::Executor::Executor(Context &context, const VkCooperativeMatrixPropertiesKHR &conf,
                                                  const Params &params)
    : m_context(context)
    , m_configuration(conf)
{
    const DeviceInterface &di = context.getDeviceInterface();
    const VkDevice device     = context.getDevice();
    Allocator &allocator      = context.getDefaultAllocator();

    m_descriptorSetLayout = DescriptorSetLayoutBuilder()
                                .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                                .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                                .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                                .addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                                .build(di, device);

    m_descriptorPool = DescriptorPoolBuilder()
                           .addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4u)
                           .build(di, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    m_descriptorSet = makeDescriptorSet(di, device, *m_descriptorPool, *m_descriptorSetLayout);

    DescriptorSetUpdateBuilder setUpdateBuilder;
    const MemoryRequirement memreq =
        MemoryRequirement::HostVisible | MemoryRequirement::Cached | MemoryRequirement::Coherent;
    const VkBufferUsageFlags usage = (VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

    // matrix A
    {
        const uint32_t count                = conf.MSize * conf.KSize;
        const VkBufferCreateInfo bufferInfo = makeBufferCreateInfo((Value(conf.AType).size() * count), usage);
        m_bufferA = de::MovePtr<BufferWithMemory>(new BufferWithMemory(di, device, allocator, bufferInfo, memreq));
        const VkDescriptorBufferInfo descriptorInfo =
            makeDescriptorBufferInfo(**m_bufferA, 0u, m_bufferA->getBufferSize());
        setUpdateBuilder.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0),
                                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorInfo);
    }

    // matrix B
    {
        const uint32_t count                = conf.KSize * conf.NSize;
        const VkBufferCreateInfo bufferInfo = makeBufferCreateInfo((Value(conf.BType).size() * count), usage);
        m_bufferB = de::MovePtr<BufferWithMemory>(new BufferWithMemory(di, device, allocator, bufferInfo, memreq));
        const VkDescriptorBufferInfo descriptorInfo =
            makeDescriptorBufferInfo(**m_bufferB, 0u, m_bufferB->getBufferSize());
        setUpdateBuilder.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1),
                                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorInfo);
    }

    // matrix C
    {
        const uint32_t count                = conf.MSize * conf.NSize;
        const VkBufferCreateInfo bufferInfo = makeBufferCreateInfo((Value(conf.CType).size() * count), usage);
        m_bufferC = de::MovePtr<BufferWithMemory>(new BufferWithMemory(di, device, allocator, bufferInfo, memreq));
        const VkDescriptorBufferInfo descriptorInfo =
            makeDescriptorBufferInfo(**m_bufferC, 0u, m_bufferC->getBufferSize());
        setUpdateBuilder.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(2),
                                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorInfo);
    }

    // matrix R
    {
        const uint32_t count                = conf.MSize * conf.NSize;
        const VkBufferCreateInfo bufferInfo = makeBufferCreateInfo((Value(conf.ResultType).size() * count), usage);
        m_bufferR = de::MovePtr<BufferWithMemory>(new BufferWithMemory(di, device, allocator, bufferInfo, memreq));
        const VkDescriptorBufferInfo descriptorInfo =
            makeDescriptorBufferInfo(**m_bufferR, 0u, m_bufferR->getBufferSize());
        setUpdateBuilder.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(3),
                                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descriptorInfo);
    }

    setUpdateBuilder.update(di, device);

    // pipeline
    {
        const uint32_t subgroupSize = context.getSubgroupProperties().subgroupSize;

        const uint32_t specData[5]{subgroupSize, conf.MSize, conf.KSize, conf.NSize,
                                   std::numeric_limits<uint32_t>::max()};

        const VkSpecializationMapEntry entries[DE_LENGTH_OF_ARRAY(specData)]{
            {0u, uint32_t(sizeof(uint32_t)) * 0u, size_t(sizeof(uint32_t))},
            {1u, uint32_t(sizeof(uint32_t)) * 1u, size_t(sizeof(uint32_t))},
            {2u, uint32_t(sizeof(uint32_t)) * 2u, size_t(sizeof(uint32_t))},
            {3u, uint32_t(sizeof(uint32_t)) * 3u, size_t(sizeof(uint32_t))},
            {4u, uint32_t(sizeof(uint32_t)) * 4u, size_t(sizeof(uint32_t))},
        };

        const VkSpecializationInfo specInfo{
            DE_LENGTH_OF_ARRAY(entries), // mapEntryCount
            entries,                     // pMapEntries
            sizeof(specData),            // dataSize
            specData                     // pData
        };

        const VkPushConstantRange pushRange{
            VK_SHADER_STAGE_COMPUTE_BIT, // stageFlags
            0u,                          // offset
            uint32_t(sizeof(uint32_t))   // size
        };

        m_pipeline = de::MovePtr<ComputePipelineWrapper>(new ComputePipelineWrapper(
            di, device, params.pipelineConstructionType, context.getBinaryCollection().get(genShaderName(conf))));
        m_pipeline->setDescriptorSetLayout(m_descriptorSetLayout.get());
        m_pipeline->addPushConstantRange(pushRange);
        m_pipeline->setSpecializationInfo(specInfo);
        m_pipeline->buildPipeline();
    }

    m_queue         = context.getDeviceQueueInfo(0).queue;
    m_commandPool   = createCommandPool(di, device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                        context.getDeviceQueueInfo(0).familyIndex);
    m_commandBuffer = allocateCommandBuffer(di, device, *m_commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
}

void CoopMtxOpConstantNullInstance::Executor::execute(Matrices targetMatrix)
{
    const DeviceInterface &di           = m_context.getDeviceInterface();
    const VkDevice device               = m_context.getDevice();
    const VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
    const VkShaderStageFlagBits stage   = VK_SHADER_STAGE_COMPUTE_BIT;

    auto populate =
        [&](VkComponentTypeKHR ct, uint32_t count, BufferWithMemory &buffer, const std::vector<float> *source)
    {
        if (source)
            Value(ct).writeBuffer(buffer, *source);
        else
        {
            ValueGenerator gen(ct);
            std::vector<float> data(count);
            for (uint32_t i = 0u; i < count; ++i)
                data[i] = gen.next();
            Value(ct).writeBuffer(buffer, data);
        }
        flushAlloc(di, device, buffer.getAllocation());
    };

    // matrix A
    {
        const uint32_t count = m_configuration.MSize * m_configuration.KSize;
        populate(m_configuration.AType, count, *m_bufferA, nullptr);
    }

    // matrix B
    {
        const uint32_t count = m_configuration.KSize * m_configuration.NSize;
        const std::vector<float> data(count, 1.0f);
        populate(m_configuration.BType, count, *m_bufferB, &data);
    }

    // matrix C
    {
        const uint32_t count = m_configuration.MSize * m_configuration.NSize;
        populate(m_configuration.CType, count, *m_bufferC, nullptr);
    }

    // matrix R
    {
        const uint32_t count = m_configuration.MSize * m_configuration.NSize;
        populate(m_configuration.ResultType, count, *m_bufferR, nullptr);
    }

    beginCommandBuffer(di, *m_commandBuffer, 0u);
    di.cmdBindDescriptorSets(*m_commandBuffer, bindPoint, m_pipeline->getPipelineLayout(), //
                             0u, 1u, &*m_descriptorSet, 0u, nullptr);
    di.cmdPushConstants(*m_commandBuffer, m_pipeline->getPipelineLayout(), //
                        stage, 0, uint32_t(sizeof(targetMatrix)), &targetMatrix);
    m_pipeline->bind(*m_commandBuffer);
    di.cmdDispatch(*m_commandBuffer, 3u, 1u, 1u);
    endCommandBuffer(di, *m_commandBuffer);
    submitCommandsAndWait(di, device, m_queue, *m_commandBuffer);
}

template <class Stream>
void CoopMtxOpConstantNullInstance::Executor::dumpMatrix(Stream &str, const std::vector<float> matrix, uint32_t rows,
                                                         uint32_t cols, const std::string &name,
                                                         VkComponentTypeKHR type) const
{
    const char endl = '\n';

    std::ostringstream header;
    header << name << ' ' << rows << 'x' << cols << ' ' << getComponentTypeKHRName(type);
    header.flush();

    str << header.str() << endl;
    str << std::string(header.str().length(), '-') << endl;

    for (uint32_t r = 0u; r < rows; ++r)
    {
        for (uint32_t c = 0u; c < cols; ++c)
        {
            if (c)
                str << ' ';
            str << matrix[r * cols + c];
        }
        str << endl;
    }
}

template <class Stream>
void CoopMtxOpConstantNullInstance::Executor::dumpMatrices(Stream &str, bool includeReference) const
{
    const std::vector<float> A = getMatrix(Matrices::A);
    const std::vector<float> B = getMatrix(Matrices::B);
    const std::vector<float> C = getMatrix(Matrices::C);
    const std::vector<float> R = getMatrix(Matrices::R);

    const char endl                              = '\n';
    const VkCooperativeMatrixPropertiesKHR &conf = getConfiguration();

    str << endl;
    dumpMatrix(str, A, conf.MSize, conf.KSize, "Matrix A", conf.AType);
    str << endl;
    dumpMatrix(str, B, conf.KSize, conf.NSize, "Matrix B", conf.BType);
    str << endl;
    dumpMatrix(str, C, conf.MSize, conf.NSize, "Matrix C", conf.CType);
    str << endl;
    dumpMatrix(str, R, conf.MSize, conf.NSize, "Matrix Result (A * B + C)", conf.ResultType);
    str << endl;

    if (includeReference)
    {
        const std::vector<float> ref = addMatrices(mulMatrices(A, B, conf.MSize, conf.NSize), C);
        dumpMatrix(str, ref, conf.MSize, conf.NSize, "Reference matrix (A * B + C)", conf.ResultType);
        str << endl;
    }
}

bool CoopMtxOpConstantNullInstance::verifyResult(const Executor &executor, Matrices targetMatrix,
                                                 std::string &errorMessage) const
{
    const std::vector<float> A = executor.getMatrix(Matrices::A);
    const std::vector<float> B = executor.getMatrix(Matrices::B);
    const std::vector<float> C = executor.getMatrix(Matrices::C);
    const std::vector<float> R = executor.getMatrix(Matrices::R);

    const VkCooperativeMatrixPropertiesKHR &conf = executor.getConfiguration();

    uint32_t mismatch  = 0u;
    uint32_t processed = 0u;

    auto cmp = [&](const std::vector<float> &reference, const std::vector<float> &result) -> void
    {
        const uint32_t N = uint32_t(reference.size());
        DE_ASSERT(N == result.size());
        processed = N;
        for (uint32_t i = 0u; i < N; ++i)
        {
            const float x = reference[i];
            const float y = result[i];
            const bool ok = x == y;
            if (!ok)
                ++mismatch;
        }
    };

    if (Matrices::All == targetMatrix)
    {
        if (isNullMatrix(A) || isNullMatrix(B) || isNullMatrix(C))
        {
            mismatch     = std::numeric_limits<uint32_t>::max();
            errorMessage = "Neither matrices A,B nor C might be null";
        }
        else
        {
            const std::vector<float> ref = addMatrices(mulMatrices(A, B, conf.MSize, conf.NSize), C);
            cmp(ref, R);
        }
    }
    else if (Matrices::A == targetMatrix || Matrices::B == targetMatrix)
    {
        if (Matrices::A == targetMatrix)
        {
            if (!(isNullMatrix(A)))
            {
                mismatch     = std::numeric_limits<uint32_t>::max();
                errorMessage = "Matrix A must be null";
            }
            if (isNullMatrix(B))
            {
                mismatch     = std::numeric_limits<uint32_t>::max();
                errorMessage = "Matrix B must not be null";
            }
        }
        else
        {
            if (isNullMatrix(A))
            {
                mismatch     = std::numeric_limits<uint32_t>::max();
                errorMessage = "Matrix A must not be null";
            }
            else if (!(isNullMatrix(B)))
            {
                mismatch     = std::numeric_limits<uint32_t>::max();
                errorMessage = "Matrix B must be null";
            }
            else
            {
                cmp(C, R);
            }
        }
    }
    else if (Matrices::C == targetMatrix)
    {
        if (!(isNullMatrix(C)))
        {
            mismatch     = std::numeric_limits<uint32_t>::max();
            errorMessage = "Matrix C must be null";
        }
        else
        {
            const std::vector<float> ref = mulMatrices(A, B, conf.MSize, conf.NSize);
            cmp(ref, R);
        }
    }
    else if (Matrices::R == targetMatrix)
    {
        if (!(isNullMatrix(R)))
        {
            mismatch     = std::numeric_limits<uint32_t>::max();
            errorMessage = "Matrix R must be null";
        }
    }
    else
    {
        DE_ASSERT(false);
    }

    if (0u != mismatch && std::numeric_limits<uint32_t>::max() != mismatch)
    {
        std::ostringstream os;
        os << "Mismatch in " << mismatch << " from " << processed << " cells";
        os.flush();
        errorMessage = os.str();
    }

    return 0u == mismatch;
}

tcu::TestStatus CoopMtxOpConstantNullInstance::iterate()
{
    tcu::TestLog &log = m_context.getTestContext().getLog();

    const VkCooperativeMatrixPropertiesKHR configuration =
        CoopMtxOpConstantNullCase::getConfigurations().at(m_iteration);
    logConfiguration(configuration, m_iteration, log);

    std::string errorMessage;
    Executor executor(m_context, configuration, m_params);

    executor.execute(Matrices::All);
    bool all_ok = verifyResult(executor, Matrices::All, errorMessage);
    if (all_ok)
    {
        log << tcu::TestLog::Message << "Configuration " << m_iteration << " - normal multiplication: PASS"
            << tcu::TestLog::EndMessage;
    }
    else
    {
        log << tcu::TestLog::Message << "Configuration " << m_iteration
            << " - normal multiplication failed: " << errorMessage << tcu::TestLog::EndMessage;

        auto stream = log << tcu::TestLog::Message;
        executor.dumpMatrices(stream, true);
        stream << tcu::TestLog::EndMessage;

        m_failCount = m_failCount + 1u;
    }

    executor.execute(m_params.matrix);
    bool sel_ok = verifyResult(executor, m_params.matrix, errorMessage);
    if (sel_ok)
    {
        log << tcu::TestLog::Message << "Configuration " << m_iteration << " - OpConstantNull: PASS"
            << tcu::TestLog::EndMessage;
    }
    else
    {
        log << tcu::TestLog::Message << "Configuration " << m_iteration << " - OpConstantNull failed: " << errorMessage
            << tcu::TestLog::EndMessage;
        if (all_ok)
        {
            m_failCount = m_failCount + 1u;
        }
    }

    const auto availableCount = CoopMtxOpConstantNullCase::getConfigurations().size();
    if (++m_iteration >= availableCount)
    {
        std::ostringstream finalMessage;
        if (m_failCount)
        {
            finalMessage << m_failCount << " from " << availableCount;
            finalMessage.flush();
            return tcu::TestStatus::fail(finalMessage.str());
        }

        finalMessage << m_iteration << " from " << availableCount;
        finalMessage.flush();
        return tcu::TestStatus::pass(finalMessage.str());
    }

    return tcu::TestStatus::incomplete();
}

} // unnamed namespace

void createCooperativeMatrixOpConstantNullTests(tcu::TestContext &testCtx, tcu::TestCaseGroup *groupCooperativeMatrix,
                                                ComputePipelineConstructionType computePipelineConstructionType)
{
    static const std::pair<Matrices, const char *> matrices[]{
        {Matrices::A, "null_a"},
        {Matrices::B, "null_b"},
        {Matrices::C, "null_c"},
        {Matrices::R, "null_r"},
    };
    de::MovePtr<tcu::TestCaseGroup> groupNullConstant(new tcu::TestCaseGroup(testCtx, "op_constant_null"));

    for (const std::pair<Matrices, const char *> &m : matrices)
    {
        Params p{};
        p.pipelineConstructionType = computePipelineConstructionType;
        p.matrix                   = m.first;
        groupNullConstant->addChild(new CoopMtxOpConstantNullCase(testCtx, m.second, p));
    }

    groupCooperativeMatrix->addChild(groupNullConstant.release());
}

} // namespace compute
} //namespace vkt
