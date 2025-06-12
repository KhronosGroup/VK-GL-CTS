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
 * \file
 * \brief Tessellation Maximum IO Tests
 *//*--------------------------------------------------------------------*/

#include "vktTessellationMaxIOTests.hpp"
#include "deDefs.h"
#include "deMath.h"
#include "deMemory.h"
#include "deSTLUtil.hpp"
#include "deStringUtil.hpp"
#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"
#include "tcuTestContext.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuVectorType.hpp"
#include "vkPrograms.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTessellationUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuImageCompare.hpp"
#include "tcuImageIO.hpp"

#include "gluVarType.hpp"
#include "gluVarTypeUtil.hpp"

#include "vkDefs.hpp"
#include "vkBarrierUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"

#include "deUniquePtr.hpp"
#include "deSharedPtr.hpp"
#include "deRandom.hpp"
#include "vktTestPackage.hpp"

#include <cmath>
#include <cstdint>
#include <iterator>
#include <sstream>
#include <string>

using namespace std;

namespace vkt
{
namespace tessellation
{

using namespace vk;

namespace
{

static const uint32_t MAXIO_RENDER_SIZE_WIDTH  = 8u;
static const uint32_t MAXIO_RENDER_SIZE_HEIGHT = 8u;
static const uint32_t kSlotSize =
    4u; // https://registry.khronos.org/vulkan/specs/1.3-extensions/html/vkspec.html#interfaces-iointerfaces-locations
static const uint32_t kMaxTessellationControlPerVertexOutputComponents   = 128;
static const uint32_t kMaxTessellationControlPerPatchOutputComponents    = 120;
static const uint32_t kMaxTessellationEvaluationPerVertexInputComponents = 128;
static const std::string kProgramNamePrefixMock                          = "mock_";

enum class TessVarType
{
    TESS_VAR_VERT,
    TESS_VAR_PATCH,
};

enum class TessReadLevel
{
    TESS_READS_OUTER,
    TESS_READS_INNER,
    TESS_READS_OUTER_INNER,
    TESS_WRITES0_INNER_1,
    TESS_WRITES0_INNER_ALL,
    TESS_WRITES0_OUTER_1,
    TESS_WRITES0_OUTER_ALL,
    TESS_WRITES0_OUTER_INNER
};

enum class Owner
{
    VERTEX = 0,
    PATCH,
};

enum class DataType
{
    INTEGER = 0,
    FLOAT,
};

// Note: 8-bit variables not available for Input/Output.
enum class BitWidth
{
    B64 = 64,
    B32 = 32,
    B16 = 16,
};

enum class DataDim
{
    SCALAR = 1,
    VEC2   = 2,
    VEC3   = 3,
    VEC4   = 4,
};

enum class Interpolation
{
    NORMAL = 0,
    FLAT,
};

enum class Direction
{
    IN = 0,
    OUT,
};

// Limits for tessellation control outputs and evaluation shader inputs
struct TessDeviceLimits
{
    uint32_t maxTessellationControlPerVertexOutputComponents;
    uint32_t maxTessellationControlPerPatchOutputComponents;
    uint32_t maxTessellationEvaluationPerVertexInputComponents;
};

// Interface variable.
struct IfaceVar
{
    static constexpr uint32_t kNumVertices = 4u;
    static constexpr uint32_t kNumPatches  = 2u;
    static constexpr uint32_t kVarsPerType = 10u;

    IfaceVar(Owner owner_, DataType dataType_, BitWidth bitWidth_, DataDim dataDim_, Interpolation interpolation_,
             uint32_t index_)
        : owner(owner_)
        , dataType(dataType_)
        , bitWidth(bitWidth_)
        , dataDim(dataDim_)
        , interpolation(interpolation_)
        , index(index_)
    {
        DE_ASSERT(!(dataType == DataType::INTEGER && interpolation == Interpolation::NORMAL));
        DE_ASSERT(!(owner == Owner::PATCH && interpolation == Interpolation::FLAT));
        DE_ASSERT(
            !(dataType == DataType::FLOAT && bitWidth == BitWidth::B64 && interpolation == Interpolation::NORMAL));
        DE_ASSERT(index < kVarsPerType);
    }

    // This constructor needs to be defined for the code to compile, but it should never be actually called.
    // To make sure it's not used, the index is defined to be very large, which should trigger the assertion in getName() below.
    IfaceVar()
        : owner(Owner::VERTEX)
        , dataType(DataType::FLOAT)
        , bitWidth(BitWidth::B32)
        , dataDim(DataDim::VEC4)
        , interpolation(Interpolation::NORMAL)
        , index(std::numeric_limits<uint32_t>::max())
    {
    }

    Owner owner;
    DataType dataType;
    BitWidth bitWidth;
    DataDim dataDim;
    Interpolation interpolation;
    uint32_t index; // In case there are several variables matching this type.

    // The variable name will be unique and depend on its type.
    std::string getName() const
    {
        DE_ASSERT(index < kVarsPerType);

        std::ostringstream name;
        name << ((owner == Owner::VERTEX) ? "vert" : "patch") << "_" << ((dataType == DataType::INTEGER) ? "i" : "f")
             << static_cast<int>(bitWidth) << "d" << static_cast<int>(dataDim) << "_"
             << ((interpolation == Interpolation::NORMAL) ? "inter" : "flat") << "_" << index;
        return name.str();
    }

    // Get location size according to the type.
    uint32_t getLocationSize() const
    {
        return ((bitWidth == BitWidth::B64 && dataDim >= DataDim::VEC3) ? 2u : 1u);
    }

    // Get the variable type in GLSL.
    std::string getGLSLType() const
    {
        const auto widthStr     = std::to_string(static_cast<int>(bitWidth));
        const auto dimStr       = std::to_string(static_cast<int>(dataDim));
        const auto shortTypeStr = ((dataType == DataType::INTEGER) ? "i" : "f");
        const auto typeStr      = ((dataType == DataType::INTEGER) ? "int" : "float");

        if (dataDim == DataDim::SCALAR)
            return typeStr + widthStr + "_t";                         // e.g. int32_t or float16_t
        return shortTypeStr + widthStr + std::string("vec") + dimStr; // e.g. i16vec2 or f64vec4.
    }

    // Get a simple declaration of type and name. This can be reused for several things.
    std::string getTypeAndName() const
    {
        return getGLSLType() + " " + getName();
    }

    std::string getTypeAndNameDecl(bool arrayDecl = false) const
    {
        std::ostringstream decl;
        decl << "    " << getTypeAndName();
        if (arrayDecl)
            decl << "[" << ((owner == Owner::PATCH) ? IfaceVar::kNumPatches : IfaceVar::kNumVertices) << "]";
        decl << ";\n";
        return decl.str();
    }

    // Variable declaration statement given its location and direction.
    std::string getLocationDecl(size_t location, Direction direction) const
    {
        std::ostringstream decl;
        decl << "layout (location=" << location << ") " << ((direction == Direction::IN) ? "in" : "out") << " "
             << ((owner == Owner::PATCH) ? "patch " : "") << ((interpolation == Interpolation::FLAT) ? "flat " : "")
             << getTypeAndName() << ((owner == Owner::VERTEX) ? "[]" : "") << ";\n";
        return decl.str();
    }

    // Get the name of the source data for this variable.
    // Tests will use a storage buffer for the per-vertex data and a uniform
    // buffer for the per-patch data. The names in those will match.
    std::string getDataSourceName() const
    {
        // per-patch data or per-vertex data buffers.
        return ((owner == Owner::PATCH) ? "ppd" : "pvd") + ("." + getName());
    }

    // Get the boolean check variable name (see below).
    std::string getCheckName() const
    {
        return "good_" + getName();
    }

    // Get the check statement that would be used in the fragment shader.
    std::string getCheckStatement(bool tcsReads = false) const
    {
        std::ostringstream check;
        const auto sourceName = getDataSourceName();
        const auto glslType   = getGLSLType();
        std::string name      = getName();
        std::string tempName  = tcsReads ? name : ("temp_" + name);

        if (owner == Owner::VERTEX)
        {
            if (tcsReads)
            {
                tempName += "[gl_InvocationID]";
            }
            else
            {
                // Temp variable declaration
                check << glslType << " " << tempName << ";\n";

                // Quad interpolation on per vertex variable
                const std::string interp =
                    "INTERP_QUAD_VAR(" + glslType + ", var_" + name + " ," + name + "[i], " + tempName + ");";
                check << interp << "\n";
            }
        }

        check << "    bool " << getCheckName() << " = ";
        if (owner == Owner::VERTEX)
        {
            // There will be 4 values in the buffers.
            std::ostringstream maxElem;
            std::ostringstream minElem;

            maxElem << glslType << "(max(max(max(" << sourceName << "[0], " << sourceName << "[1]), " << sourceName
                    << "[2]), " << sourceName << "[3]))";
            minElem << glslType << "(min(min(min(" << sourceName << "[0], " << sourceName << "[1]), " << sourceName
                    << "[2]), " << sourceName << "[3]))";

            if (dataDim == DataDim::SCALAR)
            {
                check << "(" << tempName << " <= " << maxElem.str() << ") && (" << tempName << " >= " << minElem.str()
                      << ")";
            }
            else
            {
                check << "all(lessThanEqual(" << tempName << ", " << maxElem.str() << ")) && "
                      << "all(greaterThanEqual(" << tempName << ", " << minElem.str() << "))";
            }
        }
        else if (owner == Owner::PATCH)
        {
            check << "((gl_PrimitiveID == 0 || gl_PrimitiveID == 1) && ("
                  << "(gl_PrimitiveID == 0 && " << name << " == " << sourceName << "[0]) || "
                  << "(gl_PrimitiveID == 1 && " << name << " == " << sourceName << "[1])))";
        }
        check << ";\n";

        return check.str();
    }

