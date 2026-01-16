/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2024 The Khronos Group Inc.
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
 * \file vktPipelineShaderComponentDecoratedInterfaceMatchingTests.cpp
 * \brief Shader component decorated interface matching tests
 *//*--------------------------------------------------------------------*/

#include "vktPipelineShaderComponentDecoratedLayoutMatchingTests.hpp"

#include "vktPipelineImageUtil.hpp"

#include "vkBuilderUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuTestCase.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuMaybe.hpp"

#include "deUniquePtr.hpp"
#include "deSTLUtil.hpp"

#include <array>
#include <cmath>
#include <sstream>
#include <type_traits>

// Uncomment below to dump shaders code to hard drive
// #define DUMP_SHADERS

#ifdef DUMP_SHADERS
#include <fstream>
#include <iostream>
#endif

namespace vkt
{
namespace pipeline
{

using namespace vk;
using tcu::TestCaseGroup;

namespace
{

struct TestParams;
enum class Types
{
    Scalar,
    Vec2,
    Vec3,
    None
};
struct Components
{
    constexpr Components(Types comp0_ = Types::None, Types comp1_ = Types::None, Types comp2_ = Types::None,
                         Types comp3_ = Types::None)
        : types({comp0_, comp1_, comp2_, comp3_})
    {
    }

    std::string testName() const;
    bool isComponent(const Types &type) const;
    int32_t consumeCount(const Types &type) const;

    const std::array<Types, 4> types;
};
enum class Modes
{
    LooseVariable,
    VariableInBlock,
    VariableInStruct
};
struct Layout
{
    Layout(const Components &components_, uint32_t location_, uint32_t loccount_, Modes mode_, uint32_t width_)
        : components(components_)
        , location(location_)
        , loccount(loccount_)
        , mode(mode_)
        , width(width_)
    {
        DE_ASSERT(width == 16 || width == 32 || width == 64);
    }

    auto getTypeName(Types type, uint32_t otherWidth = 0u) const -> const char *;

    const Components components;
    const uint32_t location;
    const uint32_t loccount;
    const Modes mode;
    const uint32_t width;
};
enum class ShaderTypes
{
    Vert,
    Tesc,
    Tese,
    Geom,
    Frag,
    Any,
    None
};
template <ShaderTypes>
struct ShaderGen;
template <>
struct ShaderGen<ShaderTypes::Any>
{
    virtual const char *name() const                                                                  = 0;
    virtual void genCode(std::ostream &str, const TestParams &params) const                           = 0;
    virtual inline auto makeSource(const std::string &code) const -> de::SharedPtr<glu::ShaderSource> = 0;

    void genExts(std::ostream &str, const Layout &layout) const;
    void genLayout(std::ostream &str, const Layout &layout, bool inOrOut,
                   const std::string &inVarName = std::string("i"), const std::string &outVarName = std::string("o"),
                   const tcu::Maybe<int32_t> &secondExtent = {}) const;
    std::string genLocSubscript(const Layout &layout, const tcu::Maybe<uint32_t> &location = {}) const;
    std::string genArrSubscript(const tcu::Maybe<int32_t> &index) const;
};
template <>
struct ShaderGen<ShaderTypes::None> : ShaderGen<ShaderTypes::Any>
{
    virtual const char *name() const override
    {
        DE_ASSERT(false);
        return "";
    }
    virtual void genCode(std::ostream &, const TestParams &) const override
    {
        DE_ASSERT(false);
    }
    virtual inline auto makeSource(const std::string &) const -> de::SharedPtr<glu::ShaderSource> override
    {
        return {};
    }
};
template <>
struct ShaderGen<ShaderTypes::Vert> : ShaderGen<ShaderTypes::Any>
{
    virtual const char *name() const override
    {
        return "vert";
    }
    virtual void genCode(std::ostream &str, const TestParams &params) const override;
    virtual inline auto makeSource(const std::string &code) const -> de::SharedPtr<glu::ShaderSource> override
    {
        return de::SharedPtr<glu::ShaderSource>(new glu::VertexSource(code));
    }
};
template <>
struct ShaderGen<ShaderTypes::Tesc> : ShaderGen<ShaderTypes::Any>
{
    virtual const char *name() const override
    {
        return "tesc";
    }
    virtual void genCode(std::ostream &str, const TestParams &params) const override;
    virtual inline auto makeSource(const std::string &code) const -> de::SharedPtr<glu::ShaderSource> override
    {
        return de::SharedPtr<glu::ShaderSource>(new glu::TessellationControlSource(code));
    }
};
template <>
struct ShaderGen<ShaderTypes::Tese> : ShaderGen<ShaderTypes::Any>
{
    virtual const char *name() const override
    {
        return "tese";
    }
    virtual void genCode(std::ostream &str, const TestParams &params) const override;
    virtual inline auto makeSource(const std::string &code) const -> de::SharedPtr<glu::ShaderSource> override
    {
        return de::SharedPtr<glu::ShaderSource>(new glu::TessellationEvaluationSource(code));
    }
};
template <>
struct ShaderGen<ShaderTypes::Geom> : ShaderGen<ShaderTypes::Any>
{
    virtual const char *name() const override
    {
        return "geom";
    }
    virtual void genCode(std::ostream &str, const TestParams &params) const override;
    virtual inline auto makeSource(const std::string &code) const -> de::SharedPtr<glu::ShaderSource> override
    {
        return de::SharedPtr<glu::ShaderSource>(new glu::GeometrySource(code));
    }
};
template <>
struct ShaderGen<ShaderTypes::Frag> : ShaderGen<ShaderTypes::Any>
{
    virtual const char *name() const override
    {
        return "frag";
    }
    virtual void genCode(std::ostream &str, const TestParams &params) const override;
    virtual inline auto makeSource(const std::string &code) const -> de::SharedPtr<glu::ShaderSource> override
    {
        return de::SharedPtr<glu::ShaderSource>(new glu::FragmentSource(code));
    }
};
template <class X>
const X &instance()
{
    static X x;
    return x;
}
template <ShaderTypes ShaderType_>
const ShaderGen<ShaderTypes::Any> &shaderGenerator()
{
    return instance<ShaderGen<ShaderType_>>();
}
const ShaderGen<ShaderTypes::Any> &shaderGenerator(ShaderTypes shaderType)
{
    const ShaderGen<ShaderTypes::Any> &(*generators[])(){
        &shaderGenerator<ShaderTypes::Vert>, &shaderGenerator<ShaderTypes::Tesc>, &shaderGenerator<ShaderTypes::Tese>,
        &shaderGenerator<ShaderTypes::Geom>, &shaderGenerator<ShaderTypes::Frag>};
    DE_ASSERT(uint32_t(shaderType) < uint32_t(ShaderTypes::Any));
    return generators[uint32_t(shaderType)]();
}
struct Flow
{
    constexpr Flow(ShaderTypes shader0 = ShaderTypes::None, ShaderTypes shader1 = ShaderTypes::None,
                   ShaderTypes shader2 = ShaderTypes::None, ShaderTypes shader3 = ShaderTypes::None,
                   ShaderTypes shader4 = ShaderTypes::None)
        : shaders({shader0, shader1, shader2, shader3, shader4})
    {
    }
    std::string toString() const;
    bool isShader(const ShaderTypes &shader) const;
    bool hasShader(const ShaderTypes &shader) const;

