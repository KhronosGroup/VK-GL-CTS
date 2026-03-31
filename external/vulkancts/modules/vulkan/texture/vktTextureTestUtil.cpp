/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 * Copyright (c) 2014 The Android Open Source Project
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
 * \brief Texture test utilities.
 *//*--------------------------------------------------------------------*/

#include "vktTextureTestUtil.hpp"

#include "deFilePath.hpp"
#include "deMath.h"
#include "tcuCompressedTexture.hpp"
#include "tcuImageIO.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuTestLog.hpp"
#include "vkBuilderUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkMemUtil.hpp"
#include "../image/vktImageTestsUtil.hpp"
#include <map>
#include <string>
#include <vector>
#include <set>
#include "vktCustomInstancesDevices.hpp"
#include "tcuCommandLine.hpp"

using tcu::TestLog;

using namespace vk;
using namespace glu::TextureTestUtil;

namespace vkt
{
namespace texture
{
namespace util
{

const char *getProgramName(Program program)
{
    switch (program)
    {
    case PROGRAM_2D_FLOAT:
        return "2D_FLOAT";
    case PROGRAM_2D_INT:
        return "2D_INT";
    case PROGRAM_2D_UINT:
        return "2D_UINT";
    case PROGRAM_2D_FETCH_LOD:
        return "2D_FETCH_LOD";
    case PROGRAM_2D_SHADOW:
        return "2D_SHADOW";
    case PROGRAM_2D_FLOAT_BIAS:
        return "2D_FLOAT_BIAS";
    case PROGRAM_2D_INT_BIAS:
        return "2D_INT_BIAS";
    case PROGRAM_2D_UINT_BIAS:
        return "2D_UINT_BIAS";
    case PROGRAM_2D_SHADOW_BIAS:
        return "2D_SHADOW_BIAS";
    case PROGRAM_1D_FLOAT:
        return "1D_FLOAT";
    case PROGRAM_1D_INT:
        return "1D_INT";
    case PROGRAM_1D_UINT:
        return "1D_UINT";
    case PROGRAM_1D_SHADOW:
        return "1D_SHADOW";
    case PROGRAM_1D_FLOAT_BIAS:
        return "1D_FLOAT_BIAS";
    case PROGRAM_1D_INT_BIAS:
        return "1D_INT_BIAS";
    case PROGRAM_1D_UINT_BIAS:
        return "1D_UINT_BIAS";
    case PROGRAM_1D_SHADOW_BIAS:
        return "1D_SHADOW_BIAS";
    case PROGRAM_CUBE_FLOAT:
        return "CUBE_FLOAT";
    case PROGRAM_CUBE_INT:
        return "CUBE_INT";
    case PROGRAM_CUBE_UINT:
        return "CUBE_UINT";
    case PROGRAM_CUBE_SHADOW:
        return "CUBE_SHADOW";
    case PROGRAM_CUBE_FLOAT_BIAS:
        return "CUBE_FLOAT_BIAS";
    case PROGRAM_CUBE_INT_BIAS:
        return "CUBE_INT_BIAS";
    case PROGRAM_CUBE_UINT_BIAS:
        return "CUBE_UINT_BIAS";
    case PROGRAM_CUBE_SHADOW_BIAS:
        return "CUBE_SHADOW_BIAS";
    case PROGRAM_2D_ARRAY_FLOAT:
        return "2D_ARRAY_FLOAT";
    case PROGRAM_2D_ARRAY_INT:
        return "2D_ARRAY_INT";
    case PROGRAM_2D_ARRAY_UINT:
        return "2D_ARRAY_UINT";
    case PROGRAM_2D_ARRAY_SHADOW:
        return "2D_ARRAY_SHADOW";
    case PROGRAM_3D_FLOAT:
        return "3D_FLOAT";
    case PROGRAM_3D_INT:
        return "3D_INT";
    case PROGRAM_3D_UINT:
        return "3D_UINT";
    case PROGRAM_3D_FETCH_LOD:
        return "3D_FETCH_LOD";
    case PROGRAM_3D_FLOAT_BIAS:
        return "3D_FLOAT_BIAS";
    case PROGRAM_3D_INT_BIAS:
        return "3D_INT_BIAS";
    case PROGRAM_3D_UINT_BIAS:
        return "3D_UINT_BIAS";
    case PROGRAM_CUBE_ARRAY_FLOAT:
        return "CUBE_ARRAY_FLOAT";
    case PROGRAM_CUBE_ARRAY_INT:
        return "CUBE_ARRAY_INT";
    case PROGRAM_CUBE_ARRAY_UINT:
        return "CUBE_ARRAY_UINT";
    case PROGRAM_CUBE_ARRAY_SHADOW:
        return "CUBE_ARRAY_SHADOW";
    case PROGRAM_1D_ARRAY_FLOAT:
        return "1D_ARRAY_FLOAT";
    case PROGRAM_1D_ARRAY_INT:
        return "1D_ARRAY_INT";
    case PROGRAM_1D_ARRAY_UINT:
        return "1D_ARRAY_UINT";
    case PROGRAM_1D_ARRAY_SHADOW:
        return "1D_ARRAY_SHADOW";
    case PROGRAM_BUFFER_FLOAT:
        return "BUFFER_FLOAT";
    case PROGRAM_BUFFER_INT:
        return "BUFFER_INT";
    case PROGRAM_BUFFER_UINT:
        return "BUFFER_UINT";
    default:
        DE_ASSERT(false);
    }
    return NULL;
}

VkImageViewType textureTypeToImageViewType(TextureBinding::Type type)
{
    switch (type)
    {
    case TextureBinding::TYPE_2D:
        return VK_IMAGE_VIEW_TYPE_2D;
    case TextureBinding::TYPE_2D_ARRAY:
        return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    case TextureBinding::TYPE_CUBE_MAP:
        return VK_IMAGE_VIEW_TYPE_CUBE;
    case TextureBinding::TYPE_3D:
        return VK_IMAGE_VIEW_TYPE_3D;
    case TextureBinding::TYPE_1D:
        return VK_IMAGE_VIEW_TYPE_1D;
    case TextureBinding::TYPE_1D_ARRAY:
        return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
    case TextureBinding::TYPE_CUBE_ARRAY:
        return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
    default:
        TCU_THROW(InternalError, "Unhandled TextureBinding");
    }
}

VkImageType imageViewTypeToImageType(VkImageViewType type)
{
    switch (type)
    {
    case VK_IMAGE_VIEW_TYPE_2D:
    case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
    case VK_IMAGE_VIEW_TYPE_CUBE:
        return VK_IMAGE_TYPE_2D;
    case VK_IMAGE_VIEW_TYPE_3D:
        return VK_IMAGE_TYPE_3D;
    case VK_IMAGE_VIEW_TYPE_1D:
    case VK_IMAGE_VIEW_TYPE_1D_ARRAY:
        return VK_IMAGE_TYPE_1D;
    case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
        return VK_IMAGE_TYPE_2D;
    default:
        TCU_THROW(InternalError, "Unhandled ImageViewType");
    }
}

void initializePrograms(vk::SourceCollections &programCollection, glu::Precision texCoordPrecision,
                        const std::vector<Program> &programs, const char *texCoordSwizzle,
                        glu::Precision fragOutputPrecision, bool unnormal, VkFormat outputFormat)
{
    static const char *vertShaderTemplate =
        "${VTX_HEADER}"
        "layout(location = 0) ${VTX_IN} highp vec4 a_position;\n"
        "layout(location = 1) ${VTX_IN} ${PRECISION} ${TEXCOORD_TYPE} a_texCoord;\n"
        "layout(location = 0) ${VTX_OUT} ${PRECISION} ${TEXCOORD_TYPE} v_texCoord;\n"
        "${VTX_OUT} gl_PerVertex { vec4 gl_Position; };\n"
        "\n"
        "void main (void)\n"
        "{\n"
        "    gl_Position = a_position;\n"
        "    v_texCoord = a_texCoord;\n"
        "}\n";

    static const char *fragShaderTemplate =
        "${FRAG_HEADER}"
        "layout(location = 0) ${FRAG_IN} ${PRECISION} ${TEXCOORD_TYPE} v_texCoord;\n"
        "layout(location = 0) out ${FRAG_PRECISION} vec4 ${FRAG_COLOR};\n"
        "layout (set=0, binding=0, std140) uniform Block \n"
        "{\n"
        "  ${PRECISION} float u_bias;\n"
        "  ${PRECISION} float u_ref;\n"
        "  ${PRECISION} vec4 u_colorScale;\n"
        "  ${PRECISION} vec4 u_colorBias;\n"
        "};\n\n"
        "layout (set=1, binding=0) uniform ${PRECISION} ${SAMPLER_TYPE} u_sampler;\n"
        "void main (void)\n"
        "{\n"
        "  ${PRECISION} ${TEXCOORD_TYPE} texCoord = v_texCoord${TEXCOORD_SWZ:opt};\n"
        "  ${FRAG_COLOR} = ${LOOKUP} * u_colorScale + u_colorBias;\n"
        "}\n";

    static const char *compShaderTemplate =
        "${COMP_HEADER}"
        "layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;\n"
        "layout (set=0, binding=0, std140) uniform Block \n"
        "{\n"
        "  ${PRECISION} float u_bias;\n"
        "  ${PRECISION} float u_ref;\n"
        "  ${PRECISION} vec2  u_viewSize;\n" // Reusing padding for view dimensions
        "  ${PRECISION} vec4  u_colorScale;\n"
        "  ${PRECISION} vec4  u_colorBias;\n"
        "  int u_lod;\n"
        "};\n\n"
        "layout(push_constant) uniform PushConstants {\n"
        "  ivec2 u_offset;\n"
        "} pc;\n\n"
        "layout (set=0, binding=1) uniform ${PRECISION} ${SAMPLER_TYPE} u_sampler;\n"
        "layout (set=0, binding=2, ${IMAGE_FORMAT}) uniform writeonly ${IMAGE_TYPE} u_outputImage;\n"
        "layout (set=0, binding=3, std430) readonly buffer Geometry \n"
        "{\n"
        "  vec4 u_texCoords[4];\n"
        "  vec4 u_positions[4];\n"
        "};\n"
        "\n"
        // Helper to interpolate at a specific screen coordinate
        "${PRECISION} ${TEXCOORD_TYPE} interpolate(vec2 p, ivec2 size)\n"
        "{\n"
        "  vec2 uv = (p + 0.5) / vec2(size);\n"
        "\n"
        // Vertices layout in buffer: 0:TL, 1:BL, 2:TR, 3:BR
        "  float w0 = u_positions[0].w; float w1 = u_positions[1].w;\n"
        "  float w2 = u_positions[2].w; float w3 = u_positions[3].w;\n"
        "\n"
        // Emulate rasterizer triangle interpolation for perspective correctness
        // Indices: 0:TL, 1:BL, 2:TR, 3:BR
        "  float b0, b1, b2, b3;\n"
        "  if (uv.x + uv.y <= 1.0)\n" // Tri 1: 0-1-2
        "  {\n"
        "    b0 = 1.0 - uv.x - uv.y;\n"
        "    b1 = uv.y;\n"
        "    b2 = uv.x;\n"
        "    b3 = 0.0;\n"
        "  }\n"
        "  else\n" // Tri 2: 2-1-3
        "  {\n"
        "    b0 = 0.0;\n"
        "    b1 = 1.0 - uv.x;\n"
        "    b2 = 1.0 - uv.y;\n"
        "    b3 = uv.x + uv.y - 1.0;\n"
        "  }\n"
        "\n"
        // Interpolate (TexCoord / W)
        "  ${TEXCOORD_TYPE} tc = \n"
        "      ${TEXCOORD_TYPE}(u_texCoords[0]) * (b0 / w0) +\n"
        "      ${TEXCOORD_TYPE}(u_texCoords[1]) * (b1 / w1) +\n"
        "      ${TEXCOORD_TYPE}(u_texCoords[2]) * (b2 / w2) +\n"
        "      ${TEXCOORD_TYPE}(u_texCoords[3]) * (b3 / w3);\n"
        "\n"
        // Interpolate (1 / W)
        "  float invW = \n"
        "      (b0 / w0) + (b1 / w1) + (b2 / w2) + (b3 / w3);\n"
        "\n"
        "  return ${TEXCOORD_TYPE}(tc / invW);\n"
        "}\n"
        "\n"
        "void main (void)\n"
        "{\n"
        "  ivec2 coord = ivec2(gl_GlobalInvocationID.xy);\n"
        "  ivec2 size  = ivec2(u_viewSize);\n"
        "  if (coord.x >= size.x || coord.y >= size.y)\n"
        "    return;\n"
        "\n"
        // Calculate Texture Coordinate at Current Pixel
        "  ${PRECISION} ${TEXCOORD_TYPE} texCoord = interpolate(vec2(coord), size)${TEXCOORD_SWZ:opt};\n"
        // Calculate Derivatives (Gradients) for Mipmapping
        // We calculate the coordinate at X+1 and Y+1 to approximate dFdx/dFdy
        "  ${PRECISION} ${TEXCOORD_TYPE} texCoordX = interpolate(vec2(coord) + vec2(1.0, 0.0), "
        "size)${TEXCOORD_SWZ:opt};\n"
        "  ${PRECISION} ${TEXCOORD_TYPE} texCoordY = interpolate(vec2(coord) + vec2(0.0, 1.0), "
        "size)${TEXCOORD_SWZ:opt};\n"
        "  ${PRECISION} ${TEXCOORD_TYPE} dPdx      = texCoordX - texCoord;\n"
        "  ${PRECISION} ${TEXCOORD_TYPE} dPdy      = texCoordY - texCoord;\n"
        "\n"
        // Lookup is performed with texture gradients
        // For bias mode, we calculate LOD manually
        "  ${RESULT_TYPE} result = ${LOOKUP} * u_colorScale + u_colorBias;\n"
        "  imageStore(u_outputImage, ${STORE_COORD}, result);\n"
        "}\n";

    tcu::StringTemplate vertexSource(vertShaderTemplate);
    tcu::StringTemplate fragmentSource(fragShaderTemplate);
    tcu::StringTemplate computeSource(compShaderTemplate);

    for (std::vector<Program>::const_iterator programIt = programs.begin(); programIt != programs.end(); ++programIt)
    {
        Program program = *programIt;
        std::map<std::string, std::string> params;

        // --- 1. FLAGS SETUP ---
        // Explicitly define flags to avoid undeclared identifier errors
        bool isCube      = de::inRange<int>(program, PROGRAM_CUBE_FLOAT, PROGRAM_CUBE_SHADOW_BIAS);
        bool isCubeArray = de::inRange<int>(program, PROGRAM_CUBE_ARRAY_FLOAT, PROGRAM_CUBE_ARRAY_SHADOW);
        bool is2DArray   = de::inRange<int>(program, PROGRAM_2D_ARRAY_FLOAT, PROGRAM_2D_ARRAY_SHADOW);
        bool is1DArray   = de::inRange<int>(program, PROGRAM_1D_ARRAY_FLOAT, PROGRAM_1D_ARRAY_SHADOW);
        bool isArray     = is2DArray || is1DArray;

        bool is1D = de::inRange<int>(program, PROGRAM_1D_FLOAT, PROGRAM_1D_SHADOW_BIAS) || is1DArray ||
                    de::inRange<int>(program, PROGRAM_BUFFER_FLOAT, PROGRAM_BUFFER_UINT);

        bool is2D = de::inRange<int>(program, PROGRAM_2D_FLOAT, PROGRAM_2D_SHADOW_BIAS) || is2DArray;

        bool is3D = de::inRange<int>(program, PROGRAM_3D_FLOAT, PROGRAM_3D_UINT_BIAS);

        // Manual check for Bias programs
        bool isBias = false;
        if (program == PROGRAM_2D_FLOAT_BIAS || program == PROGRAM_2D_INT_BIAS || program == PROGRAM_2D_UINT_BIAS ||
            program == PROGRAM_2D_SHADOW_BIAS || program == PROGRAM_1D_FLOAT_BIAS || program == PROGRAM_1D_INT_BIAS ||
            program == PROGRAM_1D_UINT_BIAS || program == PROGRAM_1D_SHADOW_BIAS || program == PROGRAM_3D_FLOAT_BIAS ||
            program == PROGRAM_3D_INT_BIAS || program == PROGRAM_3D_UINT_BIAS || program == PROGRAM_CUBE_FLOAT_BIAS ||
            program == PROGRAM_CUBE_INT_BIAS || program == PROGRAM_CUBE_UINT_BIAS ||
            program == PROGRAM_CUBE_SHADOW_BIAS)
        {
            isBias = true;
        }

        const std::string version = glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450);

        params["FRAG_HEADER"] = version + "\n";
        params["VTX_HEADER"]  = version + "\n";
        params["COMP_HEADER"] = version + "\n";
        params["VTX_IN"]      = "in";
        params["VTX_OUT"]     = "out";
        params["FRAG_IN"]     = "in";
        params["FRAG_COLOR"]  = "dEQP_FragColor";

        params["IMAGE_FORMAT"] = image::getShaderImageFormatQualifier(mapVkFormat(outputFormat));
        params["IMAGE_TYPE"]   = "image2D";
        params["RESULT_TYPE"]  = "vec4";

        params["PRECISION"]      = glu::getPrecisionName(texCoordPrecision);
        params["FRAG_PRECISION"] = glu::getPrecisionName(fragOutputPrecision);

        if (isCubeArray)
            params["TEXCOORD_TYPE"] = "vec4";
        else if (isCube || (is2D && isArray) || is3D)
            params["TEXCOORD_TYPE"] = "vec3";
        else if ((is1D && isArray) || is2D)
            params["TEXCOORD_TYPE"] = "vec2";
        else if (is1D)
            params["TEXCOORD_TYPE"] = "float";
        else
            DE_ASSERT(false);

        if (texCoordSwizzle)
            params["TEXCOORD_SWZ"] = std::string(".") + texCoordSwizzle;

        bool isOutput1D = de::inRange<int>(program, PROGRAM_1D_FLOAT, PROGRAM_1D_SHADOW_BIAS);
        if (isOutput1D)
        {
            params["IMAGE_TYPE"]  = "image1D";
            params["STORE_COORD"] = "int(coord.x + pc.u_offset.x)";
        }
        else
        {
            params["IMAGE_TYPE"]  = "image2D";
            params["STORE_COORD"] = "coord + pc.u_offset";
        }

        const char *sampler = nullptr;
        std::string lookupFrag;
        std::string lookupComp;

        std::string texture = unnormal ? "textureLod" : "texture";
        std::string lod     = unnormal ? ", 0" : "";
        std::string biasArg = isBias ? ", u_bias" : "";

        // Determine Gradient Arguments (dPdx, dPdy)
        std::string gradArgs;
        // Determine Size & Deriv Calc (for Manual LOD in Bias mode)
        std::string sizeCalc;
        std::string derivCalc;

        if (isCubeArray)
        {
            gradArgs  = "dPdx.xyz, dPdy.xyz";
            sizeCalc  = "float(textureSize(u_sampler, 0).x)";
            derivCalc = "max(length(dPdx.xyz), length(dPdy.xyz)) * " + sizeCalc;
        }
        else if (isCube)
        {
            gradArgs  = "dPdx, dPdy"; // dPdx is vec3
            sizeCalc  = "float(textureSize(u_sampler, 0).x)";
            derivCalc = "max(length(dPdx), length(dPdy)) * " + sizeCalc;
        }
        else if (is3D)
        {
            gradArgs  = "dPdx, dPdy";
            sizeCalc  = "vec3(textureSize(u_sampler, 0))";
            derivCalc = "max(length(dPdx * " + sizeCalc + "), length(dPdy * " + sizeCalc + "))";
        }
        else if (is2DArray)
        {
            gradArgs  = "dPdx.xy, dPdy.xy";
            sizeCalc  = "vec2(textureSize(u_sampler, 0).xy)";
            derivCalc = "max(length(dPdx.xy * " + sizeCalc + "), length(dPdy.xy * " + sizeCalc + "))";
        }
        else if (is2D)
        {
            gradArgs  = "dPdx, dPdy";
            sizeCalc  = "vec2(textureSize(u_sampler, 0))";
            derivCalc = "max(length(dPdx * " + sizeCalc + "), length(dPdy * " + sizeCalc + "))";
        }
        else if (is1DArray)
        {
            gradArgs  = "dPdx.x, dPdy.x";
            sizeCalc  = "float(textureSize(u_sampler, 0).x)";
            derivCalc = "max(length(dPdx.x * " + sizeCalc + "), length(dPdy.x * " + sizeCalc + "))";
        }
        else if (is1D)
        {
            gradArgs  = "dPdx, dPdy"; // dPdx is float
            sizeCalc  = "float(textureSize(u_sampler, 0))";
            derivCalc = "max(length(dPdx * " + sizeCalc + "), length(dPdy * " + sizeCalc + "))";
        }
        else
        {
            gradArgs = "dPdx, dPdy";
        }

        bool needsGrad = !unnormal && !de::inRange<int>(program, PROGRAM_2D_FETCH_LOD, PROGRAM_2D_FETCH_LOD) &&
                         !de::inRange<int>(program, PROGRAM_3D_FETCH_LOD, PROGRAM_3D_FETCH_LOD) &&
                         !de::inRange<int>(program, PROGRAM_BUFFER_FLOAT, PROGRAM_BUFFER_UINT);

        switch (program)
        {
        // --- 2D ---
        case PROGRAM_2D_FLOAT:
        case PROGRAM_2D_FLOAT_BIAS:
            sampler    = "sampler2D";
            lookupFrag = texture + "(u_sampler, texCoord" + (isBias ? biasArg : lod) + ")";
            if (isBias)
                lookupComp = "textureLod(u_sampler, texCoord, log2(" + derivCalc + ") + u_bias)";
            else
                lookupComp = needsGrad ? "textureGrad(u_sampler, texCoord, " + gradArgs + ")" : lookupFrag;
            break;

        case PROGRAM_2D_INT:
        case PROGRAM_2D_INT_BIAS:
            sampler    = "isampler2D";
            lookupFrag = "vec4(" + texture + "(u_sampler, texCoord" + (isBias ? biasArg : lod) + "))";
            if (isBias)
                lookupComp = "vec4(textureGrad(u_sampler, texCoord, dPdx * exp2(u_bias), dPdy * exp2(u_bias)))";
            else
                lookupComp = lookupFrag;
            break;
        case PROGRAM_2D_UINT:
        case PROGRAM_2D_UINT_BIAS:
            sampler    = "usampler2D";
            lookupFrag = "vec4(" + texture + "(u_sampler, texCoord" + (isBias ? biasArg : lod) + "))";
            if (isBias)
                lookupComp = "vec4(textureGrad(u_sampler, texCoord, dPdx * exp2(u_bias), dPdy * exp2(u_bias)))";
            else
                lookupComp = lookupFrag;
            break;

        case PROGRAM_2D_SHADOW:
        case PROGRAM_2D_SHADOW_BIAS:
            sampler    = "sampler2DShadow";
            lookupFrag = "vec4(" + texture + "(u_sampler, vec3(texCoord, u_ref)" + (isBias ? biasArg : lod) +
                         "), 0.0, 0.0, 1.0)";
            if (isBias)
                lookupComp = "vec4(textureGrad(u_sampler, vec3(texCoord, u_ref), dPdx * exp2(u_bias), dPdy * "
                             "exp2(u_bias)), 0.0, 0.0, 1.0)";
            else
                lookupComp =
                    needsGrad ? "vec4(textureGrad(u_sampler, vec3(texCoord, u_ref), " + gradArgs + "), 0.0, 0.0, 1.0)" :
                                lookupFrag;
            break;

        case PROGRAM_2D_FETCH_LOD:
            sampler    = "sampler2D";
            lookupFrag = "texelFetch(u_sampler, ivec2(texCoord * vec2(64.f)), 3)";
            lookupComp = lookupFrag;
            break;

        // --- 1D ---
        case PROGRAM_1D_FLOAT:
        case PROGRAM_1D_FLOAT_BIAS:
            sampler    = "sampler1D";
            lookupFrag = texture + "(u_sampler, texCoord" + (isBias ? biasArg : lod) + ")";
            if (isBias)
                lookupComp = "textureLod(u_sampler, texCoord, log2(" + derivCalc + ") + u_bias)";
            else
                lookupComp = needsGrad ? "textureGrad(u_sampler, texCoord, " + gradArgs + ")" : lookupFrag;
            break;
        case PROGRAM_1D_INT:
        case PROGRAM_1D_INT_BIAS:
            sampler    = "isampler1D";
            lookupFrag = "vec4(" + texture + "(u_sampler, texCoord" + (isBias ? biasArg : lod) + "))";
            if (isBias)
                lookupComp = "vec4(textureGrad(u_sampler, texCoord, dPdx * exp2(u_bias), dPdy * exp2(u_bias)))";
            else
                lookupComp = lookupFrag;
            break;
        case PROGRAM_1D_UINT:
        case PROGRAM_1D_UINT_BIAS:
            sampler    = "usampler1D";
            lookupFrag = "vec4(" + texture + "(u_sampler, texCoord" + (isBias ? biasArg : lod) + "))";
            if (isBias)
                lookupComp = "vec4(textureGrad(u_sampler, texCoord, dPdx * exp2(u_bias), dPdy * exp2(u_bias)))";
            else
                lookupComp = lookupFrag;
            break;
        case PROGRAM_1D_SHADOW:
        case PROGRAM_1D_SHADOW_BIAS:
            sampler    = "sampler1DShadow";
            lookupFrag = "vec4(" + texture + "(u_sampler, vec3(texCoord, 0.0, u_ref)" + (isBias ? biasArg : lod) +
                         "), 0.0, 0.0, 1.0)";
            if (isBias)
                lookupComp = "vec4(textureGrad(u_sampler, vec3(texCoord, 0.0, u_ref), dPdx * exp2(u_bias), dPdy * "
                             "exp2(u_bias)), 0.0, 0.0, 1.0)";
            else
                lookupComp = needsGrad ? "vec4(textureGrad(u_sampler, vec3(texCoord, 0.0, u_ref), " + gradArgs +
                                             "), 0.0, 0.0, 1.0)" :
                                         lookupFrag;
            break;
            break;

        // --- 3D ---
        case PROGRAM_3D_FLOAT:
        case PROGRAM_3D_FLOAT_BIAS:
            sampler    = "sampler3D";
            lookupFrag = texture + "(u_sampler, texCoord" + (isBias ? biasArg : lod) + ")";
            if (isBias)
                lookupComp = "textureLod(u_sampler, texCoord, log2(" + derivCalc + ") + u_bias)";
            else
                lookupComp = needsGrad ? "textureGrad(u_sampler, texCoord, " + gradArgs + ")" : lookupFrag;
            break;
        case PROGRAM_3D_INT:
        case PROGRAM_3D_INT_BIAS:
            sampler    = "isampler3D";
            lookupFrag = "vec4(" + texture + "(u_sampler, texCoord" + (isBias ? biasArg : lod) + "))";
            if (isBias)
                lookupComp = "vec4(textureGrad(u_sampler, texCoord, dPdx * exp2(u_bias), dPdy * exp2(u_bias)))";
            else
                lookupComp = lookupFrag;
            break;
        case PROGRAM_3D_UINT:
        case PROGRAM_3D_UINT_BIAS:
            sampler    = "usampler3D";
            lookupFrag = "vec4(" + texture + "(u_sampler, texCoord" + (isBias ? biasArg : lod) + "))";
            if (isBias)
                lookupComp = "vec4(textureGrad(u_sampler, texCoord, dPdx * exp2(u_bias), dPdy * exp2(u_bias)))";
            else
                lookupComp = lookupFrag;
            break;

        // --- CUBE ---
        case PROGRAM_CUBE_FLOAT:
        case PROGRAM_CUBE_FLOAT_BIAS:
            sampler    = "samplerCube";
            lookupFrag = texture + "(u_sampler, texCoord" + (isBias ? biasArg : "") + ")";
            if (isBias)
                lookupComp = "textureGrad(u_sampler, texCoord, dPdx * exp2(u_bias), dPdy * exp2(u_bias))";
            else
                lookupComp = needsGrad ? "textureGrad(u_sampler, texCoord, " + gradArgs + ")" : lookupFrag;
            break;
        case PROGRAM_CUBE_INT:
        case PROGRAM_CUBE_INT_BIAS:
            sampler    = "isamplerCube";
            lookupFrag = "vec4(" + texture + "(u_sampler, texCoord" + (isBias ? biasArg : "") + "))";
            if (isBias)
                lookupComp = "vec4(textureGrad(u_sampler, texCoord, dPdx * exp2(u_bias), dPdy * exp2(u_bias)))";
            else
                lookupComp = lookupFrag;
            break;
        case PROGRAM_CUBE_UINT:
        case PROGRAM_CUBE_UINT_BIAS:
            sampler    = "usamplerCube";
            lookupFrag = "vec4(" + texture + "(u_sampler, texCoord" + (isBias ? biasArg : "") + "))";
            if (isBias)
                lookupComp = "vec4(textureGrad(u_sampler, texCoord, dPdx * exp2(u_bias), dPdy * exp2(u_bias)))";
            else
                lookupComp = lookupFrag;
            break;
        case PROGRAM_CUBE_SHADOW:
        case PROGRAM_CUBE_SHADOW_BIAS:
            sampler = "samplerCubeShadow";
            lookupFrag =
                "vec4(" + texture + "(u_sampler, vec4(texCoord, u_ref)" + (isBias ? biasArg : "") + "), 0.0, 0.0, 1.0)";
            if (isBias)
                lookupComp = "vec4(textureGrad(u_sampler, vec4(texCoord, u_ref), dPdx * exp2(u_bias), dPdy * "
                             "exp2(u_bias)), 0.0, 0.0, 1.0)";
            else
                lookupComp =
                    needsGrad ? "vec4(textureGrad(u_sampler, vec4(texCoord, u_ref), " + gradArgs + "), 0.0, 0.0, 1.0)" :
                                lookupFrag;
            break;

        // --- 2D ARRAY ---
        case PROGRAM_2D_ARRAY_FLOAT:
            sampler    = "sampler2DArray";
            lookupFrag = texture + "(u_sampler, texCoord)";
            if (isBias)
                lookupComp = "textureLod(u_sampler, texCoord, log2(" + derivCalc + ") + u_bias)";
            else
                lookupComp = needsGrad ? "textureGrad(u_sampler, texCoord, " + gradArgs + ")" : lookupFrag;
            break;
        case PROGRAM_2D_ARRAY_INT:
            sampler    = "isampler2DArray";
            lookupFrag = "vec4(" + texture + "(u_sampler, texCoord))";
            lookupComp = lookupFrag;
            break;
        case PROGRAM_2D_ARRAY_UINT:
            sampler    = "usampler2DArray";
            lookupFrag = "vec4(" + texture + "(u_sampler, texCoord))";
            lookupComp = lookupFrag;
            break;
        case PROGRAM_2D_ARRAY_SHADOW:
            sampler    = "sampler2DArrayShadow";
            lookupFrag = "vec4(" + texture + "(u_sampler, vec4(texCoord, u_ref)), 0.0, 0.0, 1.0)";
            lookupComp = needsGrad ?
                             "vec4(textureGrad(u_sampler, vec4(texCoord, u_ref), " + gradArgs + "), 0.0, 0.0, 1.0)" :
                             lookupFrag;
            break;

        // --- 1D ARRAY ---
        case PROGRAM_1D_ARRAY_FLOAT:
            sampler    = "sampler1DArray";
            lookupFrag = texture + "(u_sampler, texCoord)";
            lookupComp = needsGrad ? "textureGrad(u_sampler, texCoord, " + gradArgs + ")" : lookupFrag;
            break;
        case PROGRAM_1D_ARRAY_INT:
            sampler    = "isampler1DArray";
            lookupFrag = "vec4(" + texture + "(u_sampler, texCoord))";
            lookupComp = lookupFrag;
            break;
        case PROGRAM_1D_ARRAY_UINT:
            sampler    = "usampler1DArray";
            lookupFrag = "vec4(" + texture + "(u_sampler, texCoord))";
            lookupComp = lookupFrag;
            break;
        case PROGRAM_1D_ARRAY_SHADOW:
            sampler    = "sampler1DArrayShadow";
            lookupFrag = "vec4(" + texture + "(u_sampler, vec3(texCoord, u_ref)), 0.0, 0.0, 1.0)";
            lookupComp = needsGrad ?
                             "vec4(textureGrad(u_sampler, vec3(texCoord, u_ref), " + gradArgs + "), 0.0, 0.0, 1.0)" :
                             lookupFrag;
            break;

        // --- CUBE ARRAY ---
        case PROGRAM_CUBE_ARRAY_FLOAT:
            sampler    = "samplerCubeArray";
            lookupFrag = texture + "(u_sampler, texCoord)";
            if (isBias)
                lookupComp = "textureLod(u_sampler, texCoord, log2(" + derivCalc + ") + u_bias)";
            else
                lookupComp = needsGrad ? "textureGrad(u_sampler, texCoord, " + gradArgs + ")" : lookupFrag;
            break;
        case PROGRAM_CUBE_ARRAY_INT:
            sampler    = "isamplerCubeArray";
            lookupFrag = "vec4(texture(u_sampler, texCoord))";
            lookupComp = lookupFrag;
            break;
        case PROGRAM_CUBE_ARRAY_UINT:
            sampler    = "usamplerCubeArray";
            lookupFrag = "vec4(texture(u_sampler, texCoord))";
            lookupComp = lookupFrag;
            break;
        case PROGRAM_CUBE_ARRAY_SHADOW:
            sampler    = "samplerCubeArrayShadow";
            lookupFrag = "vec4(" + texture + "(u_sampler, texCoord, u_ref), 0.0, 0.0, 1.0)";
            lookupComp = needsGrad ? "vec4(textureGrad(u_sampler, texCoord, u_ref, " + gradArgs + "), 0.0, 0.0, 1.0)" :
                                     lookupFrag;
            break;

        // --- BUFFER ---
        case PROGRAM_BUFFER_FLOAT:
        case PROGRAM_BUFFER_INT:
        case PROGRAM_BUFFER_UINT:
            sampler    = (program == PROGRAM_BUFFER_FLOAT) ?
                             "samplerBuffer" :
                             ((program == PROGRAM_BUFFER_INT) ? "isamplerBuffer" : "usamplerBuffer");
            lookupFrag = (program == PROGRAM_BUFFER_FLOAT) ? "texelFetch(u_sampler, int(texCoord))" :
                                                             "vec4(texelFetch(u_sampler, int(texCoord)))";
            lookupComp = lookupFrag;
            break;

        default:
            // Fallback
            if (is2DArray)
                sampler = "sampler2DArray";
            else if (is1DArray)
                sampler = "sampler1DArray";
            else if (isCubeArray)
                sampler = "samplerCubeArray";
            else
                sampler = "sampler2D";

            lookupFrag = texture + "(u_sampler, texCoord)";
            lookupComp = lookupFrag;
            break;
        }

        params["SAMPLER_TYPE"] = sampler;

        params["LOOKUP"] = lookupFrag;
        programCollection.glslSources.add("vertex_" + std::string(getProgramName(program)))
            << glu::VertexSource(vertexSource.specialize(params));
        programCollection.glslSources.add("fragment_" + std::string(getProgramName(program)))
            << glu::FragmentSource(fragmentSource.specialize(params));

        params["LOOKUP"] = lookupComp;
        programCollection.glslSources.add("compute_" + std::string(getProgramName(program)))
            << glu::ComputeSource(computeSource.specialize(params));
    }
}

uint32_t getQueueNdx(Context &ctx, bool useCompute)
{
    const uint32_t queueNdx = useCompute ? ctx.getComputeQueueFamilyIndex() : ctx.getUniversalQueueFamilyIndex();

    if (useCompute && (queueNdx == VK_QUEUE_FAMILY_IGNORED))
        TCU_THROW(NotSupportedError, "Exclusive compute queue not supported.");

    return 0;
}

TextureBinding::TextureBinding(Context &context, bool useCompute)
    : m_context(context)
    , m_device(context.getDevice())
    , m_allocator(context.getDefaultAllocator())
    , m_useCompute(useCompute)
{
}

TextureBinding::TextureBinding(Context &context, VkDevice device, Allocator &allocator,
                               const TestTextureSp &textureData, const TextureBinding::Type type,
                               const vk::VkImageAspectFlags aspectMask,
                               const TextureBinding::ImageBackingMode backingMode,
                               const VkComponentMapping componentMapping, bool useCompute)
    : m_context(context)
    , m_device(device)
    , m_allocator(allocator)
    , m_type(type)
    , m_backingMode(backingMode)
    , m_textureData(textureData)
    , m_aspectMask(aspectMask)
    , m_componentMapping(componentMapping)
    , m_useCompute(useCompute)
{
    updateTextureData(m_textureData, m_type);
}

VkImageAspectFlags guessAspectMask(const vk::VkFormat format)
{
    tcu::TextureFormat textureFormat = mapVkFormat(format);
    const bool isShadowTexture       = tcu::hasDepthComponent(textureFormat.order);
    const bool isStencilTexture      = tcu::hasStencilComponent(textureFormat.order);
    return isShadowTexture  ? VK_IMAGE_ASPECT_DEPTH_BIT :
           isStencilTexture ? VK_IMAGE_ASPECT_STENCIL_BIT :
                              VK_IMAGE_ASPECT_COLOR_BIT;
}

void TextureBinding::updateTextureData(const TestTextureSp &textureData, const TextureBinding::Type textureType)
{
    const DeviceInterface &vkd          = m_context.getDeviceInterface();
    const bool sparse                   = m_backingMode == IMAGE_BACKING_MODE_SPARSE;
    const uint32_t queueFamilyIndex     = getQueueNdx(m_context, m_useCompute);
    const uint32_t queueFamilyIndices[] = {queueFamilyIndex, m_context.getSparseQueueFamilyIndex()};
    m_type                              = textureType;
    m_textureData                       = textureData;

    const bool isCube = (m_type == TYPE_CUBE_MAP) || (m_type == TYPE_CUBE_ARRAY);
    VkImageCreateFlags imageCreateFlags =
        (isCube ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0) |
        (sparse ? (VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT) : 0);
    const VkImageViewType imageViewType     = textureTypeToImageViewType(textureType);
    const VkImageType imageType             = imageViewTypeToImageType(imageViewType);
    const VkImageTiling imageTiling         = VK_IMAGE_TILING_OPTIMAL;
    const VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    const VkFormat format                   = textureData->isCompressed() ?
                                                  mapCompressedTextureFormat(textureData->getCompressedLevel(0, 0).getFormat()) :
                                                  mapTextureFormat(textureData->getTextureFormat());
    const tcu::UVec3 textureDimension       = textureData->getTextureDimension();
    const uint32_t mipLevels                = textureData->getNumLevels();
    const uint32_t arraySize                = textureData->getArraySize();
    vk::VkImageFormatProperties imageFormatProperties;
    const VkResult imageFormatQueryResult = m_context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(
        m_context.getPhysicalDevice(), format, imageType, imageTiling, imageUsageFlags, imageCreateFlags,
        &imageFormatProperties);
    const VkSharingMode sharingMode = (sparse && queueFamilyIndex != m_context.getSparseQueueFamilyIndex()) ?
                                          VK_SHARING_MODE_CONCURRENT :
                                          VK_SHARING_MODE_EXCLUSIVE;
    const VkQueue queue             = getDeviceQueue(vkd, m_device, queueFamilyIndex, 0);

    if ((m_context.getTestContext().getCommandLine().isComputeOnly() || m_useCompute) && (isDepthStencilFormat(format)))
        TCU_THROW(NotSupportedError, (std::string("Format: ") + vk::getFormatName(format)).c_str() +
                                         std::string(" not supported with compute-only"));

    if (imageFormatQueryResult == VK_ERROR_FORMAT_NOT_SUPPORTED)
    {
        TCU_THROW(NotSupportedError, (std::string("Format not supported: ") + vk::getFormatName(format)).c_str());
    }
    else
        VK_CHECK(imageFormatQueryResult);

    if (sparse)
    {
        uint32_t numSparseImageProperties = 0;
#ifndef CTS_USES_VULKANSC
        m_context.getInstanceInterface().getPhysicalDeviceSparseImageFormatProperties(
            m_context.getPhysicalDevice(), format, imageType, VK_SAMPLE_COUNT_1_BIT, imageUsageFlags, imageTiling,
            &numSparseImageProperties, nullptr);
#endif // CTS_USES_VULKANSC
        if (numSparseImageProperties == 0)
            TCU_THROW(NotSupportedError,
                      (std::string("Sparse format not supported: ") + vk::getFormatName(format)).c_str());
    }

    if (imageFormatProperties.maxArrayLayers < arraySize)
        TCU_THROW(NotSupportedError, ("Maximum array layers number for this format is not enough for this test."));

    if (imageFormatProperties.maxMipLevels < mipLevels)
        TCU_THROW(NotSupportedError, ("Maximum mimap level number for this format is not enough for this test."));

    if (imageFormatProperties.maxExtent.width < textureDimension.x() ||
        imageFormatProperties.maxExtent.height < textureDimension.y() ||
        imageFormatProperties.maxExtent.depth < textureDimension.z())
    {
        TCU_THROW(NotSupportedError, ("Maximum image dimension for this format is not enough for this test."));
    }

    // Create image
    const VkImageCreateInfo imageParams = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, // VkStructureType sType;
        nullptr,                             // const void* pNext;
        imageCreateFlags,                    // VkImageCreateFlags flags;
        imageType,                           // VkImageType imageType;
        format,                              // VkFormat format;
        {                                    // VkExtent3D extent;
         (uint32_t)textureDimension.x(), (uint32_t)textureDimension.y(), (uint32_t)textureDimension.z()},
        mipLevels,                                           // uint32_t mipLevels;
        arraySize,                                           // uint32_t arrayLayers;
        VK_SAMPLE_COUNT_1_BIT,                               // VkSampleCountFlagBits samples;
        imageTiling,                                         // VkImageTiling tiling;
        imageUsageFlags,                                     // VkImageUsageFlags usage;
        sharingMode,                                         // VkSharingMode sharingMode;
        sharingMode == VK_SHARING_MODE_CONCURRENT ? 2u : 1u, // uint32_t queueFamilyIndexCount;
        queueFamilyIndices,                                  // const uint32_t* pQueueFamilyIndices;
        VK_IMAGE_LAYOUT_UNDEFINED                            // VkImageLayout initialLayout;
    };