    // Get an assignment statement for an out variable.
    std::string getAssignmentStatement(const std::string &leftPrefix, const std::string &rightPrefix) const
    {
        const auto name    = getName();
        const auto typeStr = getGLSLType();
        std::ostringstream stmt;

        stmt << "    " << leftPrefix << (leftPrefix.empty() ? "" : ".") << name
             << ((owner == Owner::VERTEX) ? "[gl_InvocationID]" : "") << " = " << typeStr << "(" << rightPrefix
             << (rightPrefix.empty() ? "" : ".") << name << "["
             << ((owner == Owner::VERTEX) ? "gl_InvocationID" : "gl_PrimitiveID") << "]);\n";
        return stmt.str();
    }

    // Get the corresponding array size based on the owner (vertex or patch)
    uint32_t getArraySize() const
    {
        return ((owner == Owner::PATCH) ? IfaceVar::kNumPatches : IfaceVar::kNumVertices);
    }

    // Note data types in the input buffers are always plain floats or ints. They will be converted to the appropriate type when
    // copying them in or out of output variables.
    std::string getGLSLBindingType() const
    {
        const auto dimStr       = std::to_string(static_cast<int>(dataDim));
        const auto shortTypeStr = ((dataType == DataType::INTEGER) ? "i" : "");
        const auto typeStr      = ((dataType == DataType::INTEGER) ? "int" : "float");

        if (dataDim == DataDim::SCALAR)
            return typeStr;                                // e.g. int or float
        return shortTypeStr + std::string("vec") + dimStr; // e.g. IVec2 or Vec4
    }

    std::string getBinding(Owner ownerType) const
    {
        std::ostringstream binding;
        if (owner == ownerType)
        {
            // Data type and variable name
            binding << getGLSLBindingType() + " " + getName();

            // Array declaration
            binding << "[" + de::toString((owner == Owner::VERTEX) ? IfaceVar::kNumVertices : IfaceVar::kNumPatches) +
                           "];\n";
        }
        return binding.str();
    }

    uint32_t getBindingCompSize() const
    {
        const uint32_t bitsPerByte = 8u;
        return uint32_t(((bitWidth == BitWidth::B16) || (bitWidth == BitWidth::B64)) ? BitWidth::B32 : bitWidth) /
               bitsPerByte;
    }

    void getBindingSize(Owner ownerType, uint32_t &size) const
    {
        if (owner == ownerType)
        {
            const uint32_t arrSize = getArraySize();
            const uint32_t dim     = uint32_t(dataDim);
            const uint32_t elemAlignment =
                uint32_t(dataDim == DataDim::VEC3 ? DataDim::VEC4 : dataDim) * getBindingCompSize();
            const uint32_t arrayStride = elemAlignment;
            const uint32_t compSize    = getBindingCompSize();

            for (uint32_t idx = 0; idx < arrSize; idx++)
            {
                // Align the next array element
                while ((size % arrayStride) != 0)
                    size++;

                for (uint32_t dimIdx = 1; dimIdx <= dim; dimIdx++)
                    size += compSize;
            }

            // Align the next block member
            while ((size % elemAlignment) != 0)
                size++;
        }
    }

    void initBinding(Owner ownerType, uint8_t *pData, uint32_t &offset, const uint32_t startValue) const
    {
        if (owner == ownerType)
        {
            const uint32_t arrSize = getArraySize();
            const uint32_t dim     = uint32_t(dataDim);
            const uint32_t elemAlignment =
                uint32_t(dataDim == DataDim::VEC3 ? DataDim::VEC4 : dataDim) * getBindingCompSize();
            const uint32_t arrayStride = elemAlignment;
            const uint32_t compSize    = getBindingCompSize();

            uint32_t ivalue              = startValue;
            const float floatSuffixes[4] = {0.25, 0.50, 0.875, 0.0};

            for (uint32_t idx = 0; idx < arrSize; idx++)
            {
                // Align the next array element
                while ((offset % arrayStride) != 0)
                    offset++;

                for (uint32_t dimIdx = 1; dimIdx <= dim; dimIdx++)
                {
                    float fvalue = float(ivalue) + floatSuffixes[dimIdx - 1];
                    float f16value =
                        float(ivalue); // TES is changing float values when assigning in shader so using integer
                    uint8_t *dest = pData + offset;
                    if (dataType == DataType::INTEGER)
                        deMemcpy(dest, &ivalue, compSize);
                    else if ((dataType == DataType::FLOAT) && (bitWidth == BitWidth::B16))
                        deMemcpy(dest, &f16value, compSize);
                    else
                        deMemcpy(dest, &fvalue, compSize);
                    offset += compSize;
                    ivalue++;
                }
                ivalue = startValue;
            }

            // Align the next block member
            while ((offset % elemAlignment) != 0)
                offset++;
        }
    }
};

using IfaceVarVec    = std::vector<IfaceVar>;
using IfaceVarVecPtr = std::unique_ptr<IfaceVarVec>;

struct MaxIOTestParams
{
    MaxIOTestParams(const bool tcsReads_, const bool tesReads_, const TessVarType tessVarType_, const bool useInt64_,
                    const bool useFloat64_, const bool useInt16_, const bool useFloat16_, IfaceVarVecPtr vars_)
        : tcsReads(tcsReads_)
        , tesReads(tesReads_)
        , tessVarType(tessVarType_)
        , useInt64(useInt64_)
        , useFloat64(useFloat64_)
        , useInt16(useInt16_)
        , useFloat16(useFloat16_)
        , ifaceVars(std::move(vars_))
    {
    }

    const bool tcsReads;
    const bool tesReads;
    const TessVarType tessVarType;

    // These need to match the list of interface variables.
    const bool useInt64;
    const bool useFloat64;
    const bool useInt16;
    const bool useFloat16;