    const std::array<ShaderTypes, 5> shaders;
};

typedef PipelineConstructionType PCT;
struct TestParams
{
    PCT pipelineConstructionType;
    VkFormat format;
    Layout layout;
    uint32_t width;
    uint32_t height;
    Flow flow;
};

class ShaderComponentDecoratedIterfaceMatchingTestInstance : public TestInstance
{
public:
    ShaderComponentDecoratedIterfaceMatchingTestInstance(Context &context, const std::string fullTestName,
                                                         const TestParams params)
        : TestInstance(context)
        , m_params(params)
        , m_fullTestName(fullTestName)
        , m_vertShaderModule()
        , m_tesCShaderModule()
        , m_tesEShaderModule()
        , m_geomShaderModule()
        , m_fragShaderModule()
    {
    }
    auto iterate(void) -> tcu::TestStatus;
    auto createRenderPass(VkImage image, VkImageView view) -> RenderPassWrapper;
    auto createPipeline(const PipelineLayoutWrapper &pipelineLayout, const RenderPassWrapper &renderPass)
        -> GraphicsPipelineWrapper;

private:
    bool verifyResult(const tcu::TextureFormat &format, const uint8_t *bufferPtr, std::string &errorText) const;
    const TestParams m_params;
    const std::string m_fullTestName;
    ShaderWrapper m_vertShaderModule;
    ShaderWrapper m_tesCShaderModule;
    ShaderWrapper m_tesEShaderModule;
    ShaderWrapper m_geomShaderModule;
    ShaderWrapper m_fragShaderModule;
};

class ShaderComponentDecoratedIterfaceMatchingTestCase : public TestCase
{
public:
    ShaderComponentDecoratedIterfaceMatchingTestCase(tcu::TestContext &testCtx, const std::string &name,
                                                     const TestParams params,
                                                     const std::string &fullTestName = std::string())
        : TestCase(testCtx, name)
        , m_params(params)
        , m_fullTestName(fullTestName)
    {
    }
    void checkSupport(Context &context) const override;
    void initPrograms(SourceCollections &programCollection) const override;
    TestInstance *createInstance(Context &ctx) const override;

private:
    const TestParams m_params;
    const std::string m_fullTestName;
};

void ShaderComponentDecoratedIterfaceMatchingTestCase::checkSupport(Context &context) const
{
    const InstanceInterface &vki                      = context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice             = context.getPhysicalDevice();
    const VkPhysicalDeviceFeatures features           = getPhysicalDeviceFeatures(vki, physicalDevice);
    const VkPhysicalDeviceVulkan11Features features11 = getPhysicalDeviceVulkan11Features(vki, physicalDevice);

    checkPipelineConstructionRequirements(vki, physicalDevice, m_params.pipelineConstructionType);

    const VkFormatProperties formatProps = getPhysicalDeviceFormatProperties(vki, physicalDevice, m_params.format);
    const VkFormatFeatureFlags reqFmtFeatures =
        (VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT);
    if ((formatProps.optimalTilingFeatures & reqFmtFeatures) != reqFmtFeatures)
    {
        TCU_THROW(NotSupportedError, "Required color image features not supported");
    }

    if (m_params.flow.hasShader(ShaderTypes::Tesc) || m_params.flow.hasShader(ShaderTypes::Tese))
    {
        DE_ASSERT(!(m_params.flow.hasShader(ShaderTypes::Tesc) ^ m_params.flow.hasShader(ShaderTypes::Tese)));
        if (!features.tessellationShader)
            TCU_THROW(NotSupportedError, "Tessellation shader not supported");
    }

    if (m_params.flow.hasShader(ShaderTypes::Geom) && !features.geometryShader)
        TCU_THROW(NotSupportedError, "Geometry shader not supported");

    if (m_params.layout.width == 16u)
    {
        const auto &features16 = context.getShaderFloat16Int8Features();
        if (!(features16.shaderFloat16 && features11.storageInputOutput16))
            TCU_THROW(NotSupportedError, "16-bit floats not supported in shader code");
    }

    if (m_params.layout.width == 64u && !features.shaderFloat64)
        TCU_THROW(NotSupportedError, "Double-precision floats not supported");
}

TestInstance *ShaderComponentDecoratedIterfaceMatchingTestCase::createInstance(Context &ctx) const
{
    return new ShaderComponentDecoratedIterfaceMatchingTestInstance(ctx, TestCase::getName(), m_params);
}

void ShaderGen<ShaderTypes::Any>::genExts(std::ostream &str, const Layout &layout) const
{
    switch (layout.width)
    {
    case 16:
        str << "#extension GL_EXT_shader_explicit_arithmetic_types_float16 : require\n";
        break;
    case 64:
        str << "#extension GL_EXT_shader_explicit_arithmetic_types_float64 : require\n";
        break;
    }
}

const char *Layout::getTypeName(Types type, uint32_t otherWidth) const
{
    DE_ASSERT(0 == otherWidth || 16 == otherWidth || 32 == otherWidth || 64 == otherWidth);
    static const char *names[][3] = {
        {"float16_t", "float", "float64_t"},
        {"f16vec2", "vec2", "f64vec2"},
        {"f16vec3", "vec3", "..."},
    };
    const uint32_t w = (0 == otherWidth) ? width : otherWidth;
    return names[uint32_t(type)][de::findLSB(w) - 4];
}

std::string ShaderGen<ShaderTypes::Any>::genArrSubscript(const tcu::Maybe<int32_t> &index) const
{
    return (false == index)  ? "[]" :
           (index.get() < 0) ? "[gl_MaxPatchVertices]" :
                               ('[' + de::toString(index.get()) + ']');
}

std::string ShaderGen<ShaderTypes::Any>::genLocSubscript(const Layout &layout,
                                                         const tcu::Maybe<uint32_t> &location) const
{
    return (true == location)      ? ('[' + de::toString(location.get()) + ']') :
           (layout.loccount == 0u) ? std::string() :
                                     ('[' + de::toString(layout.loccount - 1u) + ']');
}

/*
 * +============================================+
 * |        betweenShadersExtent                |
 * +=============+======+=======================+
 * |    value    | rank |          type         |
 * +-------------+------+-----------------------+
 * |   nothing   |  0   |                       |
 * |      0      |  1   |           []          |
 * |  positive   |  1   |       [positive]      |
 * |  negative   |  1   | [gl_MaxPatchVertices] |
 * +-------------+------+-----------------------+
 */
void ShaderGen<ShaderTypes::Any>::genLayout(std::ostream &str, const Layout &layout, bool inOrOut,
                                            const std::string &inVarName, const std::string &outVarName,
                                            const tcu::Maybe<int32_t> &betweenShadersExtent) const
{
    std::string indent;
    const std::string dir      = inOrOut ? "in" : "out";
    const std::string &varName = inOrOut ? inVarName : outVarName;
    const std::string bsExtent =
        (false == betweenShadersExtent)   ? "" :
        (betweenShadersExtent.get() == 0) ? "[]" :
        (betweenShadersExtent.get() > 0)  ? ("[" + de::toString(betweenShadersExtent.get()) + "]") :
                                            "[gl_MaxPatchVertices]";
    const std::string locExtent = (layout.loccount == 0u) ? std::string() : genArrSubscript(layout.loccount);

    if (layout.mode != Modes::LooseVariable)
    {
        indent = "    ";
        str << "layout(location = " << layout.location << ") " << dir << ' ' << (inOrOut ? "In" : "Out") << '\n';
        str << "{\n";
    }
    for (uint32_t i = 0u; i < layout.components.types.size(); ++i)
    {
        const auto component = layout.components.types.at(i);
        if (layout.components.isComponent(component))
        {
            str << indent << "layout("
                << "location = " << layout.location << ", "
                << "component = " << i << ") " << ((layout.mode == Modes::LooseVariable) ? dir : "") << ' ' << "flat "
                << layout.getTypeName(component) << ' ' << varName << i;
            if (layout.mode == Modes::LooseVariable)
                str << bsExtent;
            str << locExtent << ";\n";
        }
    }
    if (layout.mode != Modes::LooseVariable)
    {
        str << "}\n";
        str << (inOrOut ? "var_in" : "var_out") << bsExtent << ";\n";
    }
}
void ShaderGen<ShaderTypes::Vert>::genCode(std::ostream &str, const TestParams &params) const
{
    const std::string var       = "o";
    const Layout &layout        = params.layout;
    const std::string outputVar = (layout.mode == Modes::LooseVariable) ? var : ("var_out." + var);

    float value = 1.0f / 8.0f;
    if (params.flow.hasShader(ShaderTypes::Tesc))
        value /= 2.0f;
    if (params.flow.hasShader(ShaderTypes::Geom))
        value /= 2.0f;

    str << "#version 450\n";
    genExts(str, layout);
    str << "layout(location = 0) in vec4 pos;\n";
    genLayout(str, layout, false, {}, var);
    str << "void main()\n";
    str << "{\n";
    str << "    gl_Position = vec4(pos.xy, 0.0, 1.0);\n";

    auto writeValues = [&](uint32_t componentWidth)
    {
        str << '(';
        for (uint32_t k = 0u; k < componentWidth; ++k)
        {
            if (k)
                str << ", ";
            str << value;
            value *= 2.0f;
        }
        str << ')';
    };

    for (uint32_t i = 0u; i < layout.components.types.size(); ++i)
    {
        const auto component = layout.components.types.at(i);
        if (layout.components.isComponent(component))
        {
            const auto cast = layout.getTypeName(component);

            str << "    " << outputVar << i << genLocSubscript(layout) << " = ";

            switch (component)
            {
            case Types::Vec3:
                str << cast;
                writeValues(3u);
                break;
            case Types::Vec2:
                str << cast;
                writeValues(2u);
                break;
            case Types::Scalar:
                str << cast;
                writeValues(1u);
                break;
            default:
                DE_ASSERT(false);
                break;
            }

            str << ";\n";
        }
    }
    str << "}\n";
}
void ShaderGen<ShaderTypes::Tesc>::genCode(std::ostream &str, const TestParams &params) const
{
    const std::string inputVar  = "i";
    const std::string outputVar = "o";
    const Layout &layout        = params.layout;

    str << "#version 450\n";
    genExts(str, layout);
    str << "#extension GL_EXT_tessellation_shader : require\n";
    str << "layout(vertices = 3) out;\n";
    genLayout(str, layout, true, inputVar, outputVar, -1);
    genLayout(str, layout, false, inputVar, outputVar, 3);
    str << "void main()\n";
    str << "{\n";
    str << "    gl_TessLevelOuter[0] = 2.0;\n";
    str << "    gl_TessLevelOuter[1] = 4.0;\n";
    str << "    gl_TessLevelOuter[2] = 6.0;\n";
    str << "    gl_TessLevelOuter[3] = 8.0;\n";
    str << "    gl_TessLevelInner[0] = 8.0;\n";
    str << "    gl_TessLevelInner[1] = 8.0;\n";
    str << "    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n\n";

    for (uint32_t variableIdx = 0u; variableIdx < layout.components.types.size(); ++variableIdx)
    {
        const auto component = layout.components.types.at(variableIdx);
        if (layout.components.isComponent(component))
        {
            str << "    ";

            if (layout.mode == Modes::LooseVariable)
            {
                str << outputVar << variableIdx << "[gl_InvocationID]";
            }
            else
            {
                str << "var_out[gl_InvocationID]." << outputVar << variableIdx;
            }

            str << genLocSubscript(layout) << " = ";

            if (layout.mode == Modes::LooseVariable)
            {
                str << inputVar << variableIdx << "[gl_InvocationID]";
            }
            else
            {
                str << "var_in[gl_InvocationID]." << inputVar << variableIdx;
            }

            str << genLocSubscript(layout) << ";\n";
        }
    }

    str << "}\n";
}
void ShaderGen<ShaderTypes::Tese>::genCode(std::ostream &str, const TestParams &params) const
{
    const std::string inputVar  = "i";
    const std::string outputVar = "o";
    const Layout &layout        = params.layout;

    str << "#version 450\n";
    genExts(str, layout);
    str << "#extension GL_EXT_tessellation_shader : require\n";
    str << "layout(triangles, cw) in;\n";
    genLayout(str, layout, true, inputVar, outputVar, -1);
    genLayout(str, layout, false, inputVar, outputVar);
    str << "void main()\n";
    str << "{\n";
    str << "    float u = gl_TessCoord.x;\n";
    str << "    float v = gl_TessCoord.y;\n";
    str << "    float w = gl_TessCoord.z;\n";
    str << "    vec4 pos0 = gl_in[0].gl_Position;\n";
    str << "    vec4 pos1 = gl_in[1].gl_Position;\n";
    str << "    vec4 pos2 = gl_in[2].gl_Position;\n";
    str << "    gl_Position = u * pos0 + v * pos1 + w * pos2;\n\n";
    for (uint32_t variableIdx = 0u; variableIdx < layout.components.types.size(); ++variableIdx)
    {
        const auto component = layout.components.types.at(variableIdx);
        if (layout.components.isComponent(component))
        {
            str << "    ";

            if (layout.mode == Modes::LooseVariable)
            {
                str << outputVar << variableIdx << genLocSubscript(layout);
            }
            else
            {
                str << "var_out." << outputVar << variableIdx << genLocSubscript(layout);
            }

            str << " = ";

            str << '(';
            if (layout.mode == Modes::LooseVariable)
            {
                str << inputVar << variableIdx << genArrSubscript(0) << genLocSubscript(layout) << " + " << inputVar
                    << variableIdx << genArrSubscript(1) << genLocSubscript(layout) << " + " << inputVar << variableIdx
                    << genArrSubscript(2) << genLocSubscript(layout);
            }
            else
            {
                str << "var_in" << genArrSubscript(0) << '.' << inputVar << variableIdx << genLocSubscript(layout)
                    << "  +  var_in" << genArrSubscript(1) << '.' << inputVar << variableIdx << genLocSubscript(layout)
                    << "  +  var_in" << genArrSubscript(2) << '.' << inputVar << variableIdx << genLocSubscript(layout);
            }
            str << ") / " << layout.getTypeName(Types::Scalar) << "(1.5);\n";
        }
    }

    str << "}\n";
}
void ShaderGen<ShaderTypes::Geom>::genCode(std::ostream &str, const TestParams &params) const
{
    const std::string inVar  = "i";
    const std::string outVar = "o";
    const Layout &layout     = params.layout;

    str << "#version 450\n\n";
    genExts(str, layout);
    str << "#extension GL_EXT_geometry_shader : require\n\n";
    genLayout(str, layout, true, inVar, {}, 3);
    str << "layout(triangles) in;\n\n";
    genLayout(str, layout, false, {}, outVar);
    str << "layout(triangle_strip, max_vertices = 3) out;\n\n";
    str << "void main()\n";
    str << "{\n";
    str << "    for (uint i = 0; i < 3; ++i)\n";
    str << "    {\n";
    str << "        gl_Position = gl_in[i].gl_Position;\n";
    for (uint32_t variableIdx = 0u; variableIdx < layout.components.types.size(); ++variableIdx)
    {
        const auto component = layout.components.types.at(variableIdx);
        if (layout.components.isComponent(component))
        {
            str << "        ";
            if (layout.mode != Modes::LooseVariable)
                str << "var_out.";
            str << outVar << variableIdx << genLocSubscript(layout);
            str << " = ";
            if (layout.mode == Modes::LooseVariable)
                str << inVar << variableIdx << "[i]";
            else
                str << "var_in[i]." << inVar << variableIdx;
            str << genLocSubscript(layout) << " * " << layout.getTypeName(Types::Scalar) << "(2.0);\n";
        }
    }
    str << "        EmitVertex();\n";
    str << "    }\n";
    str << "    EndPrimitive();\n";
    str << "}\n";
}
void ShaderGen<ShaderTypes::Frag>::genCode(std::ostream &str, const TestParams &params) const
{
    float value                    = 0.125f;
    int32_t consumed               = 0;
    const std::string variableName = "i";
    const Layout &layout           = params.layout;

    str << "#version 450\n";
    genExts(str, layout);
    str << "layout(location = 0) out vec4 dEQP_color;\n";
    genLayout(str, layout, true, variableName);
    str << "void main()\n";
    str << "{\n";
    str << "    dEQP_color = vec4(";
    for (uint32_t variableIdx = 0u; variableIdx < layout.components.types.size(); ++variableIdx)
    {
        const auto component = layout.components.types.at(variableIdx);
        if (layout.components.isComponent(component))
        {
            consumed += layout.components.consumeCount(component);

            if (variableIdx > 0u)
                str << ", ";

            str << layout.getTypeName(component, 32) << '(';

            if (layout.mode != Modes::LooseVariable)
                str << "var_in.";

            str << variableName << variableIdx << genLocSubscript(layout);

            str << ')';
        }
    }
    value = std::ldexp(value, consumed);
    while (consumed++ < 4)
    {
        str << ", " << value;
        value *= 2.0f;
    }
    str << ");\n";
    str << "}\n";
}

#ifdef DUMP_SHADERS
void saveShader(const std::string &name, const std::string &code)
{
    bool equals = false;
    std::ifstream existingShaderFile(name);
    if (existingShaderFile.is_open())
    {
        std::string existingCode;
        existingShaderFile >> existingCode;
        equals = code == existingCode;
        existingShaderFile.close();
    }
    if (false == equals)
    {
        std::ofstream newShaderFile(name);
        if (newShaderFile.is_open())
        {
            newShaderFile << code;
            newShaderFile.close();
        }
        else
        {
            std::cout << "Unable to write " << name << " file" << std::endl;
        }
    }
}
#endif

void ShaderComponentDecoratedIterfaceMatchingTestCase::initPrograms(SourceCollections &programCollection) const
{
    for (uint32_t shaderIdx = 0u; shaderIdx < m_params.flow.shaders.size(); ++shaderIdx)
    {
        if (uint32_t(m_params.flow.shaders.at(shaderIdx)) >= uint32_t(ShaderTypes::Any))
            break;
        const ShaderGen<ShaderTypes::Any> &generator = shaderGenerator(m_params.flow.shaders.at(shaderIdx));
        std::ostringstream code;
        generator.genCode(code, m_params);
        code.flush();
        programCollection.glslSources.add(generator.name()) << *generator.makeSource(code.str());
#ifdef DUMP_SHADERS
        saveShader((m_fullTestName + '.' + generator.name()), code.str());
#endif
    }
}

GraphicsPipelineWrapper ShaderComponentDecoratedIterfaceMatchingTestInstance::createPipeline(
    const PipelineLayoutWrapper &pipelineLayout, const RenderPassWrapper &renderPass)
{
    const DeviceInterface &vkd            = m_context.getDeviceInterface();
    const InstanceInterface &vki          = m_context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice = m_context.getPhysicalDevice();
    const VkDevice device                 = m_context.getDevice();
    const bool hasTessellation            = m_params.flow.hasShader(ShaderTypes::Tesc);
    const bool hasGeometry                = m_params.flow.hasShader(ShaderTypes::Geom);
    const VkPrimitiveTopology topology =
        hasTessellation ? VK_PRIMITIVE_TOPOLOGY_PATCH_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    const uint32_t subpass = 0u;
    const std::vector<VkViewport> viewports{makeViewport(m_params.width, m_params.height)};
    const std::vector<VkRect2D> scissors{makeRect2D(m_params.width, m_params.height)};

    const auto bindingDesc =
        makeVertexInputBindingDescription(0u, uint32_t(sizeof(tcu::Vec4)), VK_VERTEX_INPUT_RATE_VERTEX);
    const auto attributeDesc = makeVertexInputAttributeDescription(0u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, 0u);

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType sType;
        (const void *)nullptr,                                     // const void* pNext;
        (VkPipelineVertexInputStateCreateFlags)0u,                 // VkPipelineVertexInputStateCreateFlags flags;
        1u,                                                        // uint32_t vertexBindingDescriptionCount;
        &bindingDesc,   // const VkVertexInputBindingDescription* pVertexBindingDescriptions;
        1u,             // uint32_t vertexAttributeDescriptionCount;
        &attributeDesc, // const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
    };