    m_textureImage = createImage(vkd, m_device, &imageParams);

    VkPipelineStageFlagBits stage =
        m_useCompute ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

    if (sparse)
    {
        pipeline::uploadTestTextureSparse(vkd, m_device, m_context.getPhysicalDevice(),
                                          m_context.getInstanceInterface(), imageParams, queue, queueFamilyIndex,
                                          m_context.getSparseQueue(), m_allocator, m_allocations, *m_textureData,
                                          *m_textureImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, stage);
    }
    else
    {
        m_textureImageMemory =
            m_allocator.allocate(getImageMemoryRequirements(vkd, m_device, *m_textureImage), MemoryRequirement::Any);
        VK_CHECK(vkd.bindImageMemory(m_device, *m_textureImage, m_textureImageMemory->getMemory(),
                                     m_textureImageMemory->getOffset()));

        pipeline::uploadTestTexture(vkd, m_device, queue, queueFamilyIndex, m_allocator, *m_textureData,
                                    *m_textureImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, stage);
    }

    updateTextureViewMipLevels(0, mipLevels - 1);
}

void TextureBinding::updateTextureViewMipLevels(uint32_t baseLevel, uint32_t maxLevel, float imageViewMinLod)
{
    const DeviceInterface &vkd              = m_context.getDeviceInterface();
    const vk::VkImageViewType imageViewType = textureTypeToImageViewType(m_type);
    const vk::VkFormat format               = m_textureData->isCompressed() ?
                                                  mapCompressedTextureFormat(m_textureData->getCompressedLevel(0, 0).getFormat()) :
                                                  mapTextureFormat(m_textureData->getTextureFormat());
    const VkImageAspectFlags aspectMask =
        (m_aspectMask != VK_IMAGE_ASPECT_FLAG_BITS_MAX_ENUM) ? m_aspectMask : guessAspectMask(format);
    const uint32_t layerCount = m_textureData->getArraySize();

    if (VK_IMAGE_VIEW_TYPE_CUBE_ARRAY == imageViewType)
        m_context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_IMAGE_CUBE_ARRAY);

#ifndef CTS_USES_VULKANSC
    vk::VkImageViewMinLodCreateInfoEXT imageViewMinLodCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_MIN_LOD_CREATE_INFO_EXT, // VkStructureType    sType
        nullptr,                                              // const void*        pNext
        imageViewMinLod,                                      // float            minLod
    };