    IfaceVarVecPtr ifaceVars;
};

using ParamsPtr = std::unique_ptr<MaxIOTestParams>;

uint32_t getMaxLocations(const TessDeviceLimits &devLimits, const MaxIOTestParams *testParams, const Owner &owner)
{
    const uint32_t usedPerVertexBuiltinLocations = 2; // Position, TessCoord
    const uint32_t usedPerPatchBuiltinLocations  = 3; // TessOuter, TessInner and PrimitiveID
    const uint32_t usedOutColorLocations         = testParams->tcsReads ? 1u : 0u;

    uint32_t perVertexLocations =
        (((testParams->tesReads) ? devLimits.maxTessellationEvaluationPerVertexInputComponents :
                                   devLimits.maxTessellationControlPerVertexOutputComponents) /
         kSlotSize) -
        (usedPerVertexBuiltinLocations + usedOutColorLocations);

    uint32_t perPatchLocations =
        (devLimits.maxTessellationControlPerPatchOutputComponents / kSlotSize) - usedPerPatchBuiltinLocations;

    return (owner == Owner::VERTEX) ? perVertexLocations : perPatchLocations;
}

// Cut the vector short to the usable number of locations.
// Usable locations depend on max TCS output and max TES read input.
void getUsableLocations(const TessDeviceLimits &limits, const MaxIOTestParams *testParams, IfaceVarVec &varVec)
{
    int32_t availablePerVertLocations  = int32_t(getMaxLocations(limits, testParams, Owner::VERTEX));
    int32_t availablePerPatchLocations = int32_t(getMaxLocations(limits, testParams, Owner::PATCH));

    uint32_t vecEnd = 0;

    for (uint32_t i = 0; i < varVec.size(); ++i)
    {
        const auto varSize = varVec[i].getLocationSize();

        if (varVec[i].owner == Owner::VERTEX)
        {
            if ((availablePerVertLocations <= 0) || int32_t(availablePerVertLocations - varSize) < 0)
                break;
            availablePerVertLocations -= varSize;
        }
        else
        {
            if ((availablePerPatchLocations <= 0) || int32_t(availablePerPatchLocations - varSize) < 0)
                break;
            availablePerPatchLocations -= varSize;
        }

        vecEnd = i;
    }
    varVec.resize(vecEnd);
}
class MaxIOTest : public TestCase
{
public:
    MaxIOTest(tcu::TestContext &testCtx, const std::string &name, ParamsPtr params);
    virtual ~MaxIOTest(void)
    {
    }
    void initPrograms(vk::SourceCollections &programCollection) const;
    void checkSupport(Context &context) const;
    TestInstance *createInstance(Context &context) const;

private:
    ParamsPtr m_testParams;
    ParamsPtr m_copyParams;
};

MaxIOTest::MaxIOTest(tcu::TestContext &testCtx, const std::string &name, ParamsPtr params)
    : TestCase(testCtx, name)
    , m_testParams(std::move(params))

{
    const TessDeviceLimits defaultDevLimits = {kMaxTessellationControlPerVertexOutputComponents,
                                               kMaxTessellationControlPerPatchOutputComponents,
                                               kMaxTessellationEvaluationPerVertexInputComponents};

    IfaceVarVecPtr varsPtr(new IfaceVarVec(*m_testParams->ifaceVars));

    getUsableLocations(defaultDevLimits, m_testParams.get(), *varsPtr);

    // Make a copy of the test parameters and replace vector
    m_copyParams = (ParamsPtr) new MaxIOTestParams(
        /*tcsReads*/ m_testParams->tcsReads,
        /*tesReads*/ m_testParams->tesReads,
        /*tessVarType*/ m_testParams->tessVarType,
        /*useInt64*/ m_testParams->useInt64,
        /*useFloat64*/ m_testParams->useFloat64,
        /*useInt16*/ m_testParams->useInt16,
        /*useFloat16*/ m_testParams->useFloat16,
        /*vars*/ std::move(varsPtr));
}

void commonShaders(const std::string &progNamePrefix, vk::SourceCollections &programCollection)
{
    // Vertex shader
    {
        std::ostringstream vert;
        vert << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
             << "vec4 positions[" << IfaceVar::kNumPatches * IfaceVar::kNumVertices << "] = vec4[](\n"
             << "    vec4(-1.0, -1.0, 0.0, 1.0),\n"
             << "    vec4( 1.0, -1.0, 0.0, 1.0),\n"
             << "    vec4(-1.0,  1.0, 0.0, 1.0),\n"
             << "    vec4( 1.0,  1.0, 0.0, 1.0),\n"

             << "    vec4(-0.5, -0.5, 0.0, 1.0),\n"
             << "    vec4( 0.5, -0.5, 0.0, 1.0),\n"
             << "    vec4(-0.5,  0.5, 0.0, 1.0),\n"
             << "    vec4( 0.5,  0.5, 0.0, 1.0)\n"

             << ");\n"
             << "out gl_PerVertex {\n"
             << "    vec4 gl_Position;\n"
             << "};\n"
             << "void main (void)\n"
             << "{\n"
             << "    gl_Position = positions[gl_VertexIndex];\n"
             << "}\n";

        programCollection.glslSources.add(progNamePrefix + "vert") << glu::VertexSource(vert.str());
    }

    // Fragment shader
    {
        std::ostringstream frag;
        frag << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
             << "layout (location=0) in vec4 inColor;\n"
             << "layout (location=0) out vec4 outColor;\n"
             << "void main()\n"
             << "{\n"
             << "    outColor = inColor;\n"
             << "}\n";
        programCollection.glslSources.add(progNamePrefix + "frag") << glu::FragmentSource(frag.str());
    }
}

void makeShaders(bool defaultProgs, vk::SourceCollections &programCollection, const MaxIOTestParams *testParams)
{
    const std::string progNamePrefix = defaultProgs ? kProgramNamePrefixMock : "";

    // Generate bindings based on variables used
    const auto &varVec = *(testParams->ifaceVars);

    // Bindings needs to match the PerVertexData and perPatchData structures.
    std::ostringstream bindings;
    uint32_t bindingIdx = 0;
    {
        if (testParams->tessVarType == TessVarType::TESS_VAR_VERT)
        {
            {
                bindings << "layout(set=0, binding=" << bindingIdx << ", std430) readonly buffer PerVertexBlock {\n";

                for (const auto &var : varVec)
                {
                    bindings << "    " << var.getBinding(Owner::VERTEX);
                }

                bindings << " } pvd;\n"
                         << "\n";

                bindingIdx++;
            }
        }

        if (testParams->tessVarType == TessVarType::TESS_VAR_PATCH)
        {
            bindings << "layout(set=0, binding=" << bindingIdx << ", std430) readonly buffer PerPatchBlock {\n";
            for (const auto &var : varVec)
            {
                bindings << "    " << var.getBinding(Owner::PATCH);
            }
            bindings << " } ppd;\n"
                     << "\n";

            bindingIdx++;
        }
    }

    const auto bindingsDecl = bindings.str();

    // Tessellation Evaluation shader
    {
        std::ostringstream tese;
        tese << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
             << "#extension GL_EXT_shader_explicit_arithmetic_types : enable\n"
             << "layout(quads, equal_spacing) in;\n"
             << "\n"
             << bindingsDecl;

        if (testParams->tcsReads)
        {
            tese << "layout (location=0) in vec4 inColor[];\n"
                 << "\n";
        }

        // Declare interface variables as Input in the tess evaluation shader.
        if (testParams->tesReads)
        {
            uint32_t usedLocations = testParams->tcsReads ? 1u : 0u;
            for (const auto &var : varVec)
            {
                tese << var.getLocationDecl(usedLocations, Direction::IN);
                usedLocations += var.getLocationSize();
            }
        }

        tese << "\n"
             << "layout (location=0) out vec4 outColor;\n"
             << "\n";

        tese << "in gl_PerVertex {\n"
             << "    vec4  gl_Position;\n"
             << "} gl_in[];\n"
             << "\n"
             << "out gl_PerVertex {\n"
             << "    vec4 gl_Position;\n"
             << "};\n";

        tese << "#define INTERP_QUAD_VAR(TYPE, VAR, INi, OUT) do { \\\n"
             << "    TYPE VAR[4]; \\\n"
             << "    for (int i = 0; i < 4; i++) VAR[i] = INi; \\\n"
             << "    { \\\n"
             << "        TYPE temp1 = TYPE(VAR[0] * TYPE(1 - gl_TessCoord[0]) + VAR[1] * TYPE(gl_TessCoord[0])); \\\n"
             << "        TYPE temp2 = TYPE(VAR[2] * TYPE(1 - gl_TessCoord[0]) + VAR[3] * TYPE(gl_TessCoord[0])); \\\n"
             << "        OUT = TYPE(temp1 * TYPE(1 - gl_TessCoord[1]) + temp2 * TYPE(gl_TessCoord[1])); \\\n"
             << "    } \\\n"
             << "} while(false)\n\n";

        tese << "void main ()\n"
             << "{\n";

        if (testParams->tesReads)
        {
            // Emit checks for each variable value in the tess evaluation shader.
            std::ostringstream allConditions;

            for (size_t i = 0; i < varVec.size(); ++i)
            {
                tese << varVec[i].getCheckStatement();
                allConditions << ((i == 0) ? "" : " && ") << varVec[i].getCheckName();
            }

            tese << "    if (" << allConditions.str() << ") {\n";

            if (testParams->tcsReads)
            {
                tese << "        INTERP_QUAD_VAR(vec4, var_color, inColor[i], outColor);\n";
            }
            else
            {
                tese << "        outColor = (gl_PrimitiveID == 0) ? vec4(0.0, 0.0, 1.0, 1.0) : vec4(1.0, 1.0, 0.0, "
                        "1.0);\n";
            }

            tese << "    } else {\n"
                 << "        outColor = vec4(0.0, 0.0, 0.0, 1.0);\n"
                 << "    }\n";
        }
        else if (testParams->tcsReads)
        {
            tese << "    INTERP_QUAD_VAR(vec4, var_color, inColor[i], outColor);\n";
        }
        else
        {
            tese << "    outColor = (gl_PrimitiveID == 0) ? vec4(0.0, 0.0, 1.0, 1.0) : vec4(1.0, 1.0, 0.0, 1.0);\n";
        }

        // gl_position
        tese << "    INTERP_QUAD_VAR(vec4, var_gl_pos, gl_in[i].gl_Position, gl_Position);\n";

        tese << "}\n";

        programCollection.glslSources.add(progNamePrefix + "tese") << glu::TessellationEvaluationSource(tese.str());
    }

    // Tessellation Control shader
    {
        const auto tescPvdPrefix = "pvd";
        const auto tescPpdPrefix = "ppd";

        std::ostringstream tesc;

        tesc << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
             << "#extension GL_EXT_tessellation_shader : require\n"
             << "#extension GL_EXT_shader_explicit_arithmetic_types : enable\n"
             << "\n"
             << "layout (vertices = " << IfaceVar::kNumVertices << ") out;\n"
             << "\n";

        if (testParams->tcsReads)
        {
            tesc << "\n"
                 << "layout (location=0) out vec4 outColor[];\n"
                 << "\n";
        }

        // Declare interface variables as Output variables.
        {
            uint32_t usedLocations = testParams->tcsReads ? 1u : 0u;
            for (const auto &var : varVec)
            {
                tesc << var.getLocationDecl(usedLocations, Direction::OUT);
                usedLocations += var.getLocationSize();
            }
        }

        tesc << "\n";
        tesc << bindingsDecl;

        tesc << "in gl_PerVertex {\n"
             << "    vec4  gl_Position;\n"
             << "} gl_in[];\n"
             << "\n"
             << "out gl_PerVertex {\n"
             << "    vec4  gl_Position;\n"
             << "} gl_out[];\n"
             << "\n";

        tesc << "void main ()\n"
             << "{\n";
        // Copy data to output variables, either from the bindings.
        for (size_t i = 0; i < varVec.size(); ++i)
        {
            const auto prefix = ((varVec[i].owner == Owner::VERTEX) ? tescPvdPrefix : tescPpdPrefix);
            tesc << varVec[i].getAssignmentStatement("", prefix);
        }

        if (testParams->tcsReads)
        {
            // Emit checks for each variable value in the tess evaluation shader.
            std::ostringstream allConditions;

            for (size_t i = 0; i < varVec.size(); ++i)
            {
                tesc << varVec[i].getCheckStatement(testParams->tcsReads);
                allConditions << ((i == 0) ? "" : " && ") << varVec[i].getCheckName();
            }

            // Emit final check.
            tesc << "    if (" << allConditions.str() << ") {\n"
                 << "        outColor[gl_InvocationID] = (gl_PrimitiveID == 0) ? vec4(0.0, 0.0, 1.0, 1.0) : vec4(1.0, "
                    "1.0, 0.0, 1.0);\n"
                 << "    } else {\n"
                 << "        outColor[gl_InvocationID] = vec4(0.0, 0.0, 0.0, 1.0);\n"
                 << "    }\n";
        }

        tesc << "   gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
             << "   gl_TessLevelOuter = float[4](1.0, 1.0, 1.0, 1.0);\n"
             << "   gl_TessLevelInner = float[2](1.0, 1.0);\n";

        tesc << "\n"
             << "}\n";

        programCollection.glslSources.add(progNamePrefix + "tesc") << glu::TessellationControlSource(tesc.str());
    }

    commonShaders(progNamePrefix, programCollection);
}

void MaxIOTest::initPrograms(vk::SourceCollections &programCollection) const
{
    makeShaders(true, programCollection, m_copyParams.get());
}

void MaxIOTest::checkSupport(Context &context) const
{
    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_TESSELLATION_SHADER);