    const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, // VkStructureType sType;
        (const void *)nullptr,                                       // const void* pNext;
        (VkPipelineInputAssemblyStateCreateFlags)0u,                 // VkPipelineInputAssemblyStateCreateFlags flags;
        topology,                                                    // VkPrimitiveTopology topology;
        VK_FALSE,                                                    // VkBool32 primitiveRestartEnable;
    };

    const VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, // VkStructureType sType;
        (const void *)nullptr,                                      // const void* pNext;
        (VkPipelineRasterizationStateCreateFlags)0u,                // VkPipelineRasterizationStateCreateFlags flags;
        VK_FALSE,                                                   // VkBool32 depthClampEnable;
        VK_FALSE,                                                   // VkBool32 rasterizerDiscardEnable;
        VK_POLYGON_MODE_FILL,                                       // VkPolygonMode polygonMode;
        VK_CULL_MODE_NONE,                                          // VkCullModeFlags cullMode;
        VK_FRONT_FACE_CLOCKWISE,                                    // VkFrontFace frontFace;
        VK_FALSE,                                                   // VkBool32 depthBiasEnable;
        0.0f,                                                       // float depthBiasConstantFactor;
        0.0f,                                                       // float depthBiasClamp;
        0.0f,                                                       // float depthBiasSlopeFactor;
        1.0f,                                                       // float lineWidth;
    };