#else
    DE_UNREF(imageViewMinLod);
#endif // CTS_USES_VULKANSC

    const vk::VkImageViewCreateInfo viewParams = {
        vk::VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType sType;
#ifndef CTS_USES_VULKANSC
        imageViewMinLod >= 0.0f ? &imageViewMinLodCreateInfo : nullptr, // const void* pNext;
#else
        nullptr, // const void* pNext;
#endif                      // CTS_USES_VULKANSC
        0u,                 // VkImageViewCreateFlags flags;
        *m_textureImage,    // VkImage image;
        imageViewType,      // VkImageViewType viewType;
        format,             // VkFormat format;
        m_componentMapping, // VkComponentMapping components;
        {
            aspectMask,               // VkImageAspectFlags aspectMask;
            baseLevel,                // uint32_t baseMipLevel;
            maxLevel - baseLevel + 1, // uint32_t levelCount;
            0,                        // uint32_t baseArrayLayer;
            layerCount                // uint32_t layerCount;
        },                            // VkImageSubresourceRange subresourceRange;
    };

    m_textureImageView = createImageView(vkd, m_device, &viewParams);
}

Move<VkDevice> createRobustBufferAccessDevice(Context &context, const VkPhysicalDeviceFeatures2 *enabledFeatures2)
{
    const float queuePriority = 1.0f;
    uint32_t queueCnt         = 1u;

    // Create a universal queue that supports graphics and compute
    VkDeviceQueueCreateInfo queueParams[2];
    queueParams[0] = {
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                    // const void* pNext;
        0u,                                         // VkDeviceQueueCreateFlags flags;
        context.getUniversalQueueFamilyIndex(),     // uint32_t queueFamilyIndex;
        1u,                                         // uint32_t queueCount;
        &queuePriority                              // const float* pQueuePriorities;
    };
    if (context.getComputeQueueFamilyIndex() != -1)
    {
        queueParams[1] = {
            VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,     // VkStructureType sType;
            nullptr,                                        // const void* pNext;
            0u,                                             // VkDeviceQueueCreateFlags flags;
            (uint32_t)context.getComputeQueueFamilyIndex(), // uint32_t queueFamilyIndex;
            1u,                                             // uint32_t queueCount;
            &queuePriority                                  // const float* pQueuePriorities;
        };
        queueCnt++;
    }

    // \note Extensions in core are not explicitly enabled even though
    //         they are in the extension list advertised to tests.
    const auto &extensionPtrs = context.getDeviceCreationExtensions();

    const VkDeviceCreateInfo deviceParams = {
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, // VkStructureType sType;
        enabledFeatures2,                     // const void* pNext;
        0u,                                   // VkDeviceCreateFlags flags;
        queueCnt,                             // uint32_t queueCreateInfoCount;
        queueParams,                          // const VkDeviceQueueCreateInfo* pQueueCreateInfos;
        0u,                                   // uint32_t enabledLayerCount;
        nullptr,                              // const char* const* ppEnabledLayerNames;
        de::sizeU32(extensionPtrs),           // uint32_t enabledExtensionCount;
        de::dataOrNull(extensionPtrs),        // const char* const* ppEnabledExtensionNames;
        nullptr                               // const VkPhysicalDeviceFeatures* pEnabledFeatures;
    };

    return createCustomDevice(context.getTestContext().getCommandLine().isValidationEnabled(),
                              context.getPlatformInterface(), context.getInstance(), context.getInstanceInterface(),
                              context.getPhysicalDevice(), &deviceParams);
}