    const auto params = dynamic_cast<MaxIOTestParams *>(m_testParams.get());

    if (params->useFloat64)
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SHADER_FLOAT64);

    if (params->useInt64)
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SHADER_INT64);

    if (params->useInt16)
        context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_SHADER_INT16);

    if (params->useFloat16)
    {
        const auto &featuresShader = context.getShaderFloat16Int8Features();
        if (!featuresShader.shaderFloat16)
            TCU_THROW(NotSupportedError, "shaderFloat16 feature not supported");
    }

    if (params->useInt16 || params->useFloat16)
    {
        const auto &featuresShader = context.get16BitStorageFeatures();
        if (!featuresShader.storageInputOutput16)
            TCU_THROW(NotSupportedError, "storageInputOutput16 feature not supported");
    }
}

// Rebuild binary programs from updated GLSL source collection
//  New source collection is sent as parameter.
//  Source collection code may not be exactly known until TestInstance
//  creation time.
//  In case of tests in this file, device limits are only known until
//  TestInstance creation time and specialization constants can not
//  be used due to variable locations in layout depending on
//  device limits.
//  The following logic was taken from vktTestPackage.
//  This logic can be moved to TestPackage if VK CTS framework
//  wants to allow generation of binary programs at TestInstance
//  iterate() time in use-cases where device limits need to be
//  used in programs.
// The logic dictates:
//   TestCase::initPrograms have at least one program.
//   Names of programs created in TestCase::initPrograms have
//   "mock_" prefix.
//   New programs built at iterate() should exactly replace
//   programs created in TestCase::initPrograms.
//   Names of new programs should be same as program names
//   in TestCase::initPrograms excluding the "mock_" prefix

void reGeneratePrograms(Context &context, vk::SourceCollections &sourceProgs)
{
    tcu::TestContext &testCtx  = context.getTestContext();
    tcu::TestLog &log          = testCtx.getLog();
    const std::string prefix   = kProgramNamePrefixMock;
    const std::string casePath = ""; // unused in build programs

    const tcu::CommandLine &commandLine = context.getTestContext().getCommandLine();
    const bool doShaderLog              = commandLine.isLogDecompiledSpirvEnabled() && log.isShaderLoggingEnabled();

    de::SharedPtr<vk::ResourceInterface> resourceInterface = context.getResourceInterface();
    vk::BinaryRegistryReader prebuiltBinRegistry(testCtx.getArchive(), "vulkan/prebuilt");
    vk::BinaryCollection &progCollection = context.getBinaryCollection();

    // If there are no new GLSL source collections
    // then continue to use the mock shaders
    if (sourceProgs.glslSources.empty())
    {
        TCU_THROW(InternalError, "New programs are missing");
    }

    if (progCollection.empty())
    {
        TCU_THROW(
            InternalError,
            "Default programs are missing. Either initPrograms was not called or called without creating any default "
            "programs. This is not allowed as it will cause vk-build-programs to generate empty programs");
    }

    // All default programs should start with 'mock_' prefix
    // if their sources are to be updated
    std::vector<std::string> defaultProgNames;
    for (vk::BinaryCollection::Iterator progIt = progCollection.begin(); progIt != progCollection.end(); ++progIt)
    {
        const std::string &progName = progIt.getName();
        if (progName.rfind(prefix, 0))
        {
            const std::string msg = "Default program: " + progName + " does not have prefix: mock_";
            TCU_THROW(InternalError, msg);
        }
        defaultProgNames.push_back(progName);
    }

    // New programs can only be added against the default programs only
    std::vector<std::string> glslProgNames;
    for (vk::GlslSourceCollection::Iterator progIter = sourceProgs.glslSources.begin();
         progIter != sourceProgs.glslSources.end(); ++progIter)
    {
        const std::string &glslProgName = prefix + progIter.getName();
        if (std::find(defaultProgNames.begin(), defaultProgNames.end(), glslProgName) == defaultProgNames.end())
        {
            const std::string msg =
                "New program: " + progIter.getName() + " does not have corresponding default program";
            TCU_THROW(InternalError, msg);
        }
        glslProgNames.push_back(progIter.getName());
    }

    if (glslProgNames.size() != defaultProgNames.size())
    {
        const std::string msg =
            "Number of new programs: " + std::to_string(glslProgNames.size()) +
            " does not match with the number of default programs: " + std::to_string(defaultProgNames.size());
        TCU_THROW(InternalError, msg);
    }

    // Discard the default programs entirely
    progCollection.clear();

    // And add the new programs
    for (vk::GlslSourceCollection::Iterator progIter = sourceProgs.glslSources.begin();
         progIter != sourceProgs.glslSources.end(); ++progIter)
    {
        {
            bool spirvVersionOk           = false;
            vk::SpirvVersion spirvVersion = progIter.getProgram().buildOptions.targetVersion;
            if (spirvVersion <= vk::getMaxSpirvVersionForVulkan(context.getUsedApiVersion()))
                spirvVersionOk = true;

            if (spirvVersion <= vk::SPIRV_VERSION_1_4)
                spirvVersionOk = context.isDeviceFunctionalitySupported("VK_KHR_spirv_1_4");

            if (!spirvVersionOk)
                TCU_THROW(NotSupportedError, "Shader requires SPIR-V higher than available");
        }

        const vk::ProgramBinary *const binProg =
            resourceInterface->buildProgram<glu::ShaderProgramInfo, vk::GlslSourceCollection::Iterator>(
                casePath, progIter, prebuiltBinRegistry, &progCollection);

        if (doShaderLog)
        {
            try
            {
                std::ostringstream disasm;

                vk::disassembleProgram(*binProg, &disasm);

                log << vk::SpirVAsmSource(disasm.str());
            }
            catch (const tcu::NotSupportedError &err)
            {
                log << err;
            }
        }
    }
}

class MaxIOTestInstance : public TestInstance
{
public:
    MaxIOTestInstance(Context &context, const MaxIOTestParams *params);
    tcu::TestStatus iterate(void) override;

private:
    const VkFormat m_colorFormat;
    const VkRect2D m_renderArea;
    const MaxIOTestParams *m_testParams;

    uint32_t m_maxTessellationControlPerVertexOutputComponents;
    uint32_t m_maxTessellationControlPerPatchOutputComponents;
    uint32_t m_maxTessellationControlTotalOutputComponents;
    uint32_t m_maxTessellationEvaluationPerVertexInputComponents;
    uint32_t m_maxTessellationEvaluationPerVertexOutputComponents;

    std::unique_ptr<tcu::TextureLevel> m_referenceLevel;

    void initLimits();
    void initShaders();
    uint32_t findDataSize(Owner ownerType);
    void initData(std::vector<uint8_t> &data, Owner ownerType);
};

void MaxIOTestInstance::initLimits()
{
    const VkPhysicalDeviceLimits &limits = m_context.getDeviceProperties().limits;

    m_maxTessellationControlPerVertexOutputComponents = limits.maxTessellationControlPerVertexOutputComponents;
    m_maxTessellationControlPerPatchOutputComponents  = limits.maxTessellationControlPerPatchOutputComponents;
    m_maxTessellationControlTotalOutputComponents     = limits.maxTessellationControlTotalOutputComponents;

    m_maxTessellationEvaluationPerVertexInputComponents  = limits.maxTessellationEvaluationInputComponents;
    m_maxTessellationEvaluationPerVertexOutputComponents = limits.maxTessellationEvaluationOutputComponents;
}

void MaxIOTestInstance::initShaders()
{
    const uint32_t usedVulkanVersion            = m_context.getUsedApiVersion();
    const vk::SpirvVersion baselineSpirvVersion = vk::getBaselineSpirvVersion(usedVulkanVersion);
    vk::ShaderBuildOptions defaultGlslBuildOptions(usedVulkanVersion, baselineSpirvVersion, 0u);
    vk::ShaderBuildOptions defaultHlslBuildOptions(usedVulkanVersion, baselineSpirvVersion, 0u);
    vk::SpirVAsmBuildOptions defaultSpirvAsmBuildOptions(usedVulkanVersion, baselineSpirvVersion);
    vk::SourceCollections sourceProgs(usedVulkanVersion, defaultGlslBuildOptions, defaultHlslBuildOptions,
                                      defaultSpirvAsmBuildOptions);
    makeShaders(false, sourceProgs, m_testParams);
    reGeneratePrograms(m_context, sourceProgs);
}

MaxIOTestInstance::MaxIOTestInstance(Context &context, const MaxIOTestParams *params)
    : TestInstance(context)
    , m_colorFormat(VK_FORMAT_R8G8B8A8_UNORM)
    , m_renderArea(makeRect2D(MAXIO_RENDER_SIZE_WIDTH, MAXIO_RENDER_SIZE_HEIGHT))
    , m_testParams(params)
{

    initLimits();

    {
        const TessDeviceLimits realDevLimits = {m_maxTessellationControlPerVertexOutputComponents,
                                                m_maxTessellationControlPerPatchOutputComponents,
                                                m_maxTessellationEvaluationPerVertexInputComponents};
        getUsableLocations(realDevLimits, m_testParams, *m_testParams->ifaceVars.get());
    }

    initShaders();
}