    const VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType sType;
        (const void *)nullptr,                                    // const void* pNext;
        (VkPipelineMultisampleStateCreateFlags)0u,                // VkPipelineMultisampleStateCreateFlags flags;
        VK_SAMPLE_COUNT_1_BIT,                                    // VkSampleCountFlagBits rasterizationSamples;
        VK_FALSE,                                                 // VkBool32 sampleShadingEnable;
        0.0f,                                                     // float minSampleShading;
        (const VkSampleMask *)nullptr,                            // const VkSampleMask* pSampleMask;
        VK_FALSE,                                                 // VkBool32 alphaToCoverageEnable;
        VK_FALSE,                                                 // VkBool32 alphaToOneEnable;
    };

    const VkPipelineColorBlendAttachmentState colorBlendAttachmentState{
        VK_FALSE,             // VkBool32                 blendEnable
        VK_BLEND_FACTOR_ZERO, // VkBlendFactor            srcColorBlendFactor
        VK_BLEND_FACTOR_ZERO, // VkBlendFactor            dstColorBlendFactor
        VK_BLEND_OP_ADD,      // VkBlendOp                colorBlendOp
        VK_BLEND_FACTOR_ZERO, // VkBlendFactor            srcAlphaBlendFactor
        VK_BLEND_FACTOR_ZERO, // VkBlendFactor            dstAlphaBlendFactor
        VK_BLEND_OP_ADD,      // VkBlendOp                alphaBlendOp
        (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
         VK_COLOR_COMPONENT_A_BIT) //        colorWriteMask
    };

    const VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, // VkStructureType                               sType
        (const void *)nullptr,                                    // const void*                                   pNext
        (VkPipelineColorBlendStateCreateFlags)0u,                 // VkPipelineColorBlendStateCreateFlags          flags
        VK_FALSE,                   // VkBool32                                      logicOpEnable
        VK_LOGIC_OP_CLEAR,          // VkLogicOp                                     logicOp
        1u,                         // uint32_t                                      attachmentCount
        &colorBlendAttachmentState, // const VkPipelineColorBlendAttachmentState*    pAttachments
        {0.0f, 0.0f, 0.0f, 0.0f}    // float                                         blendConstants[4]
    };

    m_vertShaderModule = ShaderWrapper(
        vkd, device, m_context.getBinaryCollection().get(instance<ShaderGen<ShaderTypes::Vert>>().name()));
    m_fragShaderModule = ShaderWrapper(
        vkd, device, m_context.getBinaryCollection().get(instance<ShaderGen<ShaderTypes::Frag>>().name()));

    if (hasTessellation)
    {
        m_tesCShaderModule = ShaderWrapper(
            vkd, device, m_context.getBinaryCollection().get(instance<ShaderGen<ShaderTypes::Tesc>>().name()));
        m_tesEShaderModule = ShaderWrapper(
            vkd, device, m_context.getBinaryCollection().get(instance<ShaderGen<ShaderTypes::Tese>>().name()));
    }

    if (hasGeometry)
    {
        m_geomShaderModule = ShaderWrapper(
            vkd, device, m_context.getBinaryCollection().get(instance<ShaderGen<ShaderTypes::Geom>>().name()));
    }

    GraphicsPipelineWrapper pipeline(vki, vkd, physicalDevice, device, m_context.getDeviceExtensions(),
                                     m_params.pipelineConstructionType);

    pipeline.setupVertexInputState(&vertexInputStateCreateInfo, &inputAssemblyStateCreateInfo)
        .setDefaultPatchControlPoints(hasTessellation ? 3u : 0u)
        .setupPreRasterizationShaderState(viewports, scissors, pipelineLayout, *renderPass, subpass, m_vertShaderModule,
                                          &rasterizationStateCreateInfo, m_tesCShaderModule, m_tesEShaderModule,
                                          m_geomShaderModule)
        .setupFragmentShaderState(pipelineLayout, *renderPass, subpass, m_fragShaderModule,
                                  (const VkPipelineDepthStencilStateCreateInfo *)nullptr, &multisampleStateCreateInfo)
        .setupFragmentOutputState(*renderPass, subpass, &colorBlendStateCreateInfo, &multisampleStateCreateInfo)
        .setMonolithicPipelineLayout(pipelineLayout)
        .buildPipeline();

    return pipeline;
}