const uint16_t GraphicsBackend::s_vertexIndices[6]          = {0, 1, 2, 2, 1, 3};
const VkDeviceSize GraphicsBackend::s_vertexIndexBufferSize = sizeof(GraphicsBackend::s_vertexIndices);

TextureRenderer::TextureRenderer(Context &context, vk::VkSampleCountFlagBits sampleCount, uint32_t renderWidth,
                                 uint32_t renderHeight, vk::VkComponentMapping componentMapping,
                                 bool requireRobustness2, bool requireImageViewMinLod, bool useCompute)
    : TextureRenderer(context, sampleCount, renderWidth, renderHeight, 1u, componentMapping, VK_IMAGE_TYPE_2D,
                      VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, requireRobustness2, requireImageViewMinLod,
                      useCompute)
{
}

TextureRenderer::TextureRenderer(Context &context, VkSampleCountFlagBits sampleCount, uint32_t renderWidth,
                                 uint32_t renderHeight, uint32_t renderDepth, VkComponentMapping componentMapping,
                                 VkImageType imageType, VkImageViewType imageViewType, vk::VkFormat imageFormat,
                                 bool requireRobustness2, bool requireImageViewMinLod, bool useCompute)
    : m_context(context)
    , m_log(context.getTestContext().getLog())
    , m_backend(nullptr)
    , m_useCompute(useCompute)
{
    if (!m_useCompute)
    {
        m_backend = RenderBackendSp(new GraphicsBackend(context, sampleCount, renderWidth, renderHeight, renderDepth,
                                                        componentMapping, imageType, imageViewType, imageFormat,
                                                        requireRobustness2, requireImageViewMinLod));
    }
    else
    {
        m_backend = RenderBackendSp(new ComputeBackend(context, sampleCount, renderWidth, renderHeight, renderDepth,
                                                       componentMapping, imageType, imageViewType, imageFormat,
                                                       requireRobustness2, requireImageViewMinLod));
    }
}

TextureRenderer::~TextureRenderer(void)
{
}

void TextureRenderer::renderQuad(tcu::Surface &result, int texUnit, const float *texCoord, TextureType texType)
{
    renderQuad(result, texUnit, texCoord, ReferenceParams(texType));
}

void TextureRenderer::renderQuad(tcu::Surface &result, int texUnit, const float *texCoord,
                                 const ReferenceParams &params)
{
    renderQuad(result.getAccess(), texUnit, texCoord, params);
}

void TextureRenderer::renderQuad(const tcu::PixelBufferAccess &result, int texUnit, const float *texCoord,
                                 const ReferenceParams &params)
{
    const float maxAnisotropy = 1.0f;
    float positions[]         = {-1.0,  -1.0f, 0.0f, 1.0f, -1.0f, +1.0f, 0.0f, 1.0f,
                                 +1.0f, -1.0f, 0.0f, 1.0f, +1.0f, +1.0f, 0.0f, 1.0f};
    renderQuad(result, positions, texUnit, texCoord, params, maxAnisotropy);
}

void TextureRenderer::renderQuad(tcu::Surface &result, const float *positions, int texUnit, const float *texCoord,
                                 const glu::TextureTestUtil::ReferenceParams &params, const float maxAnisotropy)
{
    renderQuad(result.getAccess(), positions, texUnit, texCoord, params, maxAnisotropy);
}

void TextureRenderer::renderQuad(const tcu::PixelBufferAccess &result, const float *positions, int texUnit,
                                 const float *texCoord, const glu::TextureTestUtil::ReferenceParams &params,
                                 const float maxAnisotropy)
{
    tcu::Vec4 wCoord = params.flags & RenderParams::PROJECTED ? params.w : tcu::Vec4(1.0f);
    bool useBias     = !!(params.flags & RenderParams::USE_BIAS);
    bool logUniforms = true; //!!(params.flags & RenderParams::LOG_UNIFORMS);
    bool imageViewMinLodIntegerTexelCoord =
        params.imageViewMinLod != 0.0f && params.samplerType == glu::TextureTestUtil::SAMPLERTYPE_FETCH_FLOAT;

    // Render quad with texture.
    float position[] = {
        positions[0] * wCoord.x(),  positions[1] * wCoord.x(),  positions[2],  positions[3] * wCoord.x(),
        positions[4] * wCoord.y(),  positions[5] * wCoord.y(),  positions[6],  positions[7] * wCoord.y(),
        positions[8] * wCoord.z(),  positions[9] * wCoord.z(),  positions[10], positions[11] * wCoord.z(),
        positions[12] * wCoord.w(), positions[13] * wCoord.w(), positions[14], positions[15] * wCoord.w()};

    Program progSpec = PROGRAM_LAST;
    int numComps     = 0;

    if (params.texType == TEXTURETYPE_2D)
    {
        numComps = 2;

        switch (params.samplerType)
        {
        case SAMPLERTYPE_FLOAT:
            progSpec = useBias ? PROGRAM_2D_FLOAT_BIAS : PROGRAM_2D_FLOAT;
            break;
        case SAMPLERTYPE_INT:
            progSpec = useBias ? PROGRAM_2D_INT_BIAS : PROGRAM_2D_INT;
            break;
        case SAMPLERTYPE_UINT:
            progSpec = useBias ? PROGRAM_2D_UINT_BIAS : PROGRAM_2D_UINT;
            break;
        case SAMPLERTYPE_SHADOW:
            progSpec = useBias ? PROGRAM_2D_SHADOW_BIAS : PROGRAM_2D_SHADOW;
            break;
        case SAMPLERTYPE_FETCH_FLOAT:
            progSpec = PROGRAM_2D_FETCH_LOD;
            break;
        default:
            DE_ASSERT(false);
        }
    }
    else if (params.texType == TEXTURETYPE_1D)
    {
        numComps = 1;

        switch (params.samplerType)
        {
        case SAMPLERTYPE_FLOAT:
            progSpec = useBias ? PROGRAM_1D_FLOAT_BIAS : PROGRAM_1D_FLOAT;
            break;
        case SAMPLERTYPE_INT:
            progSpec = useBias ? PROGRAM_1D_INT_BIAS : PROGRAM_1D_INT;
            break;
        case SAMPLERTYPE_UINT:
            progSpec = useBias ? PROGRAM_1D_UINT_BIAS : PROGRAM_1D_UINT;
            break;
        case SAMPLERTYPE_SHADOW:
            progSpec = useBias ? PROGRAM_1D_SHADOW_BIAS : PROGRAM_1D_SHADOW;
            break;
        default:
            DE_ASSERT(false);
        }
    }
    else if (params.texType == TEXTURETYPE_CUBE)
    {
        numComps = 3;

        switch (params.samplerType)
        {
        case SAMPLERTYPE_FLOAT:
            progSpec = useBias ? PROGRAM_CUBE_FLOAT_BIAS : PROGRAM_CUBE_FLOAT;
            break;
        case SAMPLERTYPE_INT:
            progSpec = useBias ? PROGRAM_CUBE_INT_BIAS : PROGRAM_CUBE_INT;
            break;
        case SAMPLERTYPE_UINT:
            progSpec = useBias ? PROGRAM_CUBE_UINT_BIAS : PROGRAM_CUBE_UINT;
            break;
        case SAMPLERTYPE_SHADOW:
            progSpec = useBias ? PROGRAM_CUBE_SHADOW_BIAS : PROGRAM_CUBE_SHADOW;
            break;
        default:
            DE_ASSERT(false);
        }
    }
    else if (params.texType == TEXTURETYPE_3D)
    {
        numComps = 3;

        switch (params.samplerType)
        {
        case SAMPLERTYPE_FLOAT:
            progSpec = useBias ? PROGRAM_3D_FLOAT_BIAS : PROGRAM_3D_FLOAT;
            break;
        case SAMPLERTYPE_INT:
            progSpec = useBias ? PROGRAM_3D_INT_BIAS : PROGRAM_3D_INT;
            break;
        case SAMPLERTYPE_UINT:
            progSpec = useBias ? PROGRAM_3D_UINT_BIAS : PROGRAM_3D_UINT;
            break;
        case SAMPLERTYPE_FETCH_FLOAT:
            progSpec = PROGRAM_3D_FETCH_LOD;
            break;
        default:
            DE_ASSERT(false);
        }
    }
    else if (params.texType == TEXTURETYPE_2D_ARRAY)
    {
        DE_ASSERT(!useBias); // \todo [2012-02-17 pyry] Support bias.

        numComps = 3;

        switch (params.samplerType)
        {
        case SAMPLERTYPE_FLOAT:
            progSpec = PROGRAM_2D_ARRAY_FLOAT;
            break;
        case SAMPLERTYPE_INT:
            progSpec = PROGRAM_2D_ARRAY_INT;
            break;
        case SAMPLERTYPE_UINT:
            progSpec = PROGRAM_2D_ARRAY_UINT;
            break;
        case SAMPLERTYPE_SHADOW:
            progSpec = PROGRAM_2D_ARRAY_SHADOW;
            break;
        default:
            DE_ASSERT(false);
        }
    }
    else if (params.texType == TEXTURETYPE_CUBE_ARRAY)
    {
        DE_ASSERT(!useBias);

        numComps = 4;

        switch (params.samplerType)
        {
        case SAMPLERTYPE_FLOAT:
            progSpec = PROGRAM_CUBE_ARRAY_FLOAT;
            break;
        case SAMPLERTYPE_INT:
            progSpec = PROGRAM_CUBE_ARRAY_INT;
            break;
        case SAMPLERTYPE_UINT:
            progSpec = PROGRAM_CUBE_ARRAY_UINT;
            break;
        case SAMPLERTYPE_SHADOW:
            progSpec = PROGRAM_CUBE_ARRAY_SHADOW;
            break;
        default:
            DE_ASSERT(false);
        }
    }
    else if (params.texType == TEXTURETYPE_1D_ARRAY)
    {
        DE_ASSERT(!useBias); // \todo [2012-02-17 pyry] Support bias.

        numComps = 2;

        switch (params.samplerType)
        {
        case SAMPLERTYPE_FLOAT:
            progSpec = PROGRAM_1D_ARRAY_FLOAT;
            break;
        case SAMPLERTYPE_INT:
            progSpec = PROGRAM_1D_ARRAY_INT;
            break;
        case SAMPLERTYPE_UINT:
            progSpec = PROGRAM_1D_ARRAY_UINT;
            break;
        case SAMPLERTYPE_SHADOW:
            progSpec = PROGRAM_1D_ARRAY_SHADOW;
            break;
        default:
            DE_ASSERT(false);
        }
    }
    else if (params.texType == TEXTURETYPE_BUFFER)
    {
        numComps = 1;

        switch (params.samplerType)
        {
        case SAMPLERTYPE_FETCH_FLOAT:
            progSpec = PROGRAM_BUFFER_FLOAT;
            break;
        case SAMPLERTYPE_FETCH_INT:
            progSpec = PROGRAM_BUFFER_INT;
            break;
        case SAMPLERTYPE_FETCH_UINT:
            progSpec = PROGRAM_BUFFER_UINT;
            break;
        default:
            DE_ASSERT(false);
        }
    }
    else
        DE_ASSERT(false);

    m_backend->createFrameResources(numComps, progSpec, position, texUnit, texCoord, params, maxAnisotropy);
    m_backend->recordCommands();

    // Upload uniform buffer data
    {
        const ShaderParameters shaderParameters = {
            params.bias,       // float     bias;       //!< User-supplied bias.
            params.ref,        // float     ref;        //!< Reference value for shadow lookups.
            tcu::Vec2(0.0f),   // tcu::Vec2 padding;    //!< Shader uniform padding.
            params.colorScale, // tcu::Vec4 colorScale; //!< Scale for texture color values.
            params.colorBias,  // tcu::Vec4 colorBias;  //!< Bias for texture color values.
            params
                .lodTexelFetch // int       lod         //!< Lod (for usage in Integer Texel Coord tests for VK_EXT_image_view_min_lod)
        };
        m_backend->uploadUniforms(shaderParameters);

        if (logUniforms)
            m_log << TestLog::Message << "u_sampler = " << texUnit << TestLog::EndMessage;

        if (useBias)
        {
            if (logUniforms)
                m_log << TestLog::Message << "u_bias = " << shaderParameters.bias << TestLog::EndMessage;
        }

        if (params.samplerType == SAMPLERTYPE_SHADOW)
        {
            if (logUniforms)
                m_log << TestLog::Message << "u_ref = " << shaderParameters.ref << TestLog::EndMessage;
        }

        if (logUniforms)
        {
            m_log << TestLog::Message << "u_colorScale = " << shaderParameters.colorScale << TestLog::EndMessage;
            m_log << TestLog::Message << "u_colorBias = " << shaderParameters.colorBias << TestLog::EndMessage;
        }

        if (imageViewMinLodIntegerTexelCoord)
        {
            if (logUniforms)
            {
                m_log << TestLog::Message << "u_lod = " << shaderParameters.lod << TestLog::EndMessage;
            }
        }
    }
    m_backend->submit();
    m_backend->getResult(result);
}

/*--------------------------------------------------------------------*//*!
 * \brief Map Vulkan sampler parameters to tcu::Sampler.
 *
 * If no mapping is found, throws tcu::InternalError.
 *
 * \param wrapU            U-component wrap mode
 * \param wrapV            V-component wrap mode
 * \param wrapW            W-component wrap mode
 * \param minFilterMode    Minification filter mode
 * \param magFilterMode    Magnification filter mode
 * \return Sampler description.
 *//*--------------------------------------------------------------------*/
tcu::Sampler createSampler(tcu::Sampler::WrapMode wrapU, tcu::Sampler::WrapMode wrapV, tcu::Sampler::WrapMode wrapW,
                           tcu::Sampler::FilterMode minFilterMode, tcu::Sampler::FilterMode magFilterMode,
                           bool normalizedCoords)
{
    return tcu::Sampler(wrapU, wrapV, wrapW, minFilterMode, magFilterMode, 0.0f /* lod threshold */,
                        normalizedCoords /* normalized coords */, tcu::Sampler::COMPAREMODE_NONE /* no compare */,
                        0 /* compare channel */, tcu::Vec4(0.0f) /* border color, not used */,
                        true /* seamless cube map */);
}

/*--------------------------------------------------------------------*//*!
 * \brief Map Vulkan sampler parameters to tcu::Sampler.
 *
 * If no mapping is found, throws tcu::InternalError.
 *
 * \param wrapU            U-component wrap mode
 * \param wrapV            V-component wrap mode
 * \param minFilterMode    Minification filter mode
 * \param minFilterMode    Magnification filter mode
 * \return Sampler description.
 *//*--------------------------------------------------------------------*/
tcu::Sampler createSampler(tcu::Sampler::WrapMode wrapU, tcu::Sampler::WrapMode wrapV,
                           tcu::Sampler::FilterMode minFilterMode, tcu::Sampler::FilterMode magFilterMode,
                           bool normalizedCoords)
{
    return createSampler(wrapU, wrapV, wrapU, minFilterMode, magFilterMode, normalizedCoords);
}

/*--------------------------------------------------------------------*//*!
 * \brief Map Vulkan sampler parameters to tcu::Sampler.
 *
 * If no mapping is found, throws tcu::InternalError.
 *
 * \param wrapU            U-component wrap mode
 * \param minFilterMode    Minification filter mode
 * \return Sampler description.
 *//*--------------------------------------------------------------------*/
tcu::Sampler createSampler(tcu::Sampler::WrapMode wrapU, tcu::Sampler::FilterMode minFilterMode,
                           tcu::Sampler::FilterMode magFilterMode, bool normalizedCoords)
{
    return createSampler(wrapU, wrapU, wrapU, minFilterMode, magFilterMode, normalizedCoords);
}