void commonGenerateReferenceLevel(const VkFormat colorFormat, const tcu::Vec4 *expectedColor,
                                  std::unique_ptr<tcu::TextureLevel> &referenceLevel)
{
    const auto format    = colorFormat;
    const auto tcuFormat = mapVkFormat(format);

    const auto iWidthOuter  = static_cast<int>(MAXIO_RENDER_SIZE_WIDTH);
    const auto iHeightOuter = static_cast<int>(MAXIO_RENDER_SIZE_HEIGHT);

    const auto iWidthInner  = static_cast<int>(MAXIO_RENDER_SIZE_WIDTH / 2);
    const auto iHeightInner = static_cast<int>(MAXIO_RENDER_SIZE_HEIGHT / 2);
    const auto distX        = (iWidthOuter - iWidthInner) / 2;
    const auto distY        = (iHeightOuter - iHeightInner) / 2;
    const auto topLeft      = tcu::UVec2(distX, distY);
    const auto widthInner   = topLeft.x() + iWidthInner;
    const auto heightInner  = topLeft.y() + iHeightInner;

    referenceLevel.reset(new tcu::TextureLevel(tcuFormat, iWidthOuter, iHeightOuter));
    const auto access = referenceLevel->getAccess();

    tcu::clear(access, expectedColor[0]);

    for (uint32_t x = topLeft.x(); x < widthInner; x++)
    {
        for (uint32_t y = topLeft.y(); y < heightInner; y++)
        {
            access.setPixel(expectedColor[1], x, y);
        }
    }
}

bool commonVerifyResult(tcu::TestLog &log, const VkFormat colorFormat,
                        std::unique_ptr<tcu::TextureLevel> &referenceLevelPtr,
                        const tcu::ConstPixelBufferAccess &resultAccess)
{
    const auto &referenceLevel = *referenceLevelPtr.get();
    const auto referenceAccess = referenceLevel.getAccess();

    const auto refWidth  = referenceAccess.getWidth();
    const auto refHeight = referenceAccess.getHeight();
    const auto refDepth  = referenceAccess.getDepth();

    const auto resWidth  = resultAccess.getWidth();
    const auto resHeight = resultAccess.getHeight();
    const auto resDepth  = resultAccess.getDepth();

    DE_ASSERT(resWidth == refWidth || resHeight == refHeight || resDepth == refDepth);

    // For release builds.
    DE_UNREF(refWidth);
    DE_UNREF(refHeight);
    DE_UNREF(refDepth);
    DE_UNREF(resWidth);
    DE_UNREF(resHeight);
    DE_UNREF(resDepth);

    const auto outputFormat   = colorFormat;
    const auto expectedFormat = mapVkFormat(outputFormat);
    const auto resFormat      = resultAccess.getFormat();
    const auto refFormat      = referenceAccess.getFormat();

    DE_ASSERT(resFormat == expectedFormat && refFormat == expectedFormat);

    // For release builds
    DE_UNREF(expectedFormat);
    DE_UNREF(resFormat);
    DE_UNREF(refFormat);

    const auto threshold = 0.005f; // 1/256 < 0.005 < 2/256
    const tcu::Vec4 thresholdVec(threshold, threshold, threshold, threshold);

    return tcu::floatThresholdCompare(log, "Result", "", referenceAccess, resultAccess, thresholdVec,
                                      tcu::COMPARE_LOG_ON_ERROR);
}

uint32_t MaxIOTestInstance::findDataSize(Owner ownerType)
{
    uint32_t size     = 0;
    const auto varVec = *m_testParams->ifaceVars;

    for (const auto var : varVec)
    {
        var.getBindingSize(ownerType, size);
    }
    return size;
}

void MaxIOTestInstance::initData(std::vector<uint8_t> &data, Owner ownerType)
{
    const auto varVec = *m_testParams->ifaceVars;
    uint32_t offset   = 0u;
    de::Random rnd(1636723398u);

    for (const auto var : varVec)
    {
        const uint32_t startValue = rnd.getInt(1000, 1231);
        var.initBinding(ownerType, &data[0], offset, startValue);
    }
}

tcu::TestStatus MaxIOTestInstance::iterate(void)
{
    const auto &vkd       = m_context.getDeviceInterface();
    const auto device     = m_context.getDevice();
    auto &alloc           = m_context.getDefaultAllocator();
    const auto queueIndex = m_context.getUniversalQueueFamilyIndex();
    const auto queue      = m_context.getUniversalQueue();

    const auto imageFormat = m_colorFormat;
    const auto tcuFormat   = mapVkFormat(imageFormat);
    const auto imageExtent = makeExtent3D(MAXIO_RENDER_SIZE_WIDTH, MAXIO_RENDER_SIZE_WIDTH, 1u);
    const auto imageUsage  = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

    const auto &binaries = m_context.getBinaryCollection();

    const auto bufStages = (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);

    const VkImageCreateInfo colorBufferInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        0u,                                  // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
        imageFormat,                         // VkFormat format;
        imageExtent,                         // VkExtent3D extent;
        1u,                                  // uint32_t mipLevels;
        1u,                                  // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        imageUsage,                          // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        0u,                                  // uint32_t queueFamilyIndexCount;
        nullptr,                             // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout initialLayout;
    };

    // Create color image and view.
    ImageWithMemory colorImage(vkd, device, alloc, colorBufferInfo, MemoryRequirement::Any);
    const auto colorSRR  = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const auto colorSRL  = makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
    const auto colorView = makeImageView(vkd, device, colorImage.get(), VK_IMAGE_VIEW_TYPE_2D, imageFormat, colorSRR);

    // Create a memory buffer for verification.
    const auto verificationBufferSize =
        static_cast<VkDeviceSize>(imageExtent.width * imageExtent.height * tcu::getPixelSize(tcuFormat));
    const auto verificationBufferUsage = (VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const auto verificationBufferInfo  = makeBufferCreateInfo(verificationBufferSize, verificationBufferUsage);

    BufferWithMemory verificationBuffer(vkd, device, alloc, verificationBufferInfo, MemoryRequirement::HostVisible);
    auto &verificationBufferAlloc = verificationBuffer.getAllocation();
    void *verificationBufferData  = verificationBufferAlloc.getHostPtr();

    // Move to constructor
    auto pvdSize       = findDataSize(Owner::VERTEX);
    const auto ppdSize = findDataSize(Owner::PATCH);

    std::vector<uint8_t> perVertexData;
    std::vector<uint8_t> perPatchData;

    // Descriptor set layout.
    DescriptorSetLayoutBuilder setLayoutBuilder;
    uint32_t descCount = 0;

    de::SharedPtr<BufferWithMemory> pvdData;
    de::SharedPtr<BufferWithMemory> ppdData;

    if (pvdSize)
    {
        perVertexData.resize(pvdSize);
        initData(perVertexData, Owner::VERTEX);

        // Create and fill buffers with this data.
        const auto pvdInfo = makeBufferCreateInfo(pvdSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        pvdData            = de::SharedPtr<BufferWithMemory>(
            new BufferWithMemory(vkd, device, alloc, pvdInfo, MemoryRequirement::HostVisible));

        auto &pvdAlloc = pvdData->getAllocation();
        void *pvdPtr   = pvdAlloc.getHostPtr();
        deMemcpy(pvdPtr, de::dataOrNull(perVertexData), pvdSize);
        flushAlloc(vkd, device, pvdAlloc);

        setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bufStages);
        descCount++;
    }

    if (ppdSize)
    {
        perPatchData.resize(ppdSize);
        initData(perPatchData, Owner::PATCH);

        const auto ppdInfo = makeBufferCreateInfo(ppdSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        ppdData            = de::SharedPtr<BufferWithMemory>(
            new BufferWithMemory(vkd, device, alloc, ppdInfo, MemoryRequirement::HostVisible));

        auto &ppdAlloc = ppdData->getAllocation();
        void *ppdPtr   = ppdAlloc.getHostPtr();
        deMemcpy(ppdPtr, de::dataOrNull(perPatchData), ppdSize);
        flushAlloc(vkd, device, ppdAlloc);

        setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bufStages);
        descCount++;
    }

    const auto setLayout = setLayoutBuilder.build(vkd, device);

    // Create and update descriptor set.
    DescriptorPoolBuilder descriptorPoolBuilder;
    descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, descCount);
    const auto descriptorPool =
        descriptorPoolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    const auto descriptorSet = makeDescriptorSet(vkd, device, descriptorPool.get(), setLayout.get());

    DescriptorSetUpdateBuilder updateBuilder;
    uint32_t bindingIdx = 0;

    if (pvdSize)
    {
        const auto pvdBufferInfo = makeDescriptorBufferInfo((*pvdData).get(), 0ull, pvdSize);
        updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(bindingIdx),
                                  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &pvdBufferInfo);
        bindingIdx++;
    }

    if (ppdSize)
    {
        const auto ppdBufferInfo = makeDescriptorBufferInfo((*ppdData).get(), 0ull, ppdSize);
        updateBuilder.writeSingle(descriptorSet.get(), DescriptorSetUpdateBuilder::Location::binding(bindingIdx),
                                  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &ppdBufferInfo);
        bindingIdx++;
    }

    updateBuilder.update(vkd, device);

    // Pipeline layout.
    const auto pipelineLayout = makePipelineLayout(vkd, device, setLayout.get());

    // Shader modules.
    const auto vertShader = createShaderModule(vkd, device, binaries.get("vert"));
    const auto tescShader = createShaderModule(vkd, device, binaries.get("tesc"));
    const auto teseShader = createShaderModule(vkd, device, binaries.get("tese"));
    const auto fragShader = createShaderModule(vkd, device, binaries.get("frag"));

    // Render pass.
    const auto renderPass = makeRenderPass(vkd, device, imageFormat);

    // Framebuffer.
    const auto framebuffer =
        makeFramebuffer(vkd, device, renderPass.get(), colorView.get(), imageExtent.width, imageExtent.height);

    // Viewport and scissor.
    const auto topHalf = makeViewport(imageExtent.width, imageExtent.height / 2u);
    const std::vector<VkViewport> viewports{makeViewport(imageExtent), topHalf};
    const std::vector<VkRect2D> scissors(2u, makeRect2D(imageExtent));

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType                             sType
        nullptr,                                                   // const void*                                 pNext
        (VkPipelineVertexInputStateCreateFlags)0,                  // VkPipelineVertexInputStateCreateFlags       flags
        0u,      // uint32_t                                    vertexBindingDescriptionCount
        nullptr, // const VkVertexInputBindingDescription*      pVertexBindingDescriptions
        0u,      // uint32_t                                    vertexAttributeDescriptionCount
        nullptr  // const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions
    };

    const auto pipeline = makeGraphicsPipeline(vkd, device, pipelineLayout.get(), vertShader.get(), tescShader.get(),
                                               teseShader.get(), VK_NULL_HANDLE, fragShader.get(), renderPass.get(),
                                               viewports, scissors, VK_PRIMITIVE_TOPOLOGY_PATCH_LIST, 0u /*subpass*/,
                                               IfaceVar::kNumVertices, &vertexInputStateCreateInfo);

    // Command pool and buffer.
    const auto cmdPool      = makeCommandPool(vkd, device, queueIndex);
    const auto cmdBufferPtr = allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto cmdBuffer    = cmdBufferPtr.get();

    beginCommandBuffer(vkd, cmdBuffer);

    // Run pipeline.
    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 0.0f);
    beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), scissors.at(0u), clearColor);
    vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());
    vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), 0u, 1u,
                              &descriptorSet.get(), 0u, nullptr);
    vkd.cmdDraw(cmdBuffer, IfaceVar::kNumPatches * IfaceVar::kNumVertices, 1u, 0u, 0u);
    endRenderPass(vkd, cmdBuffer);

    // Copy color buffer to verification buffer.
    const auto colorAccess   = (VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT);
    const auto transferRead  = VK_ACCESS_TRANSFER_READ_BIT;
    const auto transferWrite = VK_ACCESS_TRANSFER_WRITE_BIT;
    const auto hostRead      = VK_ACCESS_HOST_READ_BIT;

    const auto preCopyBarrier =
        makeImageMemoryBarrier(colorAccess, transferRead, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, colorImage.get(), colorSRR);
    const auto postCopyBarrier = makeMemoryBarrier(transferWrite, hostRead);
    const auto copyRegion      = vk::makeBufferImageCopy(imageExtent, colorSRL);

    vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
                           0u, nullptr, 0u, nullptr, 1u, &preCopyBarrier);
    vkd.cmdCopyImageToBuffer(cmdBuffer, colorImage.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                             verificationBuffer.get(), 1u, &copyRegion);
    vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u,
                           &postCopyBarrier, 0u, nullptr, 0u, nullptr);

    endCommandBuffer(vkd, cmdBuffer);
    submitCommandsAndWait(vkd, device, queue, cmdBuffer);

    // Generate reference image and compare results.
    const tcu::IVec3 iExtent(static_cast<int>(imageExtent.width), static_cast<int>(imageExtent.height), 1);
    const tcu::ConstPixelBufferAccess verificationAccess(tcuFormat, iExtent, verificationBufferData);

    // default expected color is blue in patch 0 and yellow in patch 1
    DE_ASSERT(IfaceVar::kNumPatches == 2u);
    const tcu::Vec4 expectedColor[2] = {tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f), tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f)};
    commonGenerateReferenceLevel(m_colorFormat, expectedColor, m_referenceLevel);
    invalidateAlloc(vkd, device, verificationBufferAlloc);
    auto &log = m_context.getTestContext().getLog();
    if (!commonVerifyResult(log, m_colorFormat, m_referenceLevel, verificationAccess))
        TCU_FAIL("Result does not match reference; check log for details");
    return tcu::TestStatus::pass("Pass");
}