RenderPassWrapper ShaderComponentDecoratedIterfaceMatchingTestInstance::createRenderPass(VkImage image,
                                                                                         VkImageView view)
{
    const DeviceInterface &vkd = m_context.getDeviceInterface();
    const VkDevice dev         = m_context.getDevice();

    const VkAttachmentDescription attachmentDescription{
        (VkAttachmentDescriptionFlags)0,         // VkAttachmentDescriptionFlags    flags
        m_params.format,                         // VkFormat                        format
        VK_SAMPLE_COUNT_1_BIT,                   // VkSampleCountFlagBits           samples
        VK_ATTACHMENT_LOAD_OP_CLEAR,             // VkAttachmentLoadOp              loadOp
        VK_ATTACHMENT_STORE_OP_STORE,            // VkAttachmentStoreOp             storeOp
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,         // VkAttachmentLoadOp              stencilLoadOp
        VK_ATTACHMENT_STORE_OP_DONT_CARE,        // VkAttachmentStoreOp             stencilStoreOp
        VK_IMAGE_LAYOUT_UNDEFINED,               // VkImageLayout                   initialLayout
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL // VkImageLayout                   finalLayout
    };

    const VkAttachmentReference attachmentReference{
        0u,                                      // uint32_t         attachment
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL // VkImageLayout    layout
    };

    const VkSubpassDescription subpassDescription{
        (VkSubpassDescriptionFlags)0,           // VkSubpassDescriptionFlags       flags
        VK_PIPELINE_BIND_POINT_GRAPHICS,        // VkPipelineBindPoint             pipelineBindPoint
        0u,                                     // uint32_t                        inputAttachmentCount
        (const VkAttachmentReference *)nullptr, // const VkAttachmentReference*    pInputAttachments
        1u,                                     // uint32_t                        colorAttachmentCount
        &attachmentReference,                   // const VkAttachmentReference*    pColorAttachments
        (const VkAttachmentReference *)nullptr, // const VkAttachmentReference*    pResolveAttachments
        (const VkAttachmentReference *)nullptr, // const VkAttachmentReference*    pDepthStencilAttachment
        0u,                                     // uint32_t                        preserveAttachmentCount
        (const uint32_t *)nullptr               // const uint32_t*                 pPreserveAttachments
    };

    const VkRenderPassCreateInfo renderPassCreateInfo{
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, // VkStructureType                   sType
        (const void *)nullptr,                     // const void*                       pNext
        (VkRenderPassCreateFlags)0,                // VkRenderPassCreateFlags           flags
        1u,                                        // uint32_t                          attachmentCount
        &attachmentDescription,                    // const VkAttachmentDescription*    pAttachments
        1u,                                        // uint32_t                          subpassCount
        &subpassDescription,                       // const VkSubpassDescription*       pSubpasses
        0u,                                        // uint32_t                          dependencyCount
        (const VkSubpassDependency *)nullptr       // const VkSubpassDependency*        pDependencies
    };

    RenderPassWrapper renderPass(m_params.pipelineConstructionType, vkd, dev, &renderPassCreateInfo);
    renderPass.createFramebuffer(vkd, dev, image, view, m_params.width, m_params.height);

    return renderPass;
}