TestTexture2DSp loadTexture2D(const tcu::Archive &archive, const std::vector<std::string> &filenames)
{
    DE_ASSERT(filenames.size() > 0);

    TestTexture2DSp texture;

    std::string ext = de::FilePath(filenames[0]).getFileExtension();

    if (ext == "png")
    {

        for (size_t fileIndex = 0; fileIndex < filenames.size(); ++fileIndex)
        {
            tcu::TextureLevel level;

            tcu::ImageIO::loadImage(level, archive, filenames[fileIndex].c_str());

            TCU_CHECK_INTERNAL(
                level.getFormat() == tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8) ||
                level.getFormat() == tcu::TextureFormat(tcu::TextureFormat::RGB, tcu::TextureFormat::UNORM_INT8));

            if (fileIndex == 0)
                texture = TestTexture2DSp(new pipeline::TestTexture2D(
                    tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8), level.getWidth(),
                    level.getHeight()));

            tcu::copy(texture->getLevel((int)fileIndex, 0), level.getAccess());
        }
    }
    else if (ext == "pkm")
    {

        for (size_t fileIndex = 0; fileIndex < filenames.size(); ++fileIndex)
        {
            // Compressed texture.
            tcu::CompressedTexture level;

            tcu::ImageIO::loadPKM(level, archive, filenames[fileIndex].c_str());

            tcu::TextureFormat uncompressedFormat = tcu::getUncompressedFormat(level.getFormat());
            std::vector<uint8_t> uncompressedData(
                uncompressedFormat.getPixelSize() * level.getWidth() * level.getHeight(), 0);
            tcu::PixelBufferAccess decompressedBuffer(uncompressedFormat, level.getWidth(), level.getHeight(), 1,
                                                      uncompressedData.data());

            tcu::TextureFormat commonFormat =
                tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8);
            std::vector<uint8_t> commonFromatData(commonFormat.getPixelSize() * level.getWidth() * level.getHeight(),
                                                  0);
            tcu::PixelBufferAccess commonFormatBuffer(commonFormat, level.getWidth(), level.getHeight(), 1,
                                                      commonFromatData.data());

            if (fileIndex == 0)
                texture =
                    TestTexture2DSp(new pipeline::TestTexture2D(commonFormat, level.getWidth(), level.getHeight()));

            level.decompress(decompressedBuffer,
                             tcu::TexDecompressionParams(tcu::TexDecompressionParams::ASTCMODE_LDR));

            tcu::copy(commonFormatBuffer, decompressedBuffer);
            tcu::copy(texture->getLevel((int)fileIndex, 0), commonFormatBuffer);
        }
    }
    else
        TCU_FAIL("Unsupported file format");

    return texture;
}

TestTextureCubeSp loadTextureCube(const tcu::Archive &archive, const std::vector<std::string> &filenames)
{
    DE_ASSERT(filenames.size() > 0);
    DE_STATIC_ASSERT(tcu::CUBEFACE_LAST == 6);
    TCU_CHECK((int)filenames.size() % tcu::CUBEFACE_LAST == 0);

    TestTextureCubeSp texture;

    std::string ext = de::FilePath(filenames[0]).getFileExtension();

    if (ext == "png")
    {

        for (size_t fileIndex = 0; fileIndex < filenames.size(); ++fileIndex)
        {
            tcu::TextureLevel level;

            tcu::ImageIO::loadImage(level, archive, filenames[fileIndex].c_str());

            TCU_CHECK_INTERNAL(
                level.getFormat() == tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8) ||
                level.getFormat() == tcu::TextureFormat(tcu::TextureFormat::RGB, tcu::TextureFormat::UNORM_INT8));

            TCU_CHECK(level.getWidth() == level.getHeight());

            if (fileIndex == 0)
                texture = TestTextureCubeSp(new pipeline::TestTextureCube(
                    tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8), level.getWidth()));

            tcu::copy(texture->getLevel((int)fileIndex / 6, (int)fileIndex % 6), level.getAccess());
        }
    }
    else if (ext == "pkm")
    {
        for (size_t fileIndex = 0; fileIndex < filenames.size(); ++fileIndex)
        {
            // Compressed texture.
            tcu::CompressedTexture level;

            tcu::ImageIO::loadPKM(level, archive, filenames[fileIndex].c_str());

            TCU_CHECK(level.getWidth() == level.getHeight());

            tcu::TextureFormat uncompressedFormat = tcu::getUncompressedFormat(level.getFormat());
            std::vector<uint8_t> uncompressedData(
                uncompressedFormat.getPixelSize() * level.getWidth() * level.getHeight(), 0);
            tcu::PixelBufferAccess decompressedBuffer(uncompressedFormat, level.getWidth(), level.getHeight(), 1,
                                                      uncompressedData.data());

            tcu::TextureFormat commonFormat =
                tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8);
            std::vector<uint8_t> commonFromatData(commonFormat.getPixelSize() * level.getWidth() * level.getHeight(),
                                                  0);
            tcu::PixelBufferAccess commonFormatBuffer(commonFormat, level.getWidth(), level.getHeight(), 1,
                                                      commonFromatData.data());

            if (fileIndex == 0)
                texture = TestTextureCubeSp(new pipeline::TestTextureCube(commonFormat, level.getWidth()));

            level.decompress(decompressedBuffer,
                             tcu::TexDecompressionParams(tcu::TexDecompressionParams::ASTCMODE_LDR));

            tcu::copy(commonFormatBuffer, decompressedBuffer);
            tcu::copy(texture->getLevel((int)fileIndex / 6, (int)fileIndex % 6), commonFormatBuffer);
        }
    }
    else
        TCU_FAIL("Unsupported file format");

    return texture;
}

TextureCommonTestCaseParameters::TextureCommonTestCaseParameters(void)
    : sampleCount(VK_SAMPLE_COUNT_1_BIT)
    , texCoordPrecision(glu::PRECISION_HIGHP)
    , minFilter(tcu::Sampler::LINEAR)
    , magFilter(tcu::Sampler::LINEAR)
    , wrapS(tcu::Sampler::REPEAT_GL)
    , format(VK_FORMAT_R8G8B8A8_UNORM)
    , unnormal(false)
    , aspectMask(VK_IMAGE_ASPECT_FLAG_BITS_MAX_ENUM)
    , testType(TEST_NORMAL)
{
}

Texture2DTestCaseParameters::Texture2DTestCaseParameters(void)
    : wrapT(tcu::Sampler::REPEAT_GL)
    , width(64)
    , height(64)
    , mipmaps(false)
{
}

TextureCubeTestCaseParameters::TextureCubeTestCaseParameters(void)
    : wrapT(tcu::Sampler::REPEAT_GL)
    , size(64)
    , seamless(true)
{
}

Texture2DArrayTestCaseParameters::Texture2DArrayTestCaseParameters(void) : wrapT(tcu::Sampler::REPEAT_GL), numLayers(8)
{
}

Texture3DTestCaseParameters::Texture3DTestCaseParameters(void) : wrapR(tcu::Sampler::REPEAT_GL), depth(64)
{
}

Texture1DTestCaseParameters::Texture1DTestCaseParameters(void) : width(64)
{
}

Texture1DArrayTestCaseParameters::Texture1DArrayTestCaseParameters(void) : numLayers(8)
{
}

TextureCubeArrayTestCaseParameters::TextureCubeArrayTestCaseParameters(void) : numLayers(8)
{
}

TextureCubeFilteringTestCaseParameters::TextureCubeFilteringTestCaseParameters(void) : onlySampleFaceInterior(false)
{
}

RenderBackend::RenderBackend(Context &context, vk::VkSampleCountFlagBits sampleCount, uint32_t renderWidth,
                             uint32_t renderHeight, uint32_t renderDepth, vk::VkComponentMapping componentMapping,
                             vk::VkImageType imageType, vk::VkImageViewType imageViewType, vk::VkFormat imageFormat,
                             bool requireRobustness2, bool requireImageViewMinLod, bool useCompute)
    : m_context(context)
    , m_log(context.getTestContext().getLog())
    , m_renderWidth(renderWidth)
    , m_renderHeight(renderHeight)
    , m_renderDepth(renderDepth)
    , m_sampleCount(sampleCount)
    , m_multisampling(m_sampleCount != VK_SAMPLE_COUNT_1_BIT)
    , m_imageFormat(imageFormat)
    , m_textureFormat(vk::mapVkFormat(m_imageFormat))
    , m_uniformBufferSize(sizeof(ShaderParameters))
    , m_resultBufferSize(renderWidth * renderHeight * m_textureFormat.getPixelSize())
    , m_viewportOffsetX(0.0f)
    , m_viewportOffsetY(0.0f)
    , m_viewportWidth((float)renderWidth)
    , m_viewportHeight((float)renderHeight)
    , m_componentMapping(componentMapping)
    , m_requireRobustness2(requireRobustness2)
    , m_requireImageViewMinLod(requireImageViewMinLod)
    , m_useCompute(useCompute)
{

    const DeviceInterface &vkd      = m_context.getDeviceInterface();
    const uint32_t queueFamilyIndex = getQueueNdx(m_context, m_useCompute);

    if (m_requireRobustness2 || m_requireImageViewMinLod)
    {
        // Note we are already checking the needed features are available in checkSupport().
        VkPhysicalDeviceRobustness2FeaturesEXT robustness2Features = initVulkanStructure();
        VkPhysicalDeviceFeatures2 features2                        = initVulkanStructure(&robustness2Features);
#ifndef CTS_USES_VULKANSC
        VkPhysicalDeviceImageViewMinLodFeaturesEXT imageViewMinLodFeatures = initVulkanStructure();
        if (m_requireImageViewMinLod)
        {
            DE_ASSERT(context.isDeviceFunctionalitySupported("VK_EXT_image_view_min_lod"));
            imageViewMinLodFeatures.minLod = true;
            if (m_requireRobustness2)
            {
                robustness2Features.pNext = &imageViewMinLodFeatures;
            }
            else
            {
                features2.pNext = &imageViewMinLodFeatures;
            }
        }
#endif
        if (m_requireRobustness2)
        {
            DE_ASSERT(context.isDeviceFunctionalitySupported("VK_KHR_robustness2") ||
                      context.isDeviceFunctionalitySupported("VK_EXT_robustness2"));
            robustness2Features.robustImageAccess2 = true;
        }

        context.getInstanceInterface().getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &features2);
        m_customDevice = createRobustBufferAccessDevice(context, &features2);
    }

    const VkDevice vkDevice = getDevice();
    m_allocator             = de::MovePtr<Allocator>(new SimpleAllocator(
        vkd, vkDevice,
        getPhysicalDeviceMemoryProperties(m_context.getInstanceInterface(), m_context.getPhysicalDevice())));

    // Command Pool
    m_commandPool = createCommandPool(vkd, vkDevice, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queueFamilyIndex);

    // Create Command Buffer
    m_commandBuffer = allocateCommandBuffer(vkd, vkDevice, *m_commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    if (m_useCompute)
    {
        if (m_multisampling)
            TCU_THROW(NotSupportedError, "Multisampling is not supported in compute-only texture renderer");

        if (isDepthStencilFormat(m_imageFormat))
            TCU_THROW(NotSupportedError, "Depth/stencil formats are not supported in compute-only texture renderer");
    }

    // Image
    {
        VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        if (m_useCompute)
            imageUsage |= VK_IMAGE_USAGE_STORAGE_BIT;
        else
            imageUsage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        VkImageFormatProperties properties;

        if ((m_context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(
                 m_context.getPhysicalDevice(), m_imageFormat, imageType, VK_IMAGE_TILING_OPTIMAL, imageUsage, 0,
                 &properties) == VK_ERROR_FORMAT_NOT_SUPPORTED))
        {
            TCU_THROW(NotSupportedError, "Format not supported");
        }

        if ((properties.sampleCounts & m_sampleCount) != m_sampleCount)
        {
            TCU_THROW(NotSupportedError, "Format not supported");
        }

        const VkImageCreateInfo imageCreateInfo = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,            // VkStructureType sType;
            nullptr,                                        // const void* pNext;
            0u,                                             // VkImageCreateFlags flags;
            imageType,                                      // VkImageType imageType;
            m_imageFormat,                                  // VkFormat format;
            {m_renderWidth, m_renderHeight, m_renderDepth}, // VkExtent3D extent;
            1u,                                             // uint32_t mipLevels;
            1u,                                             // uint32_t arrayLayers;
            m_sampleCount,                                  // VkSampleCountFlagBits samples;
            VK_IMAGE_TILING_OPTIMAL,                        // VkImageTiling tiling;
            imageUsage,                                     // VkImageUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,                      // VkSharingMode sharingMode;
            1u,                                             // uint32_t queueFamilyIndexCount;
            &queueFamilyIndex,                              // const uint32_t* pQueueFamilyIndices;
            VK_IMAGE_LAYOUT_UNDEFINED                       // VkImageLayout initialLayout;
        };

        m_image = vk::createImage(vkd, vkDevice, &imageCreateInfo, nullptr);

        m_imageMemory =
            m_allocator->allocate(getImageMemoryRequirements(vkd, vkDevice, *m_image), MemoryRequirement::Any);
        VK_CHECK(vkd.bindImageMemory(vkDevice, *m_image, m_imageMemory->getMemory(), m_imageMemory->getOffset()));
    }

    // Image View
    {
        const VkImageViewCreateInfo imageViewCreateInfo = {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType sType;
            nullptr,                                  // const void* pNext;
            0u,                                       // VkImageViewCreateFlags flags;
            *m_image,                                 // VkImage image;
            imageViewType,                            // VkImageViewType viewType;
            m_imageFormat,                            // VkFormat format;
            makeComponentMappingRGBA(),               // VkComponentMapping components;
            {
                VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                0u,                        // uint32_t baseMipLevel;
                1u,                        // uint32_t mipLevels;
                0u,                        // uint32_t baseArrayLayer;
                1u,                        // uint32_t arraySize;
            },                             // VkImageSubresourceRange subresourceRange;
        };

        m_imageView = vk::createImageView(vkd, vkDevice, &imageViewCreateInfo, nullptr);
    }

    if (m_multisampling)
    {
        {
            // Resolved Image
            const VkImageUsageFlags imageUsage =
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            VkImageFormatProperties properties;

            if ((m_context.getInstanceInterface().getPhysicalDeviceImageFormatProperties(
                     m_context.getPhysicalDevice(), m_imageFormat, imageType, VK_IMAGE_TILING_OPTIMAL, imageUsage, 0,
                     &properties) == VK_ERROR_FORMAT_NOT_SUPPORTED))
            {
                TCU_THROW(NotSupportedError, "Format not supported");
            }

            const VkImageCreateInfo imageCreateInfo = {
                VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,            // VkStructureType sType;
                nullptr,                                        // const void* pNext;
                0u,                                             // VkImageCreateFlags flags;
                imageType,                                      // VkImageType imageType;
                m_imageFormat,                                  // VkFormat format;
                {m_renderWidth, m_renderHeight, m_renderDepth}, // VkExtent3D extent;
                1u,                                             // uint32_t mipLevels;
                1u,                                             // uint32_t arrayLayers;
                VK_SAMPLE_COUNT_1_BIT,                          // VkSampleCountFlagBits samples;
                VK_IMAGE_TILING_OPTIMAL,                        // VkImageTiling tiling;
                imageUsage,                                     // VkImageUsageFlags usage;
                VK_SHARING_MODE_EXCLUSIVE,                      // VkSharingMode sharingMode;
                1u,                                             // uint32_t queueFamilyIndexCount;
                &queueFamilyIndex,                              // const uint32_t* pQueueFamilyIndices;
                VK_IMAGE_LAYOUT_UNDEFINED                       // VkImageLayout initialLayout;
            };

            m_resolvedImage       = vk::createImage(vkd, vkDevice, &imageCreateInfo, nullptr);
            m_resolvedImageMemory = m_allocator->allocate(getImageMemoryRequirements(vkd, vkDevice, *m_resolvedImage),
                                                          MemoryRequirement::Any);
            VK_CHECK(vkd.bindImageMemory(vkDevice, *m_resolvedImage, m_resolvedImageMemory->getMemory(),
                                         m_resolvedImageMemory->getOffset()));
        }

        // Resolved Image View
        {
            const VkImageViewCreateInfo imageViewCreateInfo = {
                VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, // VkStructureType sType;
                nullptr,                                  // const void* pNext;
                0u,                                       // VkImageViewCreateFlags flags;
                *m_resolvedImage,                         // VkImage image;
                imageViewType,                            // VkImageViewType viewType;
                m_imageFormat,                            // VkFormat format;
                makeComponentMappingRGBA(),               // VkComponentMapping components;
                {
                    VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
                    0u,                        // uint32_t baseMipLevel;
                    1u,                        // uint32_t mipLevels;
                    0u,                        // uint32_t baseArrayLayer;
                    1u,                        // uint32_t arraySize;
                },                             // VkImageSubresourceRange subresourceRange;
            };

            m_resolvedImageView = vk::createImageView(vkd, vkDevice, &imageViewCreateInfo, nullptr);
        }
    }

    // Uniform Buffer
    {
        const VkBufferCreateInfo bufferCreateInfo = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
            nullptr,                              // const void* pNext;
            0u,                                   // VkBufferCreateFlags flags;
            m_uniformBufferSize,                  // VkDeviceSize size;
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,   // VkBufferUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
            1u,                                   // uint32_t queueFamilyIndexCount;
            &queueFamilyIndex                     // const uint32_t* pQueueFamilyIndices;
        };

        m_uniformBuffer       = createBuffer(vkd, vkDevice, &bufferCreateInfo);
        m_uniformBufferMemory = m_allocator->allocate(getBufferMemoryRequirements(vkd, vkDevice, *m_uniformBuffer),
                                                      MemoryRequirement::HostVisible);

        VK_CHECK(vkd.bindBufferMemory(vkDevice, *m_uniformBuffer, m_uniformBufferMemory->getMemory(),
                                      m_uniformBufferMemory->getOffset()));
    }

    // Result Buffer
    {
        const VkBufferCreateInfo bufferCreateInfo = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
            nullptr,                              // const void* pNext;
            0u,                                   // VkBufferCreateFlags flags;
            m_resultBufferSize,                   // VkDeviceSize size;
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,     // VkBufferUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
            1u,                                   // uint32_t queueFamilyIndexCount;
            &queueFamilyIndex                     // const uint32_t* pQueueFamilyIndices;
        };

        m_resultBuffer       = createBuffer(vkd, vkDevice, &bufferCreateInfo);
        m_resultBufferMemory = m_allocator->allocate(getBufferMemoryRequirements(vkd, vkDevice, *m_resultBuffer),
                                                     MemoryRequirement::HostVisible);

        VK_CHECK(vkd.bindBufferMemory(vkDevice, *m_resultBuffer, m_resultBufferMemory->getMemory(),
                                      m_resultBufferMemory->getOffset()));
    }
}