TestInstance *MaxIOTest::createInstance(Context &context) const
{
    return new MaxIOTestInstance(context, m_testParams.get());
}

class LevelIOTest : public TestCase
{
public:
    LevelIOTest(tcu::TestContext &testCtx, const std::string &name, const TessReadLevel tessReadLevel);
    virtual ~LevelIOTest(void)
    {
    }
    void initPrograms(vk::SourceCollections &programCollection) const;
    void checkSupport(Context &context) const;
    TestInstance *createInstance(Context &context) const;

private:
    const TessReadLevel m_tessReadLevel;
};

LevelIOTest::LevelIOTest(tcu::TestContext &testCtx, const std::string &name, const TessReadLevel tessReadLevel)
    : TestCase(testCtx, name)
    , m_tessReadLevel(tessReadLevel)
{
}

void LevelIOTest::initPrograms(vk::SourceCollections &programCollection) const
{
    // Tessellation Evaluation shader
    {
        std::ostringstream tese;
        tese << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
             << "layout(quads, equal_spacing) in;\n"
             << "\n";

        tese << "layout (location=0) in patch vec4 perPatchColor;\n"
             << "\n";

        tese << "layout (location=0) out vec4 outColor;\n"
             << "\n";

        tese << "in gl_PerVertex {\n"
             << "    vec4  gl_Position;\n"
             << "} gl_in[];\n"
             << "\n"
             << "out gl_PerVertex {\n"
             << "    vec4 gl_Position;\n"
             << "};\n";

        tese << "#define INTERP_QUAD_VAR(TYPE, VAR, INi, OUT) do { \\\n"
             << "    TYPE VAR[4]; \\\n"
             << "    for (int i = 0; i < 4; i++) VAR[i] = INi; \\\n"
             << "    { \\\n"
             << "        TYPE temp1 = TYPE(VAR[0] * TYPE(1 - gl_TessCoord[0]) + VAR[1] * TYPE(gl_TessCoord[0])); \\\n"
             << "        TYPE temp2 = TYPE(VAR[2] * TYPE(1 - gl_TessCoord[0]) + VAR[3] * TYPE(gl_TessCoord[0])); \\\n"
             << "        OUT = TYPE(temp1 * TYPE(1 - gl_TessCoord[1]) + temp2 * TYPE(gl_TessCoord[1])); \\\n"
             << "    } \\\n"
             << "} while(false)\n\n";

        tese << "void main ()\n"
             << "{\n";

        switch (m_tessReadLevel)
        {
        case TessReadLevel::TESS_READS_OUTER:
        {
            tese << "    float varOuter0 = gl_TessLevelOuter[0];\n"
                 << "    float varOuter1 = gl_TessLevelOuter[1];\n"
                 << "    float varOuter2 = gl_TessLevelOuter[2];\n"
                 << "    float varOuter3 = gl_TessLevelOuter[3];\n"
                 << "    vec4 colorData = vec4(perPatchColor.x * varOuter0, perPatchColor.y * varOuter1, "
                    "perPatchColor.z * varOuter2, perPatchColor.w * varOuter3);\n";
        }
        break;
        case TessReadLevel::TESS_READS_INNER:
        {
            tese << "    float varInner0 = gl_TessLevelInner[0];\n"
                 << "    float varInner1 = gl_TessLevelInner[1];\n"
                 << "    vec4 colorData = vec4(perPatchColor.x * varInner0, perPatchColor.y * varInner1, "
                    "perPatchColor.z * varInner0, perPatchColor.w * varInner1);\n";
        }
        break;
        case TessReadLevel::TESS_READS_OUTER_INNER:
        {
            tese << "    float varOuter0 = gl_TessLevelOuter[0];\n"
                 << "    float varOuter1 = gl_TessLevelOuter[1];\n"
                 << "    float varOuter2 = gl_TessLevelOuter[2];\n"
                 << "    float varOuter3 = gl_TessLevelOuter[3];\n"
                 << "    float varInner0 = gl_TessLevelInner[0];\n"
                 << "    float varInner1 = gl_TessLevelInner[1];\n"
                 << "    vec4 colorData = vec4(perPatchColor.x * varOuter0, perPatchColor.y * varOuter1, "
                    "perPatchColor.z * varOuter2, perPatchColor.w * varOuter3);\n"
                 << "    colorData = vec4(perPatchColor.x * varInner0, perPatchColor.y * varInner1, perPatchColor.z * "
                    "varInner0, perPatchColor.w * varInner1);\n";
        }
        break;
        case TessReadLevel::TESS_WRITES0_INNER_1:
        case TessReadLevel::TESS_WRITES0_INNER_ALL:
        case TessReadLevel::TESS_WRITES0_OUTER_1:
        case TessReadLevel::TESS_WRITES0_OUTER_ALL:
        case TessReadLevel::TESS_WRITES0_OUTER_INNER:
            break;
        default:
            DE_ASSERT(false);
        }

        if (m_tessReadLevel < TessReadLevel::TESS_WRITES0_INNER_1)
        {
            tese << "    outColor = colorData;\n";
        }
        else
        {
            tese << "    outColor = perPatchColor;\n";
        }

        // gl_position
        tese << "    INTERP_QUAD_VAR(vec4, var_gl_pos, gl_in[i].gl_Position, gl_Position);\n";

        tese << "}\n";
        programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(tese.str());
    }

    // Tessellation Control shader
    {
        std::ostringstream tesc;
        tesc << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450) << "\n"
             << "#extension GL_EXT_tessellation_shader : require\n"
             << "\n"
             << "layout (vertices = " << IfaceVar::kNumVertices << ") out;\n"
             << "\n";

        tesc << "layout (location=0) out patch vec4 perPatchColor;\n"
             << "\n";

        tesc << "in gl_PerVertex {\n"
             << "    vec4  gl_Position;\n"
             << "} gl_in[];\n"
             << "\n"
             << "out gl_PerVertex {\n"
             << "    vec4  gl_Position;\n"
             << "} gl_out[];\n"
             << "\n";

        tesc << "void main ()\n"
             << "{\n";

        tesc << "   perPatchColor = (gl_PrimitiveID == 0) ? vec4(0.0, 0.0, 1.0, 1.0) : vec4(1.0, 1.0, 0.0, 1.0);\n";

        tesc << "   gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n";

        if (m_tessReadLevel < TessReadLevel::TESS_WRITES0_OUTER_1)
        {
            tesc << "   gl_TessLevelOuter = float[4](1.0, 1.0, 1.0, 1.0);\n"
                 << "   gl_TessLevelInner = float[2](1.0, 1.0);\n";
        }
        else
        {
            float outer0     = 1.0f;
            float outerOther = 1.0f;
            float inner0     = 1.0f;
            float innerOther = 1.0f;

            switch (m_tessReadLevel)
            {
            case TessReadLevel::TESS_WRITES0_OUTER_1:
            {
                outer0     = 0.0f;
                outerOther = 1.0f;
            }
            break;
            case TessReadLevel::TESS_WRITES0_OUTER_ALL:
            {
                outer0     = 0.0f;
                outerOther = 0.0f;
            }
            break;
            case TessReadLevel::TESS_WRITES0_INNER_1:
            {
                inner0     = 0.0f;
                innerOther = 1.0f;
            }
            break;
            case TessReadLevel::TESS_WRITES0_INNER_ALL:

            {
                inner0     = 0.0f;
                innerOther = 0.0f;
            }
            break;
            case TessReadLevel::TESS_WRITES0_OUTER_INNER:
            {
                outer0 = inner0 = outerOther = innerOther = 0.0f;
            }
            break;
            default:
                DE_ASSERT(false);
            }

            const std::string outer0Str     = std::to_string(outer0);
            const std::string outerOtherStr = std::to_string(outerOther);
            const std::string inner0Str     = std::to_string(inner0);
            const std::string innerOtherStr = std::to_string(innerOther);

            tesc << "   gl_TessLevelOuter = float[4](" << outer0Str << ", " << outerOtherStr << ", " << outerOtherStr
                 << ", " << outerOtherStr << ");\n"
                 << "   gl_TessLevelInner = float[2](" << inner0Str << ", " << innerOtherStr << ");\n";
        }

        tesc << "}\n";

        programCollection.glslSources.add("tesc") << glu::TessellationControlSource(tesc.str());
    }

    {
        const std::string progNamePrefix = "";
        commonShaders(progNamePrefix, programCollection);
    }
}