bool ShaderComponentDecoratedIterfaceMatchingTestInstance::verifyResult(const tcu::TextureFormat &format,
                                                                        const uint8_t *bufferPtr,
                                                                        std::string &errorText) const
{
    const tcu::Vec4 ref(0.125f, 0.25f, 0.5f, 1.0f);
    const tcu::ConstPixelBufferAccess result(format, tcu::IVec3(m_params.width, m_params.height, 1), bufferPtr);
    for (int32_t y = 0; y < int32_t(m_params.height); ++y)
    {
        for (int32_t x = 0; x < int32_t(m_params.width); ++x)
        {
            const auto px = result.getPixel(x, y);
            if (px != ref)
            {
                std::ostringstream str;
                str << "First mismatch at (" << x << ',' << y << "); ";
                str << "Expected: " << ref << ", got: " << px;
                errorText = str.str();
                return false;
            }
        }
    }

    return true;
}

tcu::TestStatus ShaderComponentDecoratedIterfaceMatchingTestInstance::iterate(void)
{
    const DeviceInterface &vkd          = m_context.getDeviceInterface();
    const VkDevice device               = m_context.getDevice();
    Allocator &allocator                = m_context.getDefaultAllocator();
    const uint32_t queueFamilyIndex     = m_context.getUniversalQueueFamilyIndex();
    const std::vector<uint32_t> qfidxs  = {queueFamilyIndex};
    const VkQueue queue                 = m_context.getUniversalQueue();
    tcu::TestLog &log                   = m_context.getTestContext().getLog();
    const tcu::TextureFormat format     = mapVkFormat(m_params.format);
    const VkDeviceSize resultBufferSize = m_params.width * m_params.height * format.getPixelSize();

    const VkImageCreateInfo colorImageCreateInfo{
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                                   // VkStructureType sType;
        (const void *)nullptr,                                                 // const void* pNext;
        (VkImageCreateFlags)0u,                                                // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                                                      // VkImageType imageType;
        m_params.format,                                                       // VkFormat format;
        {m_params.width, m_params.height, 1u},                                 // VkExtent3D extent;
        1u,                                                                    // uint32_t mipLevels;
        1u,                                                                    // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,                                                 // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,                                               // VkImageTiling tiling;
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,                                             // VkSharingMode sharingMode;
        1u,                                                                    // uint32_t queueFamilyIndexCount;
        &queueFamilyIndex,                                                     // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,                                             // VkImageLayout initialLayout;
    };
    ImageWithMemory colorImage(vkd, device, allocator, colorImageCreateInfo, MemoryRequirement::Any);

    const VkImageSubresourceRange subresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
    const VkImageViewCreateInfo colorImageViewCreateInfo{
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType sType;
        (const void *)nullptr,                    // const void* pNext;
        (VkImageViewCreateFlags)0u,               // VkImageViewCreateFlags flags;
        *colorImage,                              // VkImage image;
        VK_IMAGE_VIEW_TYPE_2D,                    // VkImageViewType viewType;
        m_params.format,                          // VkFormat format;
        makeComponentMappingRGBA(),               // VkComponentMapping components;
        subresourceRange                          // VkImageSubresourceRange subresourceRange;
    };
    Move<VkImageView> colorImageView   = createImageView(vkd, device, &colorImageViewCreateInfo);
    const RenderPassWrapper renderPass = createRenderPass(*colorImage, *colorImageView);
    const PipelineLayoutWrapper pipelineLayout(m_params.pipelineConstructionType, vkd, device);
    GraphicsPipelineWrapper graphicsPipeline = createPipeline(pipelineLayout, renderPass);

    const uint32_t vertexCount = 6u;
    const auto vertexBufferCI =
        makeBufferCreateInfo(vertexCount * 4u * sizeof(float), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, qfidxs);
    BufferWithMemory vertexBuffer(vkd, device, allocator, vertexBufferCI, MemoryRequirement::HostVisible);
    {
        std::vector<float> vertices{-1.0f, +1.0f, 0.0f, 0.0f, -1.0f, -1.0f, 0.0f, 0.0f, +1.0f, -1.0f, 0.0f, 0.0f,
                                    +1.0f, -1.0f, 0.0f, 0.0f, +1.0f, +1.0f, 0.0f, 0.0f, -1.0f, +1.0f, 0.0f, 0.0f};
        DE_ASSERT((vertices.size() / 4) == vertexCount);
        deMemcpy(vertexBuffer.getAllocation().getHostPtr(), vertices.data(), vertices.size() * sizeof(float));
    }

    const VkBufferUsageFlags resultBufferUsage =
        (VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const auto resultBufferCI = makeBufferCreateInfo(resultBufferSize, resultBufferUsage, qfidxs);
    BufferWithMemory resultBuffer(vkd, device, allocator, resultBufferCI, MemoryRequirement::HostVisible);

    Move<VkCommandPool> cmdPool     = makeCommandPool(vkd, device, queueFamilyIndex);
    Move<VkCommandBuffer> cmdBuffer = allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    beginCommandBuffer(vkd, *cmdBuffer);
    graphicsPipeline.bind(*cmdBuffer);
    vkd.cmdBindVertexBuffers(*cmdBuffer, 0, 1, &vertexBuffer.get(), &static_cast<const VkDeviceSize &>(0ull));
    renderPass.begin(vkd, *cmdBuffer, makeRect2D(m_params.width, m_params.height),
                     tcu::Vec4(0.5f, -1.0f, 0.625f, 1.0f));
    vkd.cmdDraw(*cmdBuffer, vertexCount, 1, 0, 0);
    renderPass.end(vkd, *cmdBuffer);
    copyImageToBuffer(vkd, *cmdBuffer, *colorImage, *resultBuffer, tcu::IVec2(m_params.width, m_params.height));
    endCommandBuffer(vkd, *cmdBuffer);
    submitCommandsAndWait(vkd, device, queue, *cmdBuffer);

    //invalidateAlloc(vkd, device, resultBuffer.getAllocation());

    std::string resultText;
    const uint8_t *bufferPtr = static_cast<uint8_t *>(resultBuffer.getAllocation().getHostPtr());
    if (verifyResult(format, bufferPtr, resultText))
        return tcu::TestStatus::pass("");

    const tcu::ConstPixelBufferAccess resultAccess(format, tcu::IVec3(m_params.width, m_params.height, 1), bufferPtr);

    log << tcu::TestLog::ImageSet("Result of rendering", "") << tcu::TestLog::Image("Result", "", resultAccess)
        << tcu::TestLog::EndImageSet;
    return tcu::TestStatus::fail(resultText);
}

bool Flow::isShader(const ShaderTypes &shader) const
{
    return (shader == ShaderTypes::Vert || shader == ShaderTypes::Tesc || shader == ShaderTypes::Tese ||
            shader == ShaderTypes::Geom || shader == ShaderTypes::Frag);
}

bool Flow::hasShader(const ShaderTypes &shader) const
{
    for (uint32_t i = 0u; i < shaders.size(); ++i)
    {
        if (shaders.at(i) == shader)
            return true;
    }
    return false;
}

std::string Flow::toString() const
{
    const std::map<ShaderTypes, const char *> shaderNames{
        {ShaderTypes::Vert, instance<ShaderGen<ShaderTypes::Vert>>().name()},
        {ShaderTypes::Tesc, instance<ShaderGen<ShaderTypes::Tesc>>().name()},
        {ShaderTypes::Tese, instance<ShaderGen<ShaderTypes::Tese>>().name()},
        {ShaderTypes::Geom, instance<ShaderGen<ShaderTypes::Geom>>().name()},
        {ShaderTypes::Frag, instance<ShaderGen<ShaderTypes::Frag>>().name()},
    };
    std::ostringstream str;
    for (uint32_t i = 0u; i < shaders.size(); ++i)
    {
        if (isShader(shaders.at(i)))
        {
            if (i)
                str << '_';
            str << shaderNames.at(shaders.at(i));
        }
    }
    str.flush();
    return str.str();
}

int32_t Components::consumeCount(const Types &type) const
{
    switch (type)
    {
    case Types::Scalar:
        return 1;
    case Types::Vec2:
        return 2;
    case Types::Vec3:
        return 3;
    default:
        break;
    }
    return 0;
}
bool Components::isComponent(const Types &type) const
{
    return (type == Types::Scalar || type == Types::Vec2 || type == Types::Vec3);
}
std::string Components::testName() const
{
    std::ostringstream str;
    for (uint32_t i = 0u; i < types.size(); ++i)
    {
        const auto component = types.at(i);
        if (isComponent(component))
        {
            if (i)
                str << '_';
            switch (component)
            {
            case Types::Scalar:
                str << "scalar";
                break;
            case Types::Vec2:
                str << "vec2";
                break;
            case Types::Vec3:
                str << "vec3";
                break;
            default:
                DE_ASSERT(false);
                break;
            }
        }
    }
    str.flush();
    return str.str();
}

constexpr Components SCALAR_SCALAR_SCALAR_SCALAR{Types::Scalar, Types::Scalar, Types::Scalar, Types::Scalar};
constexpr Components SCALAR_SCALAR_VEC2{Types::Scalar, Types::Scalar, Types::Vec2};
constexpr Components SCALAR_VEC2_SCALAR{Types::Scalar, Types::Vec2, Types::None, Types::Scalar};
constexpr Components VEC2_SCALAR_SCALAR{Types::Vec2, Types::None, Types::Scalar, Types::Scalar};
constexpr Components SCALAR_VEC3{Types::Scalar, Types::Vec3};
constexpr Components VEC3_SCALAR{Types::Vec3, Types::None, Types::None, Types::Scalar};
constexpr Components VEC2_VEC2{Types::Vec2, Types::None, Types::Vec2};
constexpr Components SCALAR_SCALAR{Types::Scalar, Types::None, Types::Scalar};
constexpr Components VEC2{Types::Vec2};

constexpr Flow VERT_FRAG                = {ShaderTypes::Vert, ShaderTypes::Frag};
constexpr Flow VERT_GEOM_FRAG           = {ShaderTypes::Vert, ShaderTypes::Geom, ShaderTypes::Frag};
constexpr Flow VERT_TESC_TESE_FRAG      = {ShaderTypes::Vert, ShaderTypes::Tesc, ShaderTypes::Tese, ShaderTypes::Frag};
constexpr Flow VERT_TESC_TESE_GEOM_FRAG = {ShaderTypes::Vert, ShaderTypes::Tesc, ShaderTypes::Tese, ShaderTypes::Geom,
                                           ShaderTypes::Frag};

template <class X, class... Y>
de::MovePtr<X> makeMovePtr(Y &&...y)
{
    return de::MovePtr<X>(new X(std::forward<Y>(y)...));
}

} // anonymous namespace