void RenderBackend::getResult(const tcu::PixelBufferAccess &result)
{
    tcu::copy(result, tcu::ConstPixelBufferAccess(m_textureFormat, tcu::IVec3(m_renderWidth, m_renderHeight, 1u),
                                                  m_resultBufferMemory->getHostPtr()));
}

void RenderBackend::add2DTexture(const TestTexture2DSp &texture, const vk::VkImageAspectFlags &aspectMask,
                                 TextureBinding::ImageBackingMode backingMode)
{
    m_textureBindings.push_back(
        TextureBindingSp(new TextureBinding(m_context, getDevice(), *m_allocator, texture, TextureBinding::TYPE_2D,
                                            aspectMask, backingMode, m_componentMapping, m_useCompute)));
}

void RenderBackend::addCubeTexture(const TestTextureCubeSp &texture, const vk::VkImageAspectFlags &aspectMask,
                                   TextureBinding::ImageBackingMode backingMode)
{
    m_textureBindings.push_back(TextureBindingSp(new TextureBinding(m_context, getDevice(), *m_allocator, texture,
                                                                    TextureBinding::TYPE_CUBE_MAP, aspectMask,
                                                                    backingMode, m_componentMapping, m_useCompute)));
}

void RenderBackend::add2DArrayTexture(const TestTexture2DArraySp &texture, const vk::VkImageAspectFlags &aspectMask,
                                      TextureBinding::ImageBackingMode backingMode)
{
    m_textureBindings.push_back(TextureBindingSp(new TextureBinding(m_context, getDevice(), *m_allocator, texture,
                                                                    TextureBinding::TYPE_2D_ARRAY, aspectMask,
                                                                    backingMode, m_componentMapping, m_useCompute)));
}

void RenderBackend::add3DTexture(const TestTexture3DSp &texture, const vk::VkImageAspectFlags &aspectMask,
                                 TextureBinding::ImageBackingMode backingMode)
{
    m_textureBindings.push_back(
        TextureBindingSp(new TextureBinding(m_context, getDevice(), *m_allocator, texture, TextureBinding::TYPE_3D,
                                            aspectMask, backingMode, m_componentMapping, m_useCompute)));
}

void RenderBackend::add1DTexture(const TestTexture1DSp &texture, const vk::VkImageAspectFlags &aspectMask,
                                 TextureBinding::ImageBackingMode backingMode)
{
    m_textureBindings.push_back(
        TextureBindingSp(new TextureBinding(m_context, getDevice(), *m_allocator, texture, TextureBinding::TYPE_1D,
                                            aspectMask, backingMode, m_componentMapping, m_useCompute)));
}

void RenderBackend::add1DArrayTexture(const TestTexture1DArraySp &texture, const vk::VkImageAspectFlags &aspectMask,
                                      TextureBinding::ImageBackingMode backingMode)
{
    m_textureBindings.push_back(TextureBindingSp(new TextureBinding(m_context, getDevice(), *m_allocator, texture,
                                                                    TextureBinding::TYPE_1D_ARRAY, aspectMask,
                                                                    backingMode, m_componentMapping, m_useCompute)));
}

void RenderBackend::addCubeArrayTexture(const TestTextureCubeArraySp &texture, const vk::VkImageAspectFlags &aspectMask,
                                        TextureBinding::ImageBackingMode backingMode)
{
    m_textureBindings.push_back(TextureBindingSp(new TextureBinding(m_context, getDevice(), *m_allocator, texture,
                                                                    TextureBinding::TYPE_CUBE_ARRAY, aspectMask,
                                                                    backingMode, m_componentMapping, m_useCompute)));
}

const pipeline::TestTexture2D &RenderBackend::get2DTexture(int textureIndex) const
{
    DE_ASSERT(m_textureBindings.size() > (size_t)textureIndex);
    DE_ASSERT(m_textureBindings[textureIndex]->getType() == TextureBinding::TYPE_2D);

    return dynamic_cast<const pipeline::TestTexture2D &>(m_textureBindings[textureIndex]->getTestTexture());
}

const pipeline::TestTextureCube &RenderBackend::getCubeTexture(int textureIndex) const
{
    DE_ASSERT(m_textureBindings.size() > (size_t)textureIndex);
    DE_ASSERT(m_textureBindings[textureIndex]->getType() == TextureBinding::TYPE_CUBE_MAP);

    return dynamic_cast<const pipeline::TestTextureCube &>(m_textureBindings[textureIndex]->getTestTexture());
}

const pipeline::TestTexture2DArray &RenderBackend::get2DArrayTexture(int textureIndex) const
{
    DE_ASSERT(m_textureBindings.size() > (size_t)textureIndex);
    DE_ASSERT(m_textureBindings[textureIndex]->getType() == TextureBinding::TYPE_2D_ARRAY);

    return dynamic_cast<const pipeline::TestTexture2DArray &>(m_textureBindings[textureIndex]->getTestTexture());
}

const pipeline::TestTexture3D &RenderBackend::get3DTexture(int textureIndex) const
{
    DE_ASSERT(m_textureBindings.size() > (size_t)textureIndex);
    DE_ASSERT(m_textureBindings[textureIndex]->getType() == TextureBinding::TYPE_3D);

    return dynamic_cast<const pipeline::TestTexture3D &>(m_textureBindings[textureIndex]->getTestTexture());
}

const pipeline::TestTexture1D &RenderBackend::get1DTexture(int textureIndex) const
{
    DE_ASSERT(m_textureBindings.size() > (size_t)textureIndex);
    DE_ASSERT(m_textureBindings[textureIndex]->getType() == TextureBinding::TYPE_1D);

    return dynamic_cast<const pipeline::TestTexture1D &>(m_textureBindings[textureIndex]->getTestTexture());
}

const pipeline::TestTexture1DArray &RenderBackend::get1DArrayTexture(int textureIndex) const
{
    DE_ASSERT(m_textureBindings.size() > (size_t)textureIndex);
    DE_ASSERT(m_textureBindings[textureIndex]->getType() == TextureBinding::TYPE_1D_ARRAY);

    return dynamic_cast<const pipeline::TestTexture1DArray &>(m_textureBindings[textureIndex]->getTestTexture());
}

const pipeline::TestTextureCubeArray &RenderBackend::getCubeArrayTexture(int textureIndex) const
{
    DE_ASSERT(m_textureBindings.size() > (size_t)textureIndex);
    DE_ASSERT(m_textureBindings[textureIndex]->getType() == TextureBinding::TYPE_CUBE_ARRAY);

    return dynamic_cast<const pipeline::TestTextureCubeArray &>(m_textureBindings[textureIndex]->getTestTexture());
}

void RenderBackend::setViewport(float viewportX, float viewportY, float viewportW, float viewportH)
{
    m_viewportHeight  = viewportH;
    m_viewportWidth   = viewportW;
    m_viewportOffsetX = viewportX;
    m_viewportOffsetY = viewportY;
}

TextureBinding *RenderBackend::getTextureBinding(int textureIndex) const
{
    DE_ASSERT(m_textureBindings.size() > (size_t)textureIndex);
    return m_textureBindings[textureIndex].get();
}

uint32_t RenderBackend::getRenderWidth(void) const
{
    return m_renderWidth;
}

uint32_t RenderBackend::getRenderHeight(void) const
{
    return m_renderHeight;
}

vk::VkDevice RenderBackend::getDevice(void) const
{
    if ((m_requireRobustness2 || m_requireImageViewMinLod) && m_useCompute &&
        (m_context.getComputeQueueFamilyIndex() == -1))
        TCU_THROW(NotSupportedError, "Exclusive compute queue not supported.");

    return (m_requireRobustness2 || m_requireImageViewMinLod) ? *m_customDevice : m_context.getDevice();
}

vk::Move<vk::VkDescriptorSet> RenderBackend::makeDescriptorSet(const vk::VkDescriptorPool descriptorPool,
                                                               const vk::VkDescriptorSetLayout setLayout) const
{
    const DeviceInterface &vkd = m_context.getDeviceInterface();
    const VkDevice vkDevice    = getDevice();

    const VkDescriptorSetAllocateInfo allocateParams = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, // VkStructureType                    sType
        nullptr,                                        // const void*                        pNext
        descriptorPool,                                 // VkDescriptorPool                    descriptorPool
        1u,                                             // uint32_t                            descriptorSetCount
        &setLayout,                                     // const VkDescriptorSetLayout*        pSetLayouts
    };
    return allocateDescriptorSet(vkd, vkDevice, &allocateParams);
}

void RenderBackend::addImageTransitionBarrier(vk::VkCommandBuffer commandBuffer, vk::VkImage image,
                                              vk::VkPipelineStageFlags srcStageMask,
                                              vk::VkPipelineStageFlags dstStageMask, vk::VkAccessFlags srcAccessMask,
                                              vk::VkAccessFlags dstAccessMask, vk::VkImageLayout oldLayout,
                                              vk::VkImageLayout newLayout) const
{
    const DeviceInterface &vkd = m_context.getDeviceInterface();

    const VkImageSubresourceRange subResourcerange = {
        VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags aspectMask;
        0,                         // uint32_t baseMipLevel;
        1,                         // uint32_t levelCount;
        0,                         // uint32_t baseArrayLayer;
        1                          // uint32_t layerCount;
    };

    const VkImageMemoryBarrier imageBarrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType sType;
        nullptr,                                // const void* pNext;
        srcAccessMask,                          // VkAccessFlags srcAccessMask;
        dstAccessMask,                          // VkAccessFlags dstAccessMask;
        oldLayout,                              // VkImageLayout oldLayout;
        newLayout,                              // VkImageLayout newLayout;
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t srcQueueFamilyIndex;
        VK_QUEUE_FAMILY_IGNORED,                // uint32_t destQueueFamilyIndex;
        image,                                  // VkImage image;
        subResourcerange                        // VkImageSubresourceRange subresourceRange;
    };

    vkd.cmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier);
}