void LevelIOTest::checkSupport(Context &context) const
{
    context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_TESSELLATION_SHADER);
}

class LevelIOTestInstance : public TestInstance
{
public:
    LevelIOTestInstance(Context &context, const TessReadLevel tessReadLevel);
    tcu::TestStatus iterate(void) override;

private:
    const VkFormat m_colorFormat;
    const VkRect2D m_renderArea;
    const TessReadLevel m_tessReadLevel;
    std::unique_ptr<tcu::TextureLevel> m_referenceLevel;
};

LevelIOTestInstance::LevelIOTestInstance(Context &context, const TessReadLevel tessReadLevel)
    : TestInstance(context)
    , m_colorFormat(VK_FORMAT_R8G8B8A8_UNORM)
    , m_renderArea(makeRect2D(MAXIO_RENDER_SIZE_WIDTH, MAXIO_RENDER_SIZE_HEIGHT))
    , m_tessReadLevel(tessReadLevel)
{
}

tcu::TestStatus LevelIOTestInstance::iterate(void)
{
    const auto &vkd       = m_context.getDeviceInterface();
    const auto device     = m_context.getDevice();
    auto &alloc           = m_context.getDefaultAllocator();
    const auto queueIndex = m_context.getUniversalQueueFamilyIndex();
    const auto queue      = m_context.getUniversalQueue();

    const auto imageFormat = m_colorFormat;
    const auto tcuFormat   = mapVkFormat(imageFormat);
    const auto imageExtent = makeExtent3D(MAXIO_RENDER_SIZE_WIDTH, MAXIO_RENDER_SIZE_WIDTH, 1u);
    const auto imageUsage  = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

    const auto &binaries = m_context.getBinaryCollection();

    const VkImageCreateInfo colorBufferInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        0u,                                  // VkImageCreateFlags flags;
        VK_IMAGE_TYPE_2D,                    // VkImageType imageType;
        imageFormat,                         // VkFormat format;
        imageExtent,                         // VkExtent3D extent;
        1u,                                  // uint32_t mipLevels;
        1u,                                  // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,               // VkSampleCountFlagBits samples;
        VK_IMAGE_TILING_OPTIMAL,             // VkImageTiling tiling;
        imageUsage,                          // VkImageUsageFlags usage;
        VK_SHARING_MODE_EXCLUSIVE,           // VkSharingMode sharingMode;
        0u,                                  // uint32_t queueFamilyIndexCount;
        nullptr,                             // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED,           // VkImageLayout initialLayout;
    };

    // Create color image and view.
    ImageWithMemory colorImage(vkd, device, alloc, colorBufferInfo, MemoryRequirement::Any);
    const auto colorSRR  = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
    const auto colorSRL  = makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
    const auto colorView = makeImageView(vkd, device, colorImage.get(), VK_IMAGE_VIEW_TYPE_2D, imageFormat, colorSRR);

    // Create a memory buffer for verification.
    const auto verificationBufferSize =
        static_cast<VkDeviceSize>(imageExtent.width * imageExtent.height * tcu::getPixelSize(tcuFormat));
    const auto verificationBufferUsage = (VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    const auto verificationBufferInfo  = makeBufferCreateInfo(verificationBufferSize, verificationBufferUsage);

    BufferWithMemory verificationBuffer(vkd, device, alloc, verificationBufferInfo, MemoryRequirement::HostVisible);
    auto &verificationBufferAlloc = verificationBuffer.getAllocation();
    void *verificationBufferData  = verificationBufferAlloc.getHostPtr();

    // Pipeline layout.
    const auto pipelineLayout = makePipelineLayout(vkd, device, VK_NULL_HANDLE);

    // Shader modules.
    const auto vertShader = createShaderModule(vkd, device, binaries.get("vert"));
    const auto tescShader = createShaderModule(vkd, device, binaries.get("tesc"));
    const auto teseShader = createShaderModule(vkd, device, binaries.get("tese"));
    const auto fragShader = createShaderModule(vkd, device, binaries.get("frag"));

    // Render pass.
    const auto renderPass = makeRenderPass(vkd, device, imageFormat);

    // Framebuffer.
    const auto framebuffer =
        makeFramebuffer(vkd, device, renderPass.get(), colorView.get(), imageExtent.width, imageExtent.height);

    // Viewport and scissor.
    const auto topHalf = makeViewport(imageExtent.width, imageExtent.height / 2u);
    const std::vector<VkViewport> viewports{makeViewport(imageExtent), topHalf};
    const std::vector<VkRect2D> scissors(2u, makeRect2D(imageExtent));

    const VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType                             sType
        nullptr,                                                   // const void*                                 pNext
        (VkPipelineVertexInputStateCreateFlags)0,                  // VkPipelineVertexInputStateCreateFlags       flags
        0u,      // uint32_t                                    vertexBindingDescriptionCount
        nullptr, // const VkVertexInputBindingDescription*      pVertexBindingDescriptions
        0u,      // uint32_t                                    vertexAttributeDescriptionCount
        nullptr  // const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions
    };

    const auto pipeline = makeGraphicsPipeline(vkd, device, pipelineLayout.get(), vertShader.get(), tescShader.get(),
                                               teseShader.get(), VK_NULL_HANDLE, fragShader.get(), renderPass.get(),
                                               viewports, scissors, VK_PRIMITIVE_TOPOLOGY_PATCH_LIST, 0u /*subpass*/,
                                               IfaceVar::kNumVertices, &vertexInputStateCreateInfo);

    // Command pool and buffer.
    const auto cmdPool      = makeCommandPool(vkd, device, queueIndex);
    const auto cmdBufferPtr = allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    const auto cmdBuffer    = cmdBufferPtr.get();

    beginCommandBuffer(vkd, cmdBuffer);

    // Run pipeline.
    const tcu::Vec4 clearColor(0.0f, 0.0f, 0.0f, 0.0f);
    beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), scissors.at(0u), clearColor);
    vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());
    vkd.cmdDraw(cmdBuffer, IfaceVar::kNumPatches * IfaceVar::kNumVertices, 1u, 0u, 0u);
    endRenderPass(vkd, cmdBuffer);

    // Copy color buffer to verification buffer.
    const auto colorAccess   = (VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT);
    const auto transferRead  = VK_ACCESS_TRANSFER_READ_BIT;
    const auto transferWrite = VK_ACCESS_TRANSFER_WRITE_BIT;
    const auto hostRead      = VK_ACCESS_HOST_READ_BIT;

    const auto preCopyBarrier =
        makeImageMemoryBarrier(colorAccess, transferRead, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, colorImage.get(), colorSRR);
    const auto postCopyBarrier = makeMemoryBarrier(transferWrite, hostRead);
    const auto copyRegion      = vk::makeBufferImageCopy(imageExtent, colorSRL);

    vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u,
                           0u, nullptr, 0u, nullptr, 1u, &preCopyBarrier);
    vkd.cmdCopyImageToBuffer(cmdBuffer, colorImage.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                             verificationBuffer.get(), 1u, &copyRegion);
    vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u,
                           &postCopyBarrier, 0u, nullptr, 0u, nullptr);

    endCommandBuffer(vkd, cmdBuffer);
    submitCommandsAndWait(vkd, device, queue, cmdBuffer);

    // Generate reference image and compare results.
    const tcu::IVec3 iExtent(static_cast<int>(imageExtent.width), static_cast<int>(imageExtent.height), 1);
    const tcu::ConstPixelBufferAccess verificationAccess(tcuFormat, iExtent, verificationBufferData);

    // default expected color is blue in patch 0 and yellow in patch 1
    DE_ASSERT(IfaceVar::kNumPatches == 2u);
    tcu::Vec4 expectedColor[2] = {tcu::Vec4(0.0f, 0.0f, 1.0f, 1.0f), tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f)};
    // If any of the outer tessellation levels are 0
    // Or all tessellation levels are 0, no output shall
    // be produced which means no color should be expected
    if ((m_tessReadLevel >= TessReadLevel::TESS_WRITES0_OUTER_1) &&
        (m_tessReadLevel <= TessReadLevel::TESS_WRITES0_OUTER_INNER))
    {
        expectedColor[0] = expectedColor[1] = clearColor;
    }

    commonGenerateReferenceLevel(m_colorFormat, expectedColor, m_referenceLevel);
    invalidateAlloc(vkd, device, verificationBufferAlloc);
    auto &log = m_context.getTestContext().getLog();
    if (!commonVerifyResult(log, m_colorFormat, m_referenceLevel, verificationAccess))
        TCU_FAIL("Result does not match reference; check log for details");
    return tcu::TestStatus(tcu::TestStatus::pass("Passed"));
}