TestCaseGroup *createShaderCompDecorLayoutMatchingTests(tcu::TestContext &testCtx,
                                                        PipelineConstructionType pipelineConstructionType)
{
    const std::pair<Modes, const char *> modes[]{
        {Modes::LooseVariable, "loose_var"}, {Modes::VariableInBlock, "in_block"},
        //{ Modes::VariableInStruct, "in_struct"        }
    };
    const std::pair<uint32_t, const char *> widths[]{{64, "float64"}, {32, "float32"}, {16, "float16"}};
    const std::pair<Components, std::array<uint32_t, 3>> componentSeries[]{{SCALAR_SCALAR_SCALAR_SCALAR, {16, 32}},
                                                                           {SCALAR_SCALAR_VEC2, {16, 32}},
                                                                           {SCALAR_VEC2_SCALAR, {16, 32}},
                                                                           {VEC2_SCALAR_SCALAR, {16, 32}},
                                                                           {SCALAR_VEC3, {16, 32}},
                                                                           {VEC3_SCALAR, {16, 32}},
                                                                           {VEC2_VEC2, {16, 32}},
                                                                           {SCALAR_SCALAR, {64}},
                                                                           {VEC2, {64}}};
    const Flow flows[]{VERT_FRAG, VERT_GEOM_FRAG, VERT_TESC_TESE_FRAG, VERT_TESC_TESE_GEOM_FRAG};
    const std::pair<uint32_t, const char *> locationCounts[]{{0, "single_location"}, {3, "multiple_locations"}};

    // Example test name: vert_frag.loose_var.float16.single_location.scalar_vec2_scalar

    auto groupRoot         = makeMovePtr<TestCaseGroup>(testCtx, "shader_layout_component_matching", "");
    uint32_t startLocation = 0u;
    for (const Flow &flow : flows)
    {
        auto groupFlow = makeMovePtr<TestCaseGroup>(testCtx, flow.toString().c_str());
        for (const std::pair<Modes, const char *> &mode : modes)
        {
            auto groupMode = makeMovePtr<TestCaseGroup>(testCtx, mode.second);
            for (const std::pair<uint32_t, const char *> &width : widths)
            {
                auto groupSize = makeMovePtr<TestCaseGroup>(testCtx, width.second);
                for (const std::pair<uint32_t, const char *> &locationCount : locationCounts)
                {
                    auto groupLocCount = makeMovePtr<TestCaseGroup>(testCtx, locationCount.second);
                    for (const std::pair<Components, std::array<uint32_t, 3>> &components : componentSeries)
                    {
                        if (!de::contains(components.second.begin(), components.second.end(), width.first))
                        {
                            continue;
                        }

                        const std::string testName = components.first.testName();

                        TestParams p{pipelineConstructionType,
                                     VK_FORMAT_R32G32B32A32_SFLOAT,
                                     Layout(components.first, ((startLocation++ % 4u) + 1u), locationCount.first,
                                            mode.first, width.first),
                                     16u, // framebuffer width
                                     16u, // framebuffer height
                                     flow};

                        std::ostringstream str;
#ifdef DUMP_SHADERS
                        str << flow.toString();
                        str << '.' << mode.second;
                        str << '.' << width.second;
                        str << '.' << locationCount.second;
                        str << '.' << testName;
#endif
                        groupLocCount->addChild(
                            new ShaderComponentDecoratedIterfaceMatchingTestCase(testCtx, testName, p, str.str()));
                    }
                    if (!groupLocCount->empty())
                        groupSize->addChild(groupLocCount.release());
                }
                if (!groupSize->empty())
                    groupMode->addChild(groupSize.release());
            }
            if (!groupMode->empty())
                groupFlow->addChild(groupMode.release());
        }
        if (!groupFlow->empty())
            groupRoot->addChild(groupFlow.release());
    }

    return groupRoot.release();
}

} // namespace pipeline
} // namespace vkt