GraphicsBackend::GraphicsBackend(Context &context, vk::VkSampleCountFlagBits sampleCount, uint32_t renderWidth,
                                 uint32_t renderHeight, uint32_t renderDepth, vk::VkComponentMapping componentMapping,
                                 vk::VkImageType imageType, vk::VkImageViewType imageViewType, vk::VkFormat imageFormat,
                                 bool requireRobustness2, bool requireImageViewMinLod)
    : RenderBackend(context, sampleCount, renderWidth, renderHeight, renderDepth, componentMapping, imageType,
                    imageViewType, imageFormat, requireRobustness2, requireImageViewMinLod)
{
    const DeviceInterface &vkd      = m_context.getDeviceInterface();
    const VkDevice vkDevice         = getDevice();
    const uint32_t queueFamilyIndex = getQueueNdx(m_context, false);

    // DescriptorPool
    {
        DescriptorPoolBuilder descriptorPoolBuilder;

        descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        m_descriptorPool =
            descriptorPoolBuilder.build(vkd, vkDevice, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 2u);
    }

    // Descriptor Sets
    {
        m_descriptorSetLayout[0] =
            DescriptorSetLayoutBuilder()
                .addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
                .build(vkd, vkDevice);

        m_descriptorSetLayout[1] =
            DescriptorSetLayoutBuilder()
                .addSingleBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                .build(vkd, vkDevice);

        m_descriptorSet[0] = makeDescriptorSet(*m_descriptorPool, *m_descriptorSetLayout[0]);
        m_descriptorSet[1] = makeDescriptorSet(*m_descriptorPool, *m_descriptorSetLayout[1]);
    }

    // Pipeline Layout
    {
        VkDescriptorSetLayout descriptorSetLayouts[2] = {*m_descriptorSetLayout[0], *m_descriptorSetLayout[1]};

        const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType sType;
            nullptr,                                       // const void* pNext;
            0u,                                            // VkPipelineLayoutCreateFlags flags;
            2u,                                            // uint32_t descriptorSetCount;
            descriptorSetLayouts,                          // const VkDescriptorSetLayout* pSetLayouts;
            0u,                                            // uint32_t pushConstantRangeCount;
            nullptr                                        // const VkPushConstantRange* pPushConstantRanges;
        };

        m_pipelineLayout = createPipelineLayout(vkd, vkDevice, &pipelineLayoutCreateInfo);
    }

    // Render Pass
    {
        const VkImageLayout imageLayout                = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        const VkAttachmentDescription attachmentDesc[] = {
            {
                0u,                               // VkAttachmentDescriptionFlags flags;
                m_imageFormat,                    // VkFormat format;
                m_sampleCount,                    // VkSampleCountFlagBits samples;
                VK_ATTACHMENT_LOAD_OP_LOAD,       // VkAttachmentLoadOp loadOp;
                VK_ATTACHMENT_STORE_OP_STORE,     // VkAttachmentStoreOp storeOp;
                VK_ATTACHMENT_LOAD_OP_DONT_CARE,  // VkAttachmentLoadOp stencilLoadOp;
                VK_ATTACHMENT_STORE_OP_DONT_CARE, // VkAttachmentStoreOp stencilStoreOp;
                imageLayout,                      // VkImageLayout initialLayout;
                imageLayout,                      // VkImageLayout finalLayout;
            },
            {
                0u,                               // VkAttachmentDescriptionFlags flags;
                m_imageFormat,                    // VkFormat format;
                VK_SAMPLE_COUNT_1_BIT,            // VkSampleCountFlagBits samples;
                VK_ATTACHMENT_LOAD_OP_DONT_CARE,  // VkAttachmentLoadOp loadOp;
                VK_ATTACHMENT_STORE_OP_STORE,     // VkAttachmentStoreOp storeOp;
                VK_ATTACHMENT_LOAD_OP_DONT_CARE,  // VkAttachmentLoadOp stencilLoadOp;
                VK_ATTACHMENT_STORE_OP_DONT_CARE, // VkAttachmentStoreOp stencilStoreOp;
                imageLayout,                      // VkImageLayout initialLayout;
                imageLayout,                      // VkImageLayout finalLayout;
            }};

        const VkAttachmentReference attachmentRef = {
            0u,          // uint32_t attachment;
            imageLayout, // VkImageLayout layout;
        };

        const VkAttachmentReference resolveAttachmentRef = {
            1u,          // uint32_t attachment;
            imageLayout, // VkImageLayout layout;
        };

        const VkSubpassDescription subpassDesc = {
            0u,                                                // VkSubpassDescriptionFlags flags;
            VK_PIPELINE_BIND_POINT_GRAPHICS,                   // VkPipelineBindPoint pipelineBindPoint;
            0u,                                                // uint32_t inputAttachmentCount;
            nullptr,                                           // const VkAttachmentReference* pInputAttachments;
            1u,                                                // uint32_t colorAttachmentCount;
            &attachmentRef,                                    // const VkAttachmentReference* pColorAttachments;
            m_multisampling ? &resolveAttachmentRef : nullptr, // const VkAttachmentReference* pResolveAttachments;
            nullptr,                                           // const VkAttachmentReference* pDepthStencilAttachment;
            0u,                                                // uint32_t preserveAttachmentCount;
            nullptr,                                           // const VkAttachmentReference* pPreserveAttachments;
        };

        const VkRenderPassCreateInfo renderPassCreateInfo = {
            VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, // VkStructureType sType;
            nullptr,                                   // const void* pNext;
            0u,                                        // VkRenderPassCreateFlags flags;
            m_multisampling ? 2u : 1u,                 // uint32_t attachmentCount;
            attachmentDesc,                            // const VkAttachmentDescription* pAttachments;
            1u,                                        // uint32_t subpassCount;
            &subpassDesc,                              // const VkSubpassDescription* pSubpasses;
            0u,                                        // uint32_t dependencyCount;
            nullptr,                                   // const VkSubpassDependency* pDependencies;
        };

        m_renderPass = createRenderPass(vkd, vkDevice, &renderPassCreateInfo, nullptr);
    }

    // Vertex index buffer
    {
        const VkBufferCreateInfo indexBufferParams = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
            nullptr,                              // const void* pNext;
            0u,                                   // VkBufferCreateFlags flags;
            s_vertexIndexBufferSize,              // VkDeviceSize size;
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,     // VkBufferUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
            1u,                                   // uint32_t queueFamilyCount;
            &queueFamilyIndex                     // const uint32_t* pQueueFamilyIndices;
        };

        m_vertexIndexBuffer       = createBuffer(vkd, vkDevice, &indexBufferParams);
        m_vertexIndexBufferMemory = m_allocator->allocate(
            getBufferMemoryRequirements(vkd, vkDevice, *m_vertexIndexBuffer), MemoryRequirement::HostVisible);

        VK_CHECK(vkd.bindBufferMemory(vkDevice, *m_vertexIndexBuffer, m_vertexIndexBufferMemory->getMemory(),
                                      m_vertexIndexBufferMemory->getOffset()));

        // Load vertices into vertex buffer
        deMemcpy(m_vertexIndexBufferMemory->getHostPtr(), s_vertexIndices, s_vertexIndexBufferSize);
        flushMappedMemoryRange(vkd, vkDevice, m_vertexIndexBufferMemory->getMemory(),
                               m_vertexIndexBufferMemory->getOffset(), VK_WHOLE_SIZE);
    }

    // FrameBuffer
    {
        const VkImageView attachments[] = {
            *m_imageView,
            *m_resolvedImageView,
        };

        const VkFramebufferCreateInfo framebufferCreateInfo = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, // VkStructureType sType;
            nullptr,                                   // const void* pNext;
            0u,                                        // VkFramebufferCreateFlags flags;
            *m_renderPass,                             // VkRenderPass renderPass;
            m_multisampling ? 2u : 1u,                 // uint32_t attachmentCount;
            attachments,                               // const VkImageView* pAttachments;
            m_renderWidth,                             // uint32_t width;
            m_renderHeight,                            // uint32_t height;
            1u,                                        // uint32_t layers;
        };

        m_frameBuffer = createFramebuffer(vkd, vkDevice, &framebufferCreateInfo, nullptr);
    }

    clearImage(*m_image);
    if (m_multisampling)
        clearImage(*m_resolvedImage);
}

void GraphicsBackend::createFrameResources(uint32_t numComps, Program progSpec, const float *positions, int texUnit,
                                           const float *texCoord, const glu::TextureTestUtil::ReferenceParams &params,
                                           const float maxAnisotropy)
{
    const DeviceInterface &vkd      = m_context.getDeviceInterface();
    const VkDevice vkDevice         = getDevice();
    const uint32_t queueFamilyIndex = getQueueNdx(m_context, false);

    Unique<VkShaderModule> vertexShaderModule(createShaderModule(
        vkd, vkDevice, m_context.getBinaryCollection().get("vertex_" + std::string(getProgramName(progSpec))), 0));
    Unique<VkShaderModule> fragmentShaderModule(createShaderModule(
        vkd, vkDevice, m_context.getBinaryCollection().get("fragment_" + std::string(getProgramName(progSpec))), 0));

    m_vertexBufferOffset                    = 0;
    const uint32_t vertexPositionStrideSize = uint32_t(sizeof(tcu::Vec4));
    const uint32_t vertexTextureStrideSize  = uint32_t(numComps * sizeof(float));
    const uint32_t positionDataSize         = vertexPositionStrideSize * 4u;
    const uint32_t textureCoordDataSize     = vertexTextureStrideSize * 4u;

    const VkPhysicalDeviceProperties properties = m_context.getDeviceProperties();

    if (positionDataSize > properties.limits.maxVertexInputAttributeOffset)
    {
        std::stringstream message;
        message << "Larger vertex input attribute offset is needed (" << positionDataSize
                << ") than the available maximum (" << properties.limits.maxVertexInputAttributeOffset << ").";
        TCU_THROW(NotSupportedError, message.str().c_str());
    }

    // Create Graphics Pipeline
    {
        const VkVertexInputBindingDescription vertexInputBindingDescription[2] = {
            {
                0u,                         // uint32_t binding;
                vertexPositionStrideSize,   // uint32_t strideInBytes;
                VK_VERTEX_INPUT_RATE_VERTEX // VkVertexInputStepRate stepRate;
            },
            {
                1u,                         // uint32_t binding;
                vertexTextureStrideSize,    // uint32_t strideInBytes;
                VK_VERTEX_INPUT_RATE_VERTEX // VkVertexInputStepRate stepRate;
            }};

        VkFormat textureCoordinateFormat = VK_FORMAT_R32G32B32A32_SFLOAT;

        switch (numComps)
        {
        case 1:
            textureCoordinateFormat = VK_FORMAT_R32_SFLOAT;
            break;
        case 2:
            textureCoordinateFormat = VK_FORMAT_R32G32_SFLOAT;
            break;
        case 3:
            textureCoordinateFormat = VK_FORMAT_R32G32B32_SFLOAT;
            break;
        case 4:
            textureCoordinateFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
            break;
        default:
            DE_ASSERT(false);
        }

        const VkVertexInputAttributeDescription vertexInputAttributeDescriptions[2] = {
            {
                0u,                            // uint32_t location;
                0u,                            // uint32_t binding;
                VK_FORMAT_R32G32B32A32_SFLOAT, // VkFormat format;
                0u                             // uint32_t offsetInBytes;
            },
            {
                1u,                      // uint32_t location;
                1u,                      // uint32_t binding;
                textureCoordinateFormat, // VkFormat format;
                positionDataSize         // uint32_t offsetInBytes;
            }};

        const VkPipelineVertexInputStateCreateInfo vertexInputStateParams = {
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                   // const void* pNext;
            0,                                                         // VkPipelineVertexInputStateCreateFlags flags;
            2u,                                                        // uint32_t bindingCount;
            vertexInputBindingDescription,   // const VkVertexInputBindingDescription* pVertexBindingDescriptions;
            2u,                              // uint32_t attributeCount;
            vertexInputAttributeDescriptions // const VkVertexInputAttributeDescription* pVertexAttributeDescriptions;
        };

        const VkViewport viewport = {
            m_viewportOffsetX, // float originX;
            m_viewportOffsetY, // float originY;
            m_viewportWidth,   // float width;
            m_viewportHeight,  // float height;
            0.0f,              // float minDepth;
            1.0f               // float maxDepth;
        };
        const std::vector<VkViewport> viewports(1, viewport);
        const std::vector<VkRect2D> scissors(1, makeRect2D(tcu::UVec2(m_renderWidth, m_renderHeight)));

        const VkPipelineMultisampleStateCreateInfo multisampleStateParams = {
            VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, // VkStructureType sType;
            nullptr,                                                  // const void* pNext;
            0u,                                                       // VkPipelineMultisampleStateCreateFlags flags;
            m_sampleCount,                                            // VkSampleCountFlagBits rasterizationSamples;
            VK_FALSE,                                                 // VkBool32 sampleShadingEnable;
            0.0f,                                                     // float minSampleShading;
            nullptr,                                                  // const VkSampleMask* pSampleMask;
            VK_FALSE,                                                 // VkBool32 alphaToCoverageEnable;
            VK_FALSE                                                  // VkBool32 alphaToOneEnable;
        };

        VkSamplerCreateInfo samplerCreateInfo =
            mapSampler(params.sampler, m_textureBindings[texUnit]->getTestTexture().getTextureFormat(), params.minLod,
                       params.maxLod, params.unnormal);

        if (maxAnisotropy > 1.0f)
        {
            samplerCreateInfo.anisotropyEnable = VK_TRUE;
            samplerCreateInfo.maxAnisotropy    = maxAnisotropy;
        }

        bool linFilt =
            (samplerCreateInfo.magFilter == VK_FILTER_LINEAR || samplerCreateInfo.minFilter == VK_FILTER_LINEAR ||
             samplerCreateInfo.mipmapMode == VK_SAMPLER_MIPMAP_MODE_LINEAR);
        if (linFilt && samplerCreateInfo.compareEnable == VK_FALSE)
        {
            const pipeline::TestTexture &testTexture = m_textureBindings[texUnit]->getTestTexture();
            const VkFormat textureFormat =
                testTexture.isCompressed() ?
                    mapCompressedTextureFormat(testTexture.getCompressedLevel(0, 0).getFormat()) :
                    mapTextureFormat(testTexture.getTextureFormat());
            const VkFormatProperties formatProperties = getPhysicalDeviceFormatProperties(
                m_context.getInstanceInterface(), m_context.getPhysicalDevice(), textureFormat);

            if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
                TCU_THROW(NotSupportedError, "Linear filtering for this image format is not supported");
        }

        m_sampler = createSampler(vkd, vkDevice, &samplerCreateInfo);

        {
            const VkDescriptorBufferInfo descriptorBufferInfo = {
                *m_uniformBuffer, // VkBuffer buffer;
                0u,               // VkDeviceSize offset;
                VK_WHOLE_SIZE     // VkDeviceSize range;
            };

            DescriptorSetUpdateBuilder()
                .writeSingle(*m_descriptorSet[0], DescriptorSetUpdateBuilder::Location::binding(0),
                             VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &descriptorBufferInfo)
                .update(vkd, vkDevice);
        }

        {
            VkDescriptorImageInfo descriptorImageInfo = {
                m_sampler.get(),                            // VkSampler sampler;
                m_textureBindings[texUnit]->getImageView(), // VkImageView imageView;
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL    // VkImageLayout imageLayout;
            };

            DescriptorSetUpdateBuilder()
                .writeSingle(*m_descriptorSet[1], DescriptorSetUpdateBuilder::Location::binding(0),
                             VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &descriptorImageInfo)
                .update(vkd, vkDevice);
        }

        m_graphicsPipeline = makeGraphicsPipeline(
            vkd,                   // const DeviceInterface&                        vk
            vkDevice,              // const VkDevice                                device
            *m_pipelineLayout,     // const VkPipelineLayout                        pipelineLayout
            *vertexShaderModule,   // const VkShaderModule                          vertexShaderModule
            VK_NULL_HANDLE,        // const VkShaderModule                          tessellationControlShaderModule
            VK_NULL_HANDLE,        // const VkShaderModule                          tessellationEvalShaderModule
            VK_NULL_HANDLE,        // const VkShaderModule                          geometryShaderModule
            *fragmentShaderModule, // const VkShaderModule                          fragmentShaderModule
            *m_renderPass,         // const VkRenderPass                            renderPass
            viewports,             // const std::vector<VkViewport>&                viewports
            scissors,              // const std::vector<VkRect2D>&                  scissors
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, // const VkPrimitiveTopology                     topology
            0u,                                  // const uint32_t                                subpass
            0u,                                  // const uint32_t                                patchControlPoints
            &vertexInputStateParams,  // const VkPipelineVertexInputStateCreateInfo*   vertexInputStateCreateInfo
            nullptr,                  // const VkPipelineRasterizationStateCreateInfo* rasterizationStateCreateInfo
            &multisampleStateParams); // const VkPipelineMultisampleStateCreateInfo*   multisampleStateCreateInfo
    }

    // Create Vertex Buffer
    {
        VkDeviceSize bufferSize = positionDataSize + textureCoordDataSize;

        // Pad the buffer size to a stride multiple for the last element so that it isn't out of bounds
        bufferSize += vertexTextureStrideSize - ((bufferSize - m_vertexBufferOffset) % vertexTextureStrideSize);

        const VkBufferCreateInfo vertexBufferParams = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
            nullptr,                              // const void* pNext;
            0u,                                   // VkBufferCreateFlags flags;
            bufferSize,                           // VkDeviceSize size;
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,    // VkBufferUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
            1u,                                   // uint32_t queueFamilyCount;
            &queueFamilyIndex                     // const uint32_t* pQueueFamilyIndices;
        };

        m_vertexBuffer       = createBuffer(vkd, vkDevice, &vertexBufferParams);
        m_vertexBufferMemory = m_allocator->allocate(getBufferMemoryRequirements(vkd, vkDevice, m_vertexBuffer.get()),
                                                     MemoryRequirement::HostVisible);

        VK_CHECK(vkd.bindBufferMemory(vkDevice, m_vertexBuffer.get(), m_vertexBufferMemory.get()->getMemory(),
                                      m_vertexBufferMemory.get()->getOffset()));

        // Load vertices into vertex buffer
        deMemcpy(m_vertexBufferMemory.get()->getHostPtr(), positions, positionDataSize);
        deMemcpy(reinterpret_cast<uint8_t *>(m_vertexBufferMemory.get()->getHostPtr()) + positionDataSize, texCoord,
                 textureCoordDataSize);
        flushMappedMemoryRange(vkd, vkDevice, m_vertexBufferMemory.get()->getMemory(),
                               m_vertexBufferMemory.get()->getOffset(), VK_WHOLE_SIZE);
    }
}

void GraphicsBackend::uploadUniforms(ShaderParameters shaderParams)
{
    const DeviceInterface &vkd = m_context.getDeviceInterface();
    const VkDevice vkDevice    = getDevice();

    deMemcpy(m_uniformBufferMemory->getHostPtr(), &shaderParams, sizeof(shaderParams));
    flushMappedMemoryRange(vkd, vkDevice, m_uniformBufferMemory->getMemory(), m_uniformBufferMemory->getOffset(),
                           VK_WHOLE_SIZE);
}