TestInstance *LevelIOTest::createInstance(Context &context) const
{
    return new LevelIOTestInstance(context, m_tessReadLevel);
}

} // namespace

tcu::TestCaseGroup *createTessIOTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> tessIOGroup(new tcu::TestCaseGroup(testCtx, "tess_io"));

    {
        de::MovePtr<tcu::TestCaseGroup> inOutTests(new tcu::TestCaseGroup(testCtx, "max_in_out"));

        const struct
        {
            bool i64;
            bool f64;
            bool i16;
            bool f16;
            const char *name;
        } requiredFeatures[] = {
            // Restrict the number of combinations to avoid creating too many tests.
            //    i64        f64        i16        f16        name
            {false, false, false, false, "32_bits_only"}, {true, false, false, false, "with_i64"},
            {false, true, false, false, "with_f64"},      {true, true, false, false, "all_but_16_bits"},
            {false, false, true, false, "with_i16"},      {false, false, false, true, "with_f16"},
            {true, true, true, true, "all_types"},
        };

        Owner ownerCases[]                 = {Owner::VERTEX, Owner::PATCH};
        DataType dataTypeCases[]           = {DataType::FLOAT, DataType::INTEGER};
        BitWidth bitWidthCases[]           = {BitWidth::B64, BitWidth::B32, BitWidth::B16};
        DataDim dataDimCases[]             = {DataDim::SCALAR, DataDim::VEC2, DataDim::VEC3, DataDim::VEC4};
        Interpolation interpolationCases[] = {Interpolation::NORMAL, Interpolation::FLAT};
        de::Random rnd(1636723398u);

        const struct
        {
            bool tcsReads;
            bool tesReads;
            TessVarType varType;
            const char *name;
        } tessTypes[] = {
            {false, true, TessVarType::TESS_VAR_VERT, "tcs_vert_writes_tes_reads"},
            {true, true, TessVarType::TESS_VAR_VERT, "tcs_vert_writes_reads_tes_reads"},
            {false, false, TessVarType::TESS_VAR_VERT, "tcs_vert_writes_tes_na"},
            {true, false, TessVarType::TESS_VAR_VERT, "tcs_vert_writes_reads_tes_na"},

            {false, true, TessVarType::TESS_VAR_PATCH, "tcs_patch_writes_tes_reads"},
            {true, true, TessVarType::TESS_VAR_PATCH, "tcs_patch_writes_reads_tes_reads"},
            {false, false, TessVarType::TESS_VAR_PATCH, "tcs_patch_writes_tes_na"},
            {true, false, TessVarType::TESS_VAR_PATCH, "tcs_patch_writes_reads_tes_na"},
        };

        for (const auto &reqs : requiredFeatures)
        {
            de::MovePtr<tcu::TestCaseGroup> reqsGroup(new tcu::TestCaseGroup(testCtx, reqs.name));

            // Generate the variable list according to the group requirements
            // and actual max locations available on the device
            IfaceVarVecPtr vertVarsPtr(new IfaceVarVec);
            IfaceVarVecPtr patchVarsPtr(new IfaceVarVec);

            for (const auto &ownerCase : ownerCases)
            {
                for (const auto &dataTypeCase : dataTypeCases)
                    for (const auto &bitWidthCase : bitWidthCases)
                        for (const auto &dataDimCase : dataDimCases)
                            for (const auto &interpolationCase : interpolationCases)
                            {
                                if (dataTypeCase == DataType::FLOAT)
                                {
                                    if (bitWidthCase == BitWidth::B64 && !reqs.f64)
                                        continue;
                                    if (bitWidthCase == BitWidth::B16 && !reqs.f16)
                                        continue;
                                }
                                else if (dataTypeCase == DataType::INTEGER)
                                {
                                    if (bitWidthCase == BitWidth::B64 && !reqs.i64)
                                        continue;
                                    if (bitWidthCase == BitWidth::B16 && !reqs.i16)
                                        continue;
                                }

                                if (dataTypeCase == DataType::INTEGER && interpolationCase == Interpolation::NORMAL)
                                    continue;

                                if (ownerCase == Owner::PATCH && interpolationCase == Interpolation::FLAT)
                                    continue;

                                if (dataTypeCase == DataType::FLOAT && bitWidthCase == BitWidth::B64 &&
                                    interpolationCase == Interpolation::NORMAL)
                                    continue;

                                for (uint32_t idx = 0u; idx < IfaceVar::kVarsPerType; ++idx)
                                {
                                    if (ownerCase == Owner::VERTEX)
                                        vertVarsPtr->push_back(IfaceVar(ownerCase, dataTypeCase, bitWidthCase,
                                                                        dataDimCase, interpolationCase, idx));
                                    else
                                        patchVarsPtr->push_back(IfaceVar(ownerCase, dataTypeCase, bitWidthCase,
                                                                         dataDimCase, interpolationCase, idx));
                                }
                            }
            }
            // Generating all permutations of the variables above would mean millions of tests,
            // so we just generate some pseudorandom permutations.
            constexpr uint32_t kPermutations = 10u;
            for (uint32_t combIdx = 0; combIdx < kPermutations; ++combIdx)
            {
                const auto caseName = "permutation_" + std::to_string(combIdx);
                de::MovePtr<tcu::TestCaseGroup> rndGroup(new tcu::TestCaseGroup(testCtx, caseName.c_str()));

                // Duplicate and shuffle vector.
                IfaceVarVecPtr permutVertVec(new IfaceVarVec(*vertVarsPtr));
                rnd.shuffle(begin(*permutVertVec), end(*permutVertVec));

                IfaceVarVecPtr permutPatchVec(new IfaceVarVec(*patchVarsPtr));
                rnd.shuffle(begin(*permutPatchVec), end(*permutPatchVec));

                for (const auto tessType : tessTypes)
                {
                    // Duplicate vector for this particular case so all variants have the same shuffle.
                    IfaceVarVecPtr paramsVec;
                    if (tessType.varType == TessVarType::TESS_VAR_VERT)
                        paramsVec = IfaceVarVecPtr(new IfaceVarVec(*permutVertVec));
                    else
                        paramsVec = IfaceVarVecPtr(new IfaceVarVec(*permutPatchVec));

                    ParamsPtr paramsPtr(new MaxIOTestParams(
                        /*tcsReads*/ tessType.tcsReads,
                        /*tesReads*/ tessType.tesReads,
                        /*tessVarType*/ tessType.varType,
                        /*useInt64*/ reqs.i64,
                        /*useFloat64*/ reqs.f64,
                        /*useInt16*/ reqs.i16,
                        /*useFloat16*/ reqs.f16,
                        /*vars*/ std::move(paramsVec)));

                    rndGroup->addChild(new MaxIOTest(testCtx, tessType.name, std::move(paramsPtr)));
                }

                reqsGroup->addChild(rndGroup.release());
            }

            inOutTests->addChild(reqsGroup.release());
        }

        tessIOGroup->addChild(inOutTests.release());
    }

    {
        de::MovePtr<tcu::TestCaseGroup> levelIOGroup(new tcu::TestCaseGroup(testCtx, "level_io"));
        // test reading of tessellation outer and inner variables from TES
        {
            const struct
            {
                TessReadLevel tessReadLevel;
                const char *name;
            } tessLevels[] = {
                {TessReadLevel::TESS_READS_INNER, "tes_reads_inner"},
                {TessReadLevel::TESS_READS_OUTER, "tes_reads_outer"},
                {TessReadLevel::TESS_READS_OUTER_INNER, "tes_reads_both"},
            };

            for (const auto &tessLevel : tessLevels)
            {
                levelIOGroup->addChild(new LevelIOTest(testCtx, tessLevel.name, tessLevel.tessReadLevel));
            }
        }

        // test writing of tessellation outer and inner variables as 0 from TCS
        {
            const struct
            {
                TessReadLevel tessReadLevel;
                const char *name;
            } tessLevels[] = {
                {TessReadLevel::TESS_WRITES0_OUTER_1, "tcs_writes0_outer_1"},
                {TessReadLevel::TESS_WRITES0_OUTER_ALL, "tcs_writes0_outer_all"},
                {TessReadLevel::TESS_WRITES0_INNER_1, "tcs_writes0_inner_1"},
                {TessReadLevel::TESS_WRITES0_INNER_ALL, "tcs_writes0_inner_all"},
                {TessReadLevel::TESS_WRITES0_OUTER_INNER, "tcs_writes0_outer_inner"},
            };

            for (const auto &tessLevel : tessLevels)
            {
                levelIOGroup->addChild(new LevelIOTest(testCtx, tessLevel.name, tessLevel.tessReadLevel));
            }
        }
        tessIOGroup->addChild(levelIOGroup.release());
    }

    return tessIOGroup.release();
}

} // namespace tessellation
} // namespace vkt