void GraphicsBackend::recordCommands()
{
    const DeviceInterface &vkd = m_context.getDeviceInterface();

    // Begin Command Buffer
    beginCommandBuffer(vkd, m_commandBuffer.get());

    // Begin Render Pass
    beginRenderPass(vkd, m_commandBuffer.get(), m_renderPass.get(), m_frameBuffer.get(),
                    makeRect2D(0, 0, m_renderWidth, m_renderHeight));

    vkd.cmdBindPipeline(m_commandBuffer.get(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline.get());
    vkd.cmdBindDescriptorSets(m_commandBuffer.get(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout.get(), 0u, 1,
                              &m_descriptorSet[0].get(), 0u, nullptr);
    vkd.cmdBindDescriptorSets(m_commandBuffer.get(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout.get(), 1u, 1,
                              &m_descriptorSet[1].get(), 0u, nullptr);
    vkd.cmdBindVertexBuffers(m_commandBuffer.get(), 0, 1, &m_vertexBuffer.get(), &m_vertexBufferOffset);
    vkd.cmdBindVertexBuffers(m_commandBuffer.get(), 1, 1, &m_vertexBuffer.get(), &m_vertexBufferOffset);
    vkd.cmdBindIndexBuffer(m_commandBuffer.get(), m_vertexIndexBuffer.get(), 0, VK_INDEX_TYPE_UINT16);
    vkd.cmdDrawIndexed(m_commandBuffer.get(), 6, 1, 0, 0, 0);
    endRenderPass(vkd, m_commandBuffer.get());

    // Copy Image
    {
        copyImageToBuffer(vkd, m_commandBuffer.get(), m_multisampling ? m_resolvedImage.get() : m_image.get(),
                          m_resultBuffer.get(), tcu::IVec2(m_renderWidth, m_renderHeight));

        addImageTransitionBarrier(
            m_commandBuffer.get(), m_multisampling ? m_resolvedImage.get() : m_image.get(),
            VK_PIPELINE_STAGE_TRANSFER_BIT,                // VkPipelineStageFlags        srcStageMask
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // VkPipelineStageFlags        dstStageMask
            VK_ACCESS_TRANSFER_READ_BIT,                   // VkAccessFlags            srcAccessMask
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,          // VkAccessFlags            dstAccessMask
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,          // VkImageLayout oldLayout;
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);     // VkImageLayout newLayout;
    }

    endCommandBuffer(vkd, m_commandBuffer.get());
}

void GraphicsBackend::clearImage(vk::VkImage image)
{
    const DeviceInterface &vkd = m_context.getDeviceInterface();
    const VkDevice vkDevice    = getDevice();
    Move<VkCommandBuffer> commandBuffer;
    const uint32_t queueFamilyIndex = getQueueNdx(m_context, false);
    const VkQueue queue             = getDeviceQueue(vkd, vkDevice, queueFamilyIndex, 0);

    const VkImageSubresourceRange subResourcerange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    commandBuffer = allocateCommandBuffer(vkd, vkDevice, *m_commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    beginCommandBuffer(vkd, *commandBuffer);

    // Transition UNDEFINED -> TRANSFER_DST
    addImageTransitionBarrier(*commandBuffer, image, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                              0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkClearColorValue color = makeClearValueColorF32(0.0f, 0.0f, 0.0f, 1.0f).color;
    vkd.cmdClearColorImage(*commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color, 1, &subResourcerange);

    // Transition TRANSFER_DST -> COLOR_ATTACHMENT (Required for Graphics RenderPass)
    addImageTransitionBarrier(*commandBuffer, image, VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                              VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    endCommandBuffer(vkd, *commandBuffer);

    submitCommandsAndWait(vkd, vkDevice, queue, commandBuffer.get());
}

void GraphicsBackend::submit()
{
    const DeviceInterface &vkd      = m_context.getDeviceInterface();
    const VkDevice vkDevice         = getDevice();
    const uint32_t queueFamilyIndex = getQueueNdx(m_context, false);
    const VkQueue queue             = getDeviceQueue(vkd, vkDevice, queueFamilyIndex, 0);

    // Submit
    submitCommandsAndWait(vkd, vkDevice, queue, m_commandBuffer.get());

    invalidateMappedMemoryRange(vkd, vkDevice, m_resultBufferMemory->getMemory(), m_resultBufferMemory->getOffset(),
                                VK_WHOLE_SIZE);
}

ComputeBackend::ComputeBackend(Context &context, vk::VkSampleCountFlagBits sampleCount, uint32_t renderWidth,
                               uint32_t renderHeight, uint32_t renderDepth, vk::VkComponentMapping componentMapping,
                               vk::VkImageType imageType, vk::VkImageViewType imageViewType, vk::VkFormat imageFormat,
                               bool requireRobustness2, bool requireImageViewMinLod)
    : RenderBackend(context, sampleCount, renderWidth, renderHeight, renderDepth, componentMapping, imageType,
                    imageViewType, imageFormat, requireRobustness2, requireImageViewMinLod, true)
{
    const DeviceInterface &vkd      = m_context.getDeviceInterface();
    const VkDevice vkDevice         = getDevice();
    const uint32_t queueFamilyIndex = getQueueNdx(m_context, true);

    // Descriptor pool
    {
        DescriptorPoolBuilder descriptorPoolBuilder;
        descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

        m_descriptorPool =
            descriptorPoolBuilder.build(vkd, vkDevice, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
    }

    // Descriptor sets
    {
        DescriptorSetLayoutBuilder layoutBuilder;
        layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);         // 0
        layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT); // 1
        layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);          // 2
        layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);         // 3

        // We only use index 0 for compute
        m_descriptorSetLayout[0] = layoutBuilder.build(vkd, vkDevice);
        m_descriptorSet[0]       = makeDescriptorSet(*m_descriptorPool, *m_descriptorSetLayout[0]);
    }

    // Pipeline layout
    {
        // Push constants
        const VkPushConstantRange pushConstantRange = {
            VK_SHADER_STAGE_COMPUTE_BIT, // VkShaderStageFlags  stageFlags;
            0u,                          // uint32_t            offset;
            sizeof(tcu::IVec2)           // uint32_t            size;
        };

        VkDescriptorSetLayout descriptorSetLayouts[] = {*m_descriptorSetLayout[0]};

        const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, // VkStructureType sType;
            nullptr,                                       // const void* pNext;
            0u,                                            // VkPipelineLayoutCreateFlags flags;
            1u,                                            // uint32_t descriptorSetCount;
            descriptorSetLayouts,                          // const VkDescriptorSetLayout* pSetLayouts;
            1u,                                            // uint32_t pushConstantRangeCount;
            &pushConstantRange                             // const VkPushConstantRange* pPushConstantRanges;
        };

        m_pipelineLayout = createPipelineLayout(vkd, vkDevice, &pipelineLayoutCreateInfo);
    }

    // Geometry buffer
    {
        const VkDeviceSize bufferSize         = sizeof(tcu::Vec4) * 8;
        const VkBufferCreateInfo bufferParams = {
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, // VkStructureType sType;
            nullptr,                              // const void* pNext;
            0u,                                   // VkBufferCreateFlags flags;
            bufferSize,                           // VkDeviceSize size;
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,   // VkBufferUsageFlags usage;
            VK_SHARING_MODE_EXCLUSIVE,            // VkSharingMode sharingMode;
            1u,                                   // uint32_t queueFamilyCount;
            &queueFamilyIndex                     // const uint32_t* pQueueFamilyIndices;
        };
        m_geometryBuffer       = createBuffer(vkd, vkDevice, &bufferParams);
        m_geometryBufferMemory = m_allocator->allocate(getBufferMemoryRequirements(vkd, vkDevice, *m_geometryBuffer),
                                                       MemoryRequirement::HostVisible);
        VK_CHECK(vkd.bindBufferMemory(vkDevice, *m_geometryBuffer, m_geometryBufferMemory->getMemory(),
                                      m_geometryBufferMemory->getOffset()));
    }

    clearImage(*m_image);
}

void ComputeBackend::createFrameResources(uint32_t numComps, Program progSpec, const float *positions, int texUnit,
                                          const float *texCoord, const glu::TextureTestUtil::ReferenceParams &params,
                                          const float maxAnisotropy)
{
    const DeviceInterface &vkd = m_context.getDeviceInterface();
    const VkDevice vkDevice    = getDevice();

    Unique<VkShaderModule> computeShaderModule(createShaderModule(
        vkd, vkDevice, m_context.getBinaryCollection().get("compute_" + std::string(getProgramName(progSpec))), 0));

    // Uploading geometry
    {
        tcu::Vec4 *ptr = reinterpret_cast<tcu::Vec4 *>(m_geometryBufferMemory->getHostPtr());

        // Texture Coordinates (Indices 0-3)
        for (int i = 0; i < 4; ++i)
        {
            for (uint32_t c = 0; c < 4; ++c)
            {
                ptr[i][c] = (c < numComps) ? texCoord[i * numComps + c] : 0.0f;
            }
        }

        // Vertex Positions (Indices 4-7) - Needed for W component in projected tests
        for (int i = 0; i < 4; ++i)
        {
            for (uint32_t c = 0; c < 4; ++c)
            {
                ptr[4 + i][c] = positions[i * 4 + c];
            }
        }

        flushMappedMemoryRange(vkd, vkDevice, m_geometryBufferMemory->getMemory(), m_geometryBufferMemory->getOffset(),
                               VK_WHOLE_SIZE);
    }

    // Creating sampler
    {
        VkSamplerCreateInfo samplerCreateInfo =
            mapSampler(params.sampler, m_textureBindings[texUnit]->getTestTexture().getTextureFormat(), params.minLod,
                       params.maxLod, params.unnormal);

        if (maxAnisotropy > 1.0f)
        {
            samplerCreateInfo.anisotropyEnable = VK_TRUE;
            samplerCreateInfo.maxAnisotropy    = maxAnisotropy;
        }

        bool linFilt =
            (samplerCreateInfo.magFilter == VK_FILTER_LINEAR || samplerCreateInfo.minFilter == VK_FILTER_LINEAR ||
             samplerCreateInfo.mipmapMode == VK_SAMPLER_MIPMAP_MODE_LINEAR);
        if (linFilt && samplerCreateInfo.compareEnable == VK_FALSE)
        {
            const pipeline::TestTexture &testTexture = m_textureBindings[texUnit]->getTestTexture();
            const VkFormat textureFormat =
                testTexture.isCompressed() ?
                    mapCompressedTextureFormat(testTexture.getCompressedLevel(0, 0).getFormat()) :
                    mapTextureFormat(testTexture.getTextureFormat());
            const VkFormatProperties formatProperties = getPhysicalDeviceFormatProperties(
                m_context.getInstanceInterface(), m_context.getPhysicalDevice(), textureFormat);

            if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
                TCU_THROW(NotSupportedError, "Linear filtering for this image format is not supported");
        }

        m_sampler = createSampler(vkd, vkDevice, &samplerCreateInfo);
    }

    // Uploading descriptors
    {
        VkDescriptorBufferInfo uboInfo    = {*m_uniformBuffer, 0u, VK_WHOLE_SIZE};
        VkDescriptorImageInfo samplerInfo = {m_sampler.get(), m_textureBindings[texUnit]->getImageView(),
                                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkDescriptorImageInfo outputInfo  = {VK_NULL_HANDLE, *m_imageView, VK_IMAGE_LAYOUT_GENERAL};
        VkDescriptorBufferInfo geomInfo   = {*m_geometryBuffer, 0u, VK_WHOLE_SIZE};

        DescriptorSetUpdateBuilder()
            .writeSingle(*m_descriptorSet[0], DescriptorSetUpdateBuilder::Location::binding(0),
                         VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &uboInfo)
            .writeSingle(*m_descriptorSet[0], DescriptorSetUpdateBuilder::Location::binding(1),
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &samplerInfo)
            .writeSingle(*m_descriptorSet[0], DescriptorSetUpdateBuilder::Location::binding(2),
                         VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &outputInfo)
            .writeSingle(*m_descriptorSet[0], DescriptorSetUpdateBuilder::Location::binding(3),
                         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &geomInfo)
            .update(vkd, vkDevice);
    }

    // Creating pipeline
    {
        const VkComputePipelineCreateInfo pipelineCreateInfo = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                                                                nullptr,
                                                                0u,
                                                                {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                                 nullptr, 0u, VK_SHADER_STAGE_COMPUTE_BIT,
                                                                 *computeShaderModule, "main", nullptr},
                                                                *m_pipelineLayout,
                                                                VK_NULL_HANDLE,
                                                                0};
        m_computePipeline = createComputePipeline(vkd, vkDevice, VK_NULL_HANDLE, &pipelineCreateInfo);
    }
}

void ComputeBackend::uploadUniforms(ShaderParameters shaderParams)
{
    const DeviceInterface &vkd = m_context.getDeviceInterface();
    const VkDevice vkDevice    = getDevice();

    shaderParams.padding = tcu::Vec2(m_viewportWidth, m_viewportHeight);

    deMemcpy(m_uniformBufferMemory->getHostPtr(), &shaderParams, sizeof(shaderParams));
    flushMappedMemoryRange(vkd, vkDevice, m_uniformBufferMemory->getMemory(), m_uniformBufferMemory->getOffset(),
                           VK_WHOLE_SIZE);
}

void ComputeBackend::recordCommands()
{
    const DeviceInterface &vkd = m_context.getDeviceInterface();

    beginCommandBuffer(vkd, *m_commandBuffer);

    vkd.cmdBindPipeline(*m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *m_computePipeline);
    vkd.cmdBindDescriptorSets(*m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *m_pipelineLayout, 0, 1,
                              &*m_descriptorSet[0], 0, nullptr);

    int32_t offset[2] = {(int32_t)m_viewportOffsetX, (int32_t)m_viewportOffsetY};
    vkd.cmdPushConstants(*m_commandBuffer, *m_pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(offset), offset);

    const uint32_t groupX = (static_cast<uint32_t>(m_viewportWidth) + 15) / 16;
    const uint32_t groupY = (static_cast<uint32_t>(m_viewportHeight) + 15) / 16;
    vkd.cmdDispatch(*m_commandBuffer, groupX, groupY, 1);

    copyImageToBuffer(vkd, *m_commandBuffer, *m_image, *m_resultBuffer, tcu::IVec2(m_renderWidth, m_renderHeight),
                      VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

    const VkBufferMemoryBarrier bufferBarrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                                                 nullptr,
                                                 VK_ACCESS_TRANSFER_WRITE_BIT,
                                                 VK_ACCESS_HOST_READ_BIT,
                                                 VK_QUEUE_FAMILY_IGNORED,
                                                 VK_QUEUE_FAMILY_IGNORED,
                                                 *m_resultBuffer,
                                                 0,
                                                 VK_WHOLE_SIZE};
    vkd.cmdPipelineBarrier(*m_commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr,
                           1, &bufferBarrier, 0, nullptr);

    endCommandBuffer(vkd, *m_commandBuffer);
}

void ComputeBackend::clearImage(vk::VkImage image)
{
    const DeviceInterface &vkd = m_context.getDeviceInterface();
    const VkDevice vkDevice    = getDevice();
    Move<VkCommandBuffer> commandBuffer;
    const uint32_t queueFamilyIndex = getQueueNdx(m_context, true);
    const VkQueue queue             = getDeviceQueue(vkd, vkDevice, queueFamilyIndex, 0);

    const VkImageSubresourceRange subResourcerange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    commandBuffer = allocateCommandBuffer(vkd, vkDevice, *m_commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    beginCommandBuffer(vkd, *commandBuffer);

    // Transition UNDEFINED -> GENERAL (General is valid for ClearColorImage and Storage)
    addImageTransitionBarrier(*commandBuffer, image, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                              0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    VkClearColorValue color = makeClearValueColorF32(0.0f, 0.0f, 0.0f, 1.0f).color;
    vkd.cmdClearColorImage(*commandBuffer, image, VK_IMAGE_LAYOUT_GENERAL, &color, 1, &subResourcerange);

    // Memory barrier to ensure clear is visible to subsequent compute shader (Transfer Write -> Shader Read/Write)
    // We stay in GENERAL layout.
    addImageTransitionBarrier(*commandBuffer, image, VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                              VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL,
                              VK_IMAGE_LAYOUT_GENERAL);

    endCommandBuffer(vkd, *commandBuffer);

    submitCommandsAndWait(vkd, vkDevice, queue, commandBuffer.get());
}

void ComputeBackend::submit()
{
    const DeviceInterface &vkd      = m_context.getDeviceInterface();
    const VkDevice vkDevice         = getDevice();
    const uint32_t queueFamilyIndex = getQueueNdx(m_context, true);
    const VkQueue queue             = getDeviceQueue(vkd, vkDevice, queueFamilyIndex, 0);

    // Submit
    submitCommandsAndWait(vkd, vkDevice, queue, m_commandBuffer.get());

    invalidateMappedMemoryRange(vkd, vkDevice, m_resultBufferMemory->getMemory(), m_resultBufferMemory->getOffset(),
                                VK_WHOLE_SIZE);
}

} // namespace util
} // namespace texture
} // namespace vkt
