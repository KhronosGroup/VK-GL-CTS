/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
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
 */ /*!
 * \file
 * \brief
 */ /*-------------------------------------------------------------------*/

/**
 */ /*!
 * \file  glcApiCoverageTests.cpp
 * \brief Conformance tests for OpenGL and OpenGL ES API coverage.
 */ /*-------------------------------------------------------------------*/

#include "deMath.h"
#include "deUniquePtr.hpp"
#include "gluContextInfo.hpp"
#include "gluDefs.hpp"
#include "gluStateReset.hpp"
#include "gluShaderProgram.hpp"
#include "gluStrUtil.hpp"
#include "glw.h"
#include "glwFunctions.hpp"
#include "tcuResource.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuTestLog.hpp"
#include "xeXMLParser.hpp"

#include "glcApiCoverageTests.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <set>

using namespace glw;
using namespace glu;

#define ENUM(name)  \
    {               \
        #name, name \
    }

namespace
{

#define GTF_TEXTURE_FORMAT_IS_ETC(texfmt) \
    ((texfmt) >= GL_COMPRESSED_R11_EAC && (texfmt) <= GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC)
#define GTF_TEXTURE_FORMAT_IS_RGTC(texfmt) \
    ((texfmt) >= GL_COMPRESSED_RED_RGTC1 && (texfmt) <= GL_COMPRESSED_SIGNED_RG_RGTC2)

///////////////////////////////////////////////////////////////
/// \brief getEnumNames
/// \param e
/// \param names
///

void getEnumNames(const GLenum e, std::set<std::string> &names)
{
    // clang-format off
    const char* (*func_ptrs[])(int) = {
        glu::getErrorName,
        glu::getTypeName,
        glu::getParamQueryName,
        glu::getProgramParamName,
        glu::getUniformParamName,
        glu::getFramebufferAttachmentName,
        glu::getFramebufferAttachmentParameterName,
        glu::getFramebufferTargetName,
        glu::getFramebufferStatusName,
        glu::getFramebufferAttachmentTypeName,
        glu::getFramebufferColorEncodingName,
        glu::getFramebufferParameterName,
        glu::getRenderbufferParameterName,
        glu::getPrimitiveTypeName,
        glu::getBlendFactorName,
        glu::getBlendEquationName,
        glu::getBufferTargetName,
        glu::getBufferBindingName,
        glu::getUsageName,
        glu::getBufferQueryName,
        glu::getFaceName,
        glu::getCompareFuncName,
        glu::getEnableCapName,
        glu::getIndexedEnableCapName,
        glu::getWindingName,
        glu::getHintModeName,
        glu::getHintName,
        glu::getStencilOpName,
        glu::getShaderTypeName,
        glu::getBufferName,
        glu::getInvalidateAttachmentName,
        glu::getDrawReadBufferName,
        glu::getTextureTargetName,
        glu::getTextureParameterName,
        glu::getTextureLevelParameterName,
        glu::getRepeatModeName,
        glu::getTextureFilterName,
        glu::getTextureWrapModeName,
        glu::getTextureSwizzleName,
        glu::getTextureCompareModeName,
        glu::getCubeMapFaceName,
        glu::getTextureDepthStencilModeName,
        glu::getPixelStoreParameterName,
        glu::getUncompressedTextureFormatName,
        glu::getCompressedTextureFormatName,
        glu::getShaderVarTypeName,
        glu::getShaderParamName,
        glu::getVertexAttribParameterNameName,
        glu::getBooleanName,
        glu::getGettableStateName,
        glu::getGettableIndexedStateName,
        glu::getGettableStringName,
        glu::getGettablePackStateName,
        glu::getPointerStateName,
        glu::getInternalFormatParameterName,
        glu::getInternalFormatTargetName,
        glu::getMultisampleParameterName,
        glu::getQueryTargetName,
        glu::getQueryParamName,
        glu::getQueryObjectParamName,
        glu::getImageAccessName,
        glu::getProgramInterfaceName,
        glu::getProgramResourcePropertyName,
        glu::getPrecisionFormatTypeName,
        glu::getTransformFeedbackTargetName,
        glu::getClampColorTargetName,
        glu::getProvokingVertexName,
        glu::getDebugMessageSourceName,
        glu::getDebugMessageTypeName,
        glu::getDebugMessageSeverityName,
        glu::getPipelineParamName,
        glu::getPatchParamName,
        glu::getTextureFormatName,
        glu::getGraphicsResetStatusName,
        glu::getClipDistanceParamName,
        glu::getConditionalRenderParamName,
        glu::getWaitEnumName,
        glu::getLogicOpParamsName,
        glu::getPolygonModeName,
        glu::getPrimSizeParamName,
        glu::getActiveTextureParamName,
        glu::getClipControlParamName,
        glu::getUniformSubroutinesParamName
    };
    // clang-format on

    for (size_t i = 0; i < sizeof(func_ptrs) / sizeof(func_ptrs[0]); i++)
        if (func_ptrs[i](e) != nullptr)
            names.insert(func_ptrs[i](e));
}

bool isNameWithinBitfield(const std::string &name, const GLenum e)
{
    tcu::Format::Bitfield<16> (*func_ptrs[])(int) = {
        glu::getBufferMaskStr,  glu::getBufferMapFlagsStr, glu::getMemoryBarrierFlagsStr, glu::getShaderTypeMaskStr,
        glu::getContextMaskStr, glu::getClientWaitMaskStr, glu::getContextProfileMaskStr};

    for (size_t i = 0; i < sizeof(func_ptrs) / sizeof(func_ptrs[0]); i++)
    {
        auto bitfield = func_ptrs[i](e);

        std::ostringstream sstr;
        bitfield.toStream(sstr);

        if (sstr.str().find(name) != std::string::npos)
            return true;
    }

    return false;
}

} // namespace

namespace glcts
{

/* Coverage test for glGetUniformIndices */
// default shaders
const GLchar *ApiCoverageTestCase::m_vert_shader =
    R"(${VERSION}
    out vec3 texCoords;
    in vec2 inPosition;
    in vec3 inTexCoord;
    void main() {
        gl_Position = vec4(inPosition.x, inPosition.y, 0.0,1.0);
        texCoords = inTexCoord;
    }
    )";

const GLchar *ApiCoverageTestCase::m_frag_shader =
    R"(${VERSION}
    ${PRECISION}
    uniform sampler2D tex0;
    in vec3 texCoords;
    out vec4 frag_color;
    void main() {
        frag_color = texture2D(tex0, texCoords.xy);
    }
    )";

std::vector<std::string> ApiCoverageTestCase::m_version_names;

/** Constructor.
 *
 *  @param context     Rendering context
 */
ApiCoverageTestCase::ApiCoverageTestCase(deqp::Context &context)
    : TestCase(context, "coverage", "Test case verifies OpenGL API coverage functionality")
    , m_is_context_ES(false)
    , m_is_transform_feedback_obj_supported(false)
{
}

/** Stub deinit method. */
void ApiCoverageTestCase::deinit()
{
    /* Left blank intentionally */
}

/** Stub init method */
void ApiCoverageTestCase::init()
{
    glu::resetState(m_context.getRenderContext(), m_context.getContextInfo());

    const glu::RenderContext &renderContext = m_context.getRenderContext();
    m_is_context_ES                         = glu::isContextTypeES(renderContext.getType());
    glu::GLSLVersion glslVersion            = glu::getContextTypeGLSLVersion(renderContext.getType());
    m_context_type                          = m_context.getRenderContext().getType();

    m_is_transform_feedback_obj_supported =
        (m_is_context_ES || glu::contextSupports(m_context_type, glu::ApiType::core(4, 0)) ||
         m_context.getContextInfo().isExtensionSupported("GL_ARB_transform_feedback2"));

    specialization_map["VERSION"] = glu::getGLSLVersionDeclaration(glslVersion);

    if (m_is_context_ES)
    {
        specialization_map["EXTENSION"] = "#extension GL_EXT_clip_cull_distance : enable";
        specialization_map["PRECISION"] = "precision highp float;";
        if (glu::contextSupports(m_context_type, glu::ApiType::es(3, 0)))
        {
            m_config_name = "CoverageES30.test";
        }
    }
    else
    {
        specialization_map["EXTENSION"] = "";
        specialization_map["PRECISION"] = "";
        if (glu::contextSupports(m_context_type, glu::ApiType::core(4, 3)))
        {
            m_config_name = "CoverageGL43.test";
        }
        else if (glu::contextSupports(m_context_type, glu::ApiType::core(4, 0)))
        {
            m_config_name = "CoverageGL40.test";
        }
        else if (glu::contextSupports(m_context_type, glu::ApiType::core(3, 3)))
        {
            m_config_name = "CoverageGL33.test";
        }
        else if (glu::contextSupports(m_context_type, glu::ApiType::core(3, 2)))
        {
            m_config_name = "CoverageGL32.test";
        }
        else if (glu::contextSupports(m_context_type, glu::ApiType::core(3, 1)))
        {
            m_config_name = "CoverageGL31.test";
        }
        else if (glu::contextSupports(m_context_type, glu::ApiType::core(3, 0)))
        {
            m_config_name = "CoverageGL30.test";
        }
    }

#ifdef GL_VERSION_1_1
    m_version_names.push_back("GL_VERSION_1_1");
#endif
#ifdef GL_VERSION_1_2
    m_version_names.push_back("GL_VERSION_1_2");
#endif
#ifdef GL_VERSION_1_3
    m_version_names.push_back("GL_VERSION_1_3");
#endif
#ifdef GL_VERSION_1_4
    m_version_names.push_back("GL_VERSION_1_4");
#endif
#ifdef GL_VERSION_1_5
    m_version_names.push_back("GL_VERSION_1_5");
#endif
#ifdef GL_VERSION_2_0
    m_version_names.push_back("GL_VERSION_2_0");
#endif
#ifdef GL_VERSION_2_1
    m_version_names.push_back("GL_VERSION_2_1");
#endif
#ifdef GL_VERSION_3_0
    m_version_names.push_back("GL_VERSION_3_0");
#endif
#ifdef GL_VERSION_3_1
    m_version_names.push_back("GL_VERSION_3_1");
#endif
#ifdef GL_VERSION_3_2
    m_version_names.push_back("GL_VERSION_3_2");
#endif
#ifdef GL_VERSION_3_3
    m_version_names.push_back("GL_VERSION_3_3");
#endif
#ifdef GL_VERSION_4_0
    m_version_names.push_back("GL_VERSION_4_0");
#endif
#ifdef GL_VERSION_4_1
    m_version_names.push_back("GL_VERSION_4_1");
#endif
#ifdef GL_VERSION_4_2
    m_version_names.push_back("GL_VERSION_4_2");
#endif
#ifdef GL_VERSION_4_3
    m_version_names.push_back("GL_VERSION_4_3");
#endif
#ifdef GL_VERSION_4_4
    m_version_names.push_back("GL_VERSION_4_4");
#endif
#ifdef GL_VERSION_4_5
    m_version_names.push_back("GL_VERSION_4_5");
#endif
#ifdef GL_VERSION_4_6
    m_version_names.push_back("GL_VERSION_4_6");
#endif
#ifdef GL_ES_VERSION_3_0
    m_version_names.push_back("GL_ES_VERSION_3_0");
#endif

    if (m_config_name.empty())
    {
        m_testCtx.getLog() << tcu::TestLog::Message << "API coverage test not supported.\n" << tcu::TestLog::EndMessage;
        throw tcu::NotSupportedError("API coverage test not supported");
    }

    ea_BlendEquation = {ENUM(GL_FUNC_ADD), ENUM(GL_FUNC_SUBTRACT), ENUM(GL_FUNC_REVERSE_SUBTRACT), {"End of List", -1}};

    ea_BlendEquationSeparate1 = {
        ENUM(GL_FUNC_ADD), ENUM(GL_FUNC_SUBTRACT), ENUM(GL_FUNC_REVERSE_SUBTRACT), {"End of List", -1}};

    ea_BlendEquationSeparate2 = {
        ENUM(GL_FUNC_ADD), ENUM(GL_FUNC_SUBTRACT), ENUM(GL_FUNC_REVERSE_SUBTRACT), {"End of List", -1}};

    ea_BlendFunc1 = {ENUM(GL_ZERO),
                     ENUM(GL_ONE),
                     ENUM(GL_SRC_COLOR),
                     ENUM(GL_ONE_MINUS_SRC_COLOR),
                     ENUM(GL_DST_COLOR),
                     ENUM(GL_ONE_MINUS_DST_COLOR),
                     ENUM(GL_SRC_ALPHA),
                     ENUM(GL_ONE_MINUS_SRC_ALPHA),
                     ENUM(GL_DST_ALPHA),
                     ENUM(GL_ONE_MINUS_DST_ALPHA),
                     ENUM(GL_CONSTANT_COLOR),
                     ENUM(GL_ONE_MINUS_CONSTANT_COLOR),
                     ENUM(GL_CONSTANT_ALPHA),
                     ENUM(GL_ONE_MINUS_CONSTANT_ALPHA),
                     ENUM(GL_SRC_ALPHA_SATURATE),
                     {"End of List", -1}};

    ea_BlendFunc2 = {ENUM(GL_ZERO),           ENUM(GL_ONE),
                     ENUM(GL_SRC_COLOR),      ENUM(GL_ONE_MINUS_SRC_COLOR),
                     ENUM(GL_DST_COLOR),      ENUM(GL_ONE_MINUS_DST_COLOR),
                     ENUM(GL_SRC_ALPHA),      ENUM(GL_ONE_MINUS_SRC_ALPHA),
                     ENUM(GL_DST_ALPHA),      ENUM(GL_ONE_MINUS_DST_ALPHA),
                     ENUM(GL_CONSTANT_COLOR), ENUM(GL_ONE_MINUS_CONSTANT_COLOR),
                     ENUM(GL_CONSTANT_ALPHA), ENUM(GL_ONE_MINUS_CONSTANT_ALPHA),
                     {"End of List", -1}};

    ea_BlendFuncSeparate1 = {ENUM(GL_ZERO),
                             ENUM(GL_ONE),
                             ENUM(GL_SRC_COLOR),
                             ENUM(GL_ONE_MINUS_SRC_COLOR),
                             ENUM(GL_DST_COLOR),
                             ENUM(GL_ONE_MINUS_DST_COLOR),
                             ENUM(GL_SRC_ALPHA),
                             ENUM(GL_ONE_MINUS_SRC_ALPHA),
                             ENUM(GL_DST_ALPHA),
                             ENUM(GL_ONE_MINUS_DST_ALPHA),
                             ENUM(GL_CONSTANT_COLOR),
                             ENUM(GL_ONE_MINUS_CONSTANT_COLOR),
                             ENUM(GL_CONSTANT_ALPHA),
                             ENUM(GL_ONE_MINUS_CONSTANT_ALPHA),
                             ENUM(GL_SRC_ALPHA_SATURATE),
                             {"End of List", -1}};

    ea_BlendFuncSeparate2 = {ENUM(GL_ZERO),           ENUM(GL_ONE),
                             ENUM(GL_SRC_COLOR),      ENUM(GL_ONE_MINUS_SRC_COLOR),
                             ENUM(GL_DST_COLOR),      ENUM(GL_ONE_MINUS_DST_COLOR),
                             ENUM(GL_SRC_ALPHA),      ENUM(GL_ONE_MINUS_SRC_ALPHA),
                             ENUM(GL_DST_ALPHA),      ENUM(GL_ONE_MINUS_DST_ALPHA),
                             ENUM(GL_CONSTANT_COLOR), ENUM(GL_ONE_MINUS_CONSTANT_COLOR),
                             ENUM(GL_CONSTANT_ALPHA), ENUM(GL_ONE_MINUS_CONSTANT_ALPHA),
                             {"End of List", -1}};

    ea_BlendFuncSeparate3 = {ENUM(GL_ZERO),
                             ENUM(GL_ONE),
                             ENUM(GL_SRC_COLOR),
                             ENUM(GL_ONE_MINUS_SRC_COLOR),
                             ENUM(GL_DST_COLOR),
                             ENUM(GL_ONE_MINUS_DST_COLOR),
                             ENUM(GL_SRC_ALPHA),
                             ENUM(GL_ONE_MINUS_SRC_ALPHA),
                             ENUM(GL_DST_ALPHA),
                             ENUM(GL_ONE_MINUS_DST_ALPHA),
                             ENUM(GL_CONSTANT_COLOR),
                             ENUM(GL_ONE_MINUS_CONSTANT_COLOR),
                             ENUM(GL_CONSTANT_ALPHA),
                             ENUM(GL_ONE_MINUS_CONSTANT_ALPHA),
                             ENUM(GL_SRC_ALPHA_SATURATE),
                             {"End of List", -1}};

    ea_BlendFuncSeparate4 = {ENUM(GL_ZERO),           ENUM(GL_ONE),
                             ENUM(GL_SRC_COLOR),      ENUM(GL_ONE_MINUS_SRC_COLOR),
                             ENUM(GL_DST_COLOR),      ENUM(GL_ONE_MINUS_DST_COLOR),
                             ENUM(GL_SRC_ALPHA),      ENUM(GL_ONE_MINUS_SRC_ALPHA),
                             ENUM(GL_DST_ALPHA),      ENUM(GL_ONE_MINUS_DST_ALPHA),
                             ENUM(GL_CONSTANT_COLOR), ENUM(GL_ONE_MINUS_CONSTANT_COLOR),
                             ENUM(GL_CONSTANT_ALPHA), ENUM(GL_ONE_MINUS_CONSTANT_ALPHA),
                             {"End of List", -1}};

    ea_BufferObjectTargets = {ENUM(GL_ARRAY_BUFFER), ENUM(GL_ELEMENT_ARRAY_BUFFER), {"End of List", -1}};

    ea_BufferObjectUsages = {ENUM(GL_STATIC_DRAW), ENUM(GL_DYNAMIC_DRAW), ENUM(GL_STREAM_DRAW), {"End of List", -1}};

    ea_ClearBufferMask = {
        ENUM(GL_DEPTH_BUFFER_BIT), ENUM(GL_STENCIL_BUFFER_BIT), ENUM(GL_COLOR_BUFFER_BIT), {"End of List", -1}};

    ea_CompressedTextureFormats = {{"End of List", -1}};

    ea_ShaderTypes = {ENUM(GL_VERTEX_SHADER), ENUM(GL_FRAGMENT_SHADER), {"End of List", -1}};

    ea_CullFaceMode = {ENUM(GL_FRONT), ENUM(GL_BACK), ENUM(GL_FRONT_AND_BACK), {"End of List", -1}};

    ea_DepthFunction = {ENUM(GL_NEVER),    ENUM(GL_LESS),   ENUM(GL_EQUAL),  ENUM(GL_LEQUAL),    ENUM(GL_GREATER),
                        ENUM(GL_NOTEQUAL), ENUM(GL_GEQUAL), ENUM(GL_ALWAYS), {"End of List", -1}};

    ea_Enable = {ENUM(GL_CULL_FACE),
                 ENUM(GL_BLEND),
                 ENUM(GL_DITHER),
                 ENUM(GL_STENCIL_TEST),
                 ENUM(GL_DEPTH_TEST),
                 ENUM(GL_SAMPLE_COVERAGE),
                 ENUM(GL_SAMPLE_ALPHA_TO_COVERAGE),
                 ENUM(GL_SCISSOR_TEST),
                 ENUM(GL_POLYGON_OFFSET_FILL),
                 {"End of List", -1}};

    ea_Primitives = {ENUM(GL_LINE_LOOP),      ENUM(GL_LINE_STRIP),   ENUM(GL_LINES),     ENUM(GL_POINTS),
                     ENUM(GL_TRIANGLE_STRIP), ENUM(GL_TRIANGLE_FAN), ENUM(GL_TRIANGLES), {"End of List", -1}};

    ea_Face = {ENUM(GL_FRONT), ENUM(GL_BACK), ENUM(GL_FRONT_AND_BACK), {"End of List", -1}};

    ea_FrameBufferTargets = {ENUM(GL_FRAMEBUFFER), {"End of List", -1}};

    ea_FrameBufferAttachments = {
        ENUM(GL_COLOR_ATTACHMENT0), ENUM(GL_DEPTH_ATTACHMENT), ENUM(GL_STENCIL_ATTACHMENT), {"End of List", -1}};

    ea_FrontFaceDirection = {ENUM(GL_CW), ENUM(GL_CCW), {"End of List", -1}};

    ea_GetBoolean = {
        ENUM(GL_SAMPLE_COVERAGE_INVERT), ENUM(GL_COLOR_WRITEMASK), ENUM(GL_DEPTH_WRITEMASK), {"End of List", -1}};

    ea_GetBufferParameter = {ENUM(GL_BUFFER_SIZE), ENUM(GL_BUFFER_USAGE), {"End of List", -1}};

    ea_GetBufferParameter_OES_mapbuffer = {{"End of List", -1}};

    ea_GetFloat = {ENUM(GL_DEPTH_RANGE),
                   ENUM(GL_LINE_WIDTH),
                   ENUM(GL_POLYGON_OFFSET_FACTOR),
                   ENUM(GL_POLYGON_OFFSET_UNITS),
                   ENUM(GL_SAMPLE_COVERAGE_VALUE),
                   ENUM(GL_COLOR_CLEAR_VALUE),
                   ENUM(GL_BLEND_COLOR),
                   ENUM(GL_ALIASED_LINE_WIDTH_RANGE),
                   {"End of List", -1}};

    ea_GetFramebufferAttachmentParameter = {ENUM(GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE),
                                            ENUM(GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME),
                                            ENUM(GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL),
                                            ENUM(GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE),
                                            {"End of List", -1}};

    ea_GetInteger = {ENUM(GL_ARRAY_BUFFER_BINDING),
                     ENUM(GL_ELEMENT_ARRAY_BUFFER_BINDING),
                     ENUM(GL_VIEWPORT),
                     ENUM(GL_CULL_FACE_MODE),
                     ENUM(GL_FRONT_FACE),
                     ENUM(GL_TEXTURE_BINDING_2D),
                     ENUM(GL_TEXTURE_BINDING_CUBE_MAP),
                     ENUM(GL_ACTIVE_TEXTURE),
                     ENUM(GL_STENCIL_WRITEMASK),
                     ENUM(GL_DEPTH_CLEAR_VALUE),
                     ENUM(GL_STENCIL_CLEAR_VALUE),
                     ENUM(GL_SCISSOR_BOX),
                     ENUM(GL_STENCIL_FUNC),
                     ENUM(GL_STENCIL_VALUE_MASK),
                     ENUM(GL_STENCIL_REF),
                     ENUM(GL_STENCIL_FAIL),
                     ENUM(GL_STENCIL_PASS_DEPTH_FAIL),
                     ENUM(GL_STENCIL_PASS_DEPTH_PASS),
                     ENUM(GL_STENCIL_BACK_FUNC),
                     ENUM(GL_STENCIL_BACK_VALUE_MASK),
                     ENUM(GL_STENCIL_BACK_REF),
                     ENUM(GL_STENCIL_BACK_FAIL),
                     ENUM(GL_STENCIL_PASS_DEPTH_FAIL),
                     ENUM(GL_STENCIL_PASS_DEPTH_PASS),
                     ENUM(GL_DEPTH_FUNC),
                     ENUM(GL_BLEND_SRC_RGB),
                     ENUM(GL_BLEND_SRC_ALPHA),
                     ENUM(GL_BLEND_DST_RGB),
                     ENUM(GL_BLEND_DST_ALPHA),
                     ENUM(GL_BLEND_EQUATION_RGB),
                     ENUM(GL_BLEND_EQUATION_ALPHA),
                     ENUM(GL_UNPACK_ALIGNMENT),
                     ENUM(GL_PACK_ALIGNMENT),
                     ENUM(GL_CURRENT_PROGRAM),
                     ENUM(GL_SUBPIXEL_BITS),
                     ENUM(GL_MAX_TEXTURE_SIZE),
                     ENUM(GL_MAX_CUBE_MAP_TEXTURE_SIZE),
                     ENUM(GL_MAX_VIEWPORT_DIMS),
                     ENUM(GL_SAMPLE_BUFFERS),
                     ENUM(GL_SAMPLES),
                     ENUM(GL_COMPRESSED_TEXTURE_FORMATS),
                     ENUM(GL_NUM_COMPRESSED_TEXTURE_FORMATS),
                     ENUM(GL_MAX_VERTEX_ATTRIBS),
                     ENUM(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS),
                     ENUM(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS),
                     ENUM(GL_MAX_TEXTURE_IMAGE_UNITS),
                     {"End of List", -1}};

    ea_GetInteger_OES_Texture_3D = {{"End of List", -1}};

    ea_GetPointer = {ENUM(GL_VERTEX_ATTRIB_ARRAY_POINTER), {"End of List", -1}};

    ea_HintTarget_OES_fragment_shader_derivative = {{"End of List", -1}};

    ea_InvalidRenderBufferFormats = {{"End of List", -1}};

    ea_RenderBufferFormats_OES_rgb8_rgba8 = {{"End of List", -1}};

    ea_RenderBufferFormats_OES_depth_component24 = {{"End of List", -1}};

    ea_RenderBufferFormats_OES_depth_component32 = {{"End of List", -1}};

    ea_RenderBufferFormats_OES_stencil1 = {{"End of List", -1}};

    ea_RenderBufferFormats_OES_stencil4 = {{"End of List", -1}};

    ea_ShaderPrecision = {{"End of List", -1}};

    ea_GetIntegerES3 = {{"End of List", -1}};

    ea_GetProgram = {ENUM(GL_DELETE_STATUS),
                     ENUM(GL_LINK_STATUS),
                     ENUM(GL_VALIDATE_STATUS),
                     ENUM(GL_ATTACHED_SHADERS),
                     ENUM(GL_INFO_LOG_LENGTH),
                     ENUM(GL_ACTIVE_UNIFORMS),
                     ENUM(GL_ACTIVE_UNIFORM_MAX_LENGTH),
                     ENUM(GL_ACTIVE_ATTRIBUTES),
                     ENUM(GL_ACTIVE_ATTRIBUTE_MAX_LENGTH),
                     {"End of List", -1}};

    ea_GetRenderBufferParameter = {ENUM(GL_RENDERBUFFER_WIDTH),
                                   ENUM(GL_RENDERBUFFER_HEIGHT),
                                   ENUM(GL_RENDERBUFFER_INTERNAL_FORMAT),
                                   {"End of List", -1}};

    ea_GetShaderStatus = {ENUM(GL_SHADER_TYPE),     ENUM(GL_DELETE_STATUS),        ENUM(GL_COMPILE_STATUS),
                          ENUM(GL_INFO_LOG_LENGTH), ENUM(GL_SHADER_SOURCE_LENGTH), {"End of List", -1}};

    ea_GetString = {
        ENUM(GL_RENDERER), ENUM(GL_SHADING_LANGUAGE_VERSION), ENUM(GL_VENDOR), ENUM(GL_VERSION), {"End of List", -1}};

    ea_GetTexParameter = {ENUM(GL_TEXTURE_MIN_FILTER),
                          ENUM(GL_TEXTURE_MAG_FILTER),
                          ENUM(GL_TEXTURE_WRAP_S),
                          ENUM(GL_TEXTURE_WRAP_T),
                          {"End of List", -1}};

    ea_GetVertexAttrib = {ENUM(GL_VERTEX_ATTRIB_ARRAY_ENABLED),    ENUM(GL_VERTEX_ATTRIB_ARRAY_SIZE),
                          ENUM(GL_VERTEX_ATTRIB_ARRAY_STRIDE),     ENUM(GL_VERTEX_ATTRIB_ARRAY_TYPE),
                          ENUM(GL_VERTEX_ATTRIB_ARRAY_NORMALIZED), {"End of List", -1}};

    ea_GetVertexAttribPointer = {ENUM(GL_VERTEX_ATTRIB_ARRAY_POINTER), {"End of List", -1}};

    ea_HintMode = {ENUM(GL_FASTEST), ENUM(GL_NICEST), ENUM(GL_DONT_CARE), {"End of List", -1}};

    ea_HintTarget = {{"End of List", -1}};

    ea_PixelStore = {ENUM(GL_PACK_ALIGNMENT), ENUM(GL_UNPACK_ALIGNMENT), {"End of List", -1}};

    ea_RenderBufferFormats = {
        ENUM(GL_RGBA4), ENUM(GL_RGB5_A1), ENUM(GL_DEPTH_COMPONENT16), ENUM(GL_STENCIL_INDEX8), {"End of List", -1}};

    ea_RenderBufferTargets = {ENUM(GL_RENDERBUFFER), {"End of List", -1}};

    ea_RenderBufferInvalidTargets = {ENUM(GL_RENDERBUFFER + 1), {"End of List", -1}};

    ea_StencilFunction = {ENUM(GL_NEVER),    ENUM(GL_LESS),   ENUM(GL_EQUAL),  ENUM(GL_LEQUAL),    ENUM(GL_GREATER),
                          ENUM(GL_NOTEQUAL), ENUM(GL_GEQUAL), ENUM(GL_ALWAYS), {"End of List", -1}};

    ea_StencilOp = {ENUM(GL_ZERO),   ENUM(GL_KEEP),      ENUM(GL_REPLACE),   ENUM(GL_INCR),      ENUM(GL_DECR),
                    ENUM(GL_INVERT), ENUM(GL_INCR_WRAP), ENUM(GL_DECR_WRAP), {"End of List", -1}};

    ea_TextureFormat = {{"End of List", -1}};

    ea_TextureMagFilter = {ENUM(GL_NEAREST), ENUM(GL_LINEAR), {"End of List", -1}};

    ea_TextureMinFilter = {ENUM(GL_NEAREST),
                           ENUM(GL_LINEAR),
                           ENUM(GL_NEAREST_MIPMAP_NEAREST),
                           ENUM(GL_LINEAR_MIPMAP_NEAREST),
                           ENUM(GL_NEAREST_MIPMAP_LINEAR),
                           ENUM(GL_LINEAR_MIPMAP_LINEAR),
                           {"End of List", -1}};

    ea_TextureTarget = {ENUM(GL_TEXTURE_2D), {"End of List", -1}};

    ea_TextureType = {ENUM(GL_UNSIGNED_BYTE), ENUM(GL_UNSIGNED_BYTE),          ENUM(GL_UNSIGNED_SHORT_5_6_5),
                      ENUM(GL_UNSIGNED_BYTE), ENUM(GL_UNSIGNED_SHORT_4_4_4_4), ENUM(GL_UNSIGNED_SHORT_5_5_5_1),
                      ENUM(GL_UNSIGNED_BYTE), ENUM(GL_UNSIGNED_BYTE),          {"End of List", -1}};

    ea_TextureWrapMode = {ENUM(GL_CLAMP_TO_EDGE), ENUM(GL_REPEAT), {"End of List", -1}};

    ea_GetBufferParameteri64v = {ENUM(GL_BUFFER_MAP_LENGTH),   ENUM(GL_BUFFER_MAP_OFFSET), ENUM(GL_BUFFER_MAPPED),
                                 ENUM(GL_BUFFER_ACCESS_FLAGS), ENUM(GL_BUFFER_USAGE),      ENUM(GL_BUFFER_SIZE),
                                 {"End of List", -1}};

    ea_ReadBuffer = {ENUM(GL_NONE),
                     ENUM(GL_BACK),
                     ENUM(GL_COLOR_ATTACHMENT0),
                     ENUM(GL_COLOR_ATTACHMENT1),
                     ENUM(GL_COLOR_ATTACHMENT2),
                     ENUM(GL_COLOR_ATTACHMENT3),
                     {"End of List", -1}};

    ea_Texture3DTarget = {ENUM(GL_TEXTURE_3D), ENUM(GL_TEXTURE_2D_ARRAY), {"End of List", -1}};

    ea_CompressedTexture3DTarget = {ENUM(GL_TEXTURE_2D_ARRAY), {"End of List", -1}};

    ea_CompressedTextureFormat = {/* RGTC */
                                  ENUM(GL_COMPRESSED_RED_RGTC1),
                                  ENUM(GL_COMPRESSED_SIGNED_RED_RGTC1),
                                  ENUM(GL_COMPRESSED_RG_RGTC2),
                                  ENUM(GL_COMPRESSED_SIGNED_RG_RGTC2),
                                  /* ETC2/EAC */
                                  ENUM(GL_COMPRESSED_R11_EAC),
                                  ENUM(GL_COMPRESSED_RG11_EAC),
                                  ENUM(GL_COMPRESSED_SIGNED_R11_EAC),
                                  ENUM(GL_COMPRESSED_SIGNED_RG11_EAC),
                                  ENUM(GL_COMPRESSED_RGB8_ETC2),
                                  ENUM(GL_COMPRESSED_SRGB8_ETC2),
                                  ENUM(GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2),
                                  ENUM(GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2),
                                  ENUM(GL_COMPRESSED_RGBA8_ETC2_EAC),
                                  ENUM(GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC),
                                  {"End of List", -1}};
    CompressedTextureSize      = {8, 8,  16, 16, /* RGTC */
                                  8, 16, 8,  16, 8, 8, 8, 8, 16, 16 /* ETC2/EAC */};

    ea_DrawBuffers = {ENUM(GL_COLOR_ATTACHMENT0),
                      ENUM(GL_COLOR_ATTACHMENT1),
                      ENUM(GL_COLOR_ATTACHMENT2),
                      ENUM(GL_COLOR_ATTACHMENT3),
                      {"End of List", -1}};

    ea_GetInteger64v = {ENUM(GL_MAX_ELEMENT_INDEX),
                        ENUM(GL_MAX_SERVER_WAIT_TIMEOUT),
                        ENUM(GL_MAX_UNIFORM_BLOCK_SIZE),
                        ENUM(GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS),
                        ENUM(GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS),
                        {"End of List", -1}};

    ea_GetSynciv = {
        ENUM(GL_OBJECT_TYPE), ENUM(GL_SYNC_STATUS), ENUM(GL_SYNC_CONDITION), ENUM(GL_SYNC_FLAGS), {"End of List", -1}};

    ea_InvalidateFramebuffer = {{"End of List", -1}};

    if (m_is_context_ES)
    {
        ea_CompressedTextureFormats = {ENUM(GL_PALETTE4_RGB8_OES),
                                       ENUM(GL_PALETTE4_RGBA8_OES),
                                       ENUM(GL_PALETTE4_R5_G6_B5_OES),
                                       ENUM(GL_PALETTE4_RGBA4_OES),
                                       ENUM(GL_PALETTE4_RGB5_A1_OES),
                                       ENUM(GL_PALETTE8_RGB8_OES),
                                       ENUM(GL_PALETTE8_RGBA8_OES),
                                       ENUM(GL_PALETTE8_R5_G6_B5_OES),
                                       ENUM(GL_PALETTE8_RGBA4_OES),
                                       ENUM(GL_PALETTE8_RGB5_A1_OES),
                                       {"End of List", -1}};

        ea_GetBufferParameter_OES_mapbuffer = {
            ENUM(GL_BUFFER_ACCESS_OES), ENUM(GL_BUFFER_MAPPED_OES), {"End of List", -1}};

        ea_GetFloat.insert(ea_GetFloat.begin(), ENUM(GL_ALIASED_POINT_SIZE_RANGE));

        ea_GetInteger.insert(ea_GetInteger.begin(), ENUM(GL_GENERATE_MIPMAP_HINT));
        ea_GetInteger.insert(ea_GetInteger.begin(), ENUM(GL_MAX_VERTEX_UNIFORM_VECTORS));
        ea_GetInteger.insert(ea_GetInteger.begin(), ENUM(GL_MAX_VARYING_VECTORS));
        ea_GetInteger.insert(ea_GetInteger.begin(), ENUM(GL_MAX_FRAGMENT_UNIFORM_VECTORS));
        ea_GetInteger.insert(ea_GetInteger.begin(), ENUM(GL_RED_BITS));
        ea_GetInteger.insert(ea_GetInteger.begin(), ENUM(GL_GREEN_BITS));
        ea_GetInteger.insert(ea_GetInteger.begin(), ENUM(GL_BLUE_BITS));
        ea_GetInteger.insert(ea_GetInteger.begin(), ENUM(GL_ALPHA_BITS));
        ea_GetInteger.insert(ea_GetInteger.begin(), ENUM(GL_DEPTH_BITS));
        ea_GetInteger.insert(ea_GetInteger.begin(), ENUM(GL_STENCIL_BITS));
        ea_GetInteger.insert(ea_GetInteger.begin(), ENUM(GL_IMPLEMENTATION_COLOR_READ_TYPE));
        ea_GetInteger.insert(ea_GetInteger.begin(), ENUM(GL_IMPLEMENTATION_COLOR_READ_FORMAT));

        ea_GetInteger_OES_Texture_3D = {
            ENUM(GL_TEXTURE_BINDING_3D_OES), ENUM(GL_MAX_3D_TEXTURE_SIZE_OES), {"End of List", -1}};

        ea_GetPointer = {ENUM(GL_VERTEX_ATTRIB_ARRAY_POINTER), ENUM(GL_BUFFER_MAP_POINTER_OES), {"End of List", -1}};

        ea_HintTarget_OES_fragment_shader_derivative = {ENUM(GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES),
                                                        {"End of List", -1}};

        ea_InvalidRenderBufferFormats = {ENUM(GL_RGB), ENUM(GL_RGBA), {"End of List", -1}};

        ea_RenderBufferFormats_OES_rgb8_rgba8 = {ENUM(GL_RGB8_OES), ENUM(GL_RGBA8_OES), {"End of List", -1}};

        ea_RenderBufferFormats_OES_depth_component24 = {ENUM(GL_DEPTH_COMPONENT24_OES), {"End of List", -1}};

        ea_RenderBufferFormats_OES_depth_component32 = {ENUM(GL_DEPTH_COMPONENT32_OES), {"End of List", -1}};

        ea_RenderBufferFormats_OES_stencil1 = {ENUM(GL_STENCIL_INDEX1_OES), {"End of List", -1}};

        ea_RenderBufferFormats_OES_stencil4 = {ENUM(GL_STENCIL_INDEX4_OES), {"End of List", -1}};

        ea_ShaderPrecision = {ENUM(GL_LOW_FLOAT),  ENUM(GL_MEDIUM_FLOAT), ENUM(GL_HIGH_FLOAT), ENUM(GL_LOW_INT),
                              ENUM(GL_MEDIUM_INT), ENUM(GL_HIGH_INT),     {"End of List", -1}};

        ea_GetString = {ENUM(GL_EXTENSIONS), ENUM(GL_RENDERER), ENUM(GL_SHADING_LANGUAGE_VERSION),
                        ENUM(GL_VENDOR),     ENUM(GL_VERSION),  {"End of List", -1}};

        ea_HintTarget.insert(ea_HintTarget.begin(), ENUM(GL_GENERATE_MIPMAP_HINT));

        ea_RenderBufferFormats.insert(ea_RenderBufferFormats.begin(), ENUM(GL_RGB565));

        ea_TextureFormat = {ENUM(GL_ALPHA),     ENUM(GL_RGB),  ENUM(GL_RGB),       ENUM(GL_RGBA),
                            ENUM(GL_RGBA),      ENUM(GL_RGBA), ENUM(GL_LUMINANCE), ENUM(GL_LUMINANCE_ALPHA),
                            {"End of List", -1}};

        if (glu::contextSupports(m_context_type, glu::ApiType::es(3, 0)))
        {
            ea_GetIntegerES3 = {ENUM(GL_MAX_VARYING_COMPONENTS), {"End of List", -1}};

            ea_GetVertexAttrib = {ENUM(GL_VERTEX_ATTRIB_ARRAY_ENABLED),
                                  ENUM(GL_VERTEX_ATTRIB_ARRAY_SIZE),
                                  ENUM(GL_VERTEX_ATTRIB_ARRAY_STRIDE),
                                  ENUM(GL_VERTEX_ATTRIB_ARRAY_TYPE),
                                  ENUM(GL_VERTEX_ATTRIB_ARRAY_NORMALIZED),
                                  ENUM(GL_VERTEX_ATTRIB_ARRAY_INTEGER),
                                  {"End of List", -1}};

            ea_HintTarget.insert(ea_HintTarget.begin(), ENUM(GL_FRAGMENT_SHADER_DERIVATIVE_HINT));

            ea_CompressedTextureFormat = {ENUM(GL_COMPRESSED_R11_EAC),
                                          ENUM(GL_COMPRESSED_RG11_EAC),
                                          ENUM(GL_COMPRESSED_SIGNED_R11_EAC),
                                          ENUM(GL_COMPRESSED_SIGNED_RG11_EAC),
                                          ENUM(GL_COMPRESSED_RGB8_ETC2),
                                          ENUM(GL_COMPRESSED_SRGB8_ETC2),
                                          ENUM(GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2),
                                          ENUM(GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2),
                                          ENUM(GL_COMPRESSED_RGBA8_ETC2_EAC),
                                          ENUM(GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC),
                                          {"End of List", -1}};
            CompressedTextureSize      = {8, 16, 8, 16, 8, 8, 8, 8, 16, 16};

            ea_InvalidateFramebuffer = {
                ENUM(GL_FRAMEBUFFER), ENUM(GL_DRAW_FRAMEBUFFER), ENUM(GL_READ_FRAMEBUFFER), {"End of List", -1}};

            // clang-format off
            funcs_map.insert({ "glReadBuffer",                 &ApiCoverageTestCase::TestCoverageGLCallReadBuffer });
            funcs_map.insert({ "glDrawRangeElements",          &ApiCoverageTestCase::TestCoverageGLCallDrawRangeElements });
            funcs_map.insert({ "glTexImage3D",                 &ApiCoverageTestCase::TestCoverageGLCallTexImage3D });
            funcs_map.insert({ "glTexSubImage3D",              &ApiCoverageTestCase::TestCoverageGLCallTexSubImage3D });
            funcs_map.insert({ "glCopyTexSubImage3D",          &ApiCoverageTestCase::TestCoverageGLCallCopyTexSubImage3D });
            funcs_map.insert({ "glCompressedTexImage3D",       &ApiCoverageTestCase::TestCoverageGLCallCompressedTexImage3D });
            funcs_map.insert({ "glCompressedTexSubImage3D",    &ApiCoverageTestCase::TestCoverageGLCallCompressedTexSubImage3D });
            funcs_map.insert({ "glGenQueries",                 &ApiCoverageTestCase::TestCoverageGLCallGenQueries });
            funcs_map.insert({ "glDeleteQueries",              &ApiCoverageTestCase::TestCoverageGLCallDeleteQueries });
            funcs_map.insert({ "glIsQuery",                    &ApiCoverageTestCase::TestCoverageGLCallIsQuery });
            funcs_map.insert({ "glBeginQuery",                 &ApiCoverageTestCase::TestCoverageGLCallBeginQuery });
            funcs_map.insert({ "glEndQuery",                   &ApiCoverageTestCase::TestCoverageGLCallEndQuery });
            funcs_map.insert({ "glGetQueryiv",                 &ApiCoverageTestCase::TestCoverageGLCallGetQueryiv });
            funcs_map.insert({ "glGetQueryObjectuiv",          &ApiCoverageTestCase::TestCoverageGLCallGetQueryObjectuiv });
            funcs_map.insert({ "glMapBufferRange",             &ApiCoverageTestCase::TestCoverageGLCallMapBufferRange });
            funcs_map.insert({ "glUnmapBuffer",                &ApiCoverageTestCase::TestCoverageGLCallUnmapBuffer });
            funcs_map.insert({ "glGetBufferPointerv",          &ApiCoverageTestCase::TestCoverageGLCallGetBufferPointerv });
            funcs_map.insert({ "glFlushMappedBufferRange",     &ApiCoverageTestCase::TestCoverageGLCallFlushMappedBufferRange });
            funcs_map.insert({ "glDrawBuffers",                &ApiCoverageTestCase::TestCoverageGLCallDrawBuffers });
            funcs_map.insert({ "glUniformMatrix2x4fv",         &ApiCoverageTestCase::TestCoverageGLCallUniformMatrix2x4fv });
            funcs_map.insert({ "glBeginTransformFeedback",     &ApiCoverageTestCase::TestCoverageGLCallBeginTransformFeedback });
            funcs_map.insert({ "glEndTransformFeedback",       &ApiCoverageTestCase::TestCoverageGLCallEndTransformFeedback });
            funcs_map.insert({ "glBindBufferRange",            &ApiCoverageTestCase::TestCoverageGLCallBindBufferRange });
            funcs_map.insert({ "glBindBufferBase",             &ApiCoverageTestCase::TestCoverageGLCallBindBufferBase });
            funcs_map.insert({ "glTransformFeedbackVaryings",  &ApiCoverageTestCase::TestCoverageGLCallTransformFeedbackVaryings });
            funcs_map.insert({ "glGetTransformFeedbackVarying", &ApiCoverageTestCase::TestCoverageGLCallGetTransformFeedbackVarying });
            funcs_map.insert({ "glVertexAttribIPointer",       &ApiCoverageTestCase::TestCoverageGLCallVertexAttribIPointer });
            funcs_map.insert({ "glGetVertexAttribIiv",         &ApiCoverageTestCase::TestCoverageGLCallGetVertexAttribIiv });
            funcs_map.insert({ "glGetVertexAttribIuiv",        &ApiCoverageTestCase::TestCoverageGLCallGetVertexAttribIuiv });
            funcs_map.insert({ "glVertexAttribI4i",            &ApiCoverageTestCase::TestCoverageGLCallVertexAttribI4i });
            funcs_map.insert({ "glVertexAttribI4ui",           &ApiCoverageTestCase::TestCoverageGLCallVertexAttribI4ui });
            funcs_map.insert({ "glVertexAttribI4iv",           &ApiCoverageTestCase::TestCoverageGLCallVertexAttribI4iv });
            funcs_map.insert({ "glVertexAttribI4uiv",          &ApiCoverageTestCase::TestCoverageGLCallVertexAttribI4uiv });
            funcs_map.insert({ "glGetUniformuiv",              &ApiCoverageTestCase::TestCoverageGLCallGetUniformuiv });
            funcs_map.insert({ "glGetFragDataLocation",        &ApiCoverageTestCase::TestCoverageGLCallGetFragDataLocation });
            funcs_map.insert({ "glUniform2ui",                 &ApiCoverageTestCase::TestCoverageGLCallUniform2ui });
            funcs_map.insert({ "glUniform2uiv",                &ApiCoverageTestCase::TestCoverageGLCallUniform2uiv });
            funcs_map.insert({ "glClearBufferiv",              &ApiCoverageTestCase::TestCoverageGLCallClearBufferiv });
            funcs_map.insert({ "glClearBufferuiv",             &ApiCoverageTestCase::TestCoverageGLCallClearBufferuiv });
            funcs_map.insert({ "glClearBufferfv",              &ApiCoverageTestCase::TestCoverageGLCallClearBufferfv });
            funcs_map.insert({ "glClearBufferfi",              &ApiCoverageTestCase::TestCoverageGLCallClearBufferfi });
            funcs_map.insert({ "glGetStringi",                 &ApiCoverageTestCase::TestCoverageGLCallGetStringi });
            funcs_map.insert({ "glBlitFramebuffer",            &ApiCoverageTestCase::TestCoverageGLCallBlitFramebuffer });
            funcs_map.insert({ "glRenderbufferStorageMultisample", &ApiCoverageTestCase::TestCoverageGLCallRenderbufferStorageMultisample });
            funcs_map.insert({ "glBindVertexArray",            &ApiCoverageTestCase::TestCoverageGLCallBindVertexArray });
            funcs_map.insert({ "glDeleteVertexArrays",         &ApiCoverageTestCase::TestCoverageGLCallDeleteVertexArrays });
            funcs_map.insert({ "glGenVertexArrays",            &ApiCoverageTestCase::TestCoverageGLCallGenVertexArrays });
            funcs_map.insert({ "glIsVertexArray",              &ApiCoverageTestCase::TestCoverageGLCallIsVertexArray });
            funcs_map.insert({ "glDrawArraysInstanced",        &ApiCoverageTestCase::TestCoverageGLCallDrawArraysInstanced });
            funcs_map.insert({ "glDrawElementsInstanced",      &ApiCoverageTestCase::TestCoverageGLCallDrawElementsInstanced });
            funcs_map.insert({ "glCopyBufferSubData",          &ApiCoverageTestCase::TestCoverageGLCallCopyBufferSubData });
            funcs_map.insert({ "glGetUniformIndices",          &ApiCoverageTestCase::TestCoverageGLCallGetUniformIndices });
            funcs_map.insert({ "glGetActiveUniformsiv",        &ApiCoverageTestCase::TestCoverageGLCallGetActiveUniformsiv });
            funcs_map.insert({ "glGetUniformBlockIndex",       &ApiCoverageTestCase::TestCoverageGLCallGetUniformBlockIndex });
            funcs_map.insert({ "glGetActiveUniformBlockiv",    &ApiCoverageTestCase::TestCoverageGLCallGetActiveUniformBlockiv });
            funcs_map.insert({ "glGetActiveUniformBlockName",  &ApiCoverageTestCase::TestCoverageGLCallGetActiveUniformBlockName });
            funcs_map.insert({ "glUniformBlockBinding",        &ApiCoverageTestCase::TestCoverageGLCallUniformBlockBinding });
            funcs_map.insert({ "glGetBufferParameteri64v",     &ApiCoverageTestCase::TestCoverageGLCallGetBufferParameteri64v });
            funcs_map.insert({ "glProgramParameteri",          &ApiCoverageTestCase::TestCoverageGLCallProgramParameteri });
            funcs_map.insert({ "glFenceSync",                  &ApiCoverageTestCase::TestCoverageGLCallFenceSync });
            funcs_map.insert({ "glIsSync",                     &ApiCoverageTestCase::TestCoverageGLCallIsSync });
            funcs_map.insert({ "glDeleteSync",                 &ApiCoverageTestCase::TestCoverageGLCallDeleteSync });
            funcs_map.insert({ "glClientWaitSync",             &ApiCoverageTestCase::TestCoverageGLCallClientWaitSync });
            funcs_map.insert({ "glWaitSync",                   &ApiCoverageTestCase::TestCoverageGLCallWaitSync });
            funcs_map.insert({ "glGetInteger64v",              &ApiCoverageTestCase::TestCoverageGLCallGetInteger64v });
            funcs_map.insert({ "glGetSynciv",                  &ApiCoverageTestCase::TestCoverageGLCallGetSynciv });
            funcs_map.insert({ "glGenSamplers",                &ApiCoverageTestCase::TestCoverageGLCallGenSamplers });
            funcs_map.insert({ "glDeleteSamplers",             &ApiCoverageTestCase::TestCoverageGLCallDeleteSamplers });
            funcs_map.insert({ "glIsSampler",                  &ApiCoverageTestCase::TestCoverageGLCallIsSampler });
            funcs_map.insert({ "glBindSampler",                &ApiCoverageTestCase::TestCoverageGLCallBindSampler });
            funcs_map.insert({ "glSamplerParameteri",          &ApiCoverageTestCase::TestCoverageGLCallSamplerParameteri });
            funcs_map.insert({ "glSamplerParameteriv",         &ApiCoverageTestCase::TestCoverageGLCallSamplerParameteriv });
            funcs_map.insert({ "glSamplerParameterf",          &ApiCoverageTestCase::TestCoverageGLCallSamplerParameterf });
            funcs_map.insert({ "glSamplerParameterfv",         &ApiCoverageTestCase::TestCoverageGLCallSamplerParameterfv });
            funcs_map.insert({ "glGetSamplerParameteriv",      &ApiCoverageTestCase::TestCoverageGLCallGetSamplerParameteriv });
            funcs_map.insert({ "glGetSamplerParameterfv",      &ApiCoverageTestCase::TestCoverageGLCallGetSamplerParameterfv });
            funcs_map.insert({ "glBindTransformFeedback",      &ApiCoverageTestCase::TestCoverageGLCallBindTransformFeedback });
            funcs_map.insert({ "glDeleteTransformFeedbacks",   &ApiCoverageTestCase::TestCoverageGLCallDeleteTransformFeedbacks });
            funcs_map.insert({ "glGenTransformFeedbacks",      &ApiCoverageTestCase::TestCoverageGLCallGenTransformFeedbacks });
            funcs_map.insert({ "glIsTransformFeedback",        &ApiCoverageTestCase::TestCoverageGLCallIsTransformFeedback });
            funcs_map.insert({ "glPauseTransformFeedback",     &ApiCoverageTestCase::TestCoverageGLCallPauseTransformFeedback });
            funcs_map.insert({ "glResumeTransformFeedback",    &ApiCoverageTestCase::TestCoverageGLCallResumeTransformFeedback });
            funcs_map.insert({ "glInvalidateFramebuffer",      &ApiCoverageTestCase::TestCoverageGLCallInvalidateFramebuffer });
            funcs_map.insert({ "glInvalidateSubFramebuffer",   &ApiCoverageTestCase::TestCoverageGLCallInvalidateSubFramebuffer });
            // clang-format on
        }

        if (glu::contextSupports(m_context_type, glu::ApiType::es(2, 0)))
        {
            // clang-format off
            funcs_map.insert({ "glActiveTexture",              &ApiCoverageTestCase::TestCoverageGLCallActiveTexture });
            funcs_map.insert({ "glAttachShader",               &ApiCoverageTestCase::TestCoverageGLCallAttachShader });
            funcs_map.insert({ "glBindAttribLocation",         &ApiCoverageTestCase::TestCoverageGLCallBindAttribLocation });
            funcs_map.insert({ "glBindBuffer",                 &ApiCoverageTestCase::TestCoverageGLCallBindBuffer });
            funcs_map.insert({ "glBindTexture",                &ApiCoverageTestCase::TestCoverageGLCallBindTexture });
            funcs_map.insert({ "glBlendColor",                 &ApiCoverageTestCase::TestCoverageGLCallBlendColor });
            funcs_map.insert({ "glBlendEquation",              &ApiCoverageTestCase::TestCoverageGLCallBlendEquation });
            funcs_map.insert({ "glBlendEquationSeparate",      &ApiCoverageTestCase::TestCoverageGLCallBlendEquationSeparate });
            funcs_map.insert({ "glBlendFunc",                  &ApiCoverageTestCase::TestCoverageGLCallBlendFunc });
            funcs_map.insert({ "glBlendFuncSeparate",          &ApiCoverageTestCase::TestCoverageGLCallBlendFuncSeparate });
            funcs_map.insert({ "glBufferData",                 &ApiCoverageTestCase::TestCoverageGLCallBufferData });
            funcs_map.insert({ "glBufferSubData",              &ApiCoverageTestCase::TestCoverageGLCallBufferSubData });
            funcs_map.insert({ "glClear",                      &ApiCoverageTestCase::TestCoverageGLCallClear });
            funcs_map.insert({ "glClearColor",                 &ApiCoverageTestCase::TestCoverageGLCallClearColor });
            funcs_map.insert({ "glClearStencil",               &ApiCoverageTestCase::TestCoverageGLCallClearStencil });
            funcs_map.insert({ "glColorMask",                  &ApiCoverageTestCase::TestCoverageGLCallColorMask });
            funcs_map.insert({ "glCompressedTexImage2D",       &ApiCoverageTestCase::TestCoverageGLCallCompressedTexImage2D });
            funcs_map.insert({ "glCompressedTexSubImage2D",    &ApiCoverageTestCase::TestCoverageGLCallCompressedTexSubImage2D });
            funcs_map.insert({ "glCopyTexImage2D",             &ApiCoverageTestCase::TestCoverageGLCallCopyTexImage2D });
            funcs_map.insert({ "glCopyTexSubImage2D",          &ApiCoverageTestCase::TestCoverageGLCallCopyTexSubImage2D });
            funcs_map.insert({ "glCreateProgram",              &ApiCoverageTestCase::TestCoverageGLCallCreateProgram });
            funcs_map.insert({ "glCreateShader",               &ApiCoverageTestCase::TestCoverageGLCallCreateShader });
            funcs_map.insert({ "glCullFace",                   &ApiCoverageTestCase::TestCoverageGLCallCullFace });
            funcs_map.insert({ "glDeleteBuffers",              &ApiCoverageTestCase::TestCoverageGLCallDeleteBuffers });
            funcs_map.insert({ "glDeleteTextures",             &ApiCoverageTestCase::TestCoverageGLCallDeleteTextures });
            funcs_map.insert({ "glDeleteProgram",              &ApiCoverageTestCase::TestCoverageGLCallDeleteProgram });
            funcs_map.insert({ "glDeleteShader",               &ApiCoverageTestCase::TestCoverageGLCallDeleteShader });
            funcs_map.insert({ "glDetachShader",               &ApiCoverageTestCase::TestCoverageGLCallDetachShader });
            funcs_map.insert({ "glDepthFunc",                  &ApiCoverageTestCase::TestCoverageGLCallDepthFunc });
            funcs_map.insert({ "glDepthMask",                  &ApiCoverageTestCase::TestCoverageGLCallDepthMask });
            funcs_map.insert({ "glDisable",                    &ApiCoverageTestCase::TestCoverageGLCallDisable });
            funcs_map.insert({ "glDisableVertexAttribArray",   &ApiCoverageTestCase::TestCoverageGLCallDisableVertexAttribArray });
            funcs_map.insert({ "glDrawArrays",                 &ApiCoverageTestCase::TestCoverageGLCallDrawArrays });
            funcs_map.insert({ "glDrawElements",               &ApiCoverageTestCase::TestCoverageGLCallDrawElements });
            funcs_map.insert({ "glEnable",                     &ApiCoverageTestCase::TestCoverageGLCallEnable });
            funcs_map.insert({ "glEnableVertexAttribArray",    &ApiCoverageTestCase::TestCoverageGLCallEnableVertexAttribArray });
            funcs_map.insert({ "glFinish",                     &ApiCoverageTestCase::TestCoverageGLCallFinish });
            funcs_map.insert({ "glFlush",                      &ApiCoverageTestCase::TestCoverageGLCallFlush });
            funcs_map.insert({ "glFrontFace",                  &ApiCoverageTestCase::TestCoverageGLCallFrontFace });
            funcs_map.insert({ "glGetActiveAttrib",            &ApiCoverageTestCase::TestCoverageGLCallGetActiveAttrib });
            funcs_map.insert({ "glGetActiveUniform",           &ApiCoverageTestCase::TestCoverageGLCallGetActiveUniform });
            funcs_map.insert({ "glGetAttachedShaders",         &ApiCoverageTestCase::TestCoverageGLCallGetAttachedShaders });
            funcs_map.insert({ "glGetAttribLocation",          &ApiCoverageTestCase::TestCoverageGLCallGetAttribLocation });
            funcs_map.insert({ "glGetBooleanv",                &ApiCoverageTestCase::TestCoverageGLCallGetBooleanv });
            funcs_map.insert({ "glGetBufferParameteriv",       &ApiCoverageTestCase::TestCoverageGLCallGetBufferParameteriv });
            funcs_map.insert({ "glGenBuffers",                 &ApiCoverageTestCase::TestCoverageGLCallGenBuffers });
            funcs_map.insert({ "glGenTextures",                &ApiCoverageTestCase::TestCoverageGLCallGenTextures });
            funcs_map.insert({ "gl.getError",                   &ApiCoverageTestCase::TestCoverageGLCallGetError });
            funcs_map.insert({ "glGetFloatv",                  &ApiCoverageTestCase::TestCoverageGLCallGetFloatv });
            funcs_map.insert({ "glGetIntegerv",                &ApiCoverageTestCase::TestCoverageGLCallGetIntegerv });
            funcs_map.insert({ "glGetProgramiv",               &ApiCoverageTestCase::TestCoverageGLCallGetProgramiv });
            funcs_map.insert({ "glGetProgramInfoLog",          &ApiCoverageTestCase::TestCoverageGLCallGetProgramInfoLog });
            funcs_map.insert({ "glGetString",                  &ApiCoverageTestCase::TestCoverageGLCallGetString });
            funcs_map.insert({ "glGetTexParameteriv",          &ApiCoverageTestCase::TestCoverageGLCallGetTexParameteriv });
            funcs_map.insert({ "glGetTexParameterfv",          &ApiCoverageTestCase::TestCoverageGLCallGetTexParameterfv });
            funcs_map.insert({ "glGetUniformfv",               &ApiCoverageTestCase::TestCoverageGLCallGetUniformfv });
            funcs_map.insert({ "glGetUniformiv",               &ApiCoverageTestCase::TestCoverageGLCallGetUniformiv });
            funcs_map.insert({ "glGetUniformLocation",         &ApiCoverageTestCase::TestCoverageGLCallGetUniformLocation });
            funcs_map.insert({ "glGetVertexAttribfv",          &ApiCoverageTestCase::TestCoverageGLCallGetVertexAttribfv });
            funcs_map.insert({ "glGetVertexAttribiv",          &ApiCoverageTestCase::TestCoverageGLCallGetVertexAttribiv });
            funcs_map.insert({ "glGetVertexAttribPointerv",    &ApiCoverageTestCase::TestCoverageGLCallGetVertexAttribPointerv });
            funcs_map.insert({ "glHint",                       &ApiCoverageTestCase::TestCoverageGLCallHint });
            funcs_map.insert({ "glIsBuffer",                   &ApiCoverageTestCase::TestCoverageGLCallIsBuffer });
            funcs_map.insert({ "glIsEnabled",                  &ApiCoverageTestCase::TestCoverageGLCallIsEnabled });
            funcs_map.insert({ "glIsProgram",                  &ApiCoverageTestCase::TestCoverageGLCallIsProgram });
            funcs_map.insert({ "glIsShader",                   &ApiCoverageTestCase::TestCoverageGLCallIsShader });
            funcs_map.insert({ "glIsTexture",                  &ApiCoverageTestCase::TestCoverageGLCallIsTexture });
            funcs_map.insert({ "glLineWidth",                  &ApiCoverageTestCase::TestCoverageGLCallLineWidth });
            funcs_map.insert({ "glLinkProgram",                &ApiCoverageTestCase::TestCoverageGLCallLinkProgram });
            funcs_map.insert({ "glPixelStorei",                &ApiCoverageTestCase::TestCoverageGLCallPixelStorei });
            funcs_map.insert({ "glPolygonOffset",              &ApiCoverageTestCase::TestCoverageGLCallPolygonOffset });
            funcs_map.insert({ "glReadPixels",                 &ApiCoverageTestCase::TestCoverageGLCallReadPixels });
            funcs_map.insert({ "glSampleCoverage",             &ApiCoverageTestCase::TestCoverageGLCallSampleCoverage });
            funcs_map.insert({ "glScissor",                    &ApiCoverageTestCase::TestCoverageGLCallScissor });
            funcs_map.insert({ "glStencilFunc",                &ApiCoverageTestCase::TestCoverageGLCallStencilFunc });
            funcs_map.insert({ "glStencilFuncSeparate",        &ApiCoverageTestCase::TestCoverageGLCallStencilFuncSeparate });
            funcs_map.insert({ "glStencilMask",                &ApiCoverageTestCase::TestCoverageGLCallStencilMask });
            funcs_map.insert({ "glStencilMaskSeparate",        &ApiCoverageTestCase::TestCoverageGLCallStencilMaskSeparate });
            funcs_map.insert({ "glStencilOp",                  &ApiCoverageTestCase::TestCoverageGLCallStencilOp });
            funcs_map.insert({ "glStencilOpSeparate",          &ApiCoverageTestCase::TestCoverageGLCallStencilOpSeparate });
            funcs_map.insert({ "glTexImage2D",                 &ApiCoverageTestCase::TestCoverageGLCallTexImage2D });
            funcs_map.insert({ "glTexParameteri",              &ApiCoverageTestCase::TestCoverageGLCallTexParameteri });
            funcs_map.insert({ "glTexParameterf",              &ApiCoverageTestCase::TestCoverageGLCallTexParameterf });
            funcs_map.insert({ "glTexParameteriv",             &ApiCoverageTestCase::TestCoverageGLCallTexParameteriv });
            funcs_map.insert({ "glTexParameterfv",             &ApiCoverageTestCase::TestCoverageGLCallTexParameterfv });
            funcs_map.insert({ "glTexSubImage2D",              &ApiCoverageTestCase::TestCoverageGLCallTexSubImage2D });
            funcs_map.insert({ "glUniform1i",                  &ApiCoverageTestCase::TestCoverageGLCallUniform1i });
            funcs_map.insert({ "glUniform2i",                  &ApiCoverageTestCase::TestCoverageGLCallUniform2i });
            funcs_map.insert({ "glUniform3i",                  &ApiCoverageTestCase::TestCoverageGLCallUniform3i });
            funcs_map.insert({ "glUniform4i",                  &ApiCoverageTestCase::TestCoverageGLCallUniform4i });
            funcs_map.insert({ "glUniform1f",                  &ApiCoverageTestCase::TestCoverageGLCallUniform1f });
            funcs_map.insert({ "glUniform2f",                  &ApiCoverageTestCase::TestCoverageGLCallUniform2f });
            funcs_map.insert({ "glUniform3f",                  &ApiCoverageTestCase::TestCoverageGLCallUniform3f });
            funcs_map.insert({ "glUniform4f",                  &ApiCoverageTestCase::TestCoverageGLCallUniform4f });
            funcs_map.insert({ "glUniform1iv",                 &ApiCoverageTestCase::TestCoverageGLCallUniform1iv });
            funcs_map.insert({ "glUniform2iv",                 &ApiCoverageTestCase::TestCoverageGLCallUniform2iv });
            funcs_map.insert({ "glUniform3iv",                 &ApiCoverageTestCase::TestCoverageGLCallUniform3iv });
            funcs_map.insert({ "glUniform4iv",                 &ApiCoverageTestCase::TestCoverageGLCallUniform4iv });
            funcs_map.insert({ "glUniform1fv",                 &ApiCoverageTestCase::TestCoverageGLCallUniform1fv });
            funcs_map.insert({ "glUniform2fv",                 &ApiCoverageTestCase::TestCoverageGLCallUniform2fv });
            funcs_map.insert({ "glUniform3fv",                 &ApiCoverageTestCase::TestCoverageGLCallUniform3fv });
            funcs_map.insert({ "glUniform4fv",                 &ApiCoverageTestCase::TestCoverageGLCallUniform4fv });
            funcs_map.insert({ "glUniformMatrix2fv",           &ApiCoverageTestCase::TestCoverageGLCallUniformMatrix2fv });
            funcs_map.insert({ "glUniformMatrix3fv",           &ApiCoverageTestCase::TestCoverageGLCallUniformMatrix3fv });
            funcs_map.insert({ "glUniformMatrix4fv",           &ApiCoverageTestCase::TestCoverageGLCallUniformMatrix4fv });
            funcs_map.insert({ "glUseProgram",                 &ApiCoverageTestCase::TestCoverageGLCallUseProgram });
            funcs_map.insert({ "glValidateProgram",            &ApiCoverageTestCase::TestCoverageGLCallValidateProgram });
            funcs_map.insert({ "glVertexAttrib1f",             &ApiCoverageTestCase::TestCoverageGLCallVertexAttrib1f });
            funcs_map.insert({ "glVertexAttrib2f",             &ApiCoverageTestCase::TestCoverageGLCallVertexAttrib2f });
            funcs_map.insert({ "glVertexAttrib3f",             &ApiCoverageTestCase::TestCoverageGLCallVertexAttrib3f });
            funcs_map.insert({ "glVertexAttrib4f",             &ApiCoverageTestCase::TestCoverageGLCallVertexAttrib4f });
            funcs_map.insert({ "glVertexAttrib1fv",            &ApiCoverageTestCase::TestCoverageGLCallVertexAttrib1fv });
            funcs_map.insert({ "glVertexAttrib2fv",            &ApiCoverageTestCase::TestCoverageGLCallVertexAttrib2fv });
            funcs_map.insert({ "glVertexAttrib3fv",            &ApiCoverageTestCase::TestCoverageGLCallVertexAttrib3fv });
            funcs_map.insert({ "glVertexAttrib4fv",            &ApiCoverageTestCase::TestCoverageGLCallVertexAttrib4fv });
            funcs_map.insert({ "glVertexAttribPointer",        &ApiCoverageTestCase::TestCoverageGLCallVertexAttribPointer });
            funcs_map.insert({ "glViewport",                   &ApiCoverageTestCase::TestCoverageGLCallViewport });
            funcs_map.insert({ "glIsRenderbuffer",             &ApiCoverageTestCase::TestCoverageGLCallIsRenderbuffer });
            funcs_map.insert({ "glBindRenderbuffer",           &ApiCoverageTestCase::TestCoverageGLCallBindRenderbuffer });
            funcs_map.insert({ "glDeleteRenderbuffers",        &ApiCoverageTestCase::TestCoverageGLCallDeleteRenderbuffers });
            funcs_map.insert({ "glGenRenderbuffers",           &ApiCoverageTestCase::TestCoverageGLCallGenRenderbuffers });
            funcs_map.insert({ "glRenderbufferStorage",        &ApiCoverageTestCase::TestCoverageGLCallRenderbufferStorage });
            funcs_map.insert({ "glGetRenderbufferParameteriv", &ApiCoverageTestCase::TestCoverageGLCallGetRenderbufferParameteriv });
            funcs_map.insert({ "glIsFramebuffer",              &ApiCoverageTestCase::TestCoverageGLCallIsFramebuffer });
            funcs_map.insert({ "glBindFramebuffer",            &ApiCoverageTestCase::TestCoverageGLCallBindFramebuffer });
            funcs_map.insert({ "glDeleteFramebuffers",         &ApiCoverageTestCase::TestCoverageGLCallDeleteFramebuffers });
            funcs_map.insert({ "glGenFramebuffers",            &ApiCoverageTestCase::TestCoverageGLCallGenFramebuffers });
            funcs_map.insert({ "glCheckFramebufferStatus",     &ApiCoverageTestCase::TestCoverageGLCallCheckFramebufferStatus });
            funcs_map.insert({ "glFramebufferTexture2D",       &ApiCoverageTestCase::TestCoverageGLCallFramebufferTexture2D });
            funcs_map.insert({ "glFramebufferRenderbuffer",    &ApiCoverageTestCase::TestCoverageGLCallFramebufferRenderbuffer });
            funcs_map.insert({ "glGetFramebufferAttachmentParameteriv", &ApiCoverageTestCase::TestCoverageGLCallGetFramebufferAttachmentParameteriv });
            funcs_map.insert({ "glGenerateMipmap",             &ApiCoverageTestCase::TestCoverageGLCallGenerateMipmap });
            funcs_map.insert({ "glCompileShader",              &ApiCoverageTestCase::TestCoverageGLCallCompileShader });
            funcs_map.insert({ "glGetShaderiv",                &ApiCoverageTestCase::TestCoverageGLCallGetShaderiv });
            funcs_map.insert({ "glGetShaderInfoLog",           &ApiCoverageTestCase::TestCoverageGLCallGetShaderInfoLog });
            funcs_map.insert({ "glGetShaderSource",            &ApiCoverageTestCase::TestCoverageGLCallGetShaderSource });
            funcs_map.insert({ "glShaderSource",               &ApiCoverageTestCase::TestCoverageGLCallShaderSource });
        /* Remaining entries are OpenGL ES-specific tests */
            funcs_map.insert({ "glClearDepthf",                &ApiCoverageTestCase::TestCoverageGLCallClearDepthf });
            funcs_map.insert({ "glDepthRangef",                &ApiCoverageTestCase::TestCoverageGLCallDepthRangef });
            funcs_map.insert({ "glFramebufferTexture3D",       &ApiCoverageTestCase::TestCoverageGLCallFramebufferTexture3DOES });
            funcs_map.insert({ "glMapBufferOES",               &ApiCoverageTestCase::TestCoverageGLCallMapBufferOES });
            funcs_map.insert({ "glTexImage3DOES",              &ApiCoverageTestCase::TestCoverageGLCallTexImage3DOES });
            funcs_map.insert({ "glTexSubImage3DOES",           &ApiCoverageTestCase::TestCoverageGLCallTexSubImage3DOES });
            funcs_map.insert({ "glCopyTexSubImage3DOES",       &ApiCoverageTestCase::TestCoverageGLCallCopyTexSubImage3DOES });
            funcs_map.insert({ "glCompressedTexImage3DOES",    &ApiCoverageTestCase::TestCoverageGLCallCompressedTexImage3DOES });
            funcs_map.insert({ "glCompressedTexSubImage3DOES", &ApiCoverageTestCase::TestCoverageGLCallCompressedTexSubImage3DOES });
            funcs_map.insert({ "glShaderBinary",               &ApiCoverageTestCase::TestCoverageGLCallShaderBinary });
            funcs_map.insert({ "glReleaseShaderCompiler",      &ApiCoverageTestCase::TestCoverageGLCallReleaseShaderCompiler });
            funcs_map.insert({ "glGetShaderPrecisionFormat",   &ApiCoverageTestCase::TestCoverageGLCallGetShaderPrecisionFormat });
            // clang-format on
        }
    }
    else
    {
        ea_HintTarget = {ENUM(GL_LINE_SMOOTH_HINT),
                         ENUM(GL_POLYGON_SMOOTH_HINT),
                         ENUM(GL_TEXTURE_COMPRESSION_HINT),
                         ENUM(GL_FRAGMENT_SHADER_DERIVATIVE_HINT),
                         {"End of List", -1}};

        ea_TextureFormat = {ENUM(GL_RED),  ENUM(GL_RG),   ENUM(GL_RGB),  ENUM(GL_RGB),
                            ENUM(GL_RGBA), ENUM(GL_RGBA), ENUM(GL_RGBA), {"End of List", -1}};

        ea_InvalidateFramebuffer = {
            ENUM(GL_FRAMEBUFFER), ENUM(GL_DRAW_FRAMEBUFFER), ENUM(GL_READ_FRAMEBUFFER), {"End of List", -1}};

        // clang-format off
        if (glu::contextSupports(m_context_type, glu::ApiType::core(4, 3)))
        {
            /* OpenGL 4.3 entry points */
            /* incomplete */
            funcs_map.insert({ "glInvalidateFramebuffer",                &ApiCoverageTestCase::TestCoverageGLCallInvalidateFramebuffer });
            funcs_map.insert({ "glInvalidateSubFramebuffer",             &ApiCoverageTestCase::TestCoverageGLCallInvalidateSubFramebuffer });
        }

        if (glu::contextSupports(m_context_type, glu::ApiType::core(4, 2)))
        {
            /* OpenGL 4.2 entry points */
            /* not implemented, yet */
        }

        if (glu::contextSupports(m_context_type, glu::ApiType::core(4, 1)))
        {
            /* OpenGL 4.1 entry points */
            /* not implemented, yet */
        }

        if (glu::contextSupports(m_context_type, glu::ApiType::core(4, 0)))
        {
            /* OpenGL 4.0 entry points */
                funcs_map.insert({ "glDrawArraysIndirect",                   &ApiCoverageTestCase::TestCoverageGLCallDrawArraysIndirect });
                funcs_map.insert({ "glDrawElementsIndirect",                 &ApiCoverageTestCase::TestCoverageGLCallDrawElementsIndirect });
                funcs_map.insert({ "glUniform1d",                            &ApiCoverageTestCase::TestCoverageGLCallUniform1d });
                funcs_map.insert({ "glUniform2d",                            &ApiCoverageTestCase::TestCoverageGLCallUniform2d });
                funcs_map.insert({ "glUniform3d",                            &ApiCoverageTestCase::TestCoverageGLCallUniform3d });
                funcs_map.insert({ "glUniform4d",                            &ApiCoverageTestCase::TestCoverageGLCallUniform4d });
                funcs_map.insert({ "glUniform1dv",                           &ApiCoverageTestCase::TestCoverageGLCallUniform1dv });
                funcs_map.insert({ "glUniform2dv",                           &ApiCoverageTestCase::TestCoverageGLCallUniform2dv });
                funcs_map.insert({ "glUniform3dv",                           &ApiCoverageTestCase::TestCoverageGLCallUniform3dv });
                funcs_map.insert({ "glUniform4dv",                           &ApiCoverageTestCase::TestCoverageGLCallUniform4dv });
                funcs_map.insert({ "glUniformMatrix2dv",                     &ApiCoverageTestCase::TestCoverageGLCallUniformMatrix2dv });
                funcs_map.insert({ "glUniformMatrix3dv",                     &ApiCoverageTestCase::TestCoverageGLCallUniformMatrix3dv });
                funcs_map.insert({ "glUniformMatrix4dv",                     &ApiCoverageTestCase::TestCoverageGLCallUniformMatrix4dv });
                funcs_map.insert({ "glUniformMatrix2x3dv",                   &ApiCoverageTestCase::TestCoverageGLCallUniformMatrix2x3dv });
                funcs_map.insert({ "glUniformMatrix2x4dv",                   &ApiCoverageTestCase::TestCoverageGLCallUniformMatrix2x4dv });
                funcs_map.insert({ "glUniformMatrix3x2dv",                   &ApiCoverageTestCase::TestCoverageGLCallUniformMatrix3x2dv });
                funcs_map.insert({ "glUniformMatrix3x4dv",                   &ApiCoverageTestCase::TestCoverageGLCallUniformMatrix3x4dv });
                funcs_map.insert({ "glUniformMatrix4x2dv",                   &ApiCoverageTestCase::TestCoverageGLCallUniformMatrix4x2dv });
                funcs_map.insert({ "glUniformMatrix4x3dv",                   &ApiCoverageTestCase::TestCoverageGLCallUniformMatrix4x3dv });
                funcs_map.insert({ "glGetUniformdv",                         &ApiCoverageTestCase::TestCoverageGLCallGetUniformdv });
                funcs_map.insert({ "glProgramUniform1dEXT",                  &ApiCoverageTestCase::TestCoverageGLCallProgramUniform1dEXT });
                funcs_map.insert({ "glProgramUniform2dEXT",                  &ApiCoverageTestCase::TestCoverageGLCallProgramUniform2dEXT });
                funcs_map.insert({ "glProgramUniform3dEXT",                  &ApiCoverageTestCase::TestCoverageGLCallProgramUniform3dEXT });
                funcs_map.insert({ "glProgramUniform4dEXT",                  &ApiCoverageTestCase::TestCoverageGLCallProgramUniform4dEXT });
                funcs_map.insert({ "glProgramUniform1dvEXT",                 &ApiCoverageTestCase::TestCoverageGLCallProgramUniform1dvEXT });
                funcs_map.insert({ "glProgramUniform2dvEXT",                 &ApiCoverageTestCase::TestCoverageGLCallProgramUniform2dvEXT });
                funcs_map.insert({ "glProgramUniform3dvEXT",                 &ApiCoverageTestCase::TestCoverageGLCallProgramUniform3dvEXT });
                funcs_map.insert({ "glProgramUniform4dvEXT",                 &ApiCoverageTestCase::TestCoverageGLCallProgramUniform4dvEXT });
                funcs_map.insert({ "glProgramUniformMatrix2dvEXT",           &ApiCoverageTestCase::TestCoverageGLCallProgramUniformMatrix2dvEXT });
                funcs_map.insert({ "glProgramUniformMatrix3dvEXT",           &ApiCoverageTestCase::TestCoverageGLCallProgramUniformMatrix3dvEXT });
                funcs_map.insert({ "glProgramUniformMatrix4dvEXT",           &ApiCoverageTestCase::TestCoverageGLCallProgramUniformMatrix4dvEXT });
                funcs_map.insert({ "glProgramUniformMatrix2x3dvEXT",         &ApiCoverageTestCase::TestCoverageGLCallProgramUniformMatrix2x3dvEXT });
                funcs_map.insert({ "glProgramUniformMatrix2x4dvEXT",         &ApiCoverageTestCase::TestCoverageGLCallProgramUniformMatrix2x4dvEXT });
                funcs_map.insert({ "glProgramUniformMatrix3x2dvEXT",         &ApiCoverageTestCase::TestCoverageGLCallProgramUniformMatrix3x2dvEXT });
                funcs_map.insert({ "glProgramUniformMatrix3x4dvEXT",         &ApiCoverageTestCase::TestCoverageGLCallProgramUniformMatrix3x4dvEXT });
                funcs_map.insert({ "glProgramUniformMatrix4x2dvEXT",         &ApiCoverageTestCase::TestCoverageGLCallProgramUniformMatrix4x2dvEXT });
                funcs_map.insert({ "glProgramUniformMatrix4x3dvEXT",         &ApiCoverageTestCase::TestCoverageGLCallProgramUniformMatrix4x3dvEXT });
                funcs_map.insert({ "glGetSubroutineUniformLocation",         &ApiCoverageTestCase::TestCoverageGLCallGetSubroutineUniformLocation });
                funcs_map.insert({ "glGetSubroutineIndex",                   &ApiCoverageTestCase::TestCoverageGLCallGetSubroutineIndex });
                funcs_map.insert({ "glGetActiveSubroutineUniformiv",         &ApiCoverageTestCase::TestCoverageGLCallGetActiveSubroutineUniformiv });
                funcs_map.insert({ "glGetActiveSubroutineUniformName",       &ApiCoverageTestCase::TestCoverageGLCallGetActiveSubroutineUniformName });
                funcs_map.insert({ "glGetActiveSubroutineName",              &ApiCoverageTestCase::TestCoverageGLCallGetActiveSubroutineName });
                funcs_map.insert({ "glUniformSubroutinesuiv",                &ApiCoverageTestCase::TestCoverageGLCallUniformSubroutinesuiv });
                funcs_map.insert({ "glGetUniformSubroutineuiv",              &ApiCoverageTestCase::TestCoverageGLCallGetUniformSubroutineuiv });
                funcs_map.insert({ "glGetProgramStageiv",                    &ApiCoverageTestCase::TestCoverageGLCallGetProgramStageiv });
                funcs_map.insert({ "glPatchParameteri",                      &ApiCoverageTestCase::TestCoverageGLCallPatchParameteri });
                funcs_map.insert({ "glPatchParameterfv",                     &ApiCoverageTestCase::TestCoverageGLCallPatchParameterfv });
                funcs_map.insert({ "glBindTransformFeedback",                &ApiCoverageTestCase::TestCoverageGLCallBindTransformFeedback }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glDeleteTransformFeedbacks",             &ApiCoverageTestCase::TestCoverageGLCallDeleteTransformFeedbacks }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glGenTransformFeedbacks",                &ApiCoverageTestCase::TestCoverageGLCallGenTransformFeedbacks }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glIsTransformFeedback",                  &ApiCoverageTestCase::TestCoverageGLCallIsTransformFeedback }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glPauseTransformFeedback",               &ApiCoverageTestCase::TestCoverageGLCallPauseTransformFeedback }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glResumeTransformFeedback",              &ApiCoverageTestCase::TestCoverageGLCallResumeTransformFeedback }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glDrawTransformFeedback",                &ApiCoverageTestCase::TestCoverageGLCallDrawTransformFeedback });
                funcs_map.insert({ "glDrawTransformFeedbackStream",          &ApiCoverageTestCase::TestCoverageGLCallDrawTransformFeedbackStream });
                funcs_map.insert({ "glBeginQueryIndexed",                    &ApiCoverageTestCase::TestCoverageGLCallBeginQueryIndexed });
                funcs_map.insert({ "glEndQueryIndexed",                      &ApiCoverageTestCase::TestCoverageGLCallEndQueryIndexed });
                funcs_map.insert({ "glGetQueryIndexediv",                    &ApiCoverageTestCase::TestCoverageGLCallGetQueryIndexediv });
        }

        if (glu::contextSupports(m_context_type, glu::ApiType::core(3, 3)))
        {
            /* OpenGL 3.3 entry points */
                funcs_map.insert({ "glBindFragDataLocationIndexed",          &ApiCoverageTestCase::TestCoverageGLCallBindFragDataLocationIndexed });
                funcs_map.insert({ "glGetFragDataIndex",                     &ApiCoverageTestCase::TestCoverageGLCallGetFragDataIndex });
                funcs_map.insert({ "glGenSamplers",                          &ApiCoverageTestCase::TestCoverageGLCallGenSamplers });     /* Shared with OpenGL ES */
                funcs_map.insert({ "glDeleteSamplers",                       &ApiCoverageTestCase::TestCoverageGLCallDeleteSamplers });  /* Shared with OpenGL ES */
                funcs_map.insert({ "glIsSampler",                            &ApiCoverageTestCase::TestCoverageGLCallIsSampler });       /* Shared with OpenGL ES */
                funcs_map.insert({ "glBindSampler",                          &ApiCoverageTestCase::TestCoverageGLCallBindSampler });     /* Shared with OpenGL ES */
                funcs_map.insert({ "glSamplerParameteri",                    &ApiCoverageTestCase::TestCoverageGLCallSamplerParameteri }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glSamplerParameteriv",                   &ApiCoverageTestCase::TestCoverageGLCallSamplerParameteriv }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glSamplerParameterf",                    &ApiCoverageTestCase::TestCoverageGLCallSamplerParameterf }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glSamplerParameterfv",                   &ApiCoverageTestCase::TestCoverageGLCallSamplerParameterfv }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glSamplerParameterIiv",                  &ApiCoverageTestCase::TestCoverageGLCallSamplerParameterIiv });
                funcs_map.insert({ "glSamplerParameterIuiv",                 &ApiCoverageTestCase::TestCoverageGLCallSamplerParameterIuiv });
                funcs_map.insert({ "glGetSamplerParameteriv",                &ApiCoverageTestCase::TestCoverageGLCallGetSamplerParameteriv }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glGetSamplerParameterIiv",               &ApiCoverageTestCase::TestCoverageGLCallGetSamplerParameterIiv });
                funcs_map.insert({ "glGetSamplerParameterfv",                &ApiCoverageTestCase::TestCoverageGLCallGetSamplerParameterfv }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glGetSamplerParameterIfv",               &ApiCoverageTestCase::TestCoverageGLCallGetSamplerParameterIfv });
                funcs_map.insert({ "glQueryCounter",                         &ApiCoverageTestCase::TestCoverageGLCallQueryCounter });
                funcs_map.insert({ "glGetQueryObjecti64v",                   &ApiCoverageTestCase::TestCoverageGLCallGetQueryObjecti64v });
                funcs_map.insert({ "glGetQueryObjectui64v",                  &ApiCoverageTestCase::TestCoverageGLCallGetQueryObjectui64v });
                funcs_map.insert({ "glVertexP2ui",                           &ApiCoverageTestCase::TestCoverageGLCallVertexP2ui });
                funcs_map.insert({ "glVertexP2uiv",                          &ApiCoverageTestCase::TestCoverageGLCallVertexP2uiv });
                funcs_map.insert({ "glVertexP3ui",                           &ApiCoverageTestCase::TestCoverageGLCallVertexP3ui });
                funcs_map.insert({ "glVertexP3uiv",                          &ApiCoverageTestCase::TestCoverageGLCallVertexP3uiv });
                funcs_map.insert({ "glVertexP4ui",                           &ApiCoverageTestCase::TestCoverageGLCallVertexP4ui });
                funcs_map.insert({ "glVertexP4uiv",                          &ApiCoverageTestCase::TestCoverageGLCallVertexP4uiv });
                funcs_map.insert({ "glTexCoordP1ui",                         &ApiCoverageTestCase::TestCoverageGLCallTexCoordP1ui });
                funcs_map.insert({ "glTexCoordP1uiv",                        &ApiCoverageTestCase::TestCoverageGLCallTexCoordP1uiv });
                funcs_map.insert({ "glTexCoordP2ui",                         &ApiCoverageTestCase::TestCoverageGLCallTexCoordP2ui });
                funcs_map.insert({ "glTexCoordP2uiv",                        &ApiCoverageTestCase::TestCoverageGLCallTexCoordP2uiv });
                funcs_map.insert({ "glTexCoordP3ui",                         &ApiCoverageTestCase::TestCoverageGLCallTexCoordP3ui });
                funcs_map.insert({ "glTexCoordP3uiv",                        &ApiCoverageTestCase::TestCoverageGLCallTexCoordP3uiv });
                funcs_map.insert({ "glTexCoordP4ui",                         &ApiCoverageTestCase::TestCoverageGLCallTexCoordP4ui });
                funcs_map.insert({ "glTexCoordP4uiv",                        &ApiCoverageTestCase::TestCoverageGLCallTexCoordP4uiv });
                funcs_map.insert({ "glMultiTexCoordP1ui",                    &ApiCoverageTestCase::TestCoverageGLCallMultiTexCoordP1ui });
                funcs_map.insert({ "glMultiTexCoordP1uiv",                   &ApiCoverageTestCase::TestCoverageGLCallMultiTexCoordP1uiv });
                funcs_map.insert({ "glMultiTexCoordP2ui",                    &ApiCoverageTestCase::TestCoverageGLCallMultiTexCoordP2ui });
                funcs_map.insert({ "glMultiTexCoordP2uiv",                   &ApiCoverageTestCase::TestCoverageGLCallMultiTexCoordP2uiv });
                funcs_map.insert({ "glMultiTexCoordP3ui",                    &ApiCoverageTestCase::TestCoverageGLCallMultiTexCoordP3ui });
                funcs_map.insert({ "glMultiTexCoordP3uiv",                   &ApiCoverageTestCase::TestCoverageGLCallMultiTexCoordP3uiv });
                funcs_map.insert({ "glMultiTexCoordP4ui",                    &ApiCoverageTestCase::TestCoverageGLCallMultiTexCoordP4ui });
                funcs_map.insert({ "glMultiTexCoordP4uiv",                   &ApiCoverageTestCase::TestCoverageGLCallMultiTexCoordP4uiv });
                funcs_map.insert({ "glNormalP3ui",                           &ApiCoverageTestCase::TestCoverageGLCallNormalP3ui });
                funcs_map.insert({ "glNormalP3uiv",                          &ApiCoverageTestCase::TestCoverageGLCallNormalP3uiv });
                funcs_map.insert({ "glColorP3ui",                            &ApiCoverageTestCase::TestCoverageGLCallColorP3ui });
                funcs_map.insert({ "glColorP3uiv",                           &ApiCoverageTestCase::TestCoverageGLCallColorP3uiv });
                funcs_map.insert({ "glColorP4ui",                            &ApiCoverageTestCase::TestCoverageGLCallColorP4ui });
                funcs_map.insert({ "glColorP4uiv",                           &ApiCoverageTestCase::TestCoverageGLCallColorP4uiv });
                funcs_map.insert({ "glSecondaryColorP3ui",                   &ApiCoverageTestCase::TestCoverageGLCallSecondaryColorP3ui });
                funcs_map.insert({ "glSecondaryColorP3uiv",                  &ApiCoverageTestCase::TestCoverageGLCallSecondaryColorP3uiv });
                funcs_map.insert({ "glVertexAttribP1ui",                     &ApiCoverageTestCase::TestCoverageGLCallVertexAttribP1ui });
                funcs_map.insert({ "glVertexAttribP1uiv",                    &ApiCoverageTestCase::TestCoverageGLCallVertexAttribP1uiv });
                funcs_map.insert({ "glVertexAttribP2ui",                     &ApiCoverageTestCase::TestCoverageGLCallVertexAttribP2ui });
                funcs_map.insert({ "glVertexAttribP2uiv",                    &ApiCoverageTestCase::TestCoverageGLCallVertexAttribP2uiv });
                funcs_map.insert({ "glVertexAttribP3ui",                     &ApiCoverageTestCase::TestCoverageGLCallVertexAttribP3ui });
                funcs_map.insert({ "glVertexAttribP3uiv",                    &ApiCoverageTestCase::TestCoverageGLCallVertexAttribP3uiv });
                funcs_map.insert({ "glVertexAttribP4ui",                     &ApiCoverageTestCase::TestCoverageGLCallVertexAttribP4ui });
                funcs_map.insert({ "glVertexAttribP4uiv",                    &ApiCoverageTestCase::TestCoverageGLCallVertexAttribP4uiv });
        }

        if (glu::contextSupports(m_context_type, glu::ApiType::core(3, 2)))
        {
            /* OpenGL 3.2 entry points */
                funcs_map.insert({ "glGetInteger64i_v",                      &ApiCoverageTestCase::TestCoverageGLCallGetInteger64i_v });
                funcs_map.insert({ "glGetBufferParameteri64v",               &ApiCoverageTestCase::TestCoverageGLCallGetBufferParameteri64v }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glProgramParameteri",                    &ApiCoverageTestCase::TestCoverageGLCallProgramParameteri }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glFramebufferTexture",                   &ApiCoverageTestCase::TestCoverageGLCallFramebufferTexture });
                funcs_map.insert({ "glDrawElementsBaseVertex",               &ApiCoverageTestCase::TestCoverageGLCallDrawElementsBaseVertex });
                funcs_map.insert({ "glDrawRangeElementsBaseVertex",          &ApiCoverageTestCase::TestCoverageGLCallDrawRangeElementsBaseVertex });
                funcs_map.insert({ "glDrawElementsInstancedBaseVertex",      &ApiCoverageTestCase::TestCoverageGLCallDrawElementsInstancedBaseVertex });
                funcs_map.insert({ "glMultiDrawElementsBaseVertex",          &ApiCoverageTestCase::TestCoverageGLCallMultiDrawElementsBaseVertex });
                funcs_map.insert({ "glProvokingVertex",                      &ApiCoverageTestCase::TestCoverageGLCallProvokingVertex });
                funcs_map.insert({ "glFenceSync",                            &ApiCoverageTestCase::TestCoverageGLCallFenceSync });      /* Shared with OpenGL ES */
                funcs_map.insert({ "glIsSync",                               &ApiCoverageTestCase::TestCoverageGLCallIsSync });         /* Shared with OpenGL ES */
                funcs_map.insert({ "glDeleteSync",                           &ApiCoverageTestCase::TestCoverageGLCallDeleteSync });     /* Shared with OpenGL ES */
                funcs_map.insert({ "glClientWaitSync",                       &ApiCoverageTestCase::TestCoverageGLCallClientWaitSync }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glWaitSync",                             &ApiCoverageTestCase::TestCoverageGLCallWaitSync });       /* Shared with OpenGL ES */
                funcs_map.insert({ "glGetInteger64v",                        &ApiCoverageTestCase::TestCoverageGLCallGetInteger64v });  /* Shared with OpenGL ES */
                funcs_map.insert({ "glGetSynciv",                            &ApiCoverageTestCase::TestCoverageGLCallGetSynciv });      /* Shared with OpenGL ES */
                funcs_map.insert({ "glTexImage2DMultisample",                &ApiCoverageTestCase::TestCoverageGLCallTexImage2DMultisample });
                funcs_map.insert({ "glTexImage3DMultisample",                &ApiCoverageTestCase::TestCoverageGLCallTexImage3DMultisample });
                funcs_map.insert({ "glGetMultisamplefv",                     &ApiCoverageTestCase::TestCoverageGLCallGetMultisamplefv });
                funcs_map.insert({ "glSampleMaski",                          &ApiCoverageTestCase::TestCoverageGLCallSampleMaski });
        }

        if (glu::contextSupports(m_context_type, glu::ApiType::core(3, 1)))
        {
            /* OpenGL 3.1 entry points */
                funcs_map.insert({ "glDrawArraysInstanced",                  &ApiCoverageTestCase::TestCoverageGLCallDrawArraysInstanced }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glDrawElementsInstanced",                &ApiCoverageTestCase::TestCoverageGLCallDrawElementsInstanced }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glTexBuffer",                            &ApiCoverageTestCase::TestCoverageGLCallTexBuffer });
                funcs_map.insert({ "glPrimitiveRestartIndex",                &ApiCoverageTestCase::TestCoverageGLCallPrimitiveRestartIndex });
                funcs_map.insert({ "glCopyBufferSubData",                    &ApiCoverageTestCase::TestCoverageGLCallCopyBufferSubData });   /* Shared with OpenGL ES */
                funcs_map.insert({ "glGetUniformIndices",                    &ApiCoverageTestCase::TestCoverageGLCallGetUniformIndices });   /* Shared with OpenGL ES */
                funcs_map.insert({ "glGetActiveUniformsiv",                  &ApiCoverageTestCase::TestCoverageGLCallGetActiveUniformsiv }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glGetActiveUniformName",                 &ApiCoverageTestCase::TestCoverageGLCallGetActiveUniformName });
                funcs_map.insert({ "glGetUniformBlockIndex",                 &ApiCoverageTestCase::TestCoverageGLCallGetUniformBlockIndex }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glGetActiveUniformBlockiv",              &ApiCoverageTestCase::TestCoverageGLCallGetActiveUniformBlockiv }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glGetActiveUniformBlockName",            &ApiCoverageTestCase::TestCoverageGLCallGetActiveUniformBlockName }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glUniformBlockBinding",                  &ApiCoverageTestCase::TestCoverageGLCallUniformBlockBinding }); /* Shared with OpenGL ES */
        }

        if (glu::contextSupports(m_context_type, glu::ApiType::core(3, 0)))
        {
            /* OpenGL 3.0 entry points */
                funcs_map.insert({ "glColorMaski",                           &ApiCoverageTestCase::TestCoverageGLCallColorMaski });
                funcs_map.insert({ "glGetBooleani_v",                        &ApiCoverageTestCase::TestCoverageGLCallGetBooleani_v });
                funcs_map.insert({ "glGetIntegeri_v",                        &ApiCoverageTestCase::TestCoverageGLCallGetIntegeri_v });
                funcs_map.insert({ "glEnablei",                              &ApiCoverageTestCase::TestCoverageGLCallEnablei });
                funcs_map.insert({ "glDisablei",                             &ApiCoverageTestCase::TestCoverageGLCallDisablei });
                funcs_map.insert({ "glIsEnabledi",                           &ApiCoverageTestCase::TestCoverageGLCallIsEnabledi });
                funcs_map.insert({ "glBeginTransformFeedback",               &ApiCoverageTestCase::TestCoverageGLCallBeginTransformFeedback }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glEndTransformFeedback",                 &ApiCoverageTestCase::TestCoverageGLCallEndTransformFeedback }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glBindBufferRange",                      &ApiCoverageTestCase::TestCoverageGLCallBindBufferRange }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glBindBufferBase",                       &ApiCoverageTestCase::TestCoverageGLCallBindBufferBase });  /* Shared with OpenGL ES */
                funcs_map.insert({ "glTransformFeedbackVaryings",            &ApiCoverageTestCase::TestCoverageGLCallTransformFeedbackVaryings }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glGetTransformFeedbackVarying",          &ApiCoverageTestCase::TestCoverageGLCallGetTransformFeedbackVarying }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glClampColor",                           &ApiCoverageTestCase::TestCoverageGLCallClampColor });
                funcs_map.insert({ "glBeginConditionalRender",               &ApiCoverageTestCase::TestCoverageGLCallBeginConditionalRender });
                funcs_map.insert({ "glEndConditionalRender",                 &ApiCoverageTestCase::TestCoverageGLCallEndConditionalRender });
                funcs_map.insert({ "glVertexAttribIPointer",                 &ApiCoverageTestCase::TestCoverageGLCallVertexAttribIPointer }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glGetVertexAttribIiv",                   &ApiCoverageTestCase::TestCoverageGLCallGetVertexAttribIiv }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glGetVertexAttribIuiv",                  &ApiCoverageTestCase::TestCoverageGLCallGetVertexAttribIuiv }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glVertexAttribI1i",                      &ApiCoverageTestCase::TestCoverageGLCallVertexAttribI1i });
                funcs_map.insert({ "glVertexAttribI2i",                      &ApiCoverageTestCase::TestCoverageGLCallVertexAttribI2i });
                funcs_map.insert({ "glVertexAttribI3i",                      &ApiCoverageTestCase::TestCoverageGLCallVertexAttribI3i });
                funcs_map.insert({ "glVertexAttribI4i",                      &ApiCoverageTestCase::TestCoverageGLCallVertexAttribI4i }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glVertexAttribI1ui",                     &ApiCoverageTestCase::TestCoverageGLCallVertexAttribI1ui });
                funcs_map.insert({ "glVertexAttribI2ui",                     &ApiCoverageTestCase::TestCoverageGLCallVertexAttribI2ui });
                funcs_map.insert({ "glVertexAttribI3ui",                     &ApiCoverageTestCase::TestCoverageGLCallVertexAttribI3ui });
                funcs_map.insert({ "glVertexAttribI4ui",                     &ApiCoverageTestCase::TestCoverageGLCallVertexAttribI4ui }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glVertexAttribI1iv",                     &ApiCoverageTestCase::TestCoverageGLCallVertexAttribI1iv });
                funcs_map.insert({ "glVertexAttribI2iv",                     &ApiCoverageTestCase::TestCoverageGLCallVertexAttribI2iv });
                funcs_map.insert({ "glVertexAttribI3iv",                     &ApiCoverageTestCase::TestCoverageGLCallVertexAttribI3iv });
                funcs_map.insert({ "glVertexAttribI4iv",                     &ApiCoverageTestCase::TestCoverageGLCallVertexAttribI4iv }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glVertexAttribI1uiv",                    &ApiCoverageTestCase::TestCoverageGLCallVertexAttribI1uiv });
                funcs_map.insert({ "glVertexAttribI2uiv",                    &ApiCoverageTestCase::TestCoverageGLCallVertexAttribI2uiv });
                funcs_map.insert({ "glVertexAttribI3uiv",                    &ApiCoverageTestCase::TestCoverageGLCallVertexAttribI3uiv });
                funcs_map.insert({ "glVertexAttribI4uiv",                    &ApiCoverageTestCase::TestCoverageGLCallVertexAttribI4uiv }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glVertexAttribI4bv",                     &ApiCoverageTestCase::TestCoverageGLCallVertexAttribI4bv });
                funcs_map.insert({ "glVertexAttribI4sv",                     &ApiCoverageTestCase::TestCoverageGLCallVertexAttribI4sv });
                funcs_map.insert({ "glVertexAttribI4ubv",                    &ApiCoverageTestCase::TestCoverageGLCallVertexAttribI4ubv });
                funcs_map.insert({ "glVertexAttribI4usv",                    &ApiCoverageTestCase::TestCoverageGLCallVertexAttribI4usv });
                funcs_map.insert({ "glGetUniformuiv",                        &ApiCoverageTestCase::TestCoverageGLCallGetUniformuiv }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glBindFragDataLocation",                 &ApiCoverageTestCase::TestCoverageGLCallBindFragDataLocation });
                funcs_map.insert({ "glGetFragDataLocation",                  &ApiCoverageTestCase::TestCoverageGLCallGetFragDataLocation }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glUniform1ui",                           &ApiCoverageTestCase::TestCoverageGLCallUniform1ui });
                funcs_map.insert({ "glUniform2ui",                           &ApiCoverageTestCase::TestCoverageGLCallUniform2ui }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glUniform3ui",                           &ApiCoverageTestCase::TestCoverageGLCallUniform3ui });
                funcs_map.insert({ "glUniform4ui",                           &ApiCoverageTestCase::TestCoverageGLCallUniform4ui });
                funcs_map.insert({ "glUniform1uiv",                          &ApiCoverageTestCase::TestCoverageGLCallUniform1uiv });
                funcs_map.insert({ "glUniform2uiv",                          &ApiCoverageTestCase::TestCoverageGLCallUniform2uiv });    /* Shared with OpenGL ES */
                funcs_map.insert({ "glUniform3uiv",                          &ApiCoverageTestCase::TestCoverageGLCallUniform3uiv });
                funcs_map.insert({ "glUniform4uiv",                          &ApiCoverageTestCase::TestCoverageGLCallUniform4uiv });
                funcs_map.insert({ "glTexParameterIiv",                      &ApiCoverageTestCase::TestCoverageGLCallTexParameterIiv });
                funcs_map.insert({ "glTexParameterIuiv",                     &ApiCoverageTestCase::TestCoverageGLCallTexParameterIuiv });
                funcs_map.insert({ "glGetTexParameterIiv",                   &ApiCoverageTestCase::TestCoverageGLCallGetTexParameterIiv });
                funcs_map.insert({ "glGetTexParameterIuiv",                  &ApiCoverageTestCase::TestCoverageGLCallGetTexParameterIuiv });
                funcs_map.insert({ "glClearBufferiv",                        &ApiCoverageTestCase::TestCoverageGLCallClearBufferiv });   /* Shared with OpenGL ES */
                funcs_map.insert({ "glClearBufferuiv",                       &ApiCoverageTestCase::TestCoverageGLCallClearBufferuiv });  /* Shared with OpenGL ES */
                funcs_map.insert({ "glClearBufferfv",                        &ApiCoverageTestCase::TestCoverageGLCallClearBufferfv });   /* Shared with OpenGL ES */
                funcs_map.insert({ "glClearBufferfi",                        &ApiCoverageTestCase::TestCoverageGLCallClearBufferfi });   /* Shared with OpenGL ES */
                funcs_map.insert({ "glGetStringi",                           &ApiCoverageTestCase::TestCoverageGLCallGetStringi });      /* Shared with OpenGL ES */
                funcs_map.insert({ "glIsRenderbuffer",                       &ApiCoverageTestCase::TestCoverageGLCallIsRenderbuffer });  /* Shared with OpenGL ES */
                funcs_map.insert({ "glBindRenderbuffer",                     &ApiCoverageTestCase::TestCoverageGLCallBindRenderbuffer });        /* Shared with OpenGL ES */
                funcs_map.insert({ "glDeleteRenderbuffers",                  &ApiCoverageTestCase::TestCoverageGLCallDeleteRenderbuffers });     /* Shared with OpenGL ES */
                funcs_map.insert({ "glGenRenderbuffers",                     &ApiCoverageTestCase::TestCoverageGLCallGenRenderbuffers });        /* Shared with OpenGL ES */
                funcs_map.insert({ "glRenderbufferStorage",                  &ApiCoverageTestCase::TestCoverageGLCallRenderbufferStorage });     /* Shared with OpenGL ES */
                funcs_map.insert({ "glGetRenderbufferParameteriv",           &ApiCoverageTestCase::TestCoverageGLCallGetRenderbufferParameteriv });      /* Shared with OpenGL ES */
                funcs_map.insert({ "glIsFramebuffer",                        &ApiCoverageTestCase::TestCoverageGLCallIsFramebuffer });   /* Shared with OpenGL ES */
                funcs_map.insert({ "glBindFramebuffer",                      &ApiCoverageTestCase::TestCoverageGLCallBindFramebuffer }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glDeleteFramebuffers",                   &ApiCoverageTestCase::TestCoverageGLCallDeleteFramebuffers });      /* Shared with OpenGL ES */
                funcs_map.insert({ "glGenFramebuffers",                      &ApiCoverageTestCase::TestCoverageGLCallGenFramebuffers }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glCheckFramebufferStatus",               &ApiCoverageTestCase::TestCoverageGLCallCheckFramebufferStatus });  /* Shared with OpenGL ES */
                funcs_map.insert({ "glFramebufferTexture1D",                 &ApiCoverageTestCase::TestCoverageGLCallFramebufferTexture1D });
                funcs_map.insert({ "glFramebufferTexture2D",                 &ApiCoverageTestCase::TestCoverageGLCallFramebufferTexture2D });    /* Shared with OpenGL ES */
                funcs_map.insert({ "glFramebufferTexture3D",                 &ApiCoverageTestCase::TestCoverageGLCallFramebufferTexture3D });
                funcs_map.insert({ "glFramebufferRenderbuffer",              &ApiCoverageTestCase::TestCoverageGLCallFramebufferRenderbuffer }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glGetFramebufferAttachmentParameteriv",  &ApiCoverageTestCase::TestCoverageGLCallGetFramebufferAttachmentParameteriv });     /* Shared with OpenGL ES */
                funcs_map.insert({ "glGenerateMipmap",                       &ApiCoverageTestCase::TestCoverageGLCallGenerateMipmap });  /* Shared with OpenGL ES */
                funcs_map.insert({ "glBlitFramebuffer",                      &ApiCoverageTestCase::TestCoverageGLCallBlitFramebuffer }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glRenderbufferStorageMultisample",       &ApiCoverageTestCase::TestCoverageGLCallRenderbufferStorageMultisample }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glFramebufferTextureLayer",              &ApiCoverageTestCase::TestCoverageGLCallFramebufferTextureLayer });
                funcs_map.insert({ "glMapBufferRange",                       &ApiCoverageTestCase::TestCoverageGLCallMapBufferRange }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glFlushMappedBufferRange",               &ApiCoverageTestCase::TestCoverageGLCallFlushMappedBufferRange }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glBindVertexArray",                      &ApiCoverageTestCase::TestCoverageGLCallBindVertexArray }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glDeleteVertexArrays",                   &ApiCoverageTestCase::TestCoverageGLCallDeleteVertexArrays }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glGenVertexArrays",                      &ApiCoverageTestCase::TestCoverageGLCallGenVertexArrays }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glIsVertexArray",                        &ApiCoverageTestCase::TestCoverageGLCallIsVertexArray }); /* Shared with OpenGL ES */
            /* OpenGL 1.0-2.1 entry points */
                funcs_map.insert({ "glCullFace",                             &ApiCoverageTestCase::TestCoverageGLCallCullFace });        /* Shared with OpenGL ES */
                funcs_map.insert({ "glFrontFace",                            &ApiCoverageTestCase::TestCoverageGLCallFrontFace });       /* Shared with OpenGL ES */
                funcs_map.insert({ "glHint",                                 &ApiCoverageTestCase::TestCoverageGLCallHint });    /* Shared with OpenGL ES */
                funcs_map.insert({ "glLineWidth",                            &ApiCoverageTestCase::TestCoverageGLCallLineWidth });       /* Shared with OpenGL ES */
                funcs_map.insert({ "glPointSize",                            &ApiCoverageTestCase::TestCoverageGLCallPointSize });
                funcs_map.insert({ "glPolygonMode",                          &ApiCoverageTestCase::TestCoverageGLCallPolygonMode });
                funcs_map.insert({ "glScissor",                              &ApiCoverageTestCase::TestCoverageGLCallScissor }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glTexParameterf",                        &ApiCoverageTestCase::TestCoverageGLCallTexParameterf });   /* Shared with OpenGL ES */
                funcs_map.insert({ "glTexParameterfv",                       &ApiCoverageTestCase::TestCoverageGLCallTexParameterfv });  /* Shared with OpenGL ES */
                funcs_map.insert({ "glTexParameteri",                        &ApiCoverageTestCase::TestCoverageGLCallTexParameteri });   /* Shared with OpenGL ES */
                funcs_map.insert({ "glTexParameteriv",                       &ApiCoverageTestCase::TestCoverageGLCallTexParameteriv });  /* Shared with OpenGL ES */
                funcs_map.insert({ "glTexImage1D",                           &ApiCoverageTestCase::TestCoverageGLCallTexImage1D });
                funcs_map.insert({ "glTexImage2D",                           &ApiCoverageTestCase::TestCoverageGLCallTexImage2D });      /* Shared with OpenGL ES */
                funcs_map.insert({ "glDrawBuffer",                           &ApiCoverageTestCase::TestCoverageGLCallDrawBuffer });
                funcs_map.insert({ "glClear",                                &ApiCoverageTestCase::TestCoverageGLCallClear });   /* Shared with OpenGL ES */
                funcs_map.insert({ "glClearColor",                           &ApiCoverageTestCase::TestCoverageGLCallClearColor });      /* Shared with OpenGL ES */
                funcs_map.insert({ "glClearStencil",                         &ApiCoverageTestCase::TestCoverageGLCallClearStencil });    /* Shared with OpenGL ES */
                funcs_map.insert({ "glClearDepth",                           &ApiCoverageTestCase::TestCoverageGLCallClearDepth });
                funcs_map.insert({ "glStencilMask",                          &ApiCoverageTestCase::TestCoverageGLCallStencilMask });     /* Shared with OpenGL ES */
                funcs_map.insert({ "glColorMask",                            &ApiCoverageTestCase::TestCoverageGLCallColorMask });       /* Shared with OpenGL ES */
                funcs_map.insert({ "glDepthMask",                            &ApiCoverageTestCase::TestCoverageGLCallDepthMask });       /* Shared with OpenGL ES */
                funcs_map.insert({ "glDisable",                              &ApiCoverageTestCase::TestCoverageGLCallDisable }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glEnable",                               &ApiCoverageTestCase::TestCoverageGLCallEnable });  /* Shared with OpenGL ES */
                funcs_map.insert({ "glFinish",                               &ApiCoverageTestCase::TestCoverageGLCallFinish });  /* Shared with OpenGL ES */
                funcs_map.insert({ "glFlush",                                &ApiCoverageTestCase::TestCoverageGLCallFlush });   /* Shared with OpenGL ES */
                funcs_map.insert({ "glBlendFunc",                            &ApiCoverageTestCase::TestCoverageGLCallBlendFunc });       /* Shared with OpenGL ES */
                funcs_map.insert({ "glLogicOp",                              &ApiCoverageTestCase::TestCoverageGLCallLogicOp });
                funcs_map.insert({ "glStencilFunc",                          &ApiCoverageTestCase::TestCoverageGLCallStencilFunc });     /* Shared with OpenGL ES */
                funcs_map.insert({ "glStencilOp",                            &ApiCoverageTestCase::TestCoverageGLCallStencilOp });       /* Shared with OpenGL ES */
                funcs_map.insert({ "glDepthFunc",                            &ApiCoverageTestCase::TestCoverageGLCallDepthFunc });       /* Shared with OpenGL ES */
                funcs_map.insert({ "glPixelStoref",                          &ApiCoverageTestCase::TestCoverageGLCallPixelStoref });
                funcs_map.insert({ "glPixelStorei",                          &ApiCoverageTestCase::TestCoverageGLCallPixelStorei });     /* Shared with OpenGL ES */
                funcs_map.insert({ "glReadBuffer",                           &ApiCoverageTestCase::TestCoverageGLCallReadBuffer });      /* Shared with OpenGL ES */
                funcs_map.insert({ "glReadPixels",                           &ApiCoverageTestCase::TestCoverageGLCallReadPixels });      /* Shared with OpenGL ES */
                funcs_map.insert({ "glGetBooleanv",                          &ApiCoverageTestCase::TestCoverageGLCallGetBooleanv });     /* Shared with OpenGL ES */
                funcs_map.insert({ "glGetDoublev",                           &ApiCoverageTestCase::TestCoverageGLCallGetDoublev });
                funcs_map.insert({ "gl.getError",                             &ApiCoverageTestCase::TestCoverageGLCallGetError });        /* Shared with OpenGL ES */
                funcs_map.insert({ "glGetFloatv",                            &ApiCoverageTestCase::TestCoverageGLCallGetFloatv });       /* Shared with OpenGL ES */
                funcs_map.insert({ "glGetIntegerv",                          &ApiCoverageTestCase::TestCoverageGLCallGetIntegerv });     /* Shared with OpenGL ES */
                funcs_map.insert({ "glGetString",                            &ApiCoverageTestCase::TestCoverageGLCallGetString });       /* Shared with OpenGL ES */
                funcs_map.insert({ "glGetTexImage",                          &ApiCoverageTestCase::TestCoverageGLCallGetTexImage });
                funcs_map.insert({ "glGetTexParameterfv",                    &ApiCoverageTestCase::TestCoverageGLCallGetTexParameterfv });       /* Shared with OpenGL ES */
                funcs_map.insert({ "glGetTexParameteriv",                    &ApiCoverageTestCase::TestCoverageGLCallGetTexParameteriv });       /* Shared with OpenGL ES */
                funcs_map.insert({ "glGetTexLevelParameterfv",               &ApiCoverageTestCase::TestCoverageGLCallGetTexLevelParameterfv });
                funcs_map.insert({ "glGetTexLevelParameteriv",               &ApiCoverageTestCase::TestCoverageGLCallGetTexLevelParameteriv });
                funcs_map.insert({ "glIsEnabled",                            &ApiCoverageTestCase::TestCoverageGLCallIsEnabled });       /* Shared with OpenGL ES */
                funcs_map.insert({ "glIsProgram",                            &ApiCoverageTestCase::TestCoverageGLCallIsProgram });
                funcs_map.insert({ "glIsShader",                             &ApiCoverageTestCase::TestCoverageGLCallIsShader });
                funcs_map.insert({ "glLinkProgram",                          &ApiCoverageTestCase::TestCoverageGLCallLinkProgram });
                funcs_map.insert({ "glShaderSource",                         &ApiCoverageTestCase::TestCoverageGLCallShaderSource });
                funcs_map.insert({ "glDepthRange",                           &ApiCoverageTestCase::TestCoverageGLCallDepthRange });
                funcs_map.insert({ "glViewport",                             &ApiCoverageTestCase::TestCoverageGLCallViewport });        /* Shared with OpenGL ES */
                funcs_map.insert({ "glDrawArrays",                           &ApiCoverageTestCase::TestCoverageGLCallDrawArrays });      /* Shared with OpenGL ES */
                funcs_map.insert({ "glDrawElements",                         &ApiCoverageTestCase::TestCoverageGLCallDrawElements });    /* Shared with OpenGL ES */
                funcs_map.insert({ "glGetPointerv",                          &ApiCoverageTestCase::TestCoverageGLCallGetPointerv });
                funcs_map.insert({ "glPolygonOffset",                        &ApiCoverageTestCase::TestCoverageGLCallPolygonOffset });   /* Shared with OpenGL ES */
                funcs_map.insert({ "glCopyTexImage1D",                       &ApiCoverageTestCase::TestCoverageGLCallCopyTexImage1D });
                funcs_map.insert({ "glCopyTexImage2D",                       &ApiCoverageTestCase::TestCoverageGLCallCopyTexImage2D });  /* Shared with OpenGL ES */
                funcs_map.insert({ "glCopyTexSubImage1D",                    &ApiCoverageTestCase::TestCoverageGLCallCopyTexSubImage1D });
                funcs_map.insert({ "glCopyTexSubImage2D",                    &ApiCoverageTestCase::TestCoverageGLCallCopyTexSubImage2D });       /* Shared with OpenGL ES */
                funcs_map.insert({ "glTexSubImage1D",                        &ApiCoverageTestCase::TestCoverageGLCallTexSubImage1D });
                funcs_map.insert({ "glTexSubImage2D",                        &ApiCoverageTestCase::TestCoverageGLCallTexSubImage2D });   /* Shared with OpenGL ES */
                funcs_map.insert({ "glBindTexture",                          &ApiCoverageTestCase::TestCoverageGLCallBindTexture });     /* Shared with OpenGL ES */
                funcs_map.insert({ "glDeleteTextures",                       &ApiCoverageTestCase::TestCoverageGLCallDeleteTextures });  /* Shared with OpenGL ES */
                funcs_map.insert({ "glGenTextures",                          &ApiCoverageTestCase::TestCoverageGLCallGenTextures });     /* Shared with OpenGL ES */
                funcs_map.insert({ "glIsTexture",                            &ApiCoverageTestCase::TestCoverageGLCallIsTexture });       /* Shared with OpenGL ES */
                funcs_map.insert({ "glBlendColor",                           &ApiCoverageTestCase::TestCoverageGLCallBlendColor });      /* Shared with OpenGL ES */
                funcs_map.insert({ "glBlendEquation",                        &ApiCoverageTestCase::TestCoverageGLCallBlendEquation });   /* Shared with OpenGL ES */
                funcs_map.insert({ "glDrawRangeElements",                    &ApiCoverageTestCase::TestCoverageGLCallDrawRangeElements }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glTexImage3D",                           &ApiCoverageTestCase::TestCoverageGLCallTexImage3D });      /* Shared with OpenGL ES */
                funcs_map.insert({ "glTexSubImage3D",                        &ApiCoverageTestCase::TestCoverageGLCallTexSubImage3D });   /* Shared with OpenGL ES */
                funcs_map.insert({ "glCopyTexSubImage3D",                    &ApiCoverageTestCase::TestCoverageGLCallCopyTexSubImage3D }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glActiveTexture",                        &ApiCoverageTestCase::TestCoverageGLCallActiveTexture });   /* Shared with OpenGL ES */
                funcs_map.insert({ "glSampleCoverage",                       &ApiCoverageTestCase::TestCoverageGLCallSampleCoverage });  /* Shared with OpenGL ES */
                funcs_map.insert({ "glCompressedTexImage3D",                 &ApiCoverageTestCase::TestCoverageGLCallCompressedTexImage3D }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glCompressedTexImage2D",                 &ApiCoverageTestCase::TestCoverageGLCallCompressedTexImage2D });    /* Shared with OpenGL ES */
                funcs_map.insert({ "glCompressedTexImage1D",                 &ApiCoverageTestCase::TestCoverageGLCallCompressedTexImage1D });
                funcs_map.insert({ "glCompressedTexSubImage3D",              &ApiCoverageTestCase::TestCoverageGLCallCompressedTexSubImage3D }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glCompressedTexSubImage2D",              &ApiCoverageTestCase::TestCoverageGLCallCompressedTexSubImage2D }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glCompressedTexSubImage1D",              &ApiCoverageTestCase::TestCoverageGLCallCompressedTexSubImage1D });
                funcs_map.insert({ "glGetCompressedTexImage",                &ApiCoverageTestCase::TestCoverageGLCallGetCompressedTexImage });
                funcs_map.insert({ "glBlendFuncSeparate",                    &ApiCoverageTestCase::TestCoverageGLCallBlendFuncSeparate });       /* Shared with OpenGL ES */
                funcs_map.insert({ "glMultiDrawArrays",                      &ApiCoverageTestCase::TestCoverageGLCallMultiDrawArrays });
                funcs_map.insert({ "glMultiDrawElements",                    &ApiCoverageTestCase::TestCoverageGLCallMultiDrawElements });
                funcs_map.insert({ "glPointParameterf",                      &ApiCoverageTestCase::TestCoverageGLCallPointParameterf });
                funcs_map.insert({ "glPointParameterfv",                     &ApiCoverageTestCase::TestCoverageGLCallPointParameterfv });
                funcs_map.insert({ "glPointParameteri",                      &ApiCoverageTestCase::TestCoverageGLCallPointParameteri });
                funcs_map.insert({ "glPointParameteriv",                     &ApiCoverageTestCase::TestCoverageGLCallPointParameteriv });
                funcs_map.insert({ "glGenQueries",                           &ApiCoverageTestCase::TestCoverageGLCallGenQueries });      /* Shared with OpenGL ES */
                funcs_map.insert({ "glDeleteQueries",                        &ApiCoverageTestCase::TestCoverageGLCallDeleteQueries });   /* Shared with OpenGL ES */
                funcs_map.insert({ "glIsQuery",                              &ApiCoverageTestCase::TestCoverageGLCallIsQuery });         /* Shared with OpenGL ES */
                funcs_map.insert({ "glBeginQuery",                           &ApiCoverageTestCase::TestCoverageGLCallBeginQuery });      /* Shared with OpenGL ES */
                funcs_map.insert({ "glEndQuery",                             &ApiCoverageTestCase::TestCoverageGLCallEndQuery });        /* Shared with OpenGL ES */
                funcs_map.insert({ "glGetQueryiv",                           &ApiCoverageTestCase::TestCoverageGLCallGetQueryiv });      /* Shared with OpenGL ES */
                funcs_map.insert({ "glGetQueryObjectiv",                     &ApiCoverageTestCase::TestCoverageGLCallGetQueryObjectiv });
                funcs_map.insert({ "glGetQueryObjectuiv",                    &ApiCoverageTestCase::TestCoverageGLCallGetQueryObjectuiv }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glBindBuffer",                           &ApiCoverageTestCase::TestCoverageGLCallBindBuffer });      /* Shared with OpenGL ES */
                funcs_map.insert({ "glDeleteBuffers",                        &ApiCoverageTestCase::TestCoverageGLCallDeleteBuffers });   /* Shared with OpenGL ES */
                funcs_map.insert({ "glGenBuffers",                           &ApiCoverageTestCase::TestCoverageGLCallGenBuffers });      /* Shared with OpenGL ES */
                funcs_map.insert({ "glIsBuffer",                             &ApiCoverageTestCase::TestCoverageGLCallIsBuffer });        /* Shared with OpenGL ES */
                funcs_map.insert({ "glBufferData",                           &ApiCoverageTestCase::TestCoverageGLCallBufferData });      /* Shared with OpenGL ES */
                funcs_map.insert({ "glBufferSubData",                        &ApiCoverageTestCase::TestCoverageGLCallBufferSubData });   /* Shared with OpenGL ES */
                funcs_map.insert({ "glGetBufferSubData",                     &ApiCoverageTestCase::TestCoverageGLCallGetBufferSubData });        /* Shared with OpenGL ES */
                funcs_map.insert({ "glMapBuffer",                            &ApiCoverageTestCase::TestCoverageGLCallMapBuffer });
                funcs_map.insert({ "glUnmapBuffer",                          &ApiCoverageTestCase::TestCoverageGLCallUnmapBuffer });      /* Shared with OpenGL ES */
                funcs_map.insert({ "glGetBufferParameteriv",                 &ApiCoverageTestCase::TestCoverageGLCallGetBufferParameteriv });    /* Shared with OpenGL ES */
                funcs_map.insert({ "glGetBufferPointerv",                    &ApiCoverageTestCase::TestCoverageGLCallGetBufferPointerv }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glBlendEquationSeparate",                &ApiCoverageTestCase::TestCoverageGLCallBlendEquationSeparate });   /* Shared with OpenGL ES */
                funcs_map.insert({ "glDrawBuffers",                          &ApiCoverageTestCase::TestCoverageGLCallDrawBuffers });    /* Shared with OpenGL ES */
                funcs_map.insert({ "glStencilOpSeparate",                    &ApiCoverageTestCase::TestCoverageGLCallStencilOpSeparate });       /* Shared with OpenGL ES */
                funcs_map.insert({ "glStencilFuncSeparate",                  &ApiCoverageTestCase::TestCoverageGLCallStencilFuncSeparate });     /* Shared with OpenGL ES */
                funcs_map.insert({ "glStencilMaskSeparate",                  &ApiCoverageTestCase::TestCoverageGLCallStencilMaskSeparate });     /* Shared with OpenGL ES */
                funcs_map.insert({ "glAttachShader",                         &ApiCoverageTestCase::TestCoverageGLCallAttachShader });    /* Shared with OpenGL ES */
                funcs_map.insert({ "glBindAttribLocation",                   &ApiCoverageTestCase::TestCoverageGLCallBindAttribLocation });      /* Shared with OpenGL ES */
                funcs_map.insert({ "glCompileShader",                        &ApiCoverageTestCase::TestCoverageGLCallCompileShader });   /* Shared with OpenGL ES */
                funcs_map.insert({ "glCreateProgram",                        &ApiCoverageTestCase::TestCoverageGLCallCreateProgram });   /* Shared with OpenGL ES */
                funcs_map.insert({ "glCreateShader",                         &ApiCoverageTestCase::TestCoverageGLCallCreateShader });    /* Shared with OpenGL ES */
                funcs_map.insert({ "glDeleteProgram",                        &ApiCoverageTestCase::TestCoverageGLCallDeleteProgram });   /* Shared with OpenGL ES */
                funcs_map.insert({ "glDeleteShader",                         &ApiCoverageTestCase::TestCoverageGLCallDeleteShader });    /* Shared with OpenGL ES */
                funcs_map.insert({ "glDetachShader",                         &ApiCoverageTestCase::TestCoverageGLCallDetachShader });    /* Shared with OpenGL ES */
                funcs_map.insert({ "glDisableVertexAttribArray",             &ApiCoverageTestCase::TestCoverageGLCallDisableVertexAttribArray });        /* Shared with OpenGL ES */
                funcs_map.insert({ "glEnableVertexAttribArray",              &ApiCoverageTestCase::TestCoverageGLCallEnableVertexAttribArray }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glGetActiveAttrib",                      &ApiCoverageTestCase::TestCoverageGLCallGetActiveAttrib }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glGetActiveUniform",                     &ApiCoverageTestCase::TestCoverageGLCallGetActiveUniform });        /* Shared with OpenGL ES */
                funcs_map.insert({ "glGetAttachedShaders",                   &ApiCoverageTestCase::TestCoverageGLCallGetAttachedShaders });      /* Shared with OpenGL ES */
                funcs_map.insert({ "glGetAttribLocation",                    &ApiCoverageTestCase::TestCoverageGLCallGetAttribLocation });       /* Shared with OpenGL ES */
                funcs_map.insert({ "glGetProgramiv",                         &ApiCoverageTestCase::TestCoverageGLCallGetProgramiv });    /* Shared with OpenGL ES */
                funcs_map.insert({ "glGetProgramInfoLog",                    &ApiCoverageTestCase::TestCoverageGLCallGetProgramInfoLog });       /* Shared with OpenGL ES */
                funcs_map.insert({ "glGetShaderiv",                          &ApiCoverageTestCase::TestCoverageGLCallGetShaderiv });     /* Shared with OpenGL ES */
                funcs_map.insert({ "glGetShaderInfoLog",                     &ApiCoverageTestCase::TestCoverageGLCallGetShaderInfoLog });        /* Shared with OpenGL ES */
                funcs_map.insert({ "glGetShaderSource",                      &ApiCoverageTestCase::TestCoverageGLCallGetShaderSource }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glGetUniformLocation",                   &ApiCoverageTestCase::TestCoverageGLCallGetUniformLocation });      /* Shared with OpenGL ES */
                funcs_map.insert({ "glGetUniformfv",                         &ApiCoverageTestCase::TestCoverageGLCallGetUniformfv });    /* Shared with OpenGL ES */
                funcs_map.insert({ "glGetUniformiv",                         &ApiCoverageTestCase::TestCoverageGLCallGetUniformiv });    /* Shared with OpenGL ES */
                funcs_map.insert({ "glGetVertexAttribdv",                    &ApiCoverageTestCase::TestCoverageGLCallGetVertexAttribdv });
                funcs_map.insert({ "glGetVertexAttribfv",                    &ApiCoverageTestCase::TestCoverageGLCallGetVertexAttribfv });       /* Shared with OpenGL ES */
                funcs_map.insert({ "glGetVertexAttribiv",                    &ApiCoverageTestCase::TestCoverageGLCallGetVertexAttribiv });       /* Shared with OpenGL ES */
                funcs_map.insert({ "glGetVertexAttribPointerv",              &ApiCoverageTestCase::TestCoverageGLCallGetVertexAttribPointerv }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glUseProgram",                           &ApiCoverageTestCase::TestCoverageGLCallUseProgram });      /* Shared with OpenGL ES */
                funcs_map.insert({ "glUniform1f",                            &ApiCoverageTestCase::TestCoverageGLCallUniform1f });       /* Shared with OpenGL ES */
                funcs_map.insert({ "glUniform2f",                            &ApiCoverageTestCase::TestCoverageGLCallUniform2f });       /* Shared with OpenGL ES */
                funcs_map.insert({ "glUniform3f",                            &ApiCoverageTestCase::TestCoverageGLCallUniform3f });       /* Shared with OpenGL ES */
                funcs_map.insert({ "glUniform4f",                            &ApiCoverageTestCase::TestCoverageGLCallUniform4f });       /* Shared with OpenGL ES */
                funcs_map.insert({ "glUniform1i",                            &ApiCoverageTestCase::TestCoverageGLCallUniform1i });       /* Shared with OpenGL ES */
                funcs_map.insert({ "glUniform2i",                            &ApiCoverageTestCase::TestCoverageGLCallUniform2i });       /* Shared with OpenGL ES */
                funcs_map.insert({ "glUniform3i",                            &ApiCoverageTestCase::TestCoverageGLCallUniform3i });       /* Shared with OpenGL ES */
                funcs_map.insert({ "glUniform4i",                            &ApiCoverageTestCase::TestCoverageGLCallUniform4i });       /* Shared with OpenGL ES */
                funcs_map.insert({ "glUniform1fv",                           &ApiCoverageTestCase::TestCoverageGLCallUniform1fv });      /* Shared with OpenGL ES */
                funcs_map.insert({ "glUniform2fv",                           &ApiCoverageTestCase::TestCoverageGLCallUniform2fv });      /* Shared with OpenGL ES */
                funcs_map.insert({ "glUniform3fv",                           &ApiCoverageTestCase::TestCoverageGLCallUniform3fv });      /* Shared with OpenGL ES */
                funcs_map.insert({ "glUniform4fv",                           &ApiCoverageTestCase::TestCoverageGLCallUniform4fv });      /* Shared with OpenGL ES */
                funcs_map.insert({ "glUniform1iv",                           &ApiCoverageTestCase::TestCoverageGLCallUniform1iv });      /* Shared with OpenGL ES */
                funcs_map.insert({ "glUniform2iv",                           &ApiCoverageTestCase::TestCoverageGLCallUniform2iv });      /* Shared with OpenGL ES */
                funcs_map.insert({ "glUniform3iv",                           &ApiCoverageTestCase::TestCoverageGLCallUniform3iv });      /* Shared with OpenGL ES */
                funcs_map.insert({ "glUniform4iv",                           &ApiCoverageTestCase::TestCoverageGLCallUniform4iv });      /* Shared with OpenGL ES */
                funcs_map.insert({ "glUniformMatrix2fv",                     &ApiCoverageTestCase::TestCoverageGLCallUniformMatrix2fv });        /* Shared with OpenGL ES */
                funcs_map.insert({ "glUniformMatrix3fv",                     &ApiCoverageTestCase::TestCoverageGLCallUniformMatrix3fv });        /* Shared with OpenGL ES */
                funcs_map.insert({ "glUniformMatrix4fv",                     &ApiCoverageTestCase::TestCoverageGLCallUniformMatrix4fv });        /* Shared with OpenGL ES */
                funcs_map.insert({ "glValidateProgram",                      &ApiCoverageTestCase::TestCoverageGLCallValidateProgram }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glVertexAttrib1d",                       &ApiCoverageTestCase::TestCoverageGLCallVertexAttrib1d });
                funcs_map.insert({ "glVertexAttrib1dv",                      &ApiCoverageTestCase::TestCoverageGLCallVertexAttrib1dv });
                funcs_map.insert({ "glVertexAttrib1f",                       &ApiCoverageTestCase::TestCoverageGLCallVertexAttrib1f });  /* Shared with OpenGL ES */
                funcs_map.insert({ "glVertexAttrib1fv",                      &ApiCoverageTestCase::TestCoverageGLCallVertexAttrib1fv }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glVertexAttrib1s",                       &ApiCoverageTestCase::TestCoverageGLCallVertexAttrib1s });
                funcs_map.insert({ "glVertexAttrib1sv",                      &ApiCoverageTestCase::TestCoverageGLCallVertexAttrib1sv });
                funcs_map.insert({ "glVertexAttrib2d",                       &ApiCoverageTestCase::TestCoverageGLCallVertexAttrib2d });
                funcs_map.insert({ "glVertexAttrib2dv",                      &ApiCoverageTestCase::TestCoverageGLCallVertexAttrib2dv });
                funcs_map.insert({ "glVertexAttrib2f",                       &ApiCoverageTestCase::TestCoverageGLCallVertexAttrib2f });  /* Shared with OpenGL ES */
                funcs_map.insert({ "glVertexAttrib2fv",                      &ApiCoverageTestCase::TestCoverageGLCallVertexAttrib2fv }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glVertexAttrib2s",                       &ApiCoverageTestCase::TestCoverageGLCallVertexAttrib2s });
                funcs_map.insert({ "glVertexAttrib2sv",                      &ApiCoverageTestCase::TestCoverageGLCallVertexAttrib2sv });
                funcs_map.insert({ "glVertexAttrib3d",                       &ApiCoverageTestCase::TestCoverageGLCallVertexAttrib3d });
                funcs_map.insert({ "glVertexAttrib3dv",                      &ApiCoverageTestCase::TestCoverageGLCallVertexAttrib3dv });
                funcs_map.insert({ "glVertexAttrib3f",                       &ApiCoverageTestCase::TestCoverageGLCallVertexAttrib3f });  /* Shared with OpenGL ES */
                funcs_map.insert({ "glVertexAttrib3fv",                      &ApiCoverageTestCase::TestCoverageGLCallVertexAttrib3fv }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glVertexAttrib3s",                       &ApiCoverageTestCase::TestCoverageGLCallVertexAttrib3s });
                funcs_map.insert({ "glVertexAttrib3sv",                      &ApiCoverageTestCase::TestCoverageGLCallVertexAttrib3sv });
                funcs_map.insert({ "glVertexAttrib4Nbv",                     &ApiCoverageTestCase::TestCoverageGLCallVertexAttrib4Nbv });
                funcs_map.insert({ "glVertexAttrib4Niv",                     &ApiCoverageTestCase::TestCoverageGLCallVertexAttrib4Niv });
                funcs_map.insert({ "glVertexAttrib4Nsv",                     &ApiCoverageTestCase::TestCoverageGLCallVertexAttrib4Nsv });
                funcs_map.insert({ "glVertexAttrib4Nub",                     &ApiCoverageTestCase::TestCoverageGLCallVertexAttrib4Nub });
                funcs_map.insert({ "glVertexAttrib4Nubv",                    &ApiCoverageTestCase::TestCoverageGLCallVertexAttrib4Nubv });
                funcs_map.insert({ "glVertexAttrib4Nuiv",                    &ApiCoverageTestCase::TestCoverageGLCallVertexAttrib4Nuiv });
                funcs_map.insert({ "glVertexAttrib4Nusv",                    &ApiCoverageTestCase::TestCoverageGLCallVertexAttrib4Nusv });
                funcs_map.insert({ "glVertexAttrib4bv",                      &ApiCoverageTestCase::TestCoverageGLCallVertexAttrib4bv });
                funcs_map.insert({ "glVertexAttrib4d",                       &ApiCoverageTestCase::TestCoverageGLCallVertexAttrib4d });
                funcs_map.insert({ "glVertexAttrib4dv",                      &ApiCoverageTestCase::TestCoverageGLCallVertexAttrib4dv });
                funcs_map.insert({ "glVertexAttrib4f",                       &ApiCoverageTestCase::TestCoverageGLCallVertexAttrib4f });  /* Shared with OpenGL ES */
                funcs_map.insert({ "glVertexAttrib4fv",                      &ApiCoverageTestCase::TestCoverageGLCallVertexAttrib4fv }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glVertexAttrib4iv",                      &ApiCoverageTestCase::TestCoverageGLCallVertexAttrib4iv });
                funcs_map.insert({ "glVertexAttrib4s",                       &ApiCoverageTestCase::TestCoverageGLCallVertexAttrib4s });
                funcs_map.insert({ "glVertexAttrib4sv",                      &ApiCoverageTestCase::TestCoverageGLCallVertexAttrib4sv });
                funcs_map.insert({ "glVertexAttrib4ubv",                     &ApiCoverageTestCase::TestCoverageGLCallVertexAttrib4ubv });
                funcs_map.insert({ "glVertexAttrib4uiv",                     &ApiCoverageTestCase::TestCoverageGLCallVertexAttrib4uiv });
                funcs_map.insert({ "glVertexAttrib4usv",                     &ApiCoverageTestCase::TestCoverageGLCallVertexAttrib4usv });
                funcs_map.insert({ "glVertexAttribPointer",                  &ApiCoverageTestCase::TestCoverageGLCallVertexAttribPointer });     /* Shared with OpenGL ES */
                funcs_map.insert({ "glUniformMatrix2x3fv",                   &ApiCoverageTestCase::TestCoverageGLCallUniformMatrix2x3fv });
                funcs_map.insert({ "glUniformMatrix3x2fv",                   &ApiCoverageTestCase::TestCoverageGLCallUniformMatrix3x2fv });
                funcs_map.insert({ "glUniformMatrix2x4fv",                   &ApiCoverageTestCase::TestCoverageGLCallUniformMatrix2x4fv }); /* Shared with OpenGL ES */
                funcs_map.insert({ "glUniformMatrix4x2fv",                   &ApiCoverageTestCase::TestCoverageGLCallUniformMatrix4x2fv });
                funcs_map.insert({ "glUniformMatrix3x4fv",                   &ApiCoverageTestCase::TestCoverageGLCallUniformMatrix3x4fv });
                funcs_map.insert({ "glUniformMatrix4x3fv",                   &ApiCoverageTestCase::TestCoverageGLCallUniformMatrix4x3fv });
        }
        // clang-format on
    }
}

bool ApiCoverageTestCase::verifyEnum(const std::string &name, const std::string &value)
{
    GLenum enumExpected     = ~0;
    GLboolean compare_value = false;

    if (value.find("GL_") != std::string::npos)
    {
        // special cases for set of enums holding the same value
        if (value == "GL_DRAW_FRAMEBUFFER_BINDING")
            enumExpected = GL_DRAW_FRAMEBUFFER_BINDING;
        else if (value == "GL_MAX_VARYING_COMPONENTS")
            enumExpected = GL_MAX_VARYING_COMPONENTS;
        else if (value == "GL_VERTEX_PROGRAM_POINT_SIZE")
            enumExpected = GL_VERTEX_PROGRAM_POINT_SIZE;
        compare_value = true;
    }
    else
        sscanf(value.c_str(), "%x", &enumExpected);

    if (name.substr(std::max(0, (int)name.size() - 4)) == "_BIT")
    {
        return isNameWithinBitfield(name, enumExpected);
    }
    else
    {
        // special case here
        if (name == "GL_INVALID_INDEX")
            return enumExpected == GL_INVALID_INDEX;
        else if (name == "GL_TIMEOUT_IGNORED")
        {
            unsigned long long expected = 0;
            sscanf(value.c_str(), "%llx", &expected);
            return expected == GL_TIMEOUT_IGNORED;
        }

        std::set<std::string> names;
        getEnumNames(enumExpected, names);

        if (enumExpected == 1)
        {
            for (auto ver_name : m_version_names)
                names.insert(ver_name);
        }

        if (compare_value)
        {
            for (auto &&found : names)
                if (value == found)
                    return true;
        }
        else
        {
            for (auto &&found : names)
                if (name == found)
                    return true;
        }
    }
    return false;
}

bool ApiCoverageTestCase::verifyFunc(const std::string &name)
{
    if (funcs_map.find(name) == funcs_map.end())
    {
        m_testCtx.getLog() << tcu::TestLog::Message << "Function coverage test not supported : " << name.c_str()
                           << tcu::TestLog::EndMessage;
        return true;
    }

    test_func_ptr func_ptr = funcs_map[name];
    if (!(this->*func_ptr)())
        return false;

    return true;
}

template <typename... Args>
inline void ApiCoverageTestCase::tcu_fail_msg(const std::string &format, Args... args)
{
    int str_size = std::snprintf(nullptr, 0, format.c_str(), args...) + 1;
    if (str_size <= 0)
        throw std::runtime_error("Formatting error.");
    size_t s = static_cast<size_t>(str_size);
    std::unique_ptr<char[]> buffer(new char[s]);
    std::snprintf(buffer.get(), s, format.c_str(), args...);
    m_testCtx.getLog() << tcu::TestLog::Message << buffer.get() << tcu::TestLog::EndMessage;
}

void ApiCoverageTestCase::tcu_msg(const std::string &msg0, const std::string &msg1)
{
    m_testCtx.getLog() << tcu::TestLog::Message << msg0.c_str() << " : " << msg1.c_str() << tcu::TestLog::EndMessage;
}

bool ApiCoverageTestCase::GetBits(GLenum target, GLenum bits, GLint *value)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    if (!m_is_context_ES)
    {
        GLint colorAttachment;
        GLenum depthAttachment   = GL_DEPTH;
        GLenum stencilAttachment = GL_STENCIL;
        GLint fbo                = 0;
        if (target == GL_READ_FRAMEBUFFER)
        {
            gl.getIntegerv(GL_READ_FRAMEBUFFER_BINDING, &fbo);
            GLU_EXPECT_NO_ERROR(gl.getError(), "getIntegerv");
        }
        else
        {
            gl.getIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
            GLU_EXPECT_NO_ERROR(gl.getError(), "getIntegerv");
        }

        if (fbo)
        {
            depthAttachment   = GL_DEPTH_ATTACHMENT;
            stencilAttachment = GL_STENCIL_ATTACHMENT;
        }
        if (target == GL_READ_FRAMEBUFFER)
        {
            gl.getIntegerv(GL_READ_BUFFER, &colorAttachment);
            GLU_EXPECT_NO_ERROR(gl.getError(), "getIntegerv");
        }
        else
        {
            gl.getIntegerv(GL_DRAW_BUFFER, &colorAttachment);
            GLU_EXPECT_NO_ERROR(gl.getError(), "getIntegerv");
        }
        if (colorAttachment == GL_BACK)
            colorAttachment = GL_BACK_LEFT;
        else if (colorAttachment == GL_FRONT)
            colorAttachment = GL_FRONT_LEFT;

        switch (bits)
        {
        case GL_RED_BITS:
            gl.getFramebufferAttachmentParameteriv(target, colorAttachment, GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE, value);
            GLU_EXPECT_NO_ERROR(gl.getError(), "getFramebufferAttachmentParameteriv");
            break;
        case GL_GREEN_BITS:
            gl.getFramebufferAttachmentParameteriv(target, colorAttachment, GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE,
                                                   value);
            GLU_EXPECT_NO_ERROR(gl.getError(), "getFramebufferAttachmentParameteriv");
            break;
        case GL_BLUE_BITS:
            gl.getFramebufferAttachmentParameteriv(target, colorAttachment, GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE, value);
            GLU_EXPECT_NO_ERROR(gl.getError(), "getFramebufferAttachmentParameteriv");
            break;
        case GL_ALPHA_BITS:
            gl.getFramebufferAttachmentParameteriv(target, colorAttachment, GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE,
                                                   value);
            GLU_EXPECT_NO_ERROR(gl.getError(), "getFramebufferAttachmentParameteriv");
            break;
        case GL_DEPTH_BITS:
        case GL_STENCIL_BITS:
            /*
             * OPENGL SPECS 4.5: Paragraph  9.2. BINDING AND MANAGING FRAMEBUFFER OBJECTS p.335
             * If the value of GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE is GL_NONE, then either no framebuffer is bound to target;
             * or a default framebuffer is queried, attachment is GL_DEPTH or GL_STENCIL,
             * and the number of depth or stencil bits, respectively, is zero....
             * and all other queries will generate an INVALID_OPERATION error.
             * */
            if (fbo == 0)
            { //default framebuffer
                gl.getFramebufferAttachmentParameteriv(target, (bits == GL_DEPTH_BITS ? GL_DEPTH : GL_STENCIL),
                                                       GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, value);
                GLU_EXPECT_NO_ERROR(gl.getError(), "getFramebufferAttachmentParameteriv");

                if (*value == GL_NONE)
                {
                    *value = 0;
                    break;
                }
            }
            switch (bits)
            {
            case GL_DEPTH_BITS:
                gl.getFramebufferAttachmentParameteriv(target, depthAttachment, GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE,
                                                       value);
                GLU_EXPECT_NO_ERROR(gl.getError(), "getFramebufferAttachmentParameteriv");
                break;
            case GL_STENCIL_BITS:
                gl.getFramebufferAttachmentParameteriv(target, stencilAttachment,
                                                       GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE, value);
                GLU_EXPECT_NO_ERROR(gl.getError(), "getFramebufferAttachmentParameteriv");
                break;
            }
            break;
        default:
            gl.getIntegerv(bits, value);
            GLU_EXPECT_NO_ERROR(gl.getError(), "getIntegerv");
            break;
        }
    }
    else
    {
        gl.getIntegerv(bits, value);
        GLU_EXPECT_NO_ERROR(gl.getError(), "getIntegerv");
    }

    const GLenum error = gl.getError();
    if (error == GL_NO_ERROR)
        return true;

    m_testCtx.getLog() << tcu::TestLog::Message << "ApiCoverageTestCase::GetBits: " << glu::getErrorName(error)
                       << tcu::TestLog::EndMessage;

    return false;
}

bool ApiCoverageTestCase::GetReadbufferBits(GLenum bits, GLint *value)
{
    return GetBits(GL_READ_FRAMEBUFFER, bits, value);
}

bool ApiCoverageTestCase::GetDrawbufferBits(GLenum bits, GLint *value)
{
    return GetBits(GL_DRAW_FRAMEBUFFER, bits, value);
}

GLint ApiCoverageTestCase::createDefaultProgram(int mode)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    std::string vert_shader = tcu::StringTemplate(m_vert_shader).specialize(specialization_map);
    std::string frag_shader = tcu::StringTemplate(m_frag_shader).specialize(specialization_map);

    GLuint program;
    GLint status        = 0;
    GLuint vertexShader = gl.createShader(GL_VERTEX_SHADER);
    GLU_EXPECT_NO_ERROR(gl.getError(), "createShader");

    GLuint fragmentShader = gl.createShader(GL_FRAGMENT_SHADER);
    GLU_EXPECT_NO_ERROR(gl.getError(), "createShader");

    const char *vert_src = vert_shader.c_str();
    gl.shaderSource(vertexShader, 1, &vert_src, NULL);
    GLU_EXPECT_NO_ERROR(gl.getError(), "shaderSource");
    gl.compileShader(vertexShader);
    GLU_EXPECT_NO_ERROR(gl.getError(), "compileShader");
    gl.getShaderiv(vertexShader, GL_COMPILE_STATUS, &status);
    GLU_EXPECT_NO_ERROR(gl.getError(), "getShaderiv");
    if (status == GL_FALSE)
    {
        GLint infoLogLength = 0;
        gl.getShaderiv(vertexShader, GL_INFO_LOG_LENGTH, &infoLogLength);

        std::vector<char> infoLogBuf(infoLogLength + 1);
        gl.getShaderInfoLog(vertexShader, (GLsizei)infoLogBuf.size(), NULL, &infoLogBuf[0]);

        m_context.getTestContext().getLog()
            << tcu::TestLog::Message << vert_src << " compilation should succed. Info Log:\n"
            << infoLogBuf.data() << tcu::TestLog::EndMessage;

        gl.deleteShader(vertexShader);

        return -1;
    }

    const char *frag_src = frag_shader.c_str();
    gl.shaderSource(fragmentShader, 1, &frag_src, NULL);
    GLU_EXPECT_NO_ERROR(gl.getError(), "shaderSource");
    gl.compileShader(fragmentShader);
    GLU_EXPECT_NO_ERROR(gl.getError(), "compileShader");
    gl.getShaderiv(fragmentShader, GL_COMPILE_STATUS, &status);
    GLU_EXPECT_NO_ERROR(gl.getError(), "getShaderiv");
    if (status == GL_FALSE)
    {
        GLint infoLogLength = 0;
        gl.getShaderiv(fragmentShader, GL_INFO_LOG_LENGTH, &infoLogLength);

        std::vector<char> infoLogBuf(infoLogLength + 1);
        gl.getShaderInfoLog(fragmentShader, (GLsizei)infoLogBuf.size(), NULL, &infoLogBuf[0]);

        m_context.getTestContext().getLog()
            << tcu::TestLog::Message << frag_src << " compilation should succed. Info Log:\n"
            << infoLogBuf.data() << tcu::TestLog::EndMessage;

        gl.deleteShader(fragmentShader);
        return -1;
    }

    program = gl.createProgram();
    GLU_EXPECT_NO_ERROR(gl.getError(), "createProgram");
    gl.attachShader(program, vertexShader);
    GLU_EXPECT_NO_ERROR(gl.getError(), "attachShader");
    gl.attachShader(program, fragmentShader);
    GLU_EXPECT_NO_ERROR(gl.getError(), "attachShader");
    gl.deleteShader(vertexShader);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteShader");
    gl.deleteShader(fragmentShader);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteShader");
    gl.bindAttribLocation(program, 0, "inPosition");
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindAttribLocation");
    gl.bindAttribLocation(program, 1, "inTexCoord");
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindAttribLocation");

    if (mode) // only transformfeedback for now
    {
        const char *ptex = "texCoords";
        gl.transformFeedbackVaryings(program, 1, &ptex, GL_SEPARATE_ATTRIBS);
        GLU_EXPECT_NO_ERROR(gl.getError(), "transformFeedbackVaryings");
    }

    gl.linkProgram(program);
    GLU_EXPECT_NO_ERROR(gl.getError(), "linkProgram");
    gl.getProgramiv(program, GL_LINK_STATUS, &status);
    GLU_EXPECT_NO_ERROR(gl.getError(), "getProgramiv");
    if (!status)
    {
        return -1;
    }

    gl.useProgram(program);
    GLU_EXPECT_NO_ERROR(gl.getError(), "useProgram");

    return (GLint)program;
}

/** Executes test iteration.
 *
 *  @return Returns STOP when test has finished executing, CONTINUE if more iterations are needed.
 */
tcu::TestNode::IterateResult ApiCoverageTestCase::iterate()
{
    bool ret              = true;
    std::string file_name = m_is_context_ES ? m_config_name : "common/" + m_config_name;

    tcu::Archive &archive = m_context.getTestContext().getArchive();
    xe::xml::Parser xmlParser;

    {
        de::UniquePtr<tcu::Resource> res(archive.getResource(file_name.c_str()));

        if (res->getSize() == 0)
        {
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
            return STOP;
        }

        std::vector<uint8_t> data(res->getSize());
        res->read(&data[0], (int)data.size());

        // feed parser with xml content
        xmlParser.feed(reinterpret_cast<const uint8_t *>(data.data()), static_cast<int>(data.size()));
    }
    xmlParser.advance();

    bool skan_enums = false;
    bool skan_funcs = false;

    std::string name;
    std::string value;

    while (true)
    {
        xe::xml::Element currElement = xmlParser.getElement();

        // stop if there is parsing error
        if (currElement == xe::xml::ELEMENT_INCOMPLETE || currElement == xe::xml::ELEMENT_END_OF_STRING)
            break;

        const char *elemName = xmlParser.getElementName();
        switch (currElement)
        {
        case xe::xml::ELEMENT_START:
            if (deStringEqual(elemName, "func"))
            {
                skan_funcs = true;
            }
            else if (deStringEqual(elemName, "enum"))
            {
                skan_enums = true;
            }
            break;

        case xe::xml::ELEMENT_DATA:
            if (skan_funcs)
            {
                if (name.empty() && deStringEqual(elemName, "name"))
                {
                    xmlParser.getDataStr(name);
                }
            }
            else if (skan_enums)
            {
                if (name.empty() && deStringEqual(elemName, "name"))
                {
                    xmlParser.getDataStr(name);
                }
                else if (value.empty() && deStringEqual(elemName, "value"))
                {
                    xmlParser.getDataStr(value);
                }
            }
            break;

        case xe::xml::ELEMENT_END:

            if (deStringEqual(elemName, "func"))
            {
                skan_funcs = false;
                if (!verifyFunc(name))
                {
                    ret = false;

                    m_testCtx.getLog() << tcu::TestLog::Message << "Function verification failed :" << name
                                       << tcu::TestLog::EndMessage;
                }

                name.clear();
            }
            else if (deStringEqual(elemName, "enum"))
            {
                skan_enums = false;

                if (!verifyEnum(name, value))
                {
                    ret = false;

                    m_testCtx.getLog() << tcu::TestLog::Message << "Enum verification failed :" << name << " : "
                                       << value << tcu::TestLog::EndMessage;
                }

                name.clear();
                value.clear();
            }

            break;

        default:
            DE_ASSERT(false);
        }

        xmlParser.advance();
    }

    if (ret)
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    else
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
    return STOP;
}

bool ApiCoverageTestCase::TestCoverageGLCallActiveTexture(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLint i                  = 0;
    bool success             = true;
    GLint numUnits           = 0;

    gl.getIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &numUnits);
    GLU_EXPECT_NO_ERROR(gl.getError(), "getIntegerv");

    if (numUnits > 32)
    {
        numUnits = 32;
    }

    for (i = 0; i < numUnits; i++)
    {
        gl.activeTexture(GL_TEXTURE0 + i);
        GLU_EXPECT_NO_ERROR(gl.getError(), "activeTexture");

        if (gl.getError() == GL_INVALID_ENUM)
        {
            tcu_fail_msg("ApiCoverageTestCase::CallActiveTexture", "Invalid enum : GL_TEXTURE%d", i);
            success = false;
        }
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallAttachShader(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLuint buf[2];
    bool success = true;
    GLint i;

    // There are tests that leak programs/shaders so we have to find a number that is not a program/shader
    for (i = 1; i < 0x7fffffff; ++i)
    {
        GLboolean isObject;
        isObject = gl.isProgram(i) || gl.isShader(i);
        GLU_EXPECT_NO_ERROR(gl.getError(), "isShader");

        if (!isObject)
        {
            break;
        }
    }
    buf[0] = i;

    for (i = i + 1; i < 0x7fffffff; ++i)
    {
        GLboolean isObject;
        isObject = gl.isProgram(i) || gl.isShader(i);
        GLU_EXPECT_NO_ERROR(gl.getError(), "isShader");

        if (!isObject)
        {
            break;
        }
    }
    buf[1] = i;

    // ??? : how should this be handled?
    gl.attachShader(buf[0], buf[1]);

    if (gl.getError() != GL_INVALID_VALUE)
    {
        tcu_fail_msg("ApiCoverageTestCase::CallAttachShader", "glAttachShader : did not return GL_INVALID_VALUE");
        success = false;
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallBindAttribLocation(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLint i;
    char strBuf[32];
    GLuint program;
    GLint maxAttribs = 0;
    bool success     = true;

    gl.getIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxAttribs);
    GLU_EXPECT_NO_ERROR(gl.getError(), "getIntegerv");

    program = gl.createProgram();
    GLU_EXPECT_NO_ERROR(gl.getError(), "createProgram");

    for (i = 0; i < maxAttribs; i++)
    {
        std::snprintf(strBuf, sizeof(strBuf), "attrib%d", i);
        gl.bindAttribLocation(program, i, strBuf);

        if (gl.getError() != GL_NO_ERROR)
        {
            tcu_fail_msg("ApiCoverageTestCase::CallBindAttribLocation", "glBindAttribLocation : failed on attrib %d",
                         i);
            success = false;
        }
    }

    gl.useProgram(0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "useProgram");
    gl.deleteProgram(program);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteProgram");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallBindBuffer(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLint i;
    bool success = true;

    for (i = 0; ea_BufferObjectTargets[i].value != -1; i++)
    {
        gl.bindBuffer(ea_BufferObjectTargets[i].value, 0);

        if (gl.getError() == GL_INVALID_ENUM)
        {
            tcu_fail_msg("ApiCoverageTestCase::CallBindBuffer", "Invalid enum : %s", ea_BufferObjectTargets[i].name);
            success = false;
        }
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallBindTexture(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    gl.bindTexture(GL_TEXTURE_2D, 0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindTexture");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallBlendColor(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    gl.enable(GL_BLEND);
    GLU_EXPECT_NO_ERROR(gl.getError(), "enable");
    gl.blendColor(-0.5f, 0.2f, 2.5f, 0.5f);
    GLU_EXPECT_NO_ERROR(gl.getError(), "blendColor");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallBlendEquation(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLint i;
    bool success = true;

    gl.enable(GL_BLEND);
    GLU_EXPECT_NO_ERROR(gl.getError(), "enable");

    for (i = 0; ea_BlendEquation[i].value != -1; i++)
    {
        gl.blendEquation(ea_BlendEquation[i].value);

        if (gl.getError() == GL_INVALID_ENUM)
        {
            tcu_fail_msg("ApiCoverageTestCase::CallBlendEquation", "Invalid enum : %s", ea_BlendEquation[i].name);
            success = false;
        }
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallBlendEquationSeparate(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLint i, j;
    bool success = true;

    gl.enable(GL_BLEND);
    GLU_EXPECT_NO_ERROR(gl.getError(), "enable");

    for (i = 0; ea_BlendEquationSeparate1[i].value != -1; i++)
    {
        for (j = 0; ea_BlendEquationSeparate2[j].value != -1; j++)
        {
            gl.blendEquationSeparate(ea_BlendEquationSeparate1[i].value, ea_BlendEquationSeparate2[j].value);

            if (gl.getError() == GL_INVALID_ENUM)
            {
                tcu_fail_msg("ApiCoverageTestCase::CallBlendEquationSeparate", "Invalid enums : (%s, %s)",
                             ea_BlendEquationSeparate1[i].name, ea_BlendEquationSeparate2[j].name);
                success = false;
            }
        }
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallBlendFunc(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLint i, j;
    bool success = true;

    gl.enable(GL_BLEND);
    GLU_EXPECT_NO_ERROR(gl.getError(), "enable");

    for (i = 0; ea_BlendFunc1[i].value != -1; i++)
    {
        for (j = 0; ea_BlendFunc2[j].value != -1; j++)
        {
            gl.blendFunc(ea_BlendFunc1[i].value, ea_BlendFunc2[j].value);

            if (gl.getError() == GL_INVALID_ENUM)
            {
                tcu_fail_msg("ApiCoverageTestCase::CallBlendEquationSeparate", "Invalid enums : (%s, %s)",
                             ea_BlendFunc1[i].name, ea_BlendFunc2[j].name);
                success = false;
            }
        }
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallBlendFuncSeparate(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLint i, j, k, l;
    bool success = true;

    gl.enable(GL_BLEND);
    GLU_EXPECT_NO_ERROR(gl.getError(), "enable");

    for (i = 0; ea_BlendFuncSeparate1[i].value != -1; i++)
    {
        for (j = 0; ea_BlendFuncSeparate2[j].value != -1; j++)
        {
            for (k = 0; ea_BlendFuncSeparate3[k].value != -1; k++)
            {
                for (l = 0; ea_BlendFuncSeparate4[l].value != -1; l++)
                {
                    gl.blendFuncSeparate(ea_BlendFuncSeparate1[i].value, ea_BlendFuncSeparate2[j].value,
                                         ea_BlendFuncSeparate3[k].value, ea_BlendFuncSeparate4[l].value);

                    if (gl.getError() == GL_INVALID_ENUM)
                    {
                        tcu_fail_msg("ApiCoverageTestCase::CallBlendEquationSeparate",
                                     "Invalid enums : (%s, %s, %s, %s)", ea_BlendFuncSeparate1[i].name,
                                     ea_BlendFuncSeparate2[j].name, ea_BlendFuncSeparate3[k].name,
                                     ea_BlendFuncSeparate4[l].name);
                        success = false;
                    }
                }
            }
        }
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallBufferData(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLint i, j;
    GLuint buf                       = 2;
    static const GLfloat dummyData[] = {1.0f, 2.0f, 3.0f};
    bool success                     = true;

    gl.genBuffers(1, &buf);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genBuffers");
    for (i = 0; ea_BufferObjectTargets[i].value != -1; i++)
    {
        for (j = 0; ea_BufferObjectUsages[j].value != -1; j++)
        {
            gl.bindBuffer(ea_BufferObjectTargets[i].value, buf);
            GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");
            gl.bufferData(ea_BufferObjectTargets[i].value, 3, dummyData, ea_BufferObjectUsages[j].value);

            if (gl.getError() == GL_INVALID_ENUM)
            {
                tcu_fail_msg("ApiCoverageTestCase::CallBufferData", "Invalid enums : (%s, %s)",
                             ea_BufferObjectTargets[i].name, ea_BufferObjectUsages[j].name);
                success = false;
            }
        }
    }
    gl.deleteBuffers(1, &buf);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteBuffers");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallBufferSubData(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLint i, j;
    GLuint buf                       = 2;
    static const GLfloat dummyData[] = {1.0f, 2.0f, 3.0f};
    bool success                     = true;

    gl.genBuffers(1, &buf);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genBuffers");
    for (i = 0; ea_BufferObjectTargets[i].value != -1; i++)
    {
        for (j = 0; ea_BufferObjectUsages[j].value != -1; j++)
        {
            gl.bindBuffer(ea_BufferObjectTargets[i].value, buf);
            GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");
            gl.bufferData(ea_BufferObjectTargets[i].value, 3, dummyData, ea_BufferObjectUsages[j].value);
            GLU_EXPECT_NO_ERROR(gl.getError(), "bufferData");
            gl.bufferSubData(ea_BufferObjectTargets[i].value, 0, 3, dummyData);

            if (gl.getError() == GL_INVALID_ENUM)
            {
                tcu_fail_msg("ApiCoverageTestCase::CallBufferData", "Invalid enums : (%s, %s)",
                             ea_BufferObjectTargets[i].name, ea_BufferObjectUsages[j].name);
                success = false;
            }
        }
    }
    gl.deleteBuffers(1, &buf);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteBuffers");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallClear(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLint i;
    bool success = true;

    for (i = 0; ea_ClearBufferMask[i].value != -1; i++)
    {
        gl.clear(ea_ClearBufferMask[i].value);

        if (gl.getError() == GL_INVALID_ENUM)
        {
            tcu_fail_msg("ApiCoverageTestCase::CallClear", "Invalid enum : %s", ea_ClearBufferMask[i].name);
            success = false;
        }
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallClearColor(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    gl.clearColor(0.0f, 0.0f, 0.0f, 1.0f);
    GLU_EXPECT_NO_ERROR(gl.getError(), "clearColor");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallClearStencil(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    gl.clearStencil(0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "clearStencil");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallColorMask(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    gl.colorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
    GLU_EXPECT_NO_ERROR(gl.getError(), "colorMask");

    return success;
}

GLsizei ApiCoverageTestCase::TestCoverageGLGetNumPaletteEntries(GLenum format)
{
    if (m_is_context_ES)
    {
        switch (format)
        {
        case GL_PALETTE4_R5_G6_B5_OES:
        case GL_PALETTE4_RGB8_OES:
        case GL_PALETTE4_RGBA4_OES:
        case GL_PALETTE4_RGB5_A1_OES:
        case GL_PALETTE4_RGBA8_OES:
            return 16;

        case GL_PALETTE8_R5_G6_B5_OES:
        case GL_PALETTE8_RGB8_OES:
        case GL_PALETTE8_RGBA4_OES:
        case GL_PALETTE8_RGB5_A1_OES:
        case GL_PALETTE8_RGBA8_OES:
            return 256;

        default:
            return 0;
        }
    }
    else
        return 0;
}

GLsizei ApiCoverageTestCase::TestCoverageGLGetPixelSize(GLenum format)
{
    if (m_is_context_ES)
    {
        switch (format)
        {
        case GL_PALETTE4_R5_G6_B5_OES:
        case GL_PALETTE4_RGBA4_OES:
        case GL_PALETTE4_RGB5_A1_OES:
        case GL_PALETTE8_R5_G6_B5_OES:
        case GL_PALETTE8_RGBA4_OES:
        case GL_PALETTE8_RGB5_A1_OES:
            return 2;

        case GL_PALETTE4_RGB8_OES:
        case GL_PALETTE8_RGB8_OES:
            return 3;

        case GL_PALETTE4_RGBA8_OES:
        case GL_PALETTE8_RGBA8_OES:
            return 4;
        default:
            return 0;
        }
    }
    else
        return 0;
}

GLsizei ApiCoverageTestCase::TestCoverageGLGetCompressedPaletteSize(GLenum internalformat)
{
    return TestCoverageGLGetPixelSize(internalformat) * TestCoverageGLGetNumPaletteEntries(internalformat);
}

GLsizei ApiCoverageTestCase::TestCoverageGLGetCompressedPixelsSize(GLenum internalformat, GLsizei width, GLsizei height,
                                                                   GLsizei border)
{
    GLsizei pixels;

    pixels = (width + border * 2) * (height + border * 2);

    if (m_is_context_ES)
    {
        switch (internalformat)
        {
        case GL_PALETTE4_RGB8_OES:
        case GL_PALETTE4_RGBA8_OES:
        case GL_PALETTE4_R5_G6_B5_OES:
        case GL_PALETTE4_RGBA4_OES:
        case GL_PALETTE4_RGB5_A1_OES:
            return ((pixels % 2) == 0) ? (pixels / 2) : (pixels / 2 + 1);

        case GL_PALETTE8_RGB8_OES:
        case GL_PALETTE8_RGBA8_OES:
        case GL_PALETTE8_R5_G6_B5_OES:
        case GL_PALETTE8_RGBA4_OES:
        case GL_PALETTE8_RGB5_A1_OES:
            return pixels;
        default:
            return 0;
        }
    }
    else
        return 0;
}

GLsizei ApiCoverageTestCase::TestCoverageGLGetCompressedTextureSize(GLenum internalformat, GLsizei width,
                                                                    GLsizei height, GLsizei border)
{
    return TestCoverageGLGetCompressedPaletteSize(internalformat) +
           TestCoverageGLGetCompressedPixelsSize(internalformat, width, height, border);
}

bool ApiCoverageTestCase::TestCoverageGLCallCompressedTexImage2D(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLubyte buf[1025]; /* big enough for 256*1 byte palette + 1 byte image */
    GLsizei size;
    GLint i;

    if (m_context.getContextInfo().isExtensionSupported("GL_OES_compressed_paletted_texture"))
    {
        for (i = 0; ea_CompressedTextureFormats[i].value != -1; i++)
        {
            size = TestCoverageGLGetCompressedTextureSize(ea_CompressedTextureFormats[i].value, 1, 1, 0);

            memset(buf, 0, sizeof(buf));

            gl.compressedTexImage2D(GL_TEXTURE_2D, 0, ea_CompressedTextureFormats[i].value, 1, 1, 0, size, buf);

            if (gl.getError() == GL_INVALID_ENUM)
            {
                tcu_fail_msg("ApiCoverageTestCase::CallCompressedTexImage2D", "Invalid enum : %s",
                             ea_CompressedTextureFormats[i].name);
                success = false;
            }

            gl.compressedTexImage2D(GL_TEXTURE_2D, 0, ea_CompressedTextureFormats[i].value, 1, 1, 0, size,
                                    (const void *)NULL);
            GLU_EXPECT_NO_ERROR(gl.getError(), "compressedTexImage2D");
            if (gl.getError() == GL_INVALID_ENUM)
            {
                tcu_fail_msg("ApiCoverageTestCase::CallCompressedTexImage2D", "Invalid enum : %s",
                             ea_CompressedTextureFormats[i].name);
                success = false;
            }
        }
    }

    /* It would be nice to test any other exported compressed texture formats, but it is not possible to know the size
       to pass in, so this is likely to result in INVALID_VALUE.
     */
    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallCompressedTexSubImage2D(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLubyte buf[1025];
    GLsizei size;
    GLint i;

    if (m_context.getContextInfo().isExtensionSupported("GL_OES_compressed_paletted_texture"))
    {
        for (i = 0; ea_CompressedTextureFormats[i].value != -1; i++)
        {
            size = TestCoverageGLGetCompressedTextureSize(ea_CompressedTextureFormats[i].value, 1, 1, 0);

            memset(buf, 0, sizeof(buf));

            gl.compressedTexImage2D(GL_TEXTURE_2D, 0, ea_CompressedTextureFormats[i].value, 1, 1, 0, size,
                                    (const void *)NULL);

            if (gl.getError() == GL_INVALID_ENUM)
            {
                tcu_fail_msg("ApiCoverageTestCase::CallCompressedTexSubImage2D", "Invalid enum : %s",
                             ea_CompressedTextureFormats[i].name);
                success = false;
            }

            gl.compressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, ea_CompressedTextureFormats[i].value, size, buf);
            GLU_EXPECT_NO_ERROR(gl.getError(), "compressedTexSubImage2D");

            /* not sure if error should be INVALID_OPERATION or INVALID_ENUM */
            gl.getError();
            GLU_EXPECT_NO_ERROR(gl.getError(), "getError");
        }
    }

    /* It would be nice to test any other exported compressed texture formats, but it is not possible to know the size
       to pass in, so this is likely to result in INVALID_VALUE.
     */
    return success;
}

GLenum ApiCoverageTestCase::TestCoverageGLGuessColorBufferFormat(void)
{
    GLsizei colorBits[4];

    GetReadbufferBits(GL_RED_BITS, &colorBits[0]);
    GetReadbufferBits(GL_GREEN_BITS, &colorBits[1]);
    GetReadbufferBits(GL_BLUE_BITS, &colorBits[2]);
    GetReadbufferBits(GL_ALPHA_BITS, &colorBits[3]);

    if (m_is_context_ES)
    {
        if (colorBits[0] == 0)
        {
            return GL_ALPHA;
        }
        else
        {
            if (colorBits[1] == 0 || colorBits[2] == 0)
            {
                if (colorBits[3] == 0)
                {
                    return GL_LUMINANCE;
                }
                else
                {
                    return GL_LUMINANCE_ALPHA;
                }
            }
            else
            {
                if (colorBits[3] == 0)
                {
                    return GL_RGB;
                }
                else
                {
                    return GL_RGBA;
                }
            }
        }
    }
    else
    {
        if (colorBits[3])
        {
            return GL_RGBA;
        }
        else if (colorBits[2])
        {
            return GL_RGB;
        }
        else if (colorBits[1])
        {
            return GL_RG;
        }
        else if (colorBits[0])
        {
            return GL_RED;
        }
        else
        {
            return GL_NONE;
        }
    }
}

GLsizei ApiCoverageTestCase::TestCoverageGLCalcTargetFormats(GLenum colorBufferFormat, GLenum *textureFormats)
{
    GLsizei i;

    i = 0;

    if (m_is_context_ES)
    {
        switch (colorBufferFormat)
        {
        case GL_ALPHA:
            textureFormats[i++] = GL_ALPHA;
            break;

        case GL_LUMINANCE:
            textureFormats[i++] = GL_LUMINANCE;
            break;

        case GL_LUMINANCE_ALPHA:
            textureFormats[i++] = GL_LUMINANCE;
            textureFormats[i++] = GL_LUMINANCE_ALPHA;
            textureFormats[i++] = GL_ALPHA;
            break;

        case GL_RGB:
            textureFormats[i++] = GL_RGB;
            textureFormats[i++] = GL_LUMINANCE;
            break;

        case GL_RGBA:
            textureFormats[i++] = GL_RGB;
            textureFormats[i++] = GL_RGBA;
            textureFormats[i++] = GL_LUMINANCE;
            textureFormats[i++] = GL_LUMINANCE_ALPHA;
            textureFormats[i++] = GL_ALPHA;
            break;
        }
    }
    else
    {
        switch (colorBufferFormat)
        {
        case GL_RED:
            textureFormats[i++] = GL_RED;
            break;

        case GL_RG:
            textureFormats[i++] = GL_RED;
            textureFormats[i++] = GL_RG;
            break;

        case GL_RGB:
            textureFormats[i++] = GL_RED;
            textureFormats[i++] = GL_RG;
            textureFormats[i++] = GL_RGB;
            break;

        case GL_RGBA:
            textureFormats[i++] = GL_RED;
            textureFormats[i++] = GL_RG;
            textureFormats[i++] = GL_RGB;
            textureFormats[i++] = GL_RGBA;
            break;
        }
    }

    return i;
}

bool ApiCoverageTestCase::TestCoverageGLCallCopyTexImage2D(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLenum colorBufferFormat, targetFormats[5];
    GLsizei numTargetFormats;
    GLint i;

    colorBufferFormat = TestCoverageGLGuessColorBufferFormat();
    numTargetFormats  = TestCoverageGLCalcTargetFormats(colorBufferFormat, targetFormats);

    for (i = 0; i != numTargetFormats; i++)
    {
        gl.copyTexImage2D(GL_TEXTURE_2D, 0, targetFormats[i], 0, 0, 1, 1, 0);

        if (gl.getError() == GL_INVALID_ENUM)
        {
            const char *invalidEnum = glu::getTextureFormatName(targetFormats[i]);
            tcu_fail_msg("ApiCoverageTestCase::CallCopyTexImage2D", "Invalid enum : %s", invalidEnum);
            success = false;
        }
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallCopyTexSubImage2D(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLubyte buf[1000];
    GLenum colorBufferFormat, targetFormats[5];
    GLsizei numTargetFormats;
    GLint i;

    colorBufferFormat = TestCoverageGLGuessColorBufferFormat();
    numTargetFormats  = TestCoverageGLCalcTargetFormats(colorBufferFormat, targetFormats);

    memset(buf, 0, sizeof(GLubyte) * 100);

    for (i = 0; i != numTargetFormats; i++)
    {
        gl.texImage2D(GL_TEXTURE_2D, 0, targetFormats[i], 1, 1, 0, targetFormats[i], GL_UNSIGNED_BYTE, buf);
        GLU_EXPECT_NO_ERROR(gl.getError(), "texImage2D");
        gl.copyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 1, 1);

        if (gl.getError() == GL_INVALID_ENUM)
        {
            const char *invalidEnum = glu::getTextureFormatName(targetFormats[i]);
            tcu_fail_msg("ApiCoverageTestCase::CallCopyTexSubImage2D", "Invalid enum : %s", invalidEnum);
            success = false;
        }

        gl.texImage2D(GL_TEXTURE_2D, 0, targetFormats[i], 1, 1, 0, targetFormats[i], GL_UNSIGNED_BYTE,
                      (const void *)NULL);
        GLU_EXPECT_NO_ERROR(gl.getError(), "texImage2D");
        gl.copyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 1, 1);

        if (gl.getError() == GL_INVALID_ENUM)
        {
            const char *invalidEnum = glu::getTextureFormatName(targetFormats[i]);
            tcu_fail_msg("ApiCoverageTestCase::CallCopyTexSubImage2D", "Invalid enum : %s", invalidEnum);
            success = false;
        }
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallCreateProgram(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLuint program;

    program = gl.createProgram();
    GLU_EXPECT_NO_ERROR(gl.getError(), "createProgram");

    gl.useProgram(0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "useProgram");
    gl.deleteProgram(program);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteProgram");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallCreateShader(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint i;

    for (i = 0; ea_ShaderTypes[i].value != -1; i++)
    {
        GLuint shader = gl.createShader(ea_ShaderTypes[i].value);

        if (gl.getError() == GL_INVALID_ENUM)
        {
            tcu_fail_msg("ApiCoverageTestCase::CallCreateShader", "Invalid enum : %s", ea_ShaderTypes[i]);
            success = false;
        }
        gl.deleteShader(shader);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteShader");
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallCullFace(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLint i;
    bool success = true;

    for (i = 0; ea_CullFaceMode[i].value != -1; i++)
    {
        gl.cullFace(ea_CullFaceMode[i].value);

        if (gl.getError() == GL_INVALID_ENUM)
        {
            tcu_fail_msg("ApiCoverageTestCase::CallCullFace", "Invalid enum : %s", ea_CullFaceMode[i].name);
            success = false;
        }
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallDeleteBuffers(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLint i;
    bool success = true;

    for (i = 0; ea_BufferObjectTargets[i].value != -1; i++)
    {
        GLuint buf[1];
        buf[0] = 2;

        gl.bindBuffer(ea_BufferObjectTargets[i].value, buf[0]);

        if (gl.getError() == GL_INVALID_ENUM)
        {
            tcu_fail_msg("ApiCoverageTestCase::CallDeleteBuffers", "Invalid enum : %s", ea_BufferObjectTargets[i].name);
            success = false;
        }

        gl.deleteBuffers(1, buf);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteBuffers");
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallDeleteTextures(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    GLuint buf[1];
    buf[0] = 2;

    gl.bindTexture(GL_TEXTURE_2D, buf[0]);

    if (gl.getError() == GL_INVALID_ENUM)
    {
        tcu_fail_msg("ApiCoverageTestCase::CallDeleteBuffers", "Invalid enum : GL_TEXTURE_2D");
        success = false;
    }

    gl.deleteTextures(1, buf);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteTextures");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallDeleteProgram(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLuint program;

    program = gl.createProgram();
    GLU_EXPECT_NO_ERROR(gl.getError(), "createProgram");

    gl.useProgram(0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "useProgram");
    gl.deleteProgram(program);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteProgram");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallDeleteShader(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint i;

    for (i = 0; ea_ShaderTypes[i].value != -1; i++)
    {
        GLuint shader = gl.createShader(ea_ShaderTypes[i].value);
        if (gl.getError() == GL_INVALID_ENUM)
        {
            tcu_fail_msg("ApiCoverageTestCase::CallDeleteShader", "Invalid enum : %s", ea_ShaderTypes[i]);
            success = false;
        }
        gl.deleteShader(shader);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteShader");
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallDetachShader(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint i;
    GLuint program = gl.createProgram();
    GLU_EXPECT_NO_ERROR(gl.getError(), "createProgram");

    for (i = 0; ea_ShaderTypes[i].value != -1; i++)
    {
        GLuint shader = gl.createShader(ea_ShaderTypes[i].value);
        GLU_EXPECT_NO_ERROR(gl.getError(), "createShader");

        gl.attachShader(program, shader);
        GLU_EXPECT_NO_ERROR(gl.getError(), "attachShader");
        gl.detachShader(program, shader);
        GLU_EXPECT_NO_ERROR(gl.getError(), "detachShader");

        gl.deleteShader(shader);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteShader");
    }

    gl.useProgram(0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "useProgram");
    gl.deleteProgram(program);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteProgram");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallDepthFunc(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLint i;
    bool success = true;

    for (i = 0; ea_DepthFunction[i].value != -1; i++)
    {
        gl.depthFunc(ea_DepthFunction[i].value);

        if (gl.getError() == GL_INVALID_ENUM)
        {
            tcu_fail_msg("ApiCoverageTestCase::CallDepthFunc", "Invalid enum : %s", ea_DepthFunction[i].name);
            success = false;
        }
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallDepthMask(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    gl.depthMask(GL_FALSE);
    GLU_EXPECT_NO_ERROR(gl.getError(), "depthMask");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallDisable(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint i;

    for (i = 0; ea_Enable[i].value != -1; i++)
    {
        gl.disable(ea_Enable[i].value);

        if (gl.getError() == GL_INVALID_ENUM)
        {
            tcu_fail_msg("ApiCoverageTestCase::CallDisable", "Invalid enum : %s", ea_Enable[i].name);
            success = false;
        }
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallDisableVertexAttribArray(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    gl.enableVertexAttribArray(0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "enableVertexAttribArray");
    gl.disableVertexAttribArray(0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "disableVertexAttribArray");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallDrawArrays(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint i;

    for (i = 0; ea_Primitives[i].value != -1; i++)
    {
        gl.drawArrays(ea_Primitives[i].value, 0, 1);

        if (gl.getError() == GL_INVALID_ENUM)
        {
            tcu_fail_msg("ApiCoverageTestCase::CallDrawArrays", "Invalid enum : %s", ea_Primitives[i].name);
            success = false;
        }
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallDrawElements(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint i;

    for (i = 0; ea_Primitives[i].value != -1; i++)
    {
        {
            GLubyte indices[1];
            indices[0] = 0;
            gl.drawElements(ea_Primitives[i].value, 1, GL_UNSIGNED_BYTE, indices);

            if (gl.getError() == GL_INVALID_ENUM)
            {
                tcu_fail_msg("ApiCoverageTestCase::CallDrawElements", "Invalid enum : %s", ea_Primitives[i].name);
                success = false;
            }
        }

        {
            GLushort indices[1];
            indices[0] = 0;
            gl.drawElements(ea_Primitives[i].value, 1, GL_UNSIGNED_SHORT, indices);

            if (gl.getError() == GL_INVALID_ENUM)
            {
                tcu_fail_msg("ApiCoverageTestCase::CallDrawElements", "Invalid enum : %s", ea_Primitives[i].name);
                success = false;
            }
        }
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallEnable(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint i;

    for (i = 0; ea_Enable[i].value != -1; i++)
    {
        gl.enable(ea_Enable[i].value);

        if (gl.getError() == GL_INVALID_ENUM)
        {
            tcu_fail_msg("ApiCoverageTestCase::CallEnable", "Invalid enum : %s", ea_Enable[i].name);
            success = false;
        }
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallEnableVertexAttribArray(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    gl.enableVertexAttribArray(0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "enableVertexAttribArray");
    gl.disableVertexAttribArray(0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "disableVertexAttribArray");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallFinish(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    gl.finish();
    GLU_EXPECT_NO_ERROR(gl.getError(), "finish");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallFlush(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    gl.flush();
    GLU_EXPECT_NO_ERROR(gl.getError(), "flush");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallFrontFace(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint i;

    for (i = 0; ea_FrontFaceDirection[i].value != -1; i++)
    {
        gl.frontFace(ea_FrontFaceDirection[i].value);

        if (gl.getError() == GL_INVALID_ENUM)
        {
            tcu_fail_msg("ApiCoverageTestCase::CallFrontFace", "Invalid enum : %s", ea_FrontFaceDirection[i].name);
            success = false;
        }
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallGetActiveAttrib(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLsizei length;
    GLint size;
    GLenum type;
    GLchar name[256];

    // ??? : will this maybe not work
    gl.getActiveAttrib(0, 0, 256, &length, &size, &type, name);
    if (gl.getError() != GL_INVALID_VALUE)
    {
        tcu_fail_msg("ApiCoverageTestCase::CallGetActiveAttrib", "Unexpected getActiveAttrib result");
        success = false;
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallGetActiveUniform(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLsizei length;
    GLint size;
    GLenum type;
    GLchar name[256];

    // ??? : will this maybe not work -> All shader examples should return GL_INVALID_OPERATION if incorrect program object
    gl.getActiveUniform(0, 0, 256, &length, &size, &type, name);
    if (gl.getError() != GL_INVALID_VALUE)
    {
        tcu_fail_msg("ApiCoverageTestCase::CallGetActiveUniform", "Unexpected getActiveUniform result");
        success = false;
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallGetAttachedShaders(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLsizei count;
    GLuint shaders[10];
    GLuint program;

    program = gl.createProgram();
    GLU_EXPECT_NO_ERROR(gl.getError(), "createProgram");

    gl.getAttachedShaders(program, 10, &count, shaders);
    GLU_EXPECT_NO_ERROR(gl.getError(), "getAttachedShaders");
    gl.useProgram(0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "useProgram");
    gl.deleteProgram(program);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteProgram");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallGetAttribLocation(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLuint program;

    program = gl.createProgram();
    GLU_EXPECT_NO_ERROR(gl.getError(), "createProgram");
    gl.getAttribLocation(program, "attrib");

    /* program is unlinked, so error should be INVALID_OPERATION */
    if (gl.getError() != GL_INVALID_OPERATION)
    {
        tcu_fail_msg("ApiCoverageTestCase::CallGetAttribLocation", "GL_INVALID_OPERATION not returned.");
        success = false;
    }

    gl.useProgram(0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "useProgram");
    gl.deleteProgram(program);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteProgram");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallGetBooleanv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint i;
    GLboolean buf[10];

    for (i = 0; ea_GetBoolean[i].value != -1; i++)
    {
        gl.getBooleanv(ea_GetBoolean[i].value, buf);

        if (gl.getError() == GL_INVALID_ENUM)
        {
            tcu_fail_msg("ApiCoverageTestCase::CallGetBooleanv", "Invalid enum : %s", ea_GetBoolean[i].name);
            success = false;
        }
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallGetBufferParameteriv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint i, j;
    GLint buf[10];

    for (j = 0; ea_BufferObjectTargets[j].value != -1; j++)
    {
        GLuint buffer = 0;
        gl.genBuffers(1, &buffer);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glGenBuffers");

        gl.bindBuffer(ea_BufferObjectTargets[j].value, buffer);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");

        for (i = 0; ea_GetBufferParameter[i].value != -1; i++)
        {
            gl.getBufferParameteriv(ea_BufferObjectTargets[j].value, ea_GetBufferParameter[i].value, buf);

            if (gl.getError() == GL_INVALID_ENUM)
            {
                tcu_fail_msg("ApiCoverageTestCase::CallGetBufferParameteriv", "Invalid enums : (%s, %s)",
                             ea_BufferObjectTargets[j].name, ea_GetBufferParameter[i].name);
                success = false;
            }
        }

        if (m_context.getContextInfo().isExtensionSupported("GL_OES_mapbuffer"))
        {
            for (i = 0; ea_GetBufferParameter_OES_mapbuffer[i].value != -1; i++)
            {
                gl.getBufferParameteriv(ea_BufferObjectTargets[j].value, ea_GetBufferParameter_OES_mapbuffer[i].value,
                                        buf);

                if (gl.getError() == GL_INVALID_ENUM)
                {
                    tcu_fail_msg("ApiCoverageTestCase::CallGetBufferParameteriv", "Invalid enums : (%s, %s)",
                                 ea_BufferObjectTargets[j].name, ea_GetBufferParameter_OES_mapbuffer[i].name);
                    success = false;
                }
            }
        }

        gl.deleteBuffers(1, &buffer);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteBuffers");
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallGenBuffers(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLuint buffers[10];

    gl.genBuffers(10, buffers);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genBuffers");
    gl.deleteBuffers(10, buffers);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteBuffers");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallGenTextures(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLuint buffers[10];

    gl.genTextures(10, buffers);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genTextures");
    gl.deleteTextures(10, buffers);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteTextures");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallGetError(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    gl.getError();
    GLU_EXPECT_NO_ERROR(gl.getError(), "getError");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallGetFloatv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint i;
    GLfloat buf[10];

    for (i = 0; ea_GetFloat[i].value != -1; i++)
    {
        gl.getFloatv(ea_GetFloat[i].value, buf);

        if (gl.getError() == GL_INVALID_ENUM)
        {
            tcu_fail_msg("ApiCoverageTestCase::CallGetFloatv", "Invalid enum : %s", ea_GetFloat[i].name);
            success = false;
        }
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallGetIntegerv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint i;
    // buf[10] too small. There are could be more the this for the GL_COMPRESSED_TEXTURE_FORMATS token.
    // The correct fix is to figure out the max value that this could be and and allocate the amount of memory.
    // But the easier fix is to assume that 256 is enough for anyone.
    GLint buf[256];
    GLboolean isES3 = GL_FALSE;

    for (i = 0; ea_GetInteger[i].value != -1; i++)
    {
        gl.getIntegerv(ea_GetInteger[i].value, buf);

        if (gl.getError() == GL_INVALID_ENUM)
        {
            tcu_fail_msg("ApiCoverageTestCase::CallGetIntegerv", "Invalid enum : %s", ea_GetInteger[i].name);
            success = false;
        }
    }

    {
        const char *versionString = (const char *)gl.getString(GL_VERSION);
        GLU_EXPECT_NO_ERROR(gl.getError(), "getString");

        double versionFloat = atof(versionString);
        if (versionFloat >= 3.0)
        {
            isES3 = GL_TRUE;
        }
    }

    if (isES3)
    {
        for (i = 0; ea_GetIntegerES3[i].value != -1; i++)
        {
            gl.getIntegerv(ea_GetIntegerES3[i].value, buf);

            if (gl.getError() == GL_INVALID_ENUM)
            {
                tcu_fail_msg("ApiCoverageTestCase::CallGetIntegerv", "Invalid enum : %s", ea_GetIntegerES3[i].name);
                success = false;
            }
        }
    }

    if (m_context.getContextInfo().isExtensionSupported("GL_OES_texture_3D"))
    {
        for (i = 0; ea_GetInteger_OES_Texture_3D[i].value != -1; i++)
        {
            gl.getIntegerv(ea_GetInteger_OES_Texture_3D[i].value, buf);

            if (gl.getError() == GL_INVALID_ENUM)
            {
                tcu_fail_msg("ApiCoverageTestCase::CallGetIntegerv", "Invalid enum : %s",
                             ea_GetInteger_OES_Texture_3D[i].name);
                success = false;
            }
        }
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallGetProgramiv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint i;
    GLint buf[10];
    GLuint program;

    program = gl.createProgram();
    GLU_EXPECT_NO_ERROR(gl.getError(), "createProgram");

    for (i = 0; ea_GetProgram[i].value != -1; i++)
    {
        gl.getProgramiv(program, ea_GetProgram[i].value, buf);

        if (gl.getError() == GL_INVALID_ENUM)
        {
            tcu_fail_msg("ApiCoverageTestCase::CallGetProgramiv", "Invalid enum : %s", ea_GetProgram[i].name);
            success = false;
        }
    }
    gl.useProgram(0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "useProgram");
    gl.deleteProgram(program);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteProgram");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallGetProgramInfoLog(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLchar infolog[1024];
    GLsizei length;
    GLuint program;

    program = gl.createProgram();
    GLU_EXPECT_NO_ERROR(gl.getError(), "createProgram");

    gl.getProgramInfoLog(program, 1024, &length, infolog);
    GLU_EXPECT_NO_ERROR(gl.getError(), "getProgramInfoLog");
    gl.useProgram(0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "useProgram");
    gl.deleteProgram(program);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteProgram");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallGetString(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint i;

    for (i = 0; ea_GetString[i].value != -1; i++)
    {
        gl.getString(ea_GetString[i].value);

        if (gl.getError() == GL_INVALID_ENUM)
        {
            tcu_fail_msg("ApiCoverageTestCase::CallGetString", "Invalid enum : %s", ea_GetString[i].name);
            success = false;
        }
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallGetTexParameteriv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint i;
    GLint buf[10];

    for (i = 0; ea_GetTexParameter[i].value != -1; i++)
    {
        gl.getTexParameteriv(GL_TEXTURE_2D, ea_GetTexParameter[i].value, buf);

        if (gl.getError() == GL_INVALID_ENUM)
        {
            tcu_fail_msg("ApiCoverageTestCase::CallGetTexParameteriv", "Invalid enum : %s", ea_GetTexParameter[i].name);
            success = false;
        }
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallGetTexParameterfv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint i;
    GLfloat buf[10];

    for (i = 0; ea_GetTexParameter[i].value != -1; i++)
    {
        gl.getTexParameterfv(GL_TEXTURE_2D, ea_GetTexParameter[i].value, buf);

        if (gl.getError() == GL_INVALID_ENUM)
        {
            tcu_fail_msg("ApiCoverageTestCase::CallGetTexParameterfv", "Invalid enum : %s", ea_GetTexParameter[i].name);
            success = false;
        }
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallGetUniformfv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLfloat buf[10];
    GLuint program;

    program = gl.createProgram();
    GLU_EXPECT_NO_ERROR(gl.getError(), "createProgram");

    gl.getUniformfv(program, 0, buf);

    /* program is unlinked, so error should be INVALID_OPERATION */
    if (gl.getError() != GL_INVALID_OPERATION)
    {
        tcu_fail_msg("ApiCoverageTestCase::CallGetUniformfv", "GL_INVALID_OPERATION not returned.");
        success = false;
    }

    gl.useProgram(0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "useProgram");
    gl.deleteProgram(program);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteProgram");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallGetUniformiv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint buf[10];
    GLuint program;

    program = gl.createProgram();
    GLU_EXPECT_NO_ERROR(gl.getError(), "createProgram");

    gl.getUniformiv(program, 0, buf);

    /* program is unlinked, so error should be INVALID_OPERATION */
    if (gl.getError() != GL_INVALID_OPERATION)
    {
        tcu_fail_msg("ApiCoverageTestCase::CallGetUniformiv", "GL_INVALID_OPERATION not returned.");
        success = false;
    }

    gl.useProgram(0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "useProgram");
    gl.deleteProgram(program);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteProgram");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallGetUniformLocation(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLuint program;

    program = gl.createProgram();
    GLU_EXPECT_NO_ERROR(gl.getError(), "createProgram");

    gl.getUniformLocation(program, "uniform1");

    /* program is unlinked, so error should be INVALID_OPERATION */
    if (gl.getError() != GL_INVALID_OPERATION)
    {
        tcu_fail_msg("ApiCoverageTestCase::CallGetUniformLocation", "GL_INVALID_OPERATION not returned.");
        success = false;
    }

    gl.useProgram(0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "useProgram");
    gl.deleteProgram(program);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteProgram");

    while (gl.getError() != GL_NO_ERROR)
    {
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallGetVertexAttribfv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLfloat buf[10];
    GLuint index;
    GLint i;

    index = 1;

    for (i = 0; ea_GetVertexAttrib[i].value != -1; i++)
    {
        gl.getVertexAttribfv(index, ea_GetVertexAttrib[i].value, buf);

        if (gl.getError() == GL_INVALID_ENUM)
        {
            tcu_fail_msg("ApiCoverageTestCase::CallGetVertexAttribfv", "Invalid enum : %s", ea_GetVertexAttrib[i].name);
            success = false;
        }
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallGetVertexAttribiv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint buf[10];
    GLuint index;
    GLint i;

    index = 1;

    for (i = 0; ea_GetVertexAttrib[i].value != -1; i++)
    {
        gl.getVertexAttribiv(index, ea_GetVertexAttrib[i].value, buf);

        if (gl.getError() == GL_INVALID_ENUM)
        {
            tcu_fail_msg("ApiCoverageTestCase::CallGetVertexAttribiv", "Invalid enum : %s", ea_GetVertexAttrib[i].name);
            success = false;
        }
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallGetVertexAttribPointerv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    void *buf;
    GLuint index;
    GLint i;

    index = 1;

    for (i = 0; ea_GetVertexAttribPointer[i].value != -1; i++)
    {
        gl.getVertexAttribPointerv(index, ea_GetVertexAttribPointer[i].value, &buf);

        if (gl.getError() == GL_INVALID_ENUM)
        {
            tcu_fail_msg("ApiCoverageTestCase::CallGetVertexAttribPointerv", "Invalid enum : %s",
                         ea_GetVertexAttribPointer[i].name);
            success = false;
        }
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallHint(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint i, j;

    for (i = 0; ea_HintTarget[i].value != -1; i++)
    {
        for (j = 0; ea_HintMode[j].value != -1; j++)
        {
            gl.hint(ea_HintTarget[i].value, ea_HintMode[j].value);

            if (gl.getError() == GL_INVALID_ENUM)
            {
                tcu_fail_msg("ApiCoverageTestCase::CallHint", "Invalid enums : (%s, %s)", ea_HintTarget[i].name,
                             ea_HintMode[j].name);
                success = false;
            }
        }
    }

    if (m_context.getContextInfo().isExtensionSupported("GL_OES_standard_derivatives"))
    {
        for (i = 0; ea_HintTarget_OES_fragment_shader_derivative[i].value != -1; i++)
        {
            for (j = 0; ea_HintMode[j].value != -1; j++)
            {
                gl.hint(ea_HintTarget_OES_fragment_shader_derivative[i].value, ea_HintMode[j].value);

                if (gl.getError() == GL_INVALID_ENUM)
                {
                    tcu_fail_msg("ApiCoverageTestCase::CallHint", "Invalid enums : (%s, %s)",
                                 ea_HintTarget_OES_fragment_shader_derivative[i].name, ea_HintMode[j].name);
                    success = false;
                }
            }
        }
    }
    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallIsBuffer(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    gl.isBuffer(0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "isBuffer");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallIsEnabled(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint i;

    for (i = 0; ea_Enable[i].value != -1; i++)
    {
        gl.isEnabled(ea_Enable[i].value);

        if (gl.getError() == GL_INVALID_ENUM)
        {
            tcu_fail_msg("ApiCoverageTestCase::CallIsEnabled", "Invalid enum : %s", ea_Enable[i].name);
            success = false;
        }
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallIsProgram(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    gl.isProgram(0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "isProgram");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallIsShader(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    gl.isShader(0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "isShader");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallIsTexture(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    gl.isTexture(0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "isTexture");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallLineWidth(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    gl.lineWidth(1.0f);
    GLU_EXPECT_NO_ERROR(gl.getError(), "lineWidth");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallLinkProgram(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLuint program;

    program = gl.createProgram();
    GLU_EXPECT_NO_ERROR(gl.getError(), "createProgram");

    gl.linkProgram(program);
    GLU_EXPECT_NO_ERROR(gl.getError(), "linkProgram");

    gl.useProgram(0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "useProgram");
    gl.deleteProgram(program);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteProgram");

    while (gl.getError() != GL_NO_ERROR)
    {
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallPixelStorei(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint i;

    for (i = 0; ea_PixelStore[i].value != -1; i++)
    {
        gl.pixelStorei(ea_PixelStore[i].value, 1);
        GLU_EXPECT_NO_ERROR(gl.getError(), "pixelStorei");
        gl.pixelStorei(ea_PixelStore[i].value, 2);
        GLU_EXPECT_NO_ERROR(gl.getError(), "pixelStorei");
        gl.pixelStorei(ea_PixelStore[i].value, 4);
        GLU_EXPECT_NO_ERROR(gl.getError(), "pixelStorei");
        gl.pixelStorei(ea_PixelStore[i].value, 8);

        if (gl.getError() == GL_INVALID_ENUM)
        {
            tcu_fail_msg("ApiCoverageTestCase::CallPixelStorei", "Invalid enum : %s", ea_PixelStore[i].name);
            success = false;
        }
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallPolygonOffset(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    gl.polygonOffset(1.0f, 0.0f);
    GLU_EXPECT_NO_ERROR(gl.getError(), "polygonOffset");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallReadPixels(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLubyte readBuf[64]; /* 4 pixels * 4 components * 4 bytes/component */
    GLint format, type;

    gl.readPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, (void *)readBuf);

    if (GL_NO_ERROR != gl.getError())
    {
        tcu_fail_msg("ApiCoverageTestCase::CallReadPixels", "Error occured during read pixel call");
        success = false;
    }

    if (m_is_context_ES)
    {
        gl.getIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &format);
        GLU_EXPECT_NO_ERROR(gl.getError(), "getIntegerv");
        gl.getIntegerv(GL_IMPLEMENTATION_COLOR_READ_TYPE, &type);
        GLU_EXPECT_NO_ERROR(gl.getError(), "getIntegerv");

        gl.readPixels(0, 0, 1, 1, format, type, (void *)readBuf);

        if (gl.getError() == GL_INVALID_ENUM)
        {
            tcu_fail_msg("ApiCoverageTestCase::CallReadPixels", "Invalid enum in read pixel call");
            success = false;
        }
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallSampleCoverage(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    gl.sampleCoverage(1.0f, GL_FALSE);
    GLU_EXPECT_NO_ERROR(gl.getError(), "sampleCoverage");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallScissor(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    gl.scissor(0, 0, 1, 1);
    GLU_EXPECT_NO_ERROR(gl.getError(), "scissor");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallStencilFunc(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint i;

    for (i = 0; ea_StencilFunction[i].value != -1; i++)
    {
        gl.stencilFunc(ea_StencilFunction[i].value, 0, 0);

        if (gl.getError() == GL_INVALID_ENUM)
        {
            tcu_fail_msg("ApiCoverageTestCase::CallStencilFunc", "Invalid enums : %s", ea_StencilFunction[i].name);
            success = false;
        }
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallStencilFuncSeparate(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint i, j;

    for (j = 0; ea_Face[j].value != -1; j++)
    {
        for (i = 0; ea_StencilFunction[i].value != -1; i++)
        {
            gl.stencilFuncSeparate(ea_Face[j].value, ea_StencilFunction[i].value, 0, 0);

            if (gl.getError() == GL_INVALID_ENUM)
            {
                tcu_fail_msg("ApiCoverageTestCase::CallStencilFunc", "Invalid enums : (%s, %s)", ea_Face[j].name,
                             ea_StencilFunction[i].name);
                success = false;
            }
        }
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallStencilMask(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    gl.stencilMask(0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "stencilMask");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallStencilMaskSeparate(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint i;

    for (i = 0; ea_Face[i].value != -1; i++)
    {
        gl.stencilMaskSeparate(ea_Face[i].value, 0);

        if (gl.getError() == GL_INVALID_ENUM)
        {
            tcu_fail_msg("ApiCoverageTestCase::CallStencilFunc", "Invalid enums : %s", ea_Face[i].name);
            success = false;
        }
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallStencilOp(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint i, j, k;

    for (i = 0; ea_StencilOp[i].value != -1; i++)
    {
        for (j = 0; ea_StencilOp[j].value != -1; j++)
        {
            for (k = 0; ea_StencilOp[k].value != -1; k++)
            {
                gl.stencilOp(ea_StencilOp[i].value, ea_StencilOp[j].value, ea_StencilOp[k].value);

                if (gl.getError() == GL_INVALID_ENUM)
                {
                    tcu_fail_msg("ApiCoverageTestCase::CallStencilFunc", "Invalid enums : (%s, %s, %s)",
                                 ea_StencilOp[i].name, ea_StencilOp[j].name, ea_StencilOp[k].name);
                    success = false;
                }
            }
        }
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallStencilOpSeparate(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint i, j, k, l;

    for (l = 0; ea_Face[l].value != -1; l++)
    {
        for (i = 0; ea_StencilOp[i].value != -1; i++)
        {
            for (j = 0; ea_StencilOp[j].value != -1; j++)
            {
                for (k = 0; ea_StencilOp[k].value != -1; k++)
                {
                    gl.stencilOpSeparate(ea_Face[l].value, ea_StencilOp[i].value, ea_StencilOp[j].value,
                                         ea_StencilOp[k].value);

                    if (gl.getError() == GL_INVALID_ENUM)
                    {
                        tcu_fail_msg("ApiCoverageTestCase::CallStencilFunc", "Invalid enums : (%s, %s, %s, %s)",
                                     ea_Face[l].name, ea_StencilOp[i].name, ea_StencilOp[j].name, ea_StencilOp[k].name);
                        success = false;
                    }
                }
            }
        }
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallTexImage2D(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLubyte buf[1000];
    GLint i, j;

    for (i = 0; ea_TextureTarget[i].value != -1; i++)
    {
        for (j = 0; ea_TextureFormat[j].value != -1; j++)
        {
            memset(buf, 0, 1000 * sizeof(GLubyte));
            gl.texImage2D(ea_TextureTarget[i].value, 0, ea_TextureFormat[j].value, 1, 1, 0, ea_TextureFormat[j].value,
                          ea_TextureType[j].value, buf);

            if (gl.getError() == GL_INVALID_ENUM)
            {
                tcu_fail_msg("ApiCoverageTestCase::CallTexImage2D", "Invalid enums : (%s, %s, %s)",
                             ea_TextureTarget[i].name, ea_TextureFormat[j].name, ea_TextureType[j].name);
                success = false;
            }

            gl.texImage2D(ea_TextureTarget[i].value, 0, ea_TextureFormat[j].value, 1, 1, 0, ea_TextureFormat[j].value,
                          ea_TextureType[j].value, (const void *)NULL);

            if (gl.getError() == GL_INVALID_ENUM)
            {
                tcu_fail_msg("ApiCoverageTestCase::CallTexImage2D", "Invalid enums : (%s, %s, %s)",
                             ea_TextureTarget[i].name, ea_TextureFormat[j].name, ea_TextureType[j].name);
                success = false;
            }
        }
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallTexParameteri(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint i, j, k;

    for (i = 0; ea_TextureTarget[i].value != -1; i++)
    {
        for (j = 0; ea_GetTexParameter[j].value != -1; j++)
        {
            switch (ea_GetTexParameter[j].value)
            {
            case GL_TEXTURE_WRAP_S:
            case GL_TEXTURE_WRAP_T:
                for (k = 0; ea_TextureWrapMode[k].value != -1; k++)
                {
                    gl.texParameteri(ea_TextureTarget[i].value, ea_GetTexParameter[j].value,
                                     ea_TextureWrapMode[k].value);

                    if (gl.getError() == GL_INVALID_ENUM)
                    {
                        tcu_fail_msg("ApiCoverageTestCase::CallTexParameteri", "Invalid enums : (%s, %s, %s)",
                                     ea_TextureTarget[i].name, ea_GetTexParameter[j].name, ea_TextureWrapMode[k].name);
                        success = false;
                    }
                }
                break;
            case GL_TEXTURE_MIN_FILTER:
                for (k = 0; ea_TextureMinFilter[k].value != -1; k++)
                {
                    gl.texParameteri(ea_TextureTarget[i].value, ea_GetTexParameter[j].value,
                                     ea_TextureMinFilter[k].value);

                    if (gl.getError() == GL_INVALID_ENUM)
                    {
                        tcu_fail_msg("ApiCoverageTestCase::CallTexParameteri", "Invalid enums : (%s, %s, %s)",
                                     ea_TextureTarget[i].name, ea_GetTexParameter[j].name, ea_TextureMinFilter[k].name);
                        success = false;
                    }
                }
                break;
            case GL_TEXTURE_MAG_FILTER:
                for (k = 0; ea_TextureMagFilter[k].value != -1; k++)
                {
                    gl.texParameteri(ea_TextureTarget[i].value, ea_GetTexParameter[j].value,
                                     ea_TextureMagFilter[k].value);

                    if (gl.getError() == GL_INVALID_ENUM)
                    {
                        tcu_fail_msg("ApiCoverageTestCase::CallTexParameteri", "Invalid enums : (%s, %s, %s)",
                                     ea_TextureTarget[i].name, ea_GetTexParameter[j].name, ea_TextureMagFilter[k].name);
                        success = false;
                    }
                }
                break;
            }
        }
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallTexParameterf(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint i, j, k;

    for (i = 0; ea_TextureTarget[i].value != -1; i++)
    {
        for (j = 0; ea_GetTexParameter[j].value != -1; j++)
        {
            switch (ea_GetTexParameter[j].value)
            {
            case GL_TEXTURE_WRAP_S:
            case GL_TEXTURE_WRAP_T:
                for (k = 0; ea_TextureWrapMode[k].value != -1; k++)
                {
                    gl.texParameterf(ea_TextureTarget[i].value, ea_GetTexParameter[j].value,
                                     (GLfloat)ea_TextureWrapMode[k].value);

                    if (gl.getError() == GL_INVALID_ENUM)
                    {
                        tcu_fail_msg("ApiCoverageTestCase::CallTexParameterf", "Invalid enums : (%s, %s, %s)",
                                     ea_TextureTarget[i].name, ea_GetTexParameter[j].name, ea_TextureWrapMode[k].name);
                        success = false;
                    }
                }
                break;
            case GL_TEXTURE_MIN_FILTER:
                for (k = 0; ea_TextureMinFilter[k].value != -1; k++)
                {
                    gl.texParameterf(ea_TextureTarget[i].value, ea_GetTexParameter[j].value,
                                     (GLfloat)ea_TextureMinFilter[k].value);

                    if (gl.getError() == GL_INVALID_ENUM)
                    {
                        tcu_fail_msg("ApiCoverageTestCase::CallTexParameterf", "Invalid enums : (%s, %s, %s)",
                                     ea_TextureTarget[i].name, ea_GetTexParameter[j].name, ea_TextureMinFilter[k].name);
                        success = false;
                    }
                }
                break;
            case GL_TEXTURE_MAG_FILTER:
                for (k = 0; ea_TextureMagFilter[k].value != -1; k++)
                {
                    gl.texParameterf(ea_TextureTarget[i].value, ea_GetTexParameter[j].value,
                                     (GLfloat)ea_TextureMagFilter[k].value);

                    if (gl.getError() == GL_INVALID_ENUM)
                    {
                        tcu_fail_msg("ApiCoverageTestCase::CallTexParameterf", "Invalid enums : (%s, %s, %s)",
                                     ea_TextureTarget[i].name, ea_GetTexParameter[j].name, ea_TextureMagFilter[k].name);
                        success = false;
                    }
                }
                break;
            }
        }
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallTexParameteriv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint i, j, k;

    for (i = 0; ea_TextureTarget[i].value != -1; i++)
    {
        for (j = 0; ea_GetTexParameter[j].value != -1; j++)
        {
            switch (ea_GetTexParameter[j].value)
            {
            case GL_TEXTURE_WRAP_S:
            case GL_TEXTURE_WRAP_T:
                for (k = 0; ea_TextureWrapMode[k].value != -1; k++)
                {
                    gl.texParameteriv(ea_TextureTarget[i].value, ea_GetTexParameter[j].value,
                                      &ea_TextureWrapMode[k].value);

                    if (gl.getError() == GL_INVALID_ENUM)
                    {
                        tcu_fail_msg("ApiCoverageTestCase::CallTexParameteriv", "Invalid enums : (%s, %s, %s)",
                                     ea_TextureTarget[i].name, ea_GetTexParameter[j].name, ea_TextureWrapMode[k].name);
                        success = false;
                    }
                }
                break;
            case GL_TEXTURE_MIN_FILTER:
                for (k = 0; ea_TextureMinFilter[k].value != -1; k++)
                {
                    gl.texParameteriv(ea_TextureTarget[i].value, ea_GetTexParameter[j].value,
                                      &ea_TextureMinFilter[k].value);

                    if (gl.getError() == GL_INVALID_ENUM)
                    {
                        tcu_fail_msg("ApiCoverageTestCase::CallTexParameteriv", "Invalid enums : (%s, %s, %s)",
                                     ea_TextureTarget[i].name, ea_GetTexParameter[j].name, ea_TextureMinFilter[k].name);
                        success = false;
                    }
                }
                break;
            case GL_TEXTURE_MAG_FILTER:
                for (k = 0; ea_TextureMagFilter[k].value != -1; k++)
                {
                    gl.texParameteriv(ea_TextureTarget[i].value, ea_GetTexParameter[j].value,
                                      &ea_TextureMagFilter[k].value);

                    if (gl.getError() == GL_INVALID_ENUM)
                    {
                        tcu_fail_msg("ApiCoverageTestCase::CallTexParameteriv", "Invalid enums : (%s, %s, %s)",
                                     ea_TextureTarget[i].name, ea_GetTexParameter[j].name, ea_TextureMagFilter[k].name);
                        success = false;
                    }
                }
                break;
            }
        }
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallTexParameterfv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLfloat value;
    GLint i, j, k;

    for (i = 0; ea_TextureTarget[i].value != -1; i++)
    {
        for (j = 0; ea_GetTexParameter[j].value != -1; j++)
        {
            switch (ea_GetTexParameter[j].value)
            {
            case GL_TEXTURE_WRAP_S:
            case GL_TEXTURE_WRAP_T:
                for (k = 0; ea_TextureWrapMode[k].value != -1; k++)
                {
                    value = (GLfloat)ea_TextureWrapMode[k].value;
                    gl.texParameterfv(ea_TextureTarget[i].value, ea_GetTexParameter[j].value, &value);

                    if (gl.getError() == GL_INVALID_ENUM)
                    {
                        tcu_fail_msg("ApiCoverageTestCase::CallTexParameterfv", "Invalid enums : (%s, %s, %s)",
                                     ea_TextureTarget[i].name, ea_GetTexParameter[j].name, ea_TextureWrapMode[k].name);
                        success = false;
                    }
                }
                break;
            case GL_TEXTURE_MIN_FILTER:
                for (k = 0; ea_TextureMinFilter[k].value != -1; k++)
                {
                    value = (GLfloat)ea_TextureMinFilter[k].value;
                    gl.texParameterfv(ea_TextureTarget[i].value, ea_GetTexParameter[j].value, &value);

                    if (gl.getError() == GL_INVALID_ENUM)
                    {
                        tcu_fail_msg("ApiCoverageTestCase::CallTexParameterfv", "Invalid enums : (%s, %s, %s)",
                                     ea_TextureTarget[i].name, ea_GetTexParameter[j].name, ea_TextureMinFilter[k].name);
                        success = false;
                    }
                }
                break;
            case GL_TEXTURE_MAG_FILTER:
                for (k = 0; ea_TextureMagFilter[k].value != -1; k++)
                {
                    value = (GLfloat)ea_TextureMagFilter[k].value;
                    gl.texParameterfv(ea_TextureTarget[i].value, ea_GetTexParameter[j].value, &value);

                    if (gl.getError() == GL_INVALID_ENUM)
                    {
                        tcu_fail_msg("ApiCoverageTestCase::CallTexParameterfv", "Invalid enums : (%s, %s, %s)",
                                     ea_TextureTarget[i].name, ea_GetTexParameter[j].name, ea_TextureMagFilter[k].name);
                        success = false;
                    }
                }
                break;
            }
        }
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallTexSubImage2D(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLubyte buf[1000];
    int i;

    for (i = 0; ea_TextureFormat[i].value != -1; i++)
    {
        memset(buf, 0, 1000 * sizeof(GLubyte));
        gl.texImage2D(GL_TEXTURE_2D, 0, ea_TextureFormat[i].value, 1, 1, 0, ea_TextureFormat[i].value,
                      ea_TextureType[i].value, buf);
        GLU_EXPECT_NO_ERROR(gl.getError(), "texImage2D");
        gl.texSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, ea_TextureFormat[i].value, ea_TextureType[i].value, buf);

        if (gl.getError() == GL_INVALID_ENUM)
        {
            tcu_fail_msg("ApiCoverageTestCase::CallTexSubImage2D", "Invalid enums : (%s, %s)", ea_TextureFormat[i].name,
                         ea_TextureType[i].name);
            success = false;
        }

        gl.texImage2D(GL_TEXTURE_2D, 0, ea_TextureFormat[i].value, 1, 1, 0, ea_TextureFormat[i].value,
                      ea_TextureType[i].value, (const void *)NULL);
        GLU_EXPECT_NO_ERROR(gl.getError(), "texImage2D");
        gl.texSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, ea_TextureFormat[i].value, ea_TextureType[i].value, buf);

        if (gl.getError() == GL_INVALID_ENUM)
        {
            tcu_fail_msg("ApiCoverageTestCase::CallTexSubImage2D", "Invalid enums : (%s, %s)", ea_TextureFormat[i].name,
                         ea_TextureType[i].name);
            success = false;
        }
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallUniform1i(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    gl.uniform1i(0, 1);

    if (gl.getError() != GL_INVALID_OPERATION)
    {
        tcu_fail_msg("ApiCoverageTestCase::CallUniform1i", "GL_INVALID_OPERATION not returned");
        success = false;
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallUniform2i(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    gl.uniform2i(0, 1, 2);

    if (gl.getError() != GL_INVALID_OPERATION)
    {
        tcu_fail_msg("ApiCoverageTestCase::CallUniform2i", "GL_INVALID_OPERATION not returned");
        success = false;
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallUniform3i(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    gl.uniform3i(0, 1, 2, 3);

    if (gl.getError() != GL_INVALID_OPERATION)
    {
        tcu_fail_msg("ApiCoverageTestCase::CallUniform3i", "GL_INVALID_OPERATION not returned");
        success = false;
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallUniform4i(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    gl.uniform4i(0, 1, 2, 3, 4);

    if (gl.getError() != GL_INVALID_OPERATION)
    {
        tcu_fail_msg("ApiCoverageTestCase::CallUniform4i", "GL_INVALID_OPERATION not returned");
        success = false;
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallUniform1f(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    gl.uniform1f(0, 1.0f);

    if (gl.getError() != GL_INVALID_OPERATION)
    {
        tcu_fail_msg("ApiCoverageTestCase::CallUniform1f", "GL_INVALID_OPERATION not returned");
        success = false;
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallUniform2f(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    gl.uniform2f(0, 1.0f, 2.0f);

    if (gl.getError() != GL_INVALID_OPERATION)
    {
        tcu_fail_msg("ApiCoverageTestCase::CallUniform2f", "GL_INVALID_OPERATION not returned");
        success = false;
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallUniform3f(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    gl.uniform3f(0, 1.0f, 2.0f, 3.0f);

    if (gl.getError() != GL_INVALID_OPERATION)
    {
        tcu_fail_msg("ApiCoverageTestCase::CallUniform3f", "GL_INVALID_OPERATION not returned");
        success = false;
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallUniform4f(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    gl.uniform4f(0, 1.0f, 2.0f, 3.0f, 4.0f);

    if (gl.getError() != GL_INVALID_OPERATION)
    {
        tcu_fail_msg("ApiCoverageTestCase::CallUniform4f", "GL_INVALID_OPERATION not returned");
        success = false;
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallUniform1iv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint buf[]              = {1, 2};

    gl.uniform1iv(0, 2, buf);

    if (gl.getError() != GL_INVALID_OPERATION)
    {
        tcu_fail_msg("ApiCoverageTestCase::CallUniform1iv", "GL_INVALID_OPERATION not returned");
        success = false;
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallUniform2iv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint buf[]              = {1, 2, 3, 4};

    gl.uniform2iv(0, 2, buf);

    if (gl.getError() != GL_INVALID_OPERATION)
    {
        tcu_fail_msg("ApiCoverageTestCase::CallUniform2iv", "GL_INVALID_OPERATION not returned");
        success = false;
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallUniform3iv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint buf[]              = {1, 2, 3, 4, 5, 6};

    gl.uniform3iv(0, 2, buf);

    if (gl.getError() != GL_INVALID_OPERATION)
    {
        tcu_fail_msg("ApiCoverageTestCase::CallUniform3iv", "GL_INVALID_OPERATION not returned");
        success = false;
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallUniform4iv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint buf[]              = {1, 2, 3, 4, 5, 6, 7, 8};

    gl.uniform4iv(0, 2, buf);

    if (gl.getError() != GL_INVALID_OPERATION)
    {
        tcu_fail_msg("ApiCoverageTestCase::CallUniform4iv", "GL_INVALID_OPERATION not returned");
        success = false;
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallUniform1fv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLfloat buf[]            = {1.0f, 2.0f};

    gl.uniform1fv(0, 2, buf);

    if (gl.getError() != GL_INVALID_OPERATION)
    {
        tcu_fail_msg("ApiCoverageTestCase::CallUniform1fv", "GL_INVALID_OPERATION not returned");
        success = false;
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallUniform2fv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLfloat buf[]            = {1.0f, 2.0f, 3.0f, 4.0f};

    gl.uniform2fv(0, 2, buf);

    if (gl.getError() != GL_INVALID_OPERATION)
    {
        tcu_fail_msg("ApiCoverageTestCase::CallUniform2fv", "GL_INVALID_OPERATION not returned");
        success = false;
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallUniform3fv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLfloat buf[]            = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};

    gl.uniform3fv(0, 2, buf);

    if (gl.getError() != GL_INVALID_OPERATION)
    {
        tcu_fail_msg("ApiCoverageTestCase::CallUniform3fv", "GL_INVALID_OPERATION not returned");
        success = false;
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallUniform4fv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLfloat buf[]            = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};

    gl.uniform4fv(0, 2, buf);

    if (gl.getError() != GL_INVALID_OPERATION)
    {
        tcu_fail_msg("ApiCoverageTestCase::CallUniform4fv", "GL_INVALID_OPERATION not returned");
        success = false;
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallUniformMatrix2fv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLfloat buf[]            = {1, 1, 2, 2};

    gl.uniformMatrix2fv(0, 1, GL_FALSE, buf);

    if (gl.getError() != GL_INVALID_OPERATION)
    {
        tcu_fail_msg("ApiCoverageTestCase::CallUniformMatrix2fv", "GL_INVALID_OPERATION not returned");
        success = false;
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallUniformMatrix3fv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLfloat buf[]            = {1, 1, 1, 2, 2, 2, 3, 3, 3};

    gl.uniformMatrix3fv(0, 1, GL_FALSE, buf);

    if (gl.getError() != GL_INVALID_OPERATION)
    {
        tcu_fail_msg("ApiCoverageTestCase::CallUniformMatrix3fv", "GL_INVALID_OPERATION not returned");
        success = false;
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallUniformMatrix4fv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLfloat buf[]            = {1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4};

    gl.uniformMatrix4fv(0, 1, GL_FALSE, buf);

    if (gl.getError() != GL_INVALID_OPERATION)
    {
        tcu_fail_msg("ApiCoverageTestCase::CallUniformMatrix4fv", "GL_INVALID_OPERATION not returned");
        success = false;
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallUseProgram(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLuint program;

    program = gl.createProgram();
    GLU_EXPECT_NO_ERROR(gl.getError(), "createProgram");

    gl.useProgram(program);

    if (gl.getError() != GL_INVALID_OPERATION)
    {
        tcu_fail_msg("ApiCoverageTestCase::CallUseProgram", "GL_INVALID_OPERATION not returned");
        success = false;
    }

    gl.useProgram(0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "useProgram");
    gl.deleteProgram(program);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteProgram");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallValidateProgram(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLuint program;

    program = gl.createProgram();
    GLU_EXPECT_NO_ERROR(gl.getError(), "createProgram");

    gl.validateProgram(program);
    GLU_EXPECT_NO_ERROR(gl.getError(), "validateProgram");

    gl.useProgram(0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "useProgram");
    gl.deleteProgram(program);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteProgram");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttrib1f(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    gl.vertexAttrib1f(0, 1.0f);
    GLU_EXPECT_NO_ERROR(gl.getError(), "vertexAttrib1f");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttrib2f(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    gl.vertexAttrib2f(0, 1.0f, 2.0f);
    GLU_EXPECT_NO_ERROR(gl.getError(), "vertexAttrib2f");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttrib3f(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    gl.vertexAttrib3f(0, 1.0f, 2.0f, 3.0f);
    GLU_EXPECT_NO_ERROR(gl.getError(), "vertexAttrib3f");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttrib4f(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    gl.vertexAttrib4f(0, 1.0f, 2.0f, 3.0f, 4.0f);
    GLU_EXPECT_NO_ERROR(gl.getError(), "vertexAttrib4f");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttrib1fv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLfloat buf[]            = {1.0f, 2.0f};

    gl.vertexAttrib1fv(0, buf);
    GLU_EXPECT_NO_ERROR(gl.getError(), "vertexAttrib1fv");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttrib2fv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLfloat buf[]            = {1.0f, 2.0f, 3.0f, 4.0f};

    gl.vertexAttrib2fv(0, buf);
    GLU_EXPECT_NO_ERROR(gl.getError(), "vertexAttrib2fv");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttrib3fv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLfloat buf[]            = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};

    gl.vertexAttrib3fv(0, buf);
    GLU_EXPECT_NO_ERROR(gl.getError(), "vertexAttrib3fv");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttrib4fv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLfloat buf[]            = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};

    gl.vertexAttrib4fv(0, buf);
    GLU_EXPECT_NO_ERROR(gl.getError(), "vertexAttrib4fv");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttribPointer(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLfloat buf[]            = {1.0f};

    GLuint vbo = 0;
    gl.genBuffers(1, &vbo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenBuffers");
    gl.bindBuffer(GL_ARRAY_BUFFER, vbo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer");
    gl.bufferData(GL_ARRAY_BUFFER, sizeof(buf), (GLvoid *)buf, GL_STATIC_DRAW);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBufferData");

    GLuint vao = 0;
    gl.genVertexArrays(1, &vao);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genVertexArrays");
    gl.bindVertexArray(vao);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindVertexArray");
    gl.vertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, 0, nullptr);
    GLU_EXPECT_NO_ERROR(gl.getError(), "vertexAttribPointer");

    if (vbo)
    {
        gl.deleteBuffers(1, &vbo);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteBuffers");
    }

    if (vao)
    {
        gl.deleteVertexArrays(1, &vao);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteVertexArrays");
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallViewport(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    gl.viewport(0, 0, 50, 50);
    GLU_EXPECT_NO_ERROR(gl.getError(), "viewport");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallIsRenderbuffer(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    gl.isRenderbuffer(0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "isRenderbuffer");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallBindRenderbuffer(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLint i;
    bool success = true;

    /* Positive tests. */
    for (i = 0; ea_RenderBufferTargets[i].value != -1; i++)
    {
        gl.bindRenderbuffer(ea_RenderBufferTargets[i].value, 0);

        if (gl.getError() == GL_INVALID_ENUM)
        {
            tcu_fail_msg("ApiCoverageTestCase::CallBindRenderBuffer", "Invalid enum : %s",
                         ea_RenderBufferTargets[i].name);
            success = false;
        }
    }

    if (!m_is_context_ES || glu::contextSupports(m_context_type, glu::ApiType::es(3, 0)))
    {

        /* Negative test for invalid target. GL_INVALID_ENUM error is expected.
           This error appears in OpenGL 4.5 Core Profile (update July 7, 2016)
           and in OpenGL ES 3.2 Core Profile specification. */

        if ((!m_is_context_ES && glu::contextSupports(m_context_type, glu::ApiType::core(4, 5))) ||
            (m_is_context_ES && glu::contextSupports(m_context_type, glu::ApiType::es(3, 2))))
        {
            for (i = 0; ea_RenderBufferInvalidTargets[i].value != -1; i++)
            {
                gl.bindRenderbuffer(ea_RenderBufferInvalidTargets[i].value, 0);

                if (gl.getError() != GL_INVALID_ENUM)
                {
                    tcu_fail_msg("ApiCoverageTestCase::CallBindRenderBuffer",
                                 "Expected INVALID_ENUM during call with invalid target : %s",
                                 ea_RenderBufferInvalidTargets[i].name);
                    success = false;
                }
            }
        }
    }

    if (!m_is_context_ES)
    {
        /* Negative test for invalid renderbuffer object. GL_INVALID_OPERATION is expected.
           This error appears in OpenGL 4.5 Core Profile. */
        if (glu::contextSupports(m_context_type, glu::ApiType::core(4, 5)))
        {
            GLuint invalid_rbo = 0;

            gl.genRenderbuffers(1, &invalid_rbo);
            GLU_EXPECT_NO_ERROR(gl.getError(), "genRenderbuffers");
            gl.deleteRenderbuffers(1, &invalid_rbo);
            GLU_EXPECT_NO_ERROR(gl.getError(), "deleteRenderbuffers");

            for (i = 0; ea_RenderBufferTargets[i].value != -1; i++)
            {
                gl.bindRenderbuffer(ea_RenderBufferTargets[i].value, invalid_rbo);

                if (gl.getError() != GL_INVALID_OPERATION)
                {
                    tcu_fail_msg("ApiCoverageTestCase::CallBindRenderBuffer",
                                 "An INVALID_OPERATION error was expected to be generated if renderbuffer is not zero "
                                 "or a name returned from a previous call to GenRenderbuffers, or if such a name has "
                                 "since been deleted with DeleteRenderbuffers. Invalid renderbuffer object : %s",
                                 invalid_rbo);
                    success = false;
                }
            }
        }
    }
    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallDeleteRenderbuffers(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLint i;
    bool success = true;
    GLuint buf[1];

    buf[0] = 2;
    while (glIsRenderbuffer(buf[0]))
    {
        buf[0] += 1;
    }

    for (i = 0; ea_RenderBufferTargets[i].value != -1; i++)
    {
        gl.bindRenderbuffer(ea_RenderBufferTargets[i].value, buf[0]);

        if (gl.getError() == GL_INVALID_ENUM)
        {
            tcu_fail_msg("ApiCoverageTestCase::CallDeleteRenderBuffer", "Invalid enum : %s",
                         ea_RenderBufferTargets[i].name);
            success = false;
        }

        gl.deleteRenderbuffers(1, buf);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteRenderbuffers");
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallGenRenderbuffers(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    GLuint buf[2];
    gl.genRenderbuffers(2, buf);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genRenderbuffers");
    gl.deleteRenderbuffers(2, buf);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteRenderbuffers");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallRenderbufferStorage(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLint i, j;
    bool success = true;
    GLuint r, f;

    gl.genRenderbuffers(1, &r);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genRenderbuffers");
    gl.genFramebuffers(1, &f);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genFramebuffers");
    gl.bindRenderbuffer(GL_RENDERBUFFER, r);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindRenderbuffer");
    gl.bindFramebuffer(GL_FRAMEBUFFER, f);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");

    for (i = 0; ea_RenderBufferTargets[i].value != -1; i++)
    {
        for (j = 0; ea_RenderBufferFormats[j].value != -1; j++)
        {
            gl.renderbufferStorage(ea_RenderBufferTargets[i].value, ea_RenderBufferFormats[j].value, 1, 1);

            if (gl.getError() == GL_INVALID_ENUM)
            {
                tcu_fail_msg("ApiCoverageTestCase::CallBindRenderBuffer", "Invalid enums : (%s, %s)",
                             ea_RenderBufferTargets[i].name, ea_RenderBufferFormats[j].name);
                success = false;
            }
        }
    }

    if (m_context.getContextInfo().isExtensionSupported("GL_OES_rgb8_rgba8"))
    {
        for (i = 0; ea_RenderBufferTargets[i].value != -1; i++)
        {
            for (j = 0; ea_RenderBufferFormats_OES_rgb8_rgba8[j].value != -1; j++)
            {
                gl.renderbufferStorage(ea_RenderBufferTargets[i].value, ea_RenderBufferFormats_OES_rgb8_rgba8[j].value,
                                       1, 1);

                if (gl.getError() == GL_INVALID_ENUM)
                {
                    tcu_fail_msg("ApiCoverageTestCase::CallBindRenderBuffer", "Invalid enums : (%s, %s)",
                                 ea_RenderBufferTargets[i].name, ea_RenderBufferFormats_OES_rgb8_rgba8[j].name);
                    success = false;
                }
            }
        }
    }

    if (m_context.getContextInfo().isExtensionSupported("GL_OES_depth24"))
    {
        for (i = 0; ea_RenderBufferTargets[i].value != -1; i++)
        {
            for (j = 0; ea_RenderBufferFormats_OES_depth_component24[j].value != -1; j++)
            {
                gl.renderbufferStorage(ea_RenderBufferTargets[i].value,
                                       ea_RenderBufferFormats_OES_depth_component24[j].value, 1, 1);

                if (gl.getError() == GL_INVALID_ENUM)
                {
                    tcu_fail_msg("ApiCoverageTestCase::CallBindRenderBuffer", "Invalid enums : (%s, %s)",
                                 ea_RenderBufferTargets[i].name, ea_RenderBufferFormats_OES_depth_component24[j].name);
                    success = false;
                }
            }
        }
    }

    if (m_context.getContextInfo().isExtensionSupported("GL_OES_depth32"))
    {
        for (i = 0; ea_RenderBufferTargets[i].value != -1; i++)
        {
            for (j = 0; ea_RenderBufferFormats_OES_depth_component32[j].value != -1; j++)
            {
                gl.renderbufferStorage(ea_RenderBufferTargets[i].value,
                                       ea_RenderBufferFormats_OES_depth_component32[j].value, 1, 1);

                if (gl.getError() == GL_INVALID_ENUM)
                {
                    tcu_fail_msg("ApiCoverageTestCase::CallBindRenderBuffer", "Invalid enums : (%s, %s)",
                                 ea_RenderBufferTargets[i].name, ea_RenderBufferFormats_OES_depth_component32[j].name);
                    success = false;
                }
            }
        }
    }

    if (m_context.getContextInfo().isExtensionSupported("GL_OES_stencil1"))
    {
        for (i = 0; ea_RenderBufferTargets[i].value != -1; i++)
        {
            for (j = 0; ea_RenderBufferFormats_OES_stencil1[j].value != -1; j++)
            {
                gl.renderbufferStorage(ea_RenderBufferTargets[i].value, ea_RenderBufferFormats_OES_stencil1[j].value, 1,
                                       1);

                if (gl.getError() == GL_INVALID_ENUM)
                {
                    tcu_fail_msg("ApiCoverageTestCase::CallBindRenderBuffer", "Invalid enums : (%s, %s)",
                                 ea_RenderBufferTargets[i].name, ea_RenderBufferFormats_OES_stencil1[j].name);
                    success = false;
                }
            }
        }
    }

    if (m_context.getContextInfo().isExtensionSupported("GL_OES_stencil4"))
    {
        for (i = 0; ea_RenderBufferTargets[i].value != -1; i++)
        {
            for (j = 0; ea_RenderBufferFormats_OES_stencil4[j].value != -1; j++)
            {
                gl.renderbufferStorage(ea_RenderBufferTargets[i].value, ea_RenderBufferFormats_OES_stencil4[j].value, 1,
                                       1);

                if (gl.getError() == GL_INVALID_ENUM)
                {
                    tcu_fail_msg("ApiCoverageTestCase::CallBindRenderBuffer", "Invalid enums : (%s, %s)",
                                 ea_RenderBufferTargets[i].name, ea_RenderBufferFormats_OES_stencil4[j].name);
                    success = false;
                }
            }
        }
    }

    if (m_is_context_ES)
    {
        for (i = 0; ea_RenderBufferTargets[i].value != -1; i++)
        {
            for (j = 0; ea_InvalidRenderBufferFormats[j].value != -1; j++)
            {
                gl.renderbufferStorage(ea_RenderBufferTargets[i].value, ea_InvalidRenderBufferFormats[j].value, 1, 1);

                if (gl.getError() != GL_INVALID_ENUM)
                {
                    tcu_fail_msg("ApiCoverageTestCase::CallBindRenderBuffer", "Invalid enums accepted: %s",
                                 ea_InvalidRenderBufferFormats[j].name);
                    success = false;
                }
            }
        }
    }

    gl.deleteRenderbuffers(1, &r);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteRenderbuffers");
    gl.deleteFramebuffers(1, &f);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteFramebuffers");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallGetRenderbufferParameteriv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint i, j;
    GLint buf[10];

    for (i = 0; ea_RenderBufferTargets[i].value != -1; i++)
    {
        for (j = 0; ea_GetRenderBufferParameter[j].value != -1; j++)
        {
            gl.getRenderbufferParameteriv(ea_RenderBufferTargets[i].value, ea_GetRenderBufferParameter[j].value, buf);

            if (gl.getError() == GL_INVALID_ENUM)
            {
                tcu_fail_msg("ApiCoverageTestCase::CallBindRenderBuffer", "Invalid enums : (%s, %s)",
                             ea_RenderBufferTargets[i].name, ea_GetRenderBufferParameter[j].name);
                success = false;
            }
        }
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallIsFramebuffer(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    gl.isFramebuffer(0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "isFramebuffer");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallBindFramebuffer(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLint i;
    bool success = true;

    for (i = 0; ea_FrameBufferTargets[i].value != -1; i++)
    {
        gl.bindFramebuffer(ea_FrameBufferTargets[i].value, 0);

        if (gl.getError() == GL_INVALID_ENUM)
        {
            tcu_fail_msg("ApiCoverageTestCase::CallBindFrameBuffer", "Invalid enum : %s",
                         ea_FrameBufferTargets[i].name);
            success = false;
        }
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallDeleteFramebuffers(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLint i;
    bool success = true;
    GLuint buf[1];

    buf[0] = 2;

    for (i = 0; ea_FrameBufferTargets[i].value != -1; i++)
    {
        gl.bindFramebuffer(ea_FrameBufferTargets[i].value, buf[0]);

        if (gl.getError() == GL_INVALID_ENUM)
        {
            tcu_fail_msg("ApiCoverageTestCase::CallDeleteFrameBuffer", "Invalid enum : %s",
                         ea_FrameBufferTargets[i].name);
            success = false;
        }

        gl.deleteFramebuffers(1, buf);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteFramebuffers");
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallGenFramebuffers(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLuint buf[2];

    gl.genFramebuffers(2, buf);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genFramebuffers");
    gl.deleteFramebuffers(2, buf);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteFramebuffers");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallCheckFramebufferStatus(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLint i;
    bool success = true;

    for (i = 0; ea_FrameBufferTargets[i].value != -1; i++)
    {
        gl.checkFramebufferStatus(ea_FrameBufferTargets[i].value);

        if (gl.getError() == GL_INVALID_ENUM)
        {
            tcu_fail_msg("ApiCoverageTestCase::CallCheckFrameBufferStatus", "Invalid enum : %s",
                         ea_FrameBufferTargets[i].name);
            success = false;
        }
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallFramebufferTexture2D(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLint i, j;
    GLint maxColorAttachments;
    bool success = true;
    GLenum error = GL_NO_ERROR;
    GLuint fb;

    /* Some framebuffer object must be bound. */
    gl.genFramebuffers(1, &fb);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genFramebuffers");
    gl.bindFramebuffer(GL_FRAMEBUFFER, fb);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");

    maxColorAttachments = 1; // ES only supports 1 color attachment

    for (i = 0; ea_FrameBufferTargets[i].value != -1; i++)
    {
        for (j = 0; ea_FrameBufferAttachments[j].value != -1; j++)
        {
            if ((ea_FrameBufferAttachments[j].value - ea_FrameBufferAttachments[0].value >= maxColorAttachments) &&
                (ea_FrameBufferAttachments[j].value != GL_DEPTH_ATTACHMENT) &&
                (ea_FrameBufferAttachments[j].value != GL_STENCIL_ATTACHMENT))
                continue;

            gl.framebufferTexture2D(ea_FrameBufferTargets[i].value, ea_FrameBufferAttachments[j].value, GL_TEXTURE_2D,
                                    0, 0);

            error = gl.getError();
            if (error == GL_INVALID_OPERATION)
            {
                tcu_fail_msg("ApiCoverageTestCase::CallFramebufferTexture2D", "Invalid operation : (%s, %s)",
                             ea_FrameBufferTargets[i].name, ea_FrameBufferAttachments[j].name);
                success = false;
            }
            if (error == GL_INVALID_ENUM)
            {
                tcu_fail_msg("ApiCoverageTestCase::CallFramebufferTexture2D", "Invalid enum : (%s, %s)",
                             ea_FrameBufferTargets[i].name, ea_FrameBufferAttachments[j].name);
                success = false;
            }
        }
    }

    gl.deleteFramebuffers(1, &fb);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteFramebuffers");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallFramebufferRenderbuffer(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLint i, j, k;
    GLint maxColorAttachments;
    bool success = true;
    GLuint fb;

    /* Some framebuffer object must be bound. */
    gl.genFramebuffers(1, &fb);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genFramebuffers");
    gl.bindFramebuffer(GL_FRAMEBUFFER, fb);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");

    maxColorAttachments = 1; // ES only supports 1 color attachment

    for (i = 0; ea_FrameBufferTargets[i].value != -1; i++)
    {
        for (j = 0; ea_RenderBufferTargets[j].value != -1; j++)
        {
            for (k = 0; ea_FrameBufferAttachments[k].value != -1; k++)
            {
                if ((ea_FrameBufferAttachments[k].value - ea_FrameBufferAttachments[0].value >= maxColorAttachments) &&
                    (ea_FrameBufferAttachments[k].value != GL_DEPTH_ATTACHMENT) &&
                    (ea_FrameBufferAttachments[k].value != GL_STENCIL_ATTACHMENT))
                    continue;

                gl.framebufferRenderbuffer(ea_FrameBufferTargets[i].value, ea_FrameBufferAttachments[k].value,
                                           ea_RenderBufferTargets[j].value, 0);

                if (gl.getError() == GL_INVALID_ENUM)
                {
                    tcu_fail_msg("ApiCoverageTestCase::CallFramebufferRenderbuffer3DOES", "Invalid enum : (%s, %s, %s)",
                                 ea_FrameBufferTargets[i].name, ea_RenderBufferTargets[j].name,
                                 ea_FrameBufferAttachments[k].name);
                    success = false;
                }
            }
        }
    }

    gl.deleteFramebuffers(1, &fb);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteFramebuffers");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallGetFramebufferAttachmentParameteriv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLint i, j, k;
    GLint maxColorAttachments;
    bool success = true;
    GLenum error = GL_NO_ERROR;
    GLint buf[10];
    GLuint tex;
    GLuint fb;

    /* Some framebuffer object must be bound. */
    gl.genTextures(1, &tex);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genTextures");
    gl.bindTexture(GL_TEXTURE_2D, tex);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindTexture");
    gl.genFramebuffers(1, &fb);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genFramebuffers");
    gl.bindFramebuffer(GL_FRAMEBUFFER, fb);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");

    maxColorAttachments = 1; // ES only supports 1 color attachment

    for (i = 0; ea_FrameBufferTargets[i].value != -1; i++)
    {
        for (j = 0; ea_FrameBufferAttachments[j].value != -1; j++)
        {
            if ((ea_FrameBufferAttachments[j].value - ea_FrameBufferAttachments[0].value >= maxColorAttachments) &&
                (ea_FrameBufferAttachments[j].value != GL_DEPTH_ATTACHMENT) &&
                (ea_FrameBufferAttachments[j].value != GL_STENCIL_ATTACHMENT))
                continue;

            /* A texture must be attached to each attachment in turn
               to have something to query about.                     */
            gl.framebufferTexture2D(GL_FRAMEBUFFER, ea_FrameBufferAttachments[j].value, GL_TEXTURE_2D, tex, 0);
            GLU_EXPECT_NO_ERROR(gl.getError(), "framebufferTexture2D");

            for (k = 0; ea_GetFramebufferAttachmentParameter[k].value != -1; k++)
            {
                gl.getFramebufferAttachmentParameteriv(ea_FrameBufferTargets[i].value,
                                                       ea_FrameBufferAttachments[j].value,
                                                       ea_GetFramebufferAttachmentParameter[k].value, buf);

                error = gl.getError();
                if (error == GL_INVALID_OPERATION)
                {
                    tcu_fail_msg("ApiCoverageTestCase::CallGetFramebufferAttachmentParameteriv",
                                 "Invalid operation : (%s, %s, %s)", ea_FrameBufferTargets[i].name,
                                 ea_FrameBufferAttachments[j].name, ea_GetFramebufferAttachmentParameter[k].name);
                    success = false;
                }
                if (error == GL_INVALID_ENUM)
                {
                    tcu_fail_msg("ApiCoverageTestCase::CallGetFramebufferAttachmentParameteriv",
                                 "Invalid enum : (%s, %s, %s)", ea_FrameBufferTargets[i].name,
                                 ea_FrameBufferAttachments[j].name, ea_GetFramebufferAttachmentParameter[k].name);
                    success = false;
                }
            }

            gl.framebufferTexture2D(GL_FRAMEBUFFER, ea_FrameBufferAttachments[j].value, GL_TEXTURE_2D, 0, 0);
            GLU_EXPECT_NO_ERROR(gl.getError(), "framebufferTexture2D");
        }
    }

    gl.deleteFramebuffers(1, &fb);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteFramebuffers");
    gl.deleteTextures(1, &tex);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteTextures");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallGenerateMipmap(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLuint tex;
    GLubyte buf[4 * 4];
    GLenum error;
    bool success = true;

    gl.genTextures(1, &tex);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genTextures");
    gl.bindTexture(GL_TEXTURE_2D, tex);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindTexture");
    gl.texImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, buf);
    GLU_EXPECT_NO_ERROR(gl.getError(), "texImage2D");
    gl.generateMipmap(GL_TEXTURE_2D);

    error = gl.getError();

    if (error != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::CallGenerateMipmap", "Error generated %x", error);
        success = false;
    }

    gl.bindTexture(GL_TEXTURE_2D, 0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindTexture");
    gl.deleteTextures(1, &tex);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteTextures");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallCompileShader(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLuint shader;
    GLenum i;
    GLboolean compilerPresent;

    if (m_is_context_ES)
    {
        gl.getBooleanv(GL_SHADER_COMPILER, &compilerPresent);
        GLU_EXPECT_NO_ERROR(gl.getError(), "getBooleanv");
    }
    else
    {
        compilerPresent = GL_TRUE;
    }

    for (i = 0; ea_ShaderTypes[i].value != -1; i++)
    {
        shader = gl.createShader(ea_ShaderTypes[i].value);
        GLU_EXPECT_NO_ERROR(gl.getError(), "createShader");

        gl.compileShader(shader);
        GLU_EXPECT_NO_ERROR(gl.getError(), "compileShader");

        if (!compilerPresent)
        {
            if (gl.getError() != GL_INVALID_OPERATION)
            {
                tcu_fail_msg("ApiCoverageTestCase::CallCompileShader",
                             "Compiler not present, expected INVALID_OPERATION");
                success = success && false;
            }
        }

        gl.deleteShader(shader);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteShader");
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallGetShaderiv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLuint shader;
    GLint i, j;
    GLint buf[10];
    GLenum error;
    GLboolean compilerPresent;

    if (m_is_context_ES)
    {
        gl.getBooleanv(GL_SHADER_COMPILER, &compilerPresent);
        GLU_EXPECT_NO_ERROR(gl.getError(), "getBooleanv");
    }
    else
    {
        compilerPresent = GL_TRUE;
    }

    for (i = 0; ea_ShaderTypes[i].value != -1; i++)
    {
        shader = gl.createShader(ea_ShaderTypes[i].value);
        GLU_EXPECT_NO_ERROR(gl.getError(), "createShader");

        for (j = 0; ea_GetShaderStatus[j].value != -1; j++)
        {
            gl.getShaderiv(shader, ea_GetShaderStatus[j].value, buf);

            error = gl.getError();

            if (!compilerPresent && (ea_GetShaderStatus[j].value == GL_COMPILE_STATUS))
            {
                if (error != GL_INVALID_OPERATION)
                {
                    tcu_fail_msg("ApiCoverageTestCase::CallGetShaderiv",
                                 "Compiler not present, expected INVALID_OPERATION");
                    success = success && false;
                }
            }
            else if (error == GL_INVALID_ENUM)
            {
                tcu_fail_msg("ApiCoverageTestCase::CallGetShaderiv", "Invalid enum : (%s, %s)", ea_ShaderTypes[i].name,
                             ea_GetShaderStatus[j].name);
                success = success && false;
            }
        }

        gl.deleteShader(shader);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteShader");
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallGetShaderInfoLog(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLuint shader;
    GLenum i;
    GLchar infolog[1024];
    GLsizei length;

    for (i = 0; ea_ShaderTypes[i].value != -1; i++)
    {
        shader = gl.createShader(ea_ShaderTypes[i].value);
        GLU_EXPECT_NO_ERROR(gl.getError(), "createShader");

        gl.getShaderInfoLog(shader, 1024, &length, infolog);
        GLU_EXPECT_NO_ERROR(gl.getError(), "getShaderInfoLog");

        gl.deleteShader(shader);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteShader");
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallGetShaderSource(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLuint shader;
    GLenum i;
    GLchar infolog[1024];
    GLsizei length;
    GLboolean compilerPresent;

    if (m_is_context_ES)
    {
        gl.getBooleanv(GL_SHADER_COMPILER, &compilerPresent);
        GLU_EXPECT_NO_ERROR(gl.getError(), "getBooleanv");
    }
    else
    {
        compilerPresent = GL_TRUE;
    }

    for (i = 0; ea_ShaderTypes[i].value != -1; i++)
    {
        shader = gl.createShader(ea_ShaderTypes[i].value);
        GLU_EXPECT_NO_ERROR(gl.getError(), "createShader");

        gl.getShaderSource(shader, 1024, &length, infolog);

        if (!compilerPresent)
        {
            if (gl.getError() != GL_INVALID_OPERATION)
            {
                tcu_fail_msg("ApiCoverageTestCase::CallGetShaderSource",
                             "Compiler not present, expected INVALID_OPERATION");
                success = success && false;
            }
        }

        gl.deleteShader(shader);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteShader");
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallShaderSource(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLuint shader;
    GLenum i;
    const GLchar *buf = "int main {}\n\0";
    GLint bufSize[1];
    GLboolean compilerPresent;

    bufSize[0] = (GLint)strlen(buf);

    if (m_is_context_ES)
    {
        gl.getBooleanv(GL_SHADER_COMPILER, &compilerPresent);
        GLU_EXPECT_NO_ERROR(gl.getError(), "getBooleanv");
    }
    else
    {
        compilerPresent = GL_TRUE;
    }

    for (i = 0; ea_ShaderTypes[i].value != -1; i++)
    {
        shader = gl.createShader(ea_ShaderTypes[i].value);
        GLU_EXPECT_NO_ERROR(gl.getError(), "createShader");

        gl.shaderSource(shader, 1, &buf, bufSize);

        if (!compilerPresent)
        {
            if (gl.getError() != GL_INVALID_OPERATION)
            {
                tcu_fail_msg("ApiCoverageTestCase::CallShaderSource",
                             "Compiler not present, expected INVALID_OPERATION");
                success = success && false;
            }
        }

        gl.deleteShader(shader);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteShader");
    }

    return success;
}

/**************************************************************************/

/* Coverage tests for OpenGL ES entry points not shared with OpenGL.
** Some of these can be re-purposed for comparable OpenGL entry points.
*/

bool ApiCoverageTestCase::TestCoverageGLCallClearDepthf(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    gl.clearDepthf(0.0f);
    GLU_EXPECT_NO_ERROR(gl.getError(), "clearDepthf");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallDepthRangef(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    gl.depthRangef(0.0, 1.0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "depthRangef");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallFramebufferTexture3DOES(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLint i, j;
    bool success = true;

    for (i = 0; ea_FrameBufferTargets[i].value != -1; i++)
    {
        for (j = 0; ea_FrameBufferAttachments[j].value != -1; j++)
        {
            if (m_context.getContextInfo().isExtensionSupported("GL_OES_texture_3D"))
            {
                gl.framebufferTexture3DOES(ea_FrameBufferTargets[i].value, ea_FrameBufferAttachments[j].value,
                                           GL_TEXTURE_2D, 0, 0, 0);
            }

            if (gl.getError() == GL_INVALID_ENUM)
            {
                tcu_fail_msg("ApiCoverageTestCase::CallFramebufferTexture3DOES", "Invalid enum : (%s, %s)",
                             ea_FrameBufferTargets[i].name, ea_FrameBufferAttachments[j].name);
                success = false;
            }
        }
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallMapBufferOES(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    if (m_context.getContextInfo().isExtensionSupported("GL_OES_mapbuffer"))
    {
        glMapBufferFunc glMapBufferOES = (glMapBufferFunc)m_context.getRenderContext().getProcAddress("glMapBufferOES");
        glUnmapBufferFunc glUnmapBufferOES =
            (glUnmapBufferFunc)m_context.getRenderContext().getProcAddress("glUnmapBufferOES");
        glGetBufferPointervFunc glGetBufferPointervOES =
            (glGetBufferPointervFunc)m_context.getRenderContext().getProcAddress("glGetBufferPointervOES");
        GLuint bufname;
        GLenum error;

        /* Set up a buffer to map */
        gl.genBuffers(1, &bufname);
        GLU_EXPECT_NO_ERROR(gl.getError(), "genBuffers");
        gl.bindBuffer(GL_ARRAY_BUFFER, bufname);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");
        gl.bufferData(GL_ARRAY_BUFFER, 4, 0, GL_STATIC_DRAW);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bufferData");

        if (glMapBufferOES)
        {
            GLuint *mapping;

            mapping = (GLuint *)glMapBufferOES(GL_ARRAY_BUFFER, GL_WRITE_ONLY_OES);

            mapping[0] = 0xDEADBEEF;

            error = gl.getError();

            if (error != GL_NO_ERROR)
            {
                tcu_fail_msg("ApiCoverageTestCase::CallMapBufferOES", "Error generated from glMapBufferOES: %x", error);
                success = false;
            }
        }
        else
        {
            tcu_fail_msg("ApiCoverageTestCase::CallMapBufferOES", "GETPROCADDRESS failed for glMapBufferOES");
            success = false;
        }

        if (glGetBufferPointervOES)
        {
            void *mapping;

            glGetBufferPointervOES(GL_ARRAY_BUFFER, GL_BUFFER_MAP_POINTER_OES, &mapping);

            error = gl.getError();

            if (error != GL_NO_ERROR)
            {
                tcu_fail_msg("ApiCoverageTestCase::CallMapBufferOES", "Error generated from glGetBufferPointervOES: %x",
                             error);
                success = false;
            }
        }
        else
        {
            tcu_fail_msg("ApiCoverageTestCase::CallMapBufferOES", "GETPROCADDRESS failed for glGetBufferPointervOES");
            success = false;
        }

        if (glUnmapBufferOES)
        {
            glUnmapBufferOES(GL_ARRAY_BUFFER);

            error = gl.getError();

            if (error != GL_NO_ERROR)
            {
                tcu_fail_msg("ApiCoverageTestCase::CallMapBufferOES", "Error generated from glUnmapBufferOES: %x",
                             error);
                success = false;
            }
        }
        else
        {
            tcu_fail_msg("ApiCoverageTestCase::CallMapBufferOES", "GETPROCADDRESS failed for glUnmapBufferOES");
            success = false;
        }

        gl.bindBuffer(GL_ARRAY_BUFFER, 0);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");
        gl.deleteBuffers(1, &bufname);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteBuffers");
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallTexImage3DOES(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    if (m_context.getContextInfo().isExtensionSupported("GL_OES_texture_3D"))
    {
        GLubyte buf[1000];
        GLint i, j;

        for (i = 0; ea_TextureTarget[i].value != -1; i++)
        {
            for (j = 0; ea_TextureFormat[j].value != -1; j++)
            {
                memset(buf, 0, 1000 * sizeof(GLubyte));
                gl.texImage3DOES(ea_TextureTarget[i].value, 0, ea_TextureFormat[j].value, 1, 1, 1, 0,
                                 ea_TextureFormat[j].value, ea_TextureType[j].value, buf);

                if (gl.getError() == GL_INVALID_ENUM)
                {
                    tcu_fail_msg("ApiCoverageTestCase::CallTexImage3D", "Invalid enums : (%s, %s, %s)",
                                 ea_TextureTarget[i].name, ea_TextureFormat[j].name, ea_TextureType[j].name);
                    success = false;
                }

                gl.texImage3DOES(ea_TextureTarget[i].value, 0, ea_TextureFormat[j].value, 1, 1, 1, 0,
                                 ea_TextureFormat[j].value, ea_TextureType[j].value, (const void *)NULL);

                if (gl.getError() == GL_INVALID_ENUM)
                {
                    tcu_fail_msg("ApiCoverageTestCase::CallTexImage3D", "Invalid enums : (%s, %s, %s)",
                                 ea_TextureTarget[i].name, ea_TextureFormat[j].name, ea_TextureType[j].name);
                    success = false;
                }
            }
        }
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallTexSubImage3DOES(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    if (m_context.getContextInfo().isExtensionSupported("GL_OES_texture_3D"))
    {
        GLubyte buf[1000];
        int i;

        for (i = 0; ea_TextureFormat[i].value != -1; i++)
        {
            memset(buf, 0, 1000 * sizeof(GLubyte));
            gl.texImage3DOES(GL_TEXTURE_2D, 0, ea_TextureFormat[i].value, 1, 1, 1, 0, ea_TextureFormat[i].value,
                             ea_TextureType[i].value, buf);

            gl.texSubImage3DOES(GL_TEXTURE_2D, 0, 0, 0, 0, 1, 1, 1, ea_TextureFormat[i].value, ea_TextureType[i].value,
                                buf);

            if (gl.getError() == GL_INVALID_ENUM)
            {
                tcu_fail_msg("ApiCoverageTestCase::CallTexSubImage3D", "Invalid enums : (%s, %s)",
                             ea_TextureFormat[i].name, ea_TextureType[i].name);
                success = false;
            }

            gl.texImage3DOES(GL_TEXTURE_2D, 0, ea_TextureFormat[i].value, 1, 1, 1, 0, ea_TextureFormat[i].value,
                             ea_TextureType[i].value, (const void *)NULL);

            gl.texSubImage3DOES(GL_TEXTURE_2D, 0, 0, 0, 0, 1, 1, 1, ea_TextureFormat[i].value, ea_TextureType[i].value,
                                buf);

            if (gl.getError() == GL_INVALID_ENUM)
            {
                tcu_fail_msg("ApiCoverageTestCase::CallTexSubImage3D", "Invalid enums : (%s, %s)",
                             ea_TextureFormat[i].name, ea_TextureType[i].name);
                success = false;
            }
        }
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallCopyTexSubImage3DOES(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    if (m_context.getContextInfo().isExtensionSupported("GL_OES_texture_3D"))
    {
        GLubyte buf[1000];
        GLint i;
        GLenum colorBufferFormat, targetFormats[5];
        GLsizei numTargetFormats;

        colorBufferFormat = TestCoverageGLGuessColorBufferFormat();
        numTargetFormats  = TestCoverageGLCalcTargetFormats(colorBufferFormat, targetFormats);

        memset(buf, 0, sizeof(GLubyte) * 100);

        for (i = 0; i != numTargetFormats; i++)
        {
            gl.texImage3DOES(GL_TEXTURE_2D, 0, targetFormats[i], 1, 1, 1, 0, targetFormats[i], GL_UNSIGNED_BYTE, buf);
            GLU_EXPECT_NO_ERROR(gl.getError(), "texImage3DOES");
            gl.copyTexSubImage3DOES(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 0, 1, 1);

            if (gl.getError() == GL_INVALID_ENUM)
            {
                const char *invalidEnum = glu::getTextureFormatName(targetFormats[i]);
                tcu_fail_msg("ApiCoverageTestCase::CallCopyTexSubImage3D", "Invalid enum : %s", invalidEnum);
                success = false;
            }

            gl.texImage3DOES(GL_TEXTURE_2D, 0, targetFormats[i], 1, 1, 1, 0, targetFormats[i], GL_UNSIGNED_BYTE,
                             (const void *)NULL);
            GLU_EXPECT_NO_ERROR(gl.getError(), "texImage3DOES");
            gl.copyTexSubImage3DOES(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 0, 1, 1);

            if (gl.getError() == GL_INVALID_ENUM)
            {
                const char *invalidEnum = glu::getTextureFormatName(targetFormats[i]);
                tcu_fail_msg("ApiCoverageTestCase::CallCopyTexSubImage3D", "Invalid enum : %s", invalidEnum);
                success = false;
            }
        }
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallCompressedTexImage3DOES(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    if (m_context.getContextInfo().isExtensionSupported("GL_OES_texture_3D"))
    {
        /* Are there any compressed 3D texture formats? Add them here... */
        gl.compressedTexImage3DOES(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 1, 0, 1, (const void *)NULL);
        GLU_EXPECT_NO_ERROR(gl.getError(), "compressedTexImage3DOES");

        /* Can't really work out what the error might be - probably INVALID_OPERATION or INVALID_VALUE or INVALID_ENUM */
        gl.getError();
        GLU_EXPECT_NO_ERROR(gl.getError(), "getError");
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallCompressedTexSubImage3DOES(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    if (m_context.getContextInfo().isExtensionSupported("GL_OES_texture_3D"))
    {
        /* Are there any compressed 3D texture formats? Add them here... */
        gl.compressedTexSubImage3DOES(GL_TEXTURE_2D, 0, 0, 0, 0, 1, 1, 1, GL_RGBA, 1, (const void *)NULL);
        GLU_EXPECT_NO_ERROR(gl.getError(), "getError");

        /* Can't really work out what the error might be - probably INVALID_OPERATION or INVALID_VALUE or INVALID_ENUM */
        gl.getError();
        GLU_EXPECT_NO_ERROR(gl.getError(), "getError");
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallShaderBinary(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLuint shader;
    GLint numBinFormats = 0;
    GLenum i;

    gl.getIntegerv(GL_NUM_SHADER_BINARY_FORMATS, &numBinFormats);
    GLU_EXPECT_NO_ERROR(gl.getError(), "getIntegerv");
    if (numBinFormats <= 0)
    {
        return true;
    }

    std::vector<GLint> binFormats(numBinFormats);
    gl.getIntegerv(GL_SHADER_BINARY_FORMATS, binFormats.data());
    GLU_EXPECT_NO_ERROR(gl.getError(), "getIntegerv");

    for (i = 0; ea_ShaderTypes[i].value != -1; i++)
    {
        shader = gl.createShader(ea_ShaderTypes[i].value);
        GLU_EXPECT_NO_ERROR(gl.getError(), "createShader");
        gl.shaderBinary(1, &shader, binFormats[0], (const void *)NULL, 0);
        /* clear the error log - see bug 4109 */
        gl.getError();

        gl.deleteShader(shader);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteShader");
    }

    return true;
}

bool ApiCoverageTestCase::TestCoverageGLCallReleaseShaderCompiler(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    if (m_is_context_ES)
    {
        bool success = true;
        GLboolean compilerPresent;

        gl.getBooleanv(GL_SHADER_COMPILER, &compilerPresent);
        GLU_EXPECT_NO_ERROR(gl.getError(), "getBooleanv");
        gl.releaseShaderCompiler();

        if (!compilerPresent)
        {
            if (gl.getError() != GL_INVALID_OPERATION)
            {
                tcu_fail_msg("ApiCoverageTestCase::CallReleaseShaderCompiler",
                             "Compiler not present, expected INVALID_OPERATION");
                success = success && false;
            }
        }

        return success;
    }
    else
    {

        // Can't release shader compiler in desktop OpenGL
        return true;
    }
}

bool ApiCoverageTestCase::TestCoverageGLCallGetShaderPrecisionFormat(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint i, j;
    GLint range[2], precision;
    GLboolean compilerPresent;
    GLenum error;

    gl.getBooleanv(GL_SHADER_COMPILER, &compilerPresent);
    GLU_EXPECT_NO_ERROR(gl.getError(), "getBooleanv");

    for (i = 0; ea_ShaderTypes[i].value != -1; i++)
    {
        for (j = 0; ea_ShaderPrecision[j].value != -1; j++)
        {
            range[0]  = 0xffffffff;
            range[1]  = 0xffffffff;
            precision = 0xffffffff;
            gl.getShaderPrecisionFormat(ea_ShaderTypes[i].value, ea_ShaderPrecision[j].value, range, &precision);

            error = gl.getError();

            if (!compilerPresent)
            {
                // Removing this error check for this release.  The spec currently states that this error should exist, but
                // the group is discussing whether this is a spec bug.  An implementer could also change their binary shader
                // extension to allow this error.  See bugzilla 4151 for more details.
                //if (error != GL_INVALID_OPERATION)
                //{
                //    tcu_fail_msg(
                //            "ApiCoverageTestCase::CallGetShaderPrecisionFormat",
                //            "Compiler not present, expected INVALID_OPERATION");
                //    success = success && false;
                //}
            }
            else if (error != GL_NONE)
            {
                tcu_fail_msg("ApiCoverageTestCase::CallGetShaderPrecisionFormat", "GL error : (%s, %s)",
                             ea_ShaderTypes[i].name, ea_ShaderPrecision[j].name);
                success = success && false;
            }
            else
            {
                if ((ea_ShaderPrecision[j].value == GL_LOW_INT) || (ea_ShaderPrecision[j].value == GL_MEDIUM_INT) ||
                    (ea_ShaderPrecision[j].value == GL_HIGH_INT))
                {
                    // Log2(1) is 0
                    if (precision != 0)
                    {
                        tcu_fail_msg("ApiCoverageTestCase::CallGetShaderPrecisionFormat",
                                     "Precision of ints must be 0");
                        success = success && false;
                    }
                }

                if ((GLuint(range[0]) == 0xffffffff) || (GLuint(range[1]) == 0xffffffff))
                {
                    tcu_fail_msg("ApiCoverageTestCase::CallGetShaderPrecisionFormat", "Range value not updated");
                    success = success && false;
                }
            }
        }
    }

    return success;
}

/* Coverage test for glReadBuffer */
bool ApiCoverageTestCase::TestCoverageGLCallReadBuffer(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    int i;
    GLint error;
    bool success = true;
    GLint origReadBuffer;
    GLint width = 32, height = 32;
    GLuint fbo = 0, rbo_color[4] = {0, 0, 0, 0};

    error = gl.getError(); // clear error from previous test, if any

    // Save original read buffer value
    gl.getIntegerv(GL_READ_BUFFER, &origReadBuffer);
    GLU_EXPECT_NO_ERROR(gl.getError(), "getIntegerv");

    for (i = 0; ea_ReadBuffer[i].value != -1; i++)
    {
        gl.readBuffer(ea_ReadBuffer[i].value);

        error = gl.getError();
        if ((error != GL_NO_ERROR) && (ea_ReadBuffer[i].value <= GL_BACK))
        {

            if (!m_is_context_ES)
            {
                glw::GLenum drawBuffer = 0;
                m_context.getRenderContext().getFunctions().getIntegerv(GL_DRAW_BUFFER, (glw::GLint *)&drawBuffer);
                bool configIsDoubleBuffered = (drawBuffer == GL_BACK);

                /* It is an expected error to call glReadBuffer(GL_BACK) on
                   a drawable that does not have a back buffer */
                if (!configIsDoubleBuffered && ea_ReadBuffer[i].value == GL_BACK && error == GL_INVALID_OPERATION)
                {
                    continue;
                }
            }
            tcu_fail_msg("ApiCoverageTestCase::ReadBuffer", "Invalid error %d for %s.", error, ea_ReadBuffer[i].name);
            success = false;
        }
    }

    // FBO
    gl.genFramebuffers(1, &fbo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genFramebuffers");
    gl.bindFramebuffer(GL_FRAMEBUFFER, fbo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");

    // RBO (color)
    gl.genRenderbuffers(4, rbo_color);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genRenderbuffers");
    gl.bindRenderbuffer(GL_RENDERBUFFER, rbo_color[0]);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindRenderbuffer");
    gl.renderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);
    GLU_EXPECT_NO_ERROR(gl.getError(), "renderbufferStorage");
    gl.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo_color[0]);
    GLU_EXPECT_NO_ERROR(gl.getError(), "framebufferRenderbuffer");

    gl.bindRenderbuffer(GL_RENDERBUFFER, rbo_color[1]);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindRenderbuffer");
    gl.renderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);
    GLU_EXPECT_NO_ERROR(gl.getError(), "renderbufferStorage");
    gl.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_RENDERBUFFER, rbo_color[1]);
    GLU_EXPECT_NO_ERROR(gl.getError(), "framebufferRenderbuffer");

    error = gl.checkFramebufferStatus(GL_FRAMEBUFFER);
    if (error != GL_FRAMEBUFFER_COMPLETE)
    {
        tcu_fail_msg("ApiCoverageTestCase::ReadBuffers", "Expected GL_FRAMEBUFFER_COMPLETE, got 0x%x", error);
        return false;
    }

    for (i = 0; ea_ReadBuffer[i].value != -1; i++)
    {
        gl.readBuffer(ea_ReadBuffer[i].value);
        error = gl.getError();

        if ((error != GL_NO_ERROR) && (ea_ReadBuffer[i].value > GL_BACK))
        {
            tcu_fail_msg("ApiCoverageTestCase::ReadBuffer", "Invalid error %d for FBO %s.", error,
                         ea_ReadBuffer[i].name);
            success = false;
        }
    }

    gl.deleteFramebuffers(1, &fbo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteFramebuffers");

    // Restore read buffer
    gl.readBuffer(origReadBuffer);
    GLU_EXPECT_NO_ERROR(gl.getError(), "readBuffer");

    gl.deleteFramebuffers(1, &fbo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteFramebuffers");
    gl.deleteRenderbuffers(4, rbo_color);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteRenderbuffers");

    return success;
}

/* Coverage test for glDrawRangeElements */
bool ApiCoverageTestCase::TestCoverageGLCallDrawRangeElements(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLint error;
    bool success             = true;
    unsigned short indices[] = {2, 1, 0, 2, 1, 0};
    GLint program;

    (void)gl.getError(); // clear error from previous test, if any

    GLuint va = 0;
    gl.genVertexArrays(1, &va);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genVertexArrays");
    gl.bindVertexArray(va);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindVertexArray");

    program = createDefaultProgram(0);

    if (program == -1)
    {
        tcu_fail_msg("ApiCoverageTestCase::DrawRangeElements", "Program create Failed");
        return false;
    }

    gl.drawRangeElements(GL_TRIANGLES, 0, 5, 3, GL_UNSIGNED_SHORT, indices);
    GLU_EXPECT_NO_ERROR(gl.getError(), "drawRangeElements");
    error = gl.getError();

    if (error != GL_NO_ERROR)
    {
        success = false;
        tcu_fail_msg("ApiCoverageTestCase::DrawRangeElements", "Incorrect error %d", error);
    }

    gl.drawRangeElements(GL_TRIANGLES, 2, 1, 3, GL_UNSIGNED_SHORT, indices);
    error = gl.getError();
    if (error != GL_INVALID_VALUE)
    {
        success = false;
        tcu_fail_msg("ApiCoverageTestCase::DrawRangeElements", "Incorrect error %d", error);
    }

    gl.deleteProgram((GLuint)program);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteProgram");

    gl.deleteVertexArrays(1, &va);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteVertexArrays");

    return success;
}

/* Coverage test for glTexImage3D */
bool ApiCoverageTestCase::TestCoverageGLCallTexImage3D(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLubyte buf[1000];
    GLint i, j;

    (void)gl.getError(); // clear error from previous test, if any

    for (i = 0; ea_Texture3DTarget[i].value != -1; i++)
    {
        for (j = 0; ea_TextureFormat[j].value != -1; j++)
        {
            memset(buf, 0, 1000 * sizeof(GLubyte));
            gl.texImage3D(ea_Texture3DTarget[i].value, 0, ea_TextureFormat[j].value, 1, 1, 1, 0,
                          ea_TextureFormat[j].value, ea_TextureType[j].value, buf);

            if (gl.getError() == GL_INVALID_ENUM)
            {
                tcu_fail_msg("ApiCoverageTestCase::CallTexImage3D", "Invalid enums : (%s, %s, %s)",
                             ea_TextureTarget[i].name, ea_TextureFormat[j].name, ea_TextureType[j].name);
                success = false;
            }

            gl.texImage3D(ea_Texture3DTarget[i].value, 0, ea_TextureFormat[j].value, 1, 1, 1, 0,
                          ea_TextureFormat[j].value, ea_TextureType[j].value, (const void *)NULL);

            if (gl.getError() == GL_INVALID_ENUM)
            {
                tcu_fail_msg("ApiCoverageTestCase::CallTexImage3D", "Invalid enums : (%s, %s, %s)",
                             ea_TextureTarget[i].name, ea_TextureFormat[j].name, ea_TextureType[j].name);
                success = false;
            }
        }
    }

    return success;
}

/* Coverage test for glTexSubImage3D */
bool ApiCoverageTestCase::TestCoverageGLCallTexSubImage3D(void)
{
    bool success             = true;
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLubyte buf[1000];
    int i;

    (void)gl.getError(); // clear error from previous test, if any

    for (i = 0; ea_TextureFormat[i].value != -1; i++)
    {
        memset(buf, 0, 1000 * sizeof(GLubyte));
        gl.texImage3D(GL_TEXTURE_3D, 0, ea_TextureFormat[i].value, 1, 1, 1, 0, ea_TextureFormat[i].value,
                      ea_TextureType[i].value, buf);
        GLU_EXPECT_NO_ERROR(gl.getError(), "texImage3D");
        gl.texSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 1, 1, 1, ea_TextureFormat[i].value, ea_TextureType[i].value, buf);

        if (gl.getError() == GL_INVALID_ENUM)
        {
            tcu_fail_msg("ApiCoverageTestCase::CallTexSubImage3D", "Invalid enums : (%s, %s)", ea_TextureFormat[i].name,
                         ea_TextureType[i].name);
            success = false;
        }

        gl.texImage3D(GL_TEXTURE_2D_ARRAY, 0, ea_TextureFormat[i].value, 1, 1, 1, 0, ea_TextureFormat[i].value,
                      ea_TextureType[i].value, (const void *)NULL);
        GLU_EXPECT_NO_ERROR(gl.getError(), "texImage3D");
        gl.texSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 1, 1, 1, ea_TextureFormat[i].value, ea_TextureType[i].value,
                         buf);

        if (gl.getError() == GL_INVALID_ENUM)
        {
            tcu_fail_msg("ApiCoverageTestCase::CallTexSubImage3D", "Invalid enums : (%s, %s)", ea_TextureFormat[i].name,
                         ea_TextureType[i].name);
            success = false;
        }
    }

    return success;
}

/* Coverage test for glCopyTexSubImage3D */
bool ApiCoverageTestCase::TestCoverageGLCallCopyTexSubImage3D(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLubyte buf[1000];
    GLint i;
    GLenum colorBufferFormat, targetFormats[5];
    GLsizei numTargetFormats;

    (void)gl.getError(); // clear error from previous test, if any

    colorBufferFormat = TestCoverageGLGuessColorBufferFormat();
    numTargetFormats  = TestCoverageGLCalcTargetFormats(colorBufferFormat, targetFormats);

    memset(buf, 0, sizeof(GLubyte) * 100);

    for (i = 0; i != numTargetFormats; i++)
    {
        gl.texImage3D(GL_TEXTURE_3D, 0, targetFormats[i], 1, 1, 1, 0, targetFormats[i], GL_UNSIGNED_BYTE, buf);
        GLU_EXPECT_NO_ERROR(gl.getError(), "texImage3D");
        gl.copyTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 0, 0, 1, 1);
        GLU_EXPECT_NO_ERROR(gl.getError(), "copyTexSubImage3D");
        if (gl.getError() == GL_INVALID_ENUM)
        {
            const char *invalidEnum = glu::getTextureFormatName(targetFormats[i]);
            tcu_fail_msg("ApiCoverageTestCase::CallCopyTexSubImage3D", "Invalid enum : %s", invalidEnum);
            success = false;
        }

        gl.texImage3D(GL_TEXTURE_2D_ARRAY, 0, targetFormats[i], 1, 1, 1, 0, targetFormats[i], GL_UNSIGNED_BYTE,
                      (const void *)NULL);
        GLU_EXPECT_NO_ERROR(gl.getError(), "texImage3D");
        gl.copyTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 0, 0, 1, 1);
        GLU_EXPECT_NO_ERROR(gl.getError(), "copyTexSubImage3D");
        if (gl.getError() == GL_INVALID_ENUM)
        {
            const char *invalidEnum = glu::getTextureFormatName(targetFormats[i]);
            tcu_fail_msg("ApiCoverageTestCase::CallCopyTexSubImage3D", "Invalid enum : %s", invalidEnum);
            success = false;
        }
    }

    return success;
}

/* Coverage test for glCompressedTexImage3D */
bool ApiCoverageTestCase::TestCoverageGLCallCompressedTexImage3D(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLubyte buf[1000];
    GLint i, j;

    bool bETCSupported = false;
    bool bRGTCSpported = false;

    if (!m_is_context_ES)
    {
        bETCSupported = m_context.getContextInfo().isExtensionSupported("GL_ARB_ES3_compatibility") ||
                        glu::contextSupports(m_context_type, glu::ApiType::core(4, 3));
        bRGTCSpported = m_context.getContextInfo().isExtensionSupported("GL_ARB_texture_compression_rgtc") ||
                        glu::contextSupports(m_context_type, glu::ApiType::core(3, 0));
    }

    (void)gl.getError(); // clear error from previous test, if any

    for (i = 0; ea_CompressedTexture3DTarget[i].value != -1; i++)
    {
        for (j = 0; ea_CompressedTextureFormat[j].value != -1; j++)
        {
            if (!m_is_context_ES)
            {
                if (GTF_TEXTURE_FORMAT_IS_ETC(ea_CompressedTextureFormat[j].value) && !bETCSupported)
                    continue;

                if (GTF_TEXTURE_FORMAT_IS_RGTC(ea_CompressedTextureFormat[j].value) && !bRGTCSpported)
                    continue;
            }
            memset(buf, 0, 1000 * sizeof(GLubyte));
            gl.compressedTexImage3D(ea_CompressedTexture3DTarget[i].value, 0, ea_CompressedTextureFormat[j].value, 4, 4,
                                    1, 0, CompressedTextureSize[j], buf);
            if (gl.getError() == GL_INVALID_ENUM)
            {
                tcu_fail_msg("ApiCoverageTestCase::CallCompressedTexImage3D", "Invalid enums : (%s, %s)",
                             ea_CompressedTexture3DTarget[i].name, ea_CompressedTextureFormat[j].name);
                success = false;
            }

            gl.compressedTexImage3D(ea_CompressedTexture3DTarget[i].value, 0, ea_CompressedTextureFormat[j].value, 4, 4,
                                    1, 0, CompressedTextureSize[j], (const void *)NULL);
            if (gl.getError() == GL_INVALID_ENUM)
            {
                tcu_fail_msg("ApiCoverageTestCase::CallCompressedTexImage3D", "Invalid enums : (%s, %s)",
                             ea_CompressedTexture3DTarget[i].name, ea_CompressedTextureFormat[j].name);
                success = false;
            }
        }
    }
    return success;
}

/* Coverage test for glCompressedTexSubImage3D */
bool ApiCoverageTestCase::TestCoverageGLCallCompressedTexSubImage3D(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLubyte buf[1000];
    GLint i, j;

    bool bETCSupported = false;
    bool bRGTCSpported = false;

    if (!m_is_context_ES)
    {
        bETCSupported = m_context.getContextInfo().isExtensionSupported("GL_ARB_ES3_compatibility") ||
                        glu::contextSupports(m_context_type, glu::ApiType::core(4, 3));
        bRGTCSpported = m_context.getContextInfo().isExtensionSupported("GL_ARB_texture_compression_rgtc") ||
                        glu::contextSupports(m_context_type, glu::ApiType::core(3, 0));
    }

    (void)gl.getError(); // clear error from previous test, if any

    for (i = 0; ea_CompressedTexture3DTarget[i].value != -1; i++)
    {
        for (j = 0; ea_CompressedTextureFormat[j].value != -1; j++)
        {
            if (!m_is_context_ES)
            {
                if (GTF_TEXTURE_FORMAT_IS_ETC(ea_CompressedTextureFormat[j].value) && !bETCSupported)
                    continue;

                if (GTF_TEXTURE_FORMAT_IS_RGTC(ea_CompressedTextureFormat[j].value) && !bRGTCSpported)
                    continue;
            }
            memset(buf, 0, 1000 * sizeof(GLubyte));
            gl.compressedTexImage3D(ea_CompressedTexture3DTarget[i].value, 0, ea_CompressedTextureFormat[j].value, 4, 4,
                                    1, 0, CompressedTextureSize[j], buf);
            gl.compressedTexSubImage3D(ea_CompressedTexture3DTarget[i].value, 0, 0, 0, 0, 4, 4, 1,
                                       ea_CompressedTextureFormat[j].value, CompressedTextureSize[j], buf);
            if (gl.getError() == GL_INVALID_ENUM)
            {
                tcu_fail_msg("ApiCoverageTestCase::CallCompressedTexSubImage3D", "Invalid enums : (%s, %s)",
                             ea_CompressedTexture3DTarget[i].name, ea_CompressedTextureFormat[j].name);
                success = false;
            }
        }
    }

    return success;
}

/* Coverage test for glGenQueries */
bool ApiCoverageTestCase::TestCoverageGLCallGenQueries(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLuint q[2];
    bool success = true;

    (void)gl.getError(); // clear error from previous test, if any

    gl.genQueries(2, q);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genQueries");

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::GenQueries", "Failed.");
        success = false;
    }
    gl.deleteQueries(2, q);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteQueries");

    return success;
}

/* Coverage test for glDeleteQueries */
bool ApiCoverageTestCase::TestCoverageGLCallDeleteQueries(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLuint q[2];
    bool success = true;

    (void)gl.getError(); // clear error from previous test, if any

    gl.genQueries(2, q);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genQueries");
    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::GenQueries", "Failed to create a query object.");
        success = false;
    }

    gl.deleteQueries(2, q);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteQueries");
    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::DeleteQueries", "Failed to deleate a query object.");
        success = false;
    }

    return success;
}

/* Coverage test for glIsQuery */
bool ApiCoverageTestCase::TestCoverageGLCallIsQuery(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLuint q[2];
    bool success = true;

    (void)gl.getError(); // clear error from previous test, if any

    gl.genQueries(2, q);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genQueries");
    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::IsQuery", "Failed to create a query object.");
        success = false;
    }

    if (gl.isQuery(0) == GL_TRUE)
    {
        tcu_fail_msg("ApiCoverageTestCase::IsQuery", "Failed on id 0.");
        success = false;
    }
    if (gl.isQuery(q[0]) == GL_TRUE)
    {
        tcu_fail_msg("ApiCoverageTestCase::IsQuery", "Failed on id %d.", q[0]);
        success = false;
    }

    gl.beginQuery(GL_ANY_SAMPLES_PASSED, q[0]);
    GLU_EXPECT_NO_ERROR(gl.getError(), "beginQuery");
    if (gl.isQuery(q[0]) == GL_FALSE)
    {
        tcu_fail_msg("ApiCoverageTestCase::IsQuery", "Failed on id %d.", q[0]);
        success = false;
    }
    gl.endQuery(GL_ANY_SAMPLES_PASSED);
    GLU_EXPECT_NO_ERROR(gl.getError(), "endQuery");

    gl.deleteQueries(2, q);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteQueries");

    return success;
}

/* Coverage test for glBeginQuery */
bool ApiCoverageTestCase::TestCoverageGLCallBeginQuery(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLuint q[2], result = 0;
    bool success = true;

    (void)gl.getError(); // clear error from previous test, if any

    gl.genQueries(2, q);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genQueries");
    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::BeginQuery", "Failed to create a query object.");
        success = false;
    }

    gl.beginQuery(GL_ANY_SAMPLES_PASSED, q[0]);
    GLU_EXPECT_NO_ERROR(gl.getError(), "beginQuery");

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::BeginQuery", "Failed to begin query.");
        success = false;
    }

    gl.clear(GL_COLOR_BUFFER_BIT);
    GLU_EXPECT_NO_ERROR(gl.getError(), "clear");
    gl.endQuery(GL_ANY_SAMPLES_PASSED);
    GLU_EXPECT_NO_ERROR(gl.getError(), "endQuery");
    gl.finish();
    GLU_EXPECT_NO_ERROR(gl.getError(), "finish");
    gl.getQueryObjectuiv(q[0], GL_QUERY_RESULT, &result);
    GLU_EXPECT_NO_ERROR(gl.getError(), "getQueryObjectuiv");
    gl.deleteQueries(2, q);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteQueries");

    return success;
}

/* Coverage test for glEndQuery */
bool ApiCoverageTestCase::TestCoverageGLCallEndQuery(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLuint q[2], result = 0;
    bool success = true;

    (void)gl.getError(); // clear error from previous test, if any

    gl.genQueries(2, q);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genQueries");
    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::EndQuery", "Failed to create a query object.");
        success = false;
    }

    gl.beginQuery(GL_ANY_SAMPLES_PASSED, q[0]);
    GLU_EXPECT_NO_ERROR(gl.getError(), "beginQuery");

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::EndQuery", "Failed to begin query.");
        success = false;
    }

    gl.clear(GL_COLOR_BUFFER_BIT);
    GLU_EXPECT_NO_ERROR(gl.getError(), "clear");
    (void)gl.getError();

    gl.endQuery(GL_ANY_SAMPLES_PASSED);
    GLU_EXPECT_NO_ERROR(gl.getError(), "endQuery");

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::EndQuery", "Failed to end query.");
        success = false;
    }

    gl.getQueryObjectuiv(q[0], GL_QUERY_RESULT, &result);
    GLU_EXPECT_NO_ERROR(gl.getError(), "getQueryObjectuiv");
    gl.deleteQueries(2, q);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteQueries");

    return success;
}

/* Coverage test for glGetQueryiv */
bool ApiCoverageTestCase::TestCoverageGLCallGetQueryiv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLuint q[2]              = {0, 0};
    GLint iresult            = 0;
    GLuint uresult           = 0;
    bool success             = true;

    (void)gl.getError(); // clear error from previous test, if any

    gl.genQueries(2, q);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genQueries");
    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::GetQueryiv", "Failed to create a query object.");
        success = false;
    }

    gl.beginQuery(GL_ANY_SAMPLES_PASSED, q[0]);
    GLU_EXPECT_NO_ERROR(gl.getError(), "beginQuery");
    gl.clear(GL_COLOR_BUFFER_BIT);
    GLU_EXPECT_NO_ERROR(gl.getError(), "clear");
    (void)gl.getError();

    gl.getQueryiv(GL_ANY_SAMPLES_PASSED, GL_CURRENT_QUERY, &iresult);
    GLU_EXPECT_NO_ERROR(gl.getError(), "getQueryiv");
    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::GetQueryiv", "Failed to get active query object.");
        success = false;
    }

    gl.endQuery(GL_ANY_SAMPLES_PASSED);
    GLU_EXPECT_NO_ERROR(gl.getError(), "endQuery");
    gl.getQueryObjectuiv(q[0], GL_QUERY_RESULT, &uresult);
    GLU_EXPECT_NO_ERROR(gl.getError(), "getQueryObjectuiv");
    gl.deleteQueries(2, q);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteQueries");

    return success;
}

/* Coverage test for glGetQueryObjectuiv */
bool ApiCoverageTestCase::TestCoverageGLCallGetQueryObjectuiv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLuint q[2], result = 0;
    bool success = true;

    (void)gl.getError(); // clear error from previous test, if any

    gl.genQueries(2, q);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genQueries");
    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::GetQueryiv", "Failed to create a query object.");
        success = false;
    }

    gl.beginQuery(GL_ANY_SAMPLES_PASSED, q[0]);
    GLU_EXPECT_NO_ERROR(gl.getError(), "beginQuery");
    gl.clear(GL_COLOR_BUFFER_BIT);
    GLU_EXPECT_NO_ERROR(gl.getError(), "clear");
    gl.endQuery(GL_ANY_SAMPLES_PASSED);
    GLU_EXPECT_NO_ERROR(gl.getError(), "endQuery");
    (void)gl.getError();

    gl.getQueryObjectuiv(q[0], GL_QUERY_RESULT, &result);
    GLU_EXPECT_NO_ERROR(gl.getError(), "getQueryObjectuiv");
    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::GetQueryObjectuiv", "Failed to get query result.");
        success = false;
    }

    gl.deleteQueries(2, q);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteQueries");

    return success;
}

/* Coverage test for glGetBufferPointerv */
bool ApiCoverageTestCase::TestCoverageGLCallGetBufferPointerv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLuint BufObj, tfObj, size = 4096;
    float *pMappedTFBuf;
    static GLfloat position[] = {
        -0.5f, -0.625f, 0.5f, 1.0f, 0.125f, 0.75f, 0.625f, 1.125f, 0.875f, -0.75f, 1.125f, 1.5f,
    };

    (void)gl.getError(); // clear error from previous test, if any

    GLuint vbo = 0;
    gl.genBuffers(1, &vbo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenBuffers");
    gl.bindBuffer(GL_ARRAY_BUFFER, vbo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer");
    gl.bufferData(GL_ARRAY_BUFFER, sizeof(position), (GLvoid *)position, GL_STATIC_DRAW);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBufferData");

    GLuint vao = 0;
    gl.genVertexArrays(1, &vao);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genVertexArrays");
    gl.bindVertexArray(vao);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindVertexArray");
    gl.enableVertexAttribArray(0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "enableVertexAttribArray");
    gl.vertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
    GLU_EXPECT_NO_ERROR(gl.getError(), "vertexAttribPointer");
    if (m_is_transform_feedback_obj_supported)
    {
        gl.genTransformFeedbacks(1, &tfObj);
        GLU_EXPECT_NO_ERROR(gl.getError(), "genTransformFeedbacks");
        gl.bindTransformFeedback(GL_TRANSFORM_FEEDBACK, tfObj);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bindTransformFeedback");
    }

    gl.genBuffers(1, &BufObj);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genBuffers");
    gl.bindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, BufObj);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindBufferBase");
    gl.bufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 4096, NULL, GL_DYNAMIC_READ);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bufferData");
    pMappedTFBuf = (float *)glMapBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, size, GL_MAP_READ_BIT);

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::GetBufferPointerv", "Setup Failed.");
        success = false;
    }

    gl.getBufferPointerv(GL_TRANSFORM_FEEDBACK_BUFFER, GL_BUFFER_MAP_POINTER, (GLvoid **)(&pMappedTFBuf));
    GLU_EXPECT_NO_ERROR(gl.getError(), "getBufferPointerv");
    if (pMappedTFBuf == NULL || gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::GetBufferPointerv", "Failed.");
        success = false;
    }

    gl.unmapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER);
    GLU_EXPECT_NO_ERROR(gl.getError(), "unmapBuffer");
    gl.deleteBuffers(1, &BufObj);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteBuffers");
    if (m_is_transform_feedback_obj_supported)
    {
        gl.deleteTransformFeedbacks(1, &tfObj);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteTransformFeedbacks");
    }

    gl.disableVertexAttribArray(0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glDisableVertexAttribArray");

    if (vbo)
    {
        gl.deleteBuffers(1, &vbo);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteBuffers");
    }

    if (vao)
    {
        gl.deleteVertexArrays(1, &vao);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteVertexArrays");
    }

    return success;
}

/* Coverage test for glMapBufferRange */
bool ApiCoverageTestCase::TestCoverageGLCallMapBufferRange(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLuint BufObj, tfObj, size = 4096;
    float *pMappedTFBuf;
    static GLfloat position[] = {
        -0.5f, -0.625f, 0.5f, 1.0f, 0.125f, 0.75f, 0.625f, 1.125f, 0.875f, -0.75f, 1.125f, 1.5f,
    };

    (void)gl.getError(); // clear error from previous test, if any

    GLuint vbo = 0;
    gl.genBuffers(1, &vbo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenBuffers");
    gl.bindBuffer(GL_ARRAY_BUFFER, vbo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer");
    gl.bufferData(GL_ARRAY_BUFFER, sizeof(position), (GLvoid *)position, GL_STATIC_DRAW);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBufferData");

    GLuint vao = 0;
    gl.genVertexArrays(1, &vao);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genVertexArrays");
    gl.bindVertexArray(vao);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindVertexArray");

    gl.enableVertexAttribArray(0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "enableVertexAttribArray");
    gl.vertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
    GLU_EXPECT_NO_ERROR(gl.getError(), "vertexAttribPointer");
    if (m_is_transform_feedback_obj_supported)
    {
        gl.genTransformFeedbacks(1, &tfObj);
        GLU_EXPECT_NO_ERROR(gl.getError(), "genTransformFeedbacks");
        gl.bindTransformFeedback(GL_TRANSFORM_FEEDBACK, tfObj);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bindTransformFeedback");
    }
    gl.genBuffers(1, &BufObj);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genBuffers");
    gl.bindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, BufObj);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindBufferBase");
    gl.bufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 4096, NULL, GL_DYNAMIC_READ);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bufferData");

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::MapBufferRange", "Setup Failed.");
        success = false;
    }

    pMappedTFBuf = (float *)glMapBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, size, GL_MAP_READ_BIT);
    if (pMappedTFBuf == NULL || gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::MapBufferRange", "Failed.");
        success = false;
    }

    gl.unmapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER);
    GLU_EXPECT_NO_ERROR(gl.getError(), "unmapBuffer");
    gl.deleteBuffers(1, &BufObj);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteBuffers");
    if (m_is_transform_feedback_obj_supported)
    {
        gl.deleteTransformFeedbacks(1, &tfObj);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteTransformFeedbacks");
    }

    if (vbo)
    {
        gl.deleteBuffers(1, &vbo);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteBuffers");
    }

    if (vao)
    {
        gl.deleteVertexArrays(1, &vao);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteVertexArrays");
    }

    return success;
}
/* Coverage test for glUnmapBuffer */
bool ApiCoverageTestCase::TestCoverageGLCallUnmapBuffer(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLuint BufObj, tfObj, size = 4096;
    static GLfloat position[] = {
        -0.5f, -0.625f, 0.5f, 1.0f, 0.125f, 0.75f, 0.625f, 1.125f, 0.875f, -0.75f, 1.125f, 1.5f,
    };

    (void)gl.getError(); // clear error from previous test, if any

    GLuint vbo = 0;
    gl.genBuffers(1, &vbo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenBuffers");
    gl.bindBuffer(GL_ARRAY_BUFFER, vbo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer");
    gl.bufferData(GL_ARRAY_BUFFER, sizeof(position), (GLvoid *)position, GL_STATIC_DRAW);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBufferData");

    GLuint vao = 0;
    gl.genVertexArrays(1, &vao);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genVertexArrays");
    gl.bindVertexArray(vao);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindVertexArray");
    gl.enableVertexAttribArray(0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "enableVertexAttribArray");
    gl.vertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
    GLU_EXPECT_NO_ERROR(gl.getError(), "vertexAttribPointer");
    if (m_is_transform_feedback_obj_supported)
    {
        gl.genTransformFeedbacks(1, &tfObj);
        GLU_EXPECT_NO_ERROR(gl.getError(), "genTransformFeedbacks");
        gl.bindTransformFeedback(GL_TRANSFORM_FEEDBACK, tfObj);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bindTransformFeedback");
    }
    gl.genBuffers(1, &BufObj);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genBuffers");
    gl.bindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, BufObj);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindBufferBase");
    gl.bufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 4096, NULL, GL_DYNAMIC_READ);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bufferData");
    float *pMappedTFBuf = (float *)glMapBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, size, GL_MAP_READ_BIT);

    if (gl.getError() != GL_NO_ERROR || pMappedTFBuf == nullptr)
    {
        tcu_fail_msg("ApiCoverageTestCase::UnmapBuffer", "Setup Failed.");
        success = false;
    }

    gl.unmapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER);
    GLU_EXPECT_NO_ERROR(gl.getError(), "unmapBuffer");
    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::UnmapBuffer", "Failed.");
        success = false;
    }

    gl.disableVertexAttribArray(0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glDisableVertexAttribArray");

    gl.deleteBuffers(1, &BufObj);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteBuffers");
    if (m_is_transform_feedback_obj_supported)
    {
        gl.deleteTransformFeedbacks(1, &tfObj);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteTransformFeedbacks");
    }

    if (vbo)
    {
        gl.deleteBuffers(1, &vbo);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteBuffers");
    }

    if (vao)
    {
        gl.deleteVertexArrays(1, &vao);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteVertexArrays");
    }

    return success;
}

/* Coverage test for glFlushMappedBufferRange */
bool ApiCoverageTestCase::TestCoverageGLCallFlushMappedBufferRange(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLuint BufObj, tfObj, size = 4096;
    static GLfloat position[] = {
        -0.5f, -0.625f, 0.5f, 1.0f, 0.125f, 0.75f, 0.625f, 1.125f, 0.875f, -0.75f, 1.125f, 1.5f,
    };

    (void)gl.getError(); // clear error from previous test, if any

    GLuint vbo = 0;
    gl.genBuffers(1, &vbo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenBuffers");
    gl.bindBuffer(GL_ARRAY_BUFFER, vbo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer");
    gl.bufferData(GL_ARRAY_BUFFER, sizeof(position), (GLvoid *)position, GL_STATIC_DRAW);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBufferData");

    GLuint vao = 0;
    gl.genVertexArrays(1, &vao);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genVertexArrays");
    gl.bindVertexArray(vao);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindVertexArray");

    gl.enableVertexAttribArray(0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "enableVertexAttribArray");
    gl.vertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
    GLU_EXPECT_NO_ERROR(gl.getError(), "vertexAttribPointer");
    if (m_is_transform_feedback_obj_supported)
    {
        gl.genTransformFeedbacks(1, &tfObj);
        GLU_EXPECT_NO_ERROR(gl.getError(), "genTransformFeedbacks");
        gl.bindTransformFeedback(GL_TRANSFORM_FEEDBACK, tfObj);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bindTransformFeedback");
    }
    gl.genBuffers(1, &BufObj);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genBuffers");
    gl.bindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, BufObj);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindBufferBase");
    gl.bufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 4096, NULL, GL_DYNAMIC_READ);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bufferData");
    float *pMappedTFBuf = (float *)glMapBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, size,
                                                    GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);

    if (gl.getError() != GL_NO_ERROR || pMappedTFBuf == nullptr)
    {
        tcu_fail_msg("ApiCoverageTestCase::FlushMappedBufferRange", "Setup Failed.");
        success = false;
    }

    gl.flushMappedBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, size);
    GLU_EXPECT_NO_ERROR(gl.getError(), "flushMappedBufferRange");
    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::FlushMappedBufferRange", "Failed.");
        success = false;
    }

    gl.unmapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER);
    GLU_EXPECT_NO_ERROR(gl.getError(), "unmapBuffer");
    gl.deleteBuffers(1, &BufObj);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteBuffers");
    if (m_is_transform_feedback_obj_supported)
    {
        gl.deleteTransformFeedbacks(1, &tfObj);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteTransformFeedbacks");
    }

    if (vbo)
    {
        gl.deleteBuffers(1, &vbo);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteBuffers");
    }

    if (vao)
    {
        gl.deleteVertexArrays(1, &vao);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteVertexArrays");
    }

    return success;
}

/* Coverage test for glDrawBuffers */
bool ApiCoverageTestCase::TestCoverageGLCallDrawBuffers(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLuint i;
    bool success = true;
    GLenum bufs[4];
    GLint width = 32, height = 32;
    GLuint fbo = 0, rbo_color[4] = {0, 0, 0, 0};

    (void)gl.getError(); // clear error from previous test, if any

    gl.genFramebuffers(1, &fbo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genFramebuffers");
    gl.bindFramebuffer(GL_FRAMEBUFFER, fbo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");
    gl.genRenderbuffers(4, rbo_color);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genRenderbuffers");

    // attach RBOs
    for (i = 0; ea_DrawBuffers[i].value != -1; i++)
    {
        gl.bindRenderbuffer(GL_RENDERBUFFER, rbo_color[i]);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bindRenderbuffer");
        gl.renderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);
        GLU_EXPECT_NO_ERROR(gl.getError(), "renderbufferStorage");
        gl.framebufferRenderbuffer(GL_FRAMEBUFFER, ea_DrawBuffers[i].value, GL_RENDERBUFFER, rbo_color[i]);
        GLU_EXPECT_NO_ERROR(gl.getError(), "framebufferRenderbuffer");
    }

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        tcu_fail_msg("ApiCoverageTestCase::DrawBuffers", "Expected GL_FRAMEBUFFER_COMPLETE.");
        gl.deleteFramebuffers(1, &fbo);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteFramebuffers");
        return false;
    }

    GLuint dbuffer = 0;
    gl.drawBuffers(1, &dbuffer);
    GLU_EXPECT_NO_ERROR(gl.getError(), "drawBuffers");

    ea_DrawBuffers[0].value = (GLint)dbuffer;
    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::DrawBuffers", "Failed %s.", ea_DrawBuffers[i].name);
        success = false;
    }

    for (i = 0; ea_DrawBuffers[i].value != -1; i++)
    {
        bufs[i] = ea_DrawBuffers[i].value;
    }

    gl.drawBuffers(4, bufs);
    GLU_EXPECT_NO_ERROR(gl.getError(), "drawBuffers");
    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::DrawBuffers", "Failed on 4 concurrent buffers.");
        success = false;
    }

    gl.deleteFramebuffers(1, &fbo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteFramebuffers");

    gl.deleteRenderbuffers(4, rbo_color);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteRenderbuffers");

    return success;
}
/* Coverage test for glUniformMatrix2x4fv */

bool ApiCoverageTestCase::TestCoverageGLCallUniformMatrix2x4fv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint program;
    GLfloat v[] = {
        1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
    };

    (void)gl.getError(); // clear error from previous test, if any

    program = createDefaultProgram(0);
    gl.uniformMatrix2x3fv(0, 1, GL_FALSE, v);

    // not a valid config for uniforms
    if (gl.getError() != GL_INVALID_OPERATION)
    {
        tcu_fail_msg("ApiCoverageTestCase::UniformMatrix2x4fv", "Failed.");
        success = false;
    }

    gl.deleteProgram((GLuint)program);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteProgram");

    return success;
}

/* Coverage test for glBeginTransformFeedback */
bool ApiCoverageTestCase::TestCoverageGLCallBeginTransformFeedback(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint program;
    GLuint tobj = 0, tbufobj = 0;

    (void)gl.getError(); // clear error from previous test, if any

    program = createDefaultProgram(1);

    if (program == -1)
    {
        tcu_fail_msg("ApiCoverageTestCase::BeginTransformFeedback", "Program create Failed");
        return false;
    }

    if (m_is_transform_feedback_obj_supported)
    {
        gl.genTransformFeedbacks(1, &tobj);
        GLU_EXPECT_NO_ERROR(gl.getError(), "genTransformFeedbacks");
        gl.bindTransformFeedback(GL_TRANSFORM_FEEDBACK, tobj);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bindTransformFeedback");
    }
    gl.genBuffers(1, &tbufobj);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genBuffers");
    gl.bindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, tbufobj);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindBufferBase");

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::BeginTransformFeedback", "Failed to create transform feedback buffer.");
        success = false;
    }

    gl.beginTransformFeedback(GL_TRIANGLES);
    GLU_EXPECT_NO_ERROR(gl.getError(), "beginTransformFeedback");

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::BeginTransformFeedback", "Failed.");
        success = false;
    }

    gl.endTransformFeedback();
    GLU_EXPECT_NO_ERROR(gl.getError(), "endTransformFeedback");

    gl.deleteBuffers(1, &tbufobj);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteBuffers");
    if (m_is_transform_feedback_obj_supported)
    {
        gl.deleteTransformFeedbacks(1, &tobj);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteTransformFeedbacks");
    }
    gl.deleteProgram((GLuint)program);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteProgram");

    return success;
}

/* Coverage test for glEndTransformFeedback */
bool ApiCoverageTestCase::TestCoverageGLCallEndTransformFeedback(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint program            = 0;
    GLuint tobj = 0, tbufobj = 0;

    (void)gl.getError(); // clear error from previous test, if any

    program = createDefaultProgram(1);

    if (program == -1)
    {
        tcu_fail_msg("ApiCoverageTestCase::EndTransformFeedback", "Program create Failed");
        return false;
    }

    if (m_is_transform_feedback_obj_supported)
    {
        gl.genTransformFeedbacks(1, &tobj);
        GLU_EXPECT_NO_ERROR(gl.getError(), "genTransformFeedbacks");
        gl.bindTransformFeedback(GL_TRANSFORM_FEEDBACK, tobj);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bindTransformFeedback");
    }
    gl.genBuffers(1, &tbufobj);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genBuffers");
    gl.bindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, tbufobj);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindBufferBase");

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::EndTransformFeedback", "Failed to create transform feedback buffer.");
        success = false;
    }

    gl.beginTransformFeedback(GL_TRIANGLES);
    GLU_EXPECT_NO_ERROR(gl.getError(), "beginTransformFeedback");

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::EndTransformFeedback", "Failed to start.");
        success = false;
    }

    gl.endTransformFeedback();
    GLU_EXPECT_NO_ERROR(gl.getError(), "endTransformFeedback");

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::EndTransformFeedback", "Failed.");
        success = false;
    }

    gl.deleteBuffers(1, &tbufobj);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteBuffers");
    if (m_is_transform_feedback_obj_supported)
    {
        gl.deleteTransformFeedbacks(1, &tobj);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteTransformFeedbacks");
    }
    gl.deleteProgram((GLuint)program);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteProgram");

    return success;
}

/* Coverage test for glBindBufferRange */
bool ApiCoverageTestCase::TestCoverageGLCallBindBufferRange(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLuint tobj = 0, tbufobj = 0;
    char data[16];

    (void)gl.getError(); // clear error from previous test, if any

    if (m_is_transform_feedback_obj_supported)
    {
        gl.genTransformFeedbacks(1, &tobj);
        GLU_EXPECT_NO_ERROR(gl.getError(), "genTransformFeedbacks");
        gl.bindTransformFeedback(GL_TRANSFORM_FEEDBACK, tobj);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bindTransformFeedback");
    }
    gl.genBuffers(1, &tbufobj);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genBuffers");
    gl.bindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, tbufobj);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindBufferBase");
    gl.bufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 16, data, GL_DYNAMIC_READ);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bufferData");

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::BindBufferRange", "Failed to create transform feedback buffer.");
        success = false;
    }

    gl.bindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 1, tbufobj, 0, 4);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindBufferRange");

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::BindBufferRange", "Failed.");
        success = false;
    }

    gl.deleteBuffers(1, &tbufobj);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteBuffers");
    if (m_is_transform_feedback_obj_supported)
    {
        gl.deleteTransformFeedbacks(1, &tobj);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteTransformFeedbacks");
    }

    return success;
}

/* Coverage test for glBindBufferBase */
bool ApiCoverageTestCase::TestCoverageGLCallBindBufferBase(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLuint tobj = 0, tbufobj = 0;

    (void)gl.getError(); // clear error from previous test, if any

    if (m_is_transform_feedback_obj_supported)
    {
        gl.genTransformFeedbacks(1, &tobj);
        GLU_EXPECT_NO_ERROR(gl.getError(), "genTransformFeedbacks");
        gl.bindTransformFeedback(GL_TRANSFORM_FEEDBACK, tobj);
        GLU_EXPECT_NO_ERROR(gl.getError(), "bindTransformFeedback");
    }
    gl.genBuffers(1, &tbufobj);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genBuffers");

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::BindBufferBase", "Failed to create transform feedback buffer.");
        success = false;
    }

    gl.bindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, tbufobj);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindBufferBase");

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::BindBufferBase", "Failed.");
        success = false;
    }

    gl.deleteBuffers(1, &tbufobj);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteBuffers");
    if (m_is_transform_feedback_obj_supported)
    {
        gl.deleteTransformFeedbacks(1, &tobj);
        GLU_EXPECT_NO_ERROR(gl.getError(), "deleteTransformFeedbacks");
    }

    return success;
}

/* Coverage test for glTransformFeedbackVaryings */
bool ApiCoverageTestCase::TestCoverageGLCallTransformFeedbackVaryings(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    const char *ptex         = "texCoords";
    GLenum err;

    gl.transformFeedbackVaryings(0, 1, &ptex, GL_SEPARATE_ATTRIBS);

    err = gl.getError();
    if ((err != GL_INVALID_OPERATION) && (err != GL_INVALID_VALUE))
    {
        tcu_fail_msg("ApiCoverageTestCase::TransformFeedbackVaryings", "Failed.");
        success = false;
    }

    return success;
}

/* Coverage test for glGetTransformFeedbackVarying */
bool ApiCoverageTestCase::TestCoverageGLCallGetTransformFeedbackVarying(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint program = 0, length = 0, size = 0;
    GLuint type = 0;
    char name[32];

    (void)gl.getError(); // clear error from previous test, if any

    program = createDefaultProgram(1);

    if (program == -1)
    {
        tcu_fail_msg("ApiCoverageTestCase::GetTransformFeedbackVarying", "Program create Failed");
        return false;
    }

    gl.getTransformFeedbackVarying(program, 0, 32, &length, &size, &type, name);
    GLU_EXPECT_NO_ERROR(gl.getError(), "getTransformFeedbackVarying");

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::GetTransformFeedbackVarying", "Failed.");
        success = false;
    }

    gl.deleteProgram((GLuint)program);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteProgram");

    return success;
}

/* Coverage test for glVertexAttribIPointer */
bool ApiCoverageTestCase::TestCoverageGLCallVertexAttribIPointer(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLfloat buf[]            = {1.0f};

    (void)gl.getError(); // clear error from previous test, if any

    GLuint vbo = 0;
    gl.genBuffers(1, &vbo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenBuffers");
    gl.bindBuffer(GL_ARRAY_BUFFER, vbo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBindBuffer");
    gl.bufferData(GL_ARRAY_BUFFER, sizeof(buf), (GLvoid *)buf, GL_STATIC_DRAW);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glBufferData");

    GLuint vao = 0;
    gl.genVertexArrays(1, &vao);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genVertexArrays");
    gl.bindVertexArray(vao);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindVertexArray");

    gl.vertexAttribIPointer(0, 1, GL_INT, 0, nullptr);
    GLU_EXPECT_NO_ERROR(gl.getError(), "vertexAttribIPointer");

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::VertexAttribIPointer", "Failed.");
        success = false;
    }

    gl.vertexAttribIPointer(0, 1, GL_FLOAT, 0, nullptr);

    if (gl.getError() != GL_INVALID_ENUM)
    {
        tcu_fail_msg("ApiCoverageTestCase::VertexAttribIPointer", "Failed.");
        success = false;
    }

    if (vbo)
    {
        gl.deleteBuffers(1, &vbo);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteBuffers");
    }

    if (vao)
    {
        gl.deleteVertexArrays(1, &vao);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteVertexArrays");
    }

    return success;
}

/* Coverage test for glGetVertexAttribIiv */
bool ApiCoverageTestCase::TestCoverageGLCallGetVertexAttribIiv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint buf[10];
    GLuint index;
    GLint i;

    index = 1;

    for (i = 0; ea_GetVertexAttrib[i].value != -1; i++)
    {
        gl.getVertexAttribIiv(index, ea_GetVertexAttrib[i].value, buf);

        if (gl.getError() == GL_INVALID_ENUM)
        {
            tcu_fail_msg("ApiCoverageTestCase::CallGetVertexAttribIiv", "Invalid enum : %s",
                         ea_GetVertexAttrib[i].name);
            success = false;
        }
    }

    return success;
}

/* Coverage test for glGetVertexAttribIuiv */
bool ApiCoverageTestCase::TestCoverageGLCallGetVertexAttribIuiv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLuint buf[10];
    GLuint index;
    GLint i;

    index = 1;

    for (i = 0; ea_GetVertexAttrib[i].value != -1; i++)
    {
        gl.getVertexAttribIuiv(index, ea_GetVertexAttrib[i].value, buf);

        if (gl.getError() == GL_INVALID_ENUM)
        {
            tcu_fail_msg("ApiCoverageTestCase::CallGetVertexAttribIuiv", "Invalid enum : %s",
                         ea_GetVertexAttrib[i].name);
            success = false;
        }
    }

    return success;
}

/* Coverage test for glVertexAttribI4i */

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttribI4i(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    gl.vertexAttribI4i(0, 1, 2, 3, 4);

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::VertexAttribI4i", "Failed.");
        success = false;
    }

    return success;
}

/* Coverage test for glVertexAttribI4iv */
bool ApiCoverageTestCase::TestCoverageGLCallVertexAttribI4iv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint buf[]              = {1, 2, 3, 4};

    gl.vertexAttribI4iv(0, buf);

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::VertexAttribI4iv", "Failed.");
        success = false;
    }

    return success;
}

/* Coverage test for glVertexAttribI4ui */
bool ApiCoverageTestCase::TestCoverageGLCallVertexAttribI4ui(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    gl.vertexAttribI4ui(0, 1, 2, 3, 4);

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::VertexAttribI4ui", "Failed.");
        success = false;
    }

    return success;
}

/* Coverage test for glVertexAttribI4uiv */
bool ApiCoverageTestCase::TestCoverageGLCallVertexAttribI4uiv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLuint buf[]             = {1, 2, 3, 4};

    gl.vertexAttribI4uiv(0, buf);

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::VertexAttribI4uiv", "Failed.");
        success = false;
    }

    return success;
}

/* Coverage test for glGetUniformuiv */
bool ApiCoverageTestCase::TestCoverageGLCallGetUniformuiv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLuint buf[10];
    GLuint program;

    program = gl.createProgram();
    GLU_EXPECT_NO_ERROR(gl.getError(), "createProgram");

    gl.getUniformuiv(program, 0, buf);

    /* program is unlinked, so error should be INVALID_OPERATION */
    if (gl.getError() != GL_INVALID_OPERATION)
    {
        tcu_fail_msg("ApiCoverageTestCase::GetUniformuiv", "GL_INVALID_OPERATION not returned.");
        success = false;
    }

    gl.useProgram(0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "useProgram");
    gl.deleteProgram(program);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteProgram");

    return success;
}

/* Coverage test for glGetFragDataLocation */
bool ApiCoverageTestCase::TestCoverageGLCallGetFragDataLocation(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLuint program;

    program = gl.createProgram();
    GLU_EXPECT_NO_ERROR(gl.getError(), "createProgram");
    gl.getFragDataLocation(program, "fragData");

    /* program is unlinked, so error should be INVALID_OPERATION */
    if (gl.getError() != GL_INVALID_OPERATION)
    {
        tcu_fail_msg("ApiCoverageTestCase::GetFragDataLocation", "GL_INVALID_OPERATION not returned.");
        success = false;
    }

    gl.deleteProgram(program);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteProgram");

    return success;
}

/* Coverage test for glUniform2ui */
bool ApiCoverageTestCase::TestCoverageGLCallUniform2ui(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    gl.uniform2ui(0, 1, 2);

    if (gl.getError() != GL_INVALID_OPERATION)
    {
        tcu_fail_msg("ApiCoverageTestCase::CallUniform2ui", "GL_INVALID_OPERATION not returned");
        success = false;
    }

    return success;
}
/* Coverage test for glUniform2uiv */
bool ApiCoverageTestCase::TestCoverageGLCallUniform2uiv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLuint buf[]             = {1, 2, 3, 4};

    gl.uniform2uiv(0, 2, buf);

    if (gl.getError() != GL_INVALID_OPERATION)
    {
        tcu_fail_msg("ApiCoverageTestCase::CallUniform2uiv", "GL_INVALID_OPERATION not returned");
        success = false;
    }

    return success;
}

/* Coverage test for glClearBufferiv */
bool ApiCoverageTestCase::TestCoverageGLCallClearBufferiv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLint color[4]           = {0, 0, 0, 0};
    bool success             = true;

    gl.clearBufferiv(GL_STENCIL, 0, color);

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::ClearBufferiv", "Failed.");
        success = false;
    }

    return success;
}

/* Coverage test for glClearBufferuiv */
bool ApiCoverageTestCase::TestCoverageGLCallClearBufferuiv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLuint color[4]          = {0, 0, 0, 0};
    bool success             = true;

    gl.clearBufferuiv(GL_COLOR, 0, color);

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::ClearBufferuiv", "Failed.");
        success = false;
    }

    return success;
}

/* Coverage test for glClearBufferfv */
bool ApiCoverageTestCase::TestCoverageGLCallClearBufferfv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLfloat color            = 0.0f;
    bool success             = true;

    gl.clearBufferfv(GL_DEPTH, 0, &color);

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::ClearBufferfv", "Failed.");
        success = false;
    }

    return success;
}

/* Coverage test for glClearBufferfi */
bool ApiCoverageTestCase::TestCoverageGLCallClearBufferfi(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLfloat depth            = 0.0f;
    GLint stencil            = 0;
    bool success             = true;

    gl.clearBufferfi(GL_DEPTH_STENCIL, 0, depth, stencil);

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::ClearBufferfi", "Failed.");
        success = false;
    }

    return success;
}

/* Coverage test for glGetStringi */

bool ApiCoverageTestCase::TestCoverageGLCallGetStringi(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLint i, max;
    bool success        = true;
    const GLubyte *pstr = NULL;

    (void)gl.getError(); // clear any unchecked errors

    gl.getIntegerv(GL_NUM_EXTENSIONS, &i);
    max = i;
    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::GetStringi", "Get NUM_EXTENSIONS failed.");
        return false;
    }

    while (i--)
    {
        pstr = gl.getStringi(GL_EXTENSIONS, i);
        if ((gl.getError() != GL_NO_ERROR) || (NULL == pstr) || (std::strlen((const char *)pstr) <= 0))
        {
            tcu_fail_msg("ApiCoverageTestCase::GetStringi", "Get EXTENSION failed.");
            return false;
        }
    }

    pstr = gl.getStringi(GL_EXTENSIONS, max);
    if (gl.getError() != GL_INVALID_VALUE)
    {
        success = false;
        tcu_fail_msg("ApiCoverageTestCase::GetStringi", "Get EXTENSION with invalid index succeeded.");
    }

    return success;
}

/* Coverage test for glBlitFramebuffer */
bool ApiCoverageTestCase::TestCoverageGLCallBlitFramebuffer(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLint error;
    bool success = true;
    GLint width = 32, height = 32;
    GLuint fbo = 0, rbo_color = 0, rbo_depth = 0;

    (void)gl.getError(); // clear error from previous test, if any

    // FBO
    gl.genFramebuffers(1, &fbo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genFramebuffers");
    gl.bindFramebuffer(GL_FRAMEBUFFER, fbo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");

    // RBO (color)
    gl.genRenderbuffers(1, &rbo_color);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genRenderbuffers");
    gl.bindRenderbuffer(GL_RENDERBUFFER, rbo_color);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindRenderbuffer");
    gl.renderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);
    GLU_EXPECT_NO_ERROR(gl.getError(), "renderbufferStorage");
    gl.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo_color);
    GLU_EXPECT_NO_ERROR(gl.getError(), "framebufferRenderbuffer");

    // RBO (depth & stencil)
    gl.genRenderbuffers(1, &rbo_depth);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genRenderbuffers");
    gl.bindRenderbuffer(GL_RENDERBUFFER, rbo_depth);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindRenderbuffer");
    gl.renderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    GLU_EXPECT_NO_ERROR(gl.getError(), "renderbufferStorage");
    gl.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo_depth);
    GLU_EXPECT_NO_ERROR(gl.getError(), "framebufferRenderbuffer");
    gl.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo_depth);
    GLU_EXPECT_NO_ERROR(gl.getError(), "framebufferRenderbuffer");

    error = gl.checkFramebufferStatus(GL_FRAMEBUFFER);
    if (error != GL_FRAMEBUFFER_COMPLETE)
    {
        tcu_fail_msg("ApiCoverageTestCase::BlitFramebuffer", "Expected GL_FRAMEBUFFER_COMPLETE, got 0x%x", error);
        return false;
    }

    gl.bindFramebuffer(GL_DRAW_FRAMEBUFFER, m_defaultFBO);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");

    // Check errors
    // No buffer bit
    gl.blitFramebuffer(0, 0, width, height, 0, 0, width, height, 0, GL_NEAREST);

    if ((error = gl.getError()) != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::BlitFramebuffer", "Expected GL_NO_ERROR, got 0x%x (1)", error);
        success = false;
    }

    // invald buffer bit
    gl.blitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT | GL_MAP_INVALIDATE_BUFFER_BIT,
                       GL_NEAREST);

    if ((error = gl.getError()) != GL_INVALID_VALUE)
    {
        tcu_fail_msg("ApiCoverageTestCase::BlitFramebuffer", "Expected GL_INVALID_VALUE, got 0x%x (2)", error);
        success = false;
    }

    // invalide filter mode
    gl.blitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT,
                       GL_LINEAR);

    if ((error = gl.getError()) != GL_INVALID_OPERATION)
    {
        tcu_fail_msg("ApiCoverageTestCase::BlitFramebuffer", "Expected GL_INVALID_OPERATION, got 0x%x (3)", error);
        success = false;
    }

    // read/draw buffer the same
    // Overlapping blits are forbidden with ES3 and allowed with GL
    gl.bindFramebuffer(GL_FRAMEBUFFER, m_defaultFBO);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");

    gl.blitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    if (m_is_context_ES && glu::contextSupports(m_context_type, glu::ApiType::es(3, 0)))
    {
        if ((error = gl.getError()) != GL_INVALID_OPERATION)
        {
            tcu_fail_msg("ApiCoverageTestCase::BlitFramebuffer", "Expected GL_INVALID_OPERATION, got 0x%x (4)", error);
            success = false;
        }
    }
    else
    {
        if (gl.getError() != GL_NO_ERROR)
        {
            tcu_fail_msg("ApiCoverageTestCase::BlitFramebuffer", "Failed!");
            success = false;
        }
    }
    // Do the correct blit
    gl.bindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");
    gl.blitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::BlitFramebuffer", "Failed!");
        success = false;
    }

    gl.deleteFramebuffers(1, &fbo);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteFramebuffers");
    gl.deleteRenderbuffers(1, &rbo_color);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteRenderbuffers");
    gl.deleteRenderbuffers(1, &rbo_depth);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteRenderbuffers");

    return success;
}

/* Coverage test for glRenderbufferStorageMultisample */

/* Enums to test for glRenderbufferStorageMultisample (may not be required) */
struct enumTestRec const ea_RenderbufferStorageMultisample[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallRenderbufferStorageMultisample(void)
{
    GLuint i;
    bool success = true;

    /* glRenderbufferStorageMultisample may need to loop over a set of enums doing something with them */
    for (i = 0; ea_RenderbufferStorageMultisample[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::RenderbufferStorageMultisample", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glFramebufferTextureLayer */

/* Enums to test for glFramebufferTextureLayer (may not be required) */
struct enumTestRec const ea_FramebufferTextureLayer[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallFramebufferTextureLayer(void)
{
    GLuint i;
    bool success = true;

    /* glFramebufferTextureLayer may need to loop over a set of enums doing something with them */
    for (i = 0; ea_FramebufferTextureLayer[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::FramebufferTextureLayer", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glBindVertexArray */
bool ApiCoverageTestCase::TestCoverageGLCallBindVertexArray(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLuint va;
    bool success = true;

    (void)gl.getError(); // clear error from previous test, if any

    gl.genVertexArrays(1, &va);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genVertexArrays");
    gl.bindVertexArray(va);

    if (gl.getError() != GL_NO_ERROR)
    {
        success = false;
        tcu_fail_msg("ApiCoverageTestCase::BindVertexArray", "Failed");
    }

    gl.deleteVertexArrays(1, &va);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteVertexArrays");

    return success;
}

/* Coverage test for glDeleteVertexArrays */
bool ApiCoverageTestCase::TestCoverageGLCallDeleteVertexArrays(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLuint va;
    bool success = true;

    (void)gl.getError(); // clear error from previous test, if any

    gl.genVertexArrays(1, &va);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genVertexArrays");
    gl.deleteVertexArrays(1, &va);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteVertexArrays");

    gl.genVertexArrays(1, &va);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genVertexArrays");
    gl.bindVertexArray(va);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindVertexArray");
    gl.deleteVertexArrays(1, &va);

    if (gl.getError() != GL_NO_ERROR)
    {
        success = false;
        tcu_fail_msg("ApiCoverageTestCase::DeleteVertexArray", "Failed");
    }

    return success;
}

/* Coverage test for glGenVertexArrays */
bool ApiCoverageTestCase::TestCoverageGLCallGenVertexArrays(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLuint va[3]             = {0, 0, 0};
    int i;
    bool success = true;

    (void)gl.getError(); // clear error from previous test, if any

    gl.genVertexArrays(3, va);

    for (i = 0; i < 3; i++)
    {
        if (va[i] == 0)
        {
            break;
        }
    }

    if ((i != 3) || (gl.getError() != GL_NO_ERROR))
    {
        success = false;
        tcu_fail_msg("ApiCoverageTestCase::GenVertexArrays", "Failed");
    }

    gl.deleteVertexArrays(3, va);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteVertexArrays");

    return success;
}

/* Coverage test for glIsVertexArray */
bool ApiCoverageTestCase::TestCoverageGLCallIsVertexArray(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLuint va;
    bool success = true;

    (void)gl.getError(); // clear error from previous test, if any

    gl.genVertexArrays(1, &va);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genVertexArrays");
    gl.bindVertexArray(va);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindVertexArray");

    if (glIsVertexArray(va) != GL_TRUE)
    {
        success = false;
        tcu_fail_msg("ApiCoverageTestCase::IsVertexArray", "Failed - TRUE");
    }
    if (glIsVertexArray(va + 1) != GL_FALSE)
    {
        success = false;
        tcu_fail_msg("ApiCoverageTestCase::IsVertexArray", "Failed - ");
    }

    gl.deleteVertexArrays(1, &va);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteVertexArrays");

    return success;
}

/* Coverage test for glDrawArraysInstanced */
bool ApiCoverageTestCase::TestCoverageGLCallDrawArraysInstanced(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    (void)gl.getError(); // clear error from previous test, if any

    gl.drawArraysInstanced(GL_POINTS - 1, 0, 3, 4);

    if (gl.getError() != GL_INVALID_ENUM)
    {
        tcu_fail_msg("ApiCoverageTestCase::DrawArraysInstanced", "Failed");
        success = false;
    }

    return success;
}

/* Coverage test for glDrawElementsInstanced */
bool ApiCoverageTestCase::TestCoverageGLCallDrawElementsInstanced(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    (void)gl.getError(); // clear error from previous test, if any

    gl.drawElementsInstanced(GL_POINTS - 1, 3, GL_UNSIGNED_INT, NULL, 4);

    if (gl.getError() != GL_INVALID_ENUM)
    {
        tcu_fail_msg("ApiCoverageTestCase::DrawElementsInstanced", "Failed");
        success = false;
    }

    return success;
}

/* Coverage test for glCopyBufferSubData */
bool ApiCoverageTestCase::TestCoverageGLCallCopyBufferSubData(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLuint cb[2];
    GLubyte rdata[64], wdata[64];

    (void)gl.getError(); // clear error from previous test, if any

    gl.copyBufferSubData(GL_PIXEL_UNPACK_BUFFER, GL_PIXEL_PACK_BUFFER, 0, 0, 64);

    if (gl.getError() != GL_INVALID_OPERATION)
    {
        tcu_fail_msg("ApiCoverageTestCase::CopyBufferSubData", "Failed (1)");
        success = false;
    }

    gl.copyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, 64);

    if (gl.getError() != GL_INVALID_OPERATION)
    {
        tcu_fail_msg("ApiCoverageTestCase::CopyBufferSubData", "Failed (2)");
        success = false;
    }

    gl.genBuffers(2, cb);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genBuffers");
    gl.bindBuffer(GL_COPY_READ_BUFFER, cb[0]);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");
    gl.bufferData(GL_COPY_READ_BUFFER, 64, rdata, GL_DYNAMIC_READ);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bufferData");
    gl.bindBuffer(GL_COPY_WRITE_BUFFER, cb[1]);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");
    gl.bufferData(GL_COPY_WRITE_BUFFER, 64, wdata, GL_STATIC_COPY);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bufferData");

    gl.copyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 8, 8, 32);

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::CopyBufferSubData", "Failed (3)");
        success = false;
    }

    gl.deleteBuffers(2, cb);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteBuffers");

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallGetUniformIndices(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    const char *uname        = "dummy";
    GLuint uindex            = 0;
    GLint program;

    (void)gl.getError(); // clear error from previous test, if any

    program = createDefaultProgram(0);

    if (program == -1)
    {
        tcu_fail_msg("ApiCoverageTestCase::GetUniformIndices", "Program create Failed");
        return false;
    }

    gl.getUniformIndices((GLuint)program, 1, &uname, &uindex);

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::GetUniformIndices", "Failed (2)");
        success = false;
    }
    if (uindex != GL_INVALID_INDEX)
    {
        tcu_fail_msg("ApiCoverageTestCase::GetUniformIndices", "Failed (3)");
        success = false;
    }

    gl.deleteProgram((GLuint)program);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteProgram");

    return success;
}

/* Coverage test for glGetActiveUniformsiv */
bool ApiCoverageTestCase::TestCoverageGLCallGetActiveUniformsiv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLuint uindex            = 0;
    GLint program;
    GLint data;

    (void)gl.getError(); // clear error from previous test, if any

    program = createDefaultProgram(0);

    if (program == -1)
    {
        tcu_fail_msg("ApiCoverageTestCase::GetActiveUniformsiv", "Program create Failed");
        return false;
    }

    gl.getActiveUniformsiv((GLuint)program, 1, &uindex, GL_UNIFORM_TYPE, &data);

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::GetActiveUniformsiv", "Failed (2)");
        success = false;
    }

    gl.deleteProgram((GLuint)program);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteProgram");

    return success;
}

/* Coverage test for glGetUniformBlockIndex */
bool ApiCoverageTestCase::TestCoverageGLCallGetUniformBlockIndex(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLuint uindex;
    GLint program;

    (void)gl.getError(); // clear error from previous test, if any

    program = createDefaultProgram(0);

    if (program == -1)
    {
        tcu_fail_msg("ApiCoverageTestCase::GetUniformBlockIndex", "Program create Failed");
        return false;
    }

    uindex = gl.getUniformBlockIndex((GLuint)program, "dummy");
    GLU_EXPECT_NO_ERROR(gl.getError(), "getUniformBlockIndex");

    if (uindex != GL_INVALID_INDEX)
    {
        tcu_fail_msg("ApiCoverageTestCase::GetUniformBlockIndex", "Failed");
        success = false;
    }

    gl.deleteProgram((GLuint)program);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteProgram");

    return success;
}

/* Coverage test for glGetActiveUniformBlockiv */
bool ApiCoverageTestCase::TestCoverageGLCallGetActiveUniformBlockiv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint program;
    GLint data;

    (void)gl.getError(); // clear error from previous test, if any

    program = createDefaultProgram(0);

    if (program == -1)
    {
        tcu_fail_msg("ApiCoverageTestCase::GetActiveUniformBlockiv", "Program create Failed");
        return false;
    }

    gl.getActiveUniformBlockiv((GLuint)program, 0, GL_UNIFORM_BLOCK_DATA_SIZE, &data);

    if (gl.getError() != GL_INVALID_VALUE)
    {
        tcu_fail_msg("ApiCoverageTestCase::GetActiveUniformBlockiv", "Failed");
        success = false;
    }

    gl.deleteProgram((GLuint)program);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteProgram");

    return success;
}

/* Coverage test for glGetActiveUniformBlockName */
bool ApiCoverageTestCase::TestCoverageGLCallGetActiveUniformBlockName(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint program;
    GLchar name[256];
    GLsizei length;

    (void)gl.getError(); // clear error from previous test, if any

    program = createDefaultProgram(0);

    if (program == -1)
    {
        tcu_fail_msg("ApiCoverageTestCase::GetActiveUniformBlockName", "Program create Failed");
        return false;
    }

    gl.getActiveUniformBlockName((GLuint)program, 0, 256, &length, name);

    if (gl.getError() != GL_INVALID_VALUE)
    {
        tcu_fail_msg("ApiCoverageTestCase::GetActiveUniformBlockName", "Failed");
        success = false;
    }

    gl.deleteProgram((GLuint)program);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteProgram");

    return success;
}

/* Coverage test for glUniformBlockBinding */
bool ApiCoverageTestCase::TestCoverageGLCallUniformBlockBinding(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint program;

    (void)gl.getError(); // clear error from previous test, if any

    program = createDefaultProgram(0);

    if (program == -1)
    {
        tcu_fail_msg("ApiCoverageTestCase::UniformBlockBinding", "Program create Failed");
        return false;
    }

    gl.uniformBlockBinding((GLuint)program, 0, 0);

    if (gl.getError() != GL_INVALID_VALUE)
    {
        tcu_fail_msg("ApiCoverageTestCase::UniformBlockBinding", "Failed");
        success = false;
    }

    gl.deleteProgram((GLuint)program);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteProgram");

    return success;
}

/* Coverage test for glGetBufferParameteri64v */
bool ApiCoverageTestCase::TestCoverageGLCallGetBufferParameteri64v(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLuint i;
    bool success = true;
    GLuint cb;
    GLuint data[64];
    GLint64 param;

    (void)gl.getError(); // clear error from previous test, if any

    gl.genBuffers(1, &cb);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genBuffers");
    gl.bindBuffer(GL_PIXEL_PACK_BUFFER, cb);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindBuffer");
    gl.bufferData(GL_PIXEL_PACK_BUFFER, 64, data, GL_STATIC_COPY);

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::GetBufferParameteri64v", "Buffer creation failure");
        return false;
    }

    for (i = 0; ea_GetBufferParameteri64v[i].value != -1; i++)
    {
        gl.getBufferParameteri64v(GL_PIXEL_PACK_BUFFER, ea_GetBufferParameteri64v[i].value, &param);

        if (gl.getError() != GL_NO_ERROR)
        {
            success = false;
        }
    }

    if (success == false)
    {
        tcu_fail_msg("ApiCoverageTestCase::GetBufferParameteri64v", "Failed");
    }
    if (param != 64)
    {
        tcu_fail_msg("ApiCoverageTestCase::GetBufferParameteri64v", "Incorrect return value");
        success = false;
    }

    gl.deleteBuffers(1, &cb);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteBuffers");

    return success;
}

/* Coverage test for glProgramParameteri */
bool ApiCoverageTestCase::TestCoverageGLCallProgramParameteri(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint program;

    (void)gl.getError(); // clear error from previous test, if any

    program = createDefaultProgram(0);

    if (program == -1)
    {
        tcu_fail_msg("ApiCoverageTestCase::ProgramParameteri", "Program create Failed");
        return false;
    }

    gl.programParameteri(program, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::ProgramParameteri", "Failed");
        success = false;
    }

    gl.deleteProgram((GLuint)program);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteProgram");

    return success;
}
/* Coverage test for glFenceSync */

/* Enums to test for glFenceSync (may not be required) */
bool ApiCoverageTestCase::TestCoverageGLCallFenceSync(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;

    (void)gl.getError(); // clear error from previous test, if any

    GLsync sobj1 = gl.fenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::FenceSync", "Failed (1)");
        success = false;
    }

    GLsync sobj2 = gl.fenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 1);

    if (gl.getError() != GL_INVALID_VALUE)
    {
        tcu_fail_msg("ApiCoverageTestCase::FenceSync", "Failed (2)");
        success = false;
    }

    gl.deleteSync(sobj1);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteSync");
    gl.deleteSync(sobj2);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteSync");

    return success;
}

/* Coverage test for glIsSync */
bool ApiCoverageTestCase::TestCoverageGLCallIsSync(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLsync sobj;

    (void)gl.getError(); // clear error from previous test, if any

    sobj = gl.fenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "fenceSync");

    if ((gl.isSync(sobj) == GL_FALSE) || (gl.getError() != GL_NO_ERROR))
    {
        tcu_fail_msg("ApiCoverageTestCase::IsSync", "Sync creation failed");
        success = false;
    }
    gl.deleteSync(sobj);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteSync");

    return success;
}

/* Coverage test for glDeleteSync */
bool ApiCoverageTestCase::TestCoverageGLCallDeleteSync(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLsync sobj;

    (void)gl.getError(); // clear error from previous test, if any

    sobj = gl.fenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "fenceSync");

    gl.deleteSync(sobj);
    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::DeleteSync", "Failed");
        success = false;
    }

    return success;
}

/* Coverage test for glClientWaitSync */
bool ApiCoverageTestCase::TestCoverageGLCallClientWaitSync(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLsync sobj;
    GLenum sresult;

    (void)gl.getError(); // clear error from previous test, if any

    // Make sure all GL commands issued before are flushed and finished.
    gl.finish();
    GLU_EXPECT_NO_ERROR(gl.getError(), "finish");

    sobj = gl.fenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::ClientWaitSync", "Sync create failed");
        success = false;
    }

    sresult = gl.clientWaitSync(sobj, 0, 1000000000ULL);
    if (gl.getError() != GL_NO_ERROR || sresult == GL_WAIT_FAILED)
    {
        tcu_fail_msg("ApiCoverageTestCase::ClientWaitSync", "Failed (1)");
        success = false;
    }

    sresult = gl.clientWaitSync(sobj, GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000ULL);
    if (gl.getError() != GL_NO_ERROR || sresult == GL_WAIT_FAILED)
    {
        tcu_fail_msg("ApiCoverageTestCase::ClientWaitSync", "Failed (2)");
        success = false;
    }

    gl.deleteSync(sobj);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteSync");

    return success;
}

/* Coverage test for glWaitSync */
bool ApiCoverageTestCase::TestCoverageGLCallWaitSync(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLsync sobj;

    (void)gl.getError(); // clear error from previous test, if any

    sobj = gl.fenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::WaitSync", "Sync create failed");
        success = false;
    }

    gl.waitSync(sobj, 0, GL_TIMEOUT_IGNORED);
    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::WaitSync", "Failed (1)");
        success = false;
    }

    gl.waitSync(sobj, 0, 1000000000ULL);
    if (gl.getError() != GL_INVALID_VALUE)
    {
        tcu_fail_msg("ApiCoverageTestCase::WaitSync", "Failed (2)");
        success = false;
    }

    gl.deleteSync(sobj);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteSync");

    return success;
}

/* Coverage test for glGetInteger64v */
bool ApiCoverageTestCase::TestCoverageGLCallGetInteger64v(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLint64 value;
    GLint i;
    bool success = true;

    (void)gl.getError(); // clear error from previous test, if any

    for (i = 0; ea_GetInteger64v[i].value != -1; i++)
    {
        gl.getInteger64v(ea_GetInteger64v[i].value, &value);

        if (gl.getError() != GL_NO_ERROR)
        {
            tcu_fail_msg("ApiCoverageTestCase::GetInteger64v", "Failed 0x%x", ea_GetInteger64v[i].value);
            success = false;
        }
    }

    return success;
}

/* Coverage test for glGetSynciv */
bool ApiCoverageTestCase::TestCoverageGLCallGetSynciv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    GLuint i;
    bool success = true;
    GLsync sobj;
    GLsizei length;
    GLint value;

    (void)gl.getError(); // clear error from previous test, if any

    sobj = gl.fenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

    for (i = 0; ea_GetSynciv[i].value != -1; i++)
    {
        gl.getSynciv(sobj, ea_GetSynciv[i].value, 1, &length, &value);

        if (gl.getError() != GL_NO_ERROR)
        {
            tcu_fail_msg("ApiCoverageTestCase::GetSynciv", "Failed 0x%x", ea_GetSynciv[i].value);
            success = false;
        }
    }

    gl.deleteSync(sobj);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteSync");

    return success;
}

/* Coverage test for glGenSamplers */
bool ApiCoverageTestCase::TestCoverageGLCallGenSamplers(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLuint sampler[2];

    (void)gl.getError(); // clear error from previous test, if any

    gl.genSamplers(2, sampler);

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::GenSamplers", "Failed.");
        success = false;
    }

    gl.deleteSamplers(2, sampler);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteSamplers");

    return success;
}

/* Coverage test for glDeleteSamplers */
bool ApiCoverageTestCase::TestCoverageGLCallDeleteSamplers(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLuint sampler[2];

    (void)gl.getError(); // clear error from previous test, if any

    gl.genSamplers(2, sampler);

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::DeleteSamplers", "Could not create a sampler.");
        success = false;
    }

    gl.deleteSamplers(2, sampler);

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::DeleteSamplers", "Failed.");
        success = false;
    }

    return success;
}

/* Coverage test for glIsSampler */
bool ApiCoverageTestCase::TestCoverageGLCallIsSampler(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLuint sampler;

    (void)gl.getError(); // clear error from previous test, if any

    gl.genSamplers(1, &sampler);

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::IsSampler", "Could not create a sampler.");
        success = false;
    }

    if ((gl.isSampler(sampler) != GL_TRUE) || (gl.getError() != GL_NO_ERROR))
    {
        tcu_fail_msg("ApiCoverageTestCase::IsSampler", "Failed.");
        success = false;
    }

    gl.bindSampler(0, sampler);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindSampler");

    if ((gl.isSampler(sampler) != GL_TRUE) || (gl.getError() != GL_NO_ERROR))
    {
        tcu_fail_msg("ApiCoverageTestCase::IsSampler", "Failed.");
        success = false;
    }

    gl.deleteSamplers(1, &sampler);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteSamplers");

    return success;
}

/* Coverage test for glBindSampler */
bool ApiCoverageTestCase::TestCoverageGLCallBindSampler(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLuint sampler;

    (void)gl.getError(); // clear error from previous test, if any

    gl.genSamplers(1, &sampler);

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::BindSampler", "Could not create a sampler.");
        success = false;
    }

    gl.bindSampler(0, sampler);

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::BindSampler", "Failed.");
        success = false;
    }

    gl.deleteSamplers(1, &sampler);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteSamplers");

    return success;
}

/* Coverage test for glSamplerParameteri */
bool ApiCoverageTestCase::TestCoverageGLCallSamplerParameteri(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLuint sampler;

    (void)gl.getError(); // clear error from previous test, if any

    gl.genSamplers(1, &sampler);

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::SamplerParameteri", "Could not create a sampler.");
        success = false;
    }

    gl.samplerParameteri(sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::SamplerParameteri", "Failed.");
        success = false;
    }

    gl.deleteSamplers(1, &sampler);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteSamplers");

    return success;
}

/* Coverage test for glSamplerParameteriv */
bool ApiCoverageTestCase::TestCoverageGLCallSamplerParameteriv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLuint sampler;
    GLint param = GL_REPEAT;

    (void)gl.getError(); // clear error from previous test, if any

    gl.genSamplers(1, &sampler);

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::SamplerParameteriv", "Could not create a sampler.");
        success = false;
    }

    gl.samplerParameteriv(sampler, GL_TEXTURE_WRAP_S, &param);

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::SamplerParameteriv", "Failed.");
        success = false;
    }

    gl.deleteSamplers(1, &sampler);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteSamplers");

    return success;
}

/* Coverage test for glSamplerParameterf */
bool ApiCoverageTestCase::TestCoverageGLCallSamplerParameterf(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLuint sampler;
    GLint parami = GL_MIRRORED_REPEAT;

    (void)gl.getError(); // clear error from previous test, if any

    gl.genSamplers(1, &sampler);

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::SamplerParameterf", "Could not create a sampler.");
        success = false;
    }

    gl.samplerParameterf(sampler, GL_TEXTURE_WRAP_R, (GLfloat)parami);

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::SamplerParameterf", "Failed.");
        success = false;
    }

    gl.deleteSamplers(1, &sampler);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteSamplers");

    return success;
}

/* Coverage test for glSamplerParameterfv */
bool ApiCoverageTestCase::TestCoverageGLCallSamplerParameterfv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLuint sampler;
    GLint parami    = GL_NEAREST;
    GLfloat param   = (GLfloat)parami;
    GLfloat *params = &param;

    (void)gl.getError(); // clear error from previous test, if any

    gl.genSamplers(1, &sampler);

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::SamplerParameterfv", "Could not create a sampler.");
        success = false;
    }

    gl.samplerParameterfv(sampler, GL_TEXTURE_MIN_FILTER, params);

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::SamplerParameterfv", "Failed.");
        success = false;
    }

    gl.deleteSamplers(1, &sampler);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteSamplers");

    return success;
}
/* Coverage test for glGetSamplerParameteriv */
bool ApiCoverageTestCase::TestCoverageGLCallGetSamplerParameteriv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLuint sampler;
    GLint param;

    (void)gl.getError(); // clear error from previous test, if any

    gl.genSamplers(1, &sampler);

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::GetSamplerParameteriv", "Could not create a sampler.");
        success = false;
    }

    gl.getSamplerParameteriv(sampler, GL_TEXTURE_MAG_FILTER, &param);

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::GetSamplerParameteriv", "Failed.");
        success = false;
    }

    gl.deleteSamplers(1, &sampler);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteSamplers");

    return success;
}

/* Coverage test for glGetSamplerParameterfv */
bool ApiCoverageTestCase::TestCoverageGLCallGetSamplerParameterfv(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLuint sampler;
    GLfloat param;

    (void)gl.getError(); // clear error from previous test, if any

    gl.genSamplers(1, &sampler);

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::GetSamplerParameterfv", "Could not create a sampler.");
        success = false;
    }

    gl.getSamplerParameterfv(sampler, GL_TEXTURE_COMPARE_FUNC, &param);

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::GetSamplerParameterfv", "Failed.");
        success = false;
    }

    gl.deleteSamplers(1, &sampler);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteSamplers");

    return success;
}

/* Coverage test for glInvalidateFramebuffer */
bool ApiCoverageTestCase::TestCoverageGLCallInvalidateFramebuffer(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint max                = 0;
    GLenum attachment;
    int i;
    GLuint f;

    for (i = 0; ea_InvalidateFramebuffer[i].value != -1; i++)
    {
        gl.invalidateFramebuffer(ea_InvalidateFramebuffer[i].value, 0, NULL);

        if (gl.getError() != GL_NO_ERROR)
        {
            tcu_fail_msg("ApiCoverageTestCase::InvalidateFramebuffer", "Failed.");
            success = false;
            break;
        }
    }

    gl.getIntegerv(GL_MAX_COLOR_ATTACHMENTS, &max);
    GLU_EXPECT_NO_ERROR(gl.getError(), "getIntegerv");
    (void)gl.getError();
    attachment = GL_COLOR_ATTACHMENT0 + max;

    gl.genFramebuffers(1, &f);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genFramebuffers");
    gl.bindFramebuffer(GL_FRAMEBUFFER, f);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindFramebuffer");

    gl.invalidateFramebuffer(GL_FRAMEBUFFER, 1, &attachment);
    if (gl.getError() != GL_INVALID_OPERATION)
    {
        tcu_fail_msg("ApiCoverageTestCase::InvalidateFramebuffer", "Failed.");
        success = false;
    }
    gl.deleteFramebuffers(1, &f);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteFramebuffers");

    return success;
}

/* Coverage test for glInvalidateSubFramebuffer */
bool ApiCoverageTestCase::TestCoverageGLCallInvalidateSubFramebuffer(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    int i;

    for (i = 0; ea_InvalidateFramebuffer[i].value != -1; i++)
    {
        gl.invalidateSubFramebuffer(ea_InvalidateFramebuffer[i].value, 0, NULL, 0, 0, 1, 1);

        if (gl.getError() != GL_NO_ERROR)
        {
            tcu_fail_msg("ApiCoverageTestCase::InvalidateSubFramebuffer", "Failed.");
            success = false;
            break;
        }
    }

    return success;
}

bool ApiCoverageTestCase::TestCoverageGLCallBindTransformFeedback(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint program            = 0;
    GLuint tobj              = 0;

    (void)gl.getError(); // clear error from previous test, if any

    program = createDefaultProgram(1);

    if (program == -1)
    {
        tcu_fail_msg("ApiCoverageTestCase::BeginTransformFeedback", "Program create Failed");
        return false;
    }

    gl.genTransformFeedbacks(1, &tobj);

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::BindTransformFeedback", "Failed to create transform feedback object.");
        success = false;
    }

    gl.bindTransformFeedback(GL_TRANSFORM_FEEDBACK, tobj);

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::BindTransformFeedback", "Failed.");
        success = false;
    }

    gl.deleteTransformFeedbacks(1, &tobj);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteTransformFeedbacks");
    gl.deleteProgram((GLuint)program);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteProgram");

    return success;
}

/* Coverage test for glDeleteTransformFeedbacks */
bool ApiCoverageTestCase::TestCoverageGLCallDeleteTransformFeedbacks(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint program            = 0;
    GLuint tobj[2]           = {0, 0};

    (void)gl.getError(); // clear error from previous test, if any

    program = createDefaultProgram(1);

    if (program == -1)
    {
        tcu_fail_msg("ApiCoverageTestCase::DeleteTransformFeedbacks", "Program create Failed");
        return false;
    }

    gl.genTransformFeedbacks(2, tobj);

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::DeleteTransformFeedbacks", "Failed to create transform feedback object.");
        success = false;
    }

    gl.deleteTransformFeedbacks(2, tobj);

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::DeleteTransformFeedbacks", "Failed.");
        success = false;
    }

    gl.deleteProgram((GLuint)program);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteProgram");

    return success;
}

/* Coverage test for glGenTransformFeedbacks */
bool ApiCoverageTestCase::TestCoverageGLCallGenTransformFeedbacks(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint program            = 0;
    GLuint tobj[2]           = {0, 0};

    (void)gl.getError(); // clear error from previous test, if any

    program = createDefaultProgram(1);

    if (program == -1)
    {
        tcu_fail_msg("ApiCoverageTestCase::GenTransformFeedbacks", "Program create Failed");
        return false;
    }

    gl.genTransformFeedbacks(2, tobj);

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::GenTransformFeedbacks", "Failed.");
        success = false;
    }

    gl.deleteTransformFeedbacks(2, tobj);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteTransformFeedbacks");
    gl.deleteProgram((GLuint)program);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteProgram");

    return success;
}

/* Coverage test for glIsTransformFeedback */
bool ApiCoverageTestCase::TestCoverageGLCallIsTransformFeedback(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint program            = 0;
    GLuint tobj              = 0;

    (void)gl.getError(); // clear error from previous test, if any

    program = createDefaultProgram(1);

    if (program == -1)
    {
        tcu_fail_msg("ApiCoverageTestCase::IsTransformFeedback", "Program create Failed");
        return false;
    }

    gl.genTransformFeedbacks(1, &tobj);

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::IsTransformFeedback", "Failed to create transform feedback object.");
        success = false;
    }

    if ((glIsTransformFeedback(tobj) != GL_FALSE) || (gl.getError() != GL_NO_ERROR))
    {
        tcu_fail_msg("ApiCoverageTestCase::IsTransformFeedback", "Failed.");
        success = false;
    }

    gl.bindTransformFeedback(GL_TRANSFORM_FEEDBACK, tobj);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindTransformFeedback");
    if ((gl.isTransformFeedback(tobj) != GL_TRUE) || (gl.getError() != GL_NO_ERROR))
    {
        tcu_fail_msg("ApiCoverageTestCase::IsTransformFeedback", "Failed.");
        success = false;
    }

    gl.deleteTransformFeedbacks(1, &tobj);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteTransformFeedbacks");
    gl.deleteProgram((GLuint)program);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteProgram");

    return success;
}

/* Coverage test for glPauseTransformFeedback */
bool ApiCoverageTestCase::TestCoverageGLCallPauseTransformFeedback(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint program            = 0;
    GLuint tobj = 0, tbufobj = 0;

    (void)gl.getError(); // clear error from previous test, if any

    program = createDefaultProgram(1);

    if (program == -1)
    {
        tcu_fail_msg("ApiCoverageTestCase::PauseTransformFeedback", "Program create Failed");
        return false;
    }

    gl.genTransformFeedbacks(1, &tobj);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genTransformFeedbacks");
    gl.bindTransformFeedback(GL_TRANSFORM_FEEDBACK, tobj);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindTransformFeedback");
    gl.genBuffers(1, &tbufobj);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genBuffers");
    gl.bindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, tbufobj);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindBufferBase");
    gl.beginTransformFeedback(GL_TRIANGLES);

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::PauseTransformFeedback", "Failed to start transform feedback.");
        success = false;
    }

    gl.pauseTransformFeedback();

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::PauseTransformFeedback", "Failed.");
        success = false;
    }

    gl.endTransformFeedback();
    GLU_EXPECT_NO_ERROR(gl.getError(), "endTransformFeedback");

    gl.deleteBuffers(1, &tbufobj);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteBuffers");
    gl.deleteTransformFeedbacks(1, &tobj);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteTransformFeedbacks");
    gl.deleteProgram((GLuint)program);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteProgram");

    return success;
}

/* Coverage test for glResumeTransformFeedback */
bool ApiCoverageTestCase::TestCoverageGLCallResumeTransformFeedback(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    bool success             = true;
    GLint program            = 0;
    GLuint tobj = 0, tbufobj = 0;

    (void)gl.getError(); // clear error from previous test, if any

    program = createDefaultProgram(1);

    if (program == -1)
    {
        tcu_fail_msg("ApiCoverageTestCase::ResumeTransformFeedback", "Program create Failed");
        return false;
    }

    gl.genTransformFeedbacks(1, &tobj);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genTransformFeedbacks");
    gl.bindTransformFeedback(GL_TRANSFORM_FEEDBACK, tobj);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindTransformFeedback");
    gl.genBuffers(1, &tbufobj);
    GLU_EXPECT_NO_ERROR(gl.getError(), "genBuffers");
    gl.bindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, tbufobj);
    GLU_EXPECT_NO_ERROR(gl.getError(), "bindBufferBase");
    gl.beginTransformFeedback(GL_TRIANGLES);
    GLU_EXPECT_NO_ERROR(gl.getError(), "beginTransformFeedback");
    gl.pauseTransformFeedback();

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::ResumeTransformFeedback", "Failed to start transform feedback.");
        success = false;
    }

    gl.resumeTransformFeedback();

    if (gl.getError() != GL_NO_ERROR)
    {
        tcu_fail_msg("ApiCoverageTestCase::ResumeTransformFeedback", "Failed.");
        success = false;
    }

    gl.endTransformFeedback();
    GLU_EXPECT_NO_ERROR(gl.getError(), "endTransformFeedback");

    gl.deleteBuffers(1, &tbufobj);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteBuffers");
    gl.deleteTransformFeedbacks(1, &tobj);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteTransformFeedbacks");
    gl.deleteProgram((GLuint)program);
    GLU_EXPECT_NO_ERROR(gl.getError(), "deleteProgram");

    return success;
}

/**************************************************************************/

/* Coverage tests for OpenGL entry points not shared with OpenGL ES.
** These are automatically generated stubs intended to fail. They
** must be filled in with actual test code for each function.
*/

/* OpenGL 1.0-2.1 entry points */

/* Coverage test for glPointSize */

/* Enums to test for glPointSize (may not be required) */
struct enumTestRec const ea_PointSize[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallPointSize(void)
{
    GLuint i;
    bool success = true;

    /* glPointSize may need to loop over a set of enums doing something with them */
    for (i = 0; ea_PointSize[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::PointSize", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glPolygonMode */

/* Enums to test for glPolygonMode (may not be required) */
struct enumTestRec const ea_PolygonMode[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallPolygonMode(void)
{
    GLuint i;
    bool success = true;

    /* glPolygonMode may need to loop over a set of enums doing something with them */
    for (i = 0; ea_PolygonMode[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::PolygonMode", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glTexImage1D */

/* Enums to test for glTexImage1D (may not be required) */
struct enumTestRec const ea_TexImage1D[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallTexImage1D(void)
{
    GLuint i;
    bool success = true;

    /* glTexImage1D may need to loop over a set of enums doing something with them */
    for (i = 0; ea_TexImage1D[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::TexImage1D", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glDrawBuffer */

/* Enums to test for glDrawBuffer (may not be required) */
struct enumTestRec const ea_DrawBuffer[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallDrawBuffer(void)
{
    GLuint i;
    bool success = true;

    /* glDrawBuffer may need to loop over a set of enums doing something with them */
    for (i = 0; ea_DrawBuffer[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::DrawBuffer", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glClearDepth */

/* Enums to test for glClearDepth (may not be required) */
struct enumTestRec const ea_ClearDepth[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallClearDepth(void)
{
    GLuint i;
    bool success = true;

    /* glClearDepth may need to loop over a set of enums doing something with them */
    for (i = 0; ea_ClearDepth[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::ClearDepth", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glLogicOp */

/* Enums to test for glLogicOp (may not be required) */
struct enumTestRec const ea_LogicOp[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallLogicOp(void)
{
    GLuint i;
    bool success = true;

    /* glLogicOp may need to loop over a set of enums doing something with them */
    for (i = 0; ea_LogicOp[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::LogicOp", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glPixelStoref */

/* Enums to test for glPixelStoref (may not be required) */
struct enumTestRec const ea_PixelStoref[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallPixelStoref(void)
{
    GLuint i;
    bool success = true;

    /* glPixelStoref may need to loop over a set of enums doing something with them */
    for (i = 0; ea_PixelStoref[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::PixelStoref", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glGetDoublev */

/* Enums to test for glGetDoublev (may not be required) */
struct enumTestRec const ea_GetDoublev[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallGetDoublev(void)
{
    GLuint i;
    bool success = true;

    /* glGetDoublev may need to loop over a set of enums doing something with them */
    for (i = 0; ea_GetDoublev[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::GetDoublev", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glGetTexImage */

/* Enums to test for glGetTexImage (may not be required) */
struct enumTestRec const ea_GetTexImage[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallGetTexImage(void)
{
    GLuint i;
    bool success = true;

    /* glGetTexImage may need to loop over a set of enums doing something with them */
    for (i = 0; ea_GetTexImage[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::GetTexImage", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glGetTexLevelParameterfv */

/* Enums to test for glGetTexLevelParameterfv (may not be required) */
struct enumTestRec const ea_GetTexLevelParameterfv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallGetTexLevelParameterfv(void)
{
    GLuint i;
    bool success = true;

    /* glGetTexLevelParameterfv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_GetTexLevelParameterfv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::GetTexLevelParameterfv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glGetTexLevelParameteriv */

/* Enums to test for glGetTexLevelParameteriv (may not be required) */
struct enumTestRec const ea_GetTexLevelParameteriv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallGetTexLevelParameteriv(void)
{
    GLuint i;
    bool success = true;

    /* glGetTexLevelParameteriv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_GetTexLevelParameteriv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::GetTexLevelParameteriv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glDepthRange */

/* Enums to test for glDepthRange (may not be required) */
struct enumTestRec const ea_DepthRange[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallDepthRange(void)
{
    GLuint i;
    bool success = true;

    /* glDepthRange may need to loop over a set of enums doing something with them */
    for (i = 0; ea_DepthRange[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::DepthRange", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glGetPointerv */

/* Enums to test for glGetPointerv (may not be required) */
struct enumTestRec const ea_GetPointerv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallGetPointerv(void)
{
    GLuint i;
    bool success = true;

    /* glGetPointerv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_GetPointerv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::GetPointerv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glCopyTexImage1D */

/* Enums to test for glCopyTexImage1D (may not be required) */
struct enumTestRec const ea_CopyTexImage1D[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallCopyTexImage1D(void)
{
    GLuint i;
    bool success = true;

    /* glCopyTexImage1D may need to loop over a set of enums doing something with them */
    for (i = 0; ea_CopyTexImage1D[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::CopyTexImage1D", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glCopyTexSubImage1D */

/* Enums to test for glCopyTexSubImage1D (may not be required) */
struct enumTestRec const ea_CopyTexSubImage1D[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallCopyTexSubImage1D(void)
{
    GLuint i;
    bool success = true;

    /* glCopyTexSubImage1D may need to loop over a set of enums doing something with them */
    for (i = 0; ea_CopyTexSubImage1D[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::CopyTexSubImage1D", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glTexSubImage1D */

/* Enums to test for glTexSubImage1D (may not be required) */
struct enumTestRec const ea_TexSubImage1D[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallTexSubImage1D(void)
{
    GLuint i;
    bool success = true;

    /* glTexSubImage1D may need to loop over a set of enums doing something with them */
    for (i = 0; ea_TexSubImage1D[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::TexSubImage1D", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glCompressedTexImage1D */

/* Enums to test for glCompressedTexImage1D (may not be required) */
struct enumTestRec const ea_CompressedTexImage1D[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallCompressedTexImage1D(void)
{
    GLuint i;
    bool success = true;

    /* glCompressedTexImage1D may need to loop over a set of enums doing something with them */
    for (i = 0; ea_CompressedTexImage1D[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::CompressedTexImage1D", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glCompressedTexSubImage1D */

/* Enums to test for glCompressedTexSubImage1D (may not be required) */
struct enumTestRec const ea_CompressedTexSubImage1D[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallCompressedTexSubImage1D(void)
{
    GLuint i;
    bool success = true;

    /* glCompressedTexSubImage1D may need to loop over a set of enums doing something with them */
    for (i = 0; ea_CompressedTexSubImage1D[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::CompressedTexSubImage1D", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glGetCompressedTexImage */

/* Enums to test for glGetCompressedTexImage (may not be required) */
struct enumTestRec const ea_GetCompressedTexImage[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallGetCompressedTexImage(void)
{
    GLuint i;
    bool success = true;

    /* glGetCompressedTexImage may need to loop over a set of enums doing something with them */
    for (i = 0; ea_GetCompressedTexImage[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::GetCompressedTexImage", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glMultiDrawArrays */

/* Enums to test for glMultiDrawArrays (may not be required) */
struct enumTestRec const ea_MultiDrawArrays[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallMultiDrawArrays(void)
{
    GLuint i;
    bool success = true;

    /* glMultiDrawArrays may need to loop over a set of enums doing something with them */
    for (i = 0; ea_MultiDrawArrays[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::MultiDrawArrays", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glMultiDrawElements */

/* Enums to test for glMultiDrawElements (may not be required) */
struct enumTestRec const ea_MultiDrawElements[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallMultiDrawElements(void)
{
    GLuint i;
    bool success = true;

    /* glMultiDrawElements may need to loop over a set of enums doing something with them */
    for (i = 0; ea_MultiDrawElements[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::MultiDrawElements", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glPointParameterf */

/* Enums to test for glPointParameterf (may not be required) */
struct enumTestRec const ea_PointParameterf[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallPointParameterf(void)
{
    GLuint i;
    bool success = true;

    /* glPointParameterf may need to loop over a set of enums doing something with them */
    for (i = 0; ea_PointParameterf[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::PointParameterf", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glPointParameterfv */

/* Enums to test for glPointParameterfv (may not be required) */
struct enumTestRec const ea_PointParameterfv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallPointParameterfv(void)
{
    GLuint i;
    bool success = true;

    /* glPointParameterfv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_PointParameterfv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::PointParameterfv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glPointParameteri */

/* Enums to test for glPointParameteri (may not be required) */
struct enumTestRec const ea_PointParameteri[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallPointParameteri(void)
{
    GLuint i;
    bool success = true;

    /* glPointParameteri may need to loop over a set of enums doing something with them */
    for (i = 0; ea_PointParameteri[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::PointParameteri", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glPointParameteriv */

/* Enums to test for glPointParameteriv (may not be required) */
struct enumTestRec const ea_PointParameteriv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallPointParameteriv(void)
{
    GLuint i;
    bool success = true;

    /* glPointParameteriv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_PointParameteriv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::PointParameteriv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glGetQueryObjectiv */

/* Enums to test for glGetQueryObjectiv (may not be required) */
struct enumTestRec const ea_GetQueryObjectiv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallGetQueryObjectiv(void)
{
    GLuint i;
    bool success = true;

    /* glGetQueryObjectiv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_GetQueryObjectiv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::GetQueryObjectiv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glGetBufferSubData */

/* Enums to test for glGetBufferSubData (may not be required) */
struct enumTestRec const ea_GetBufferSubData[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallGetBufferSubData(void)
{
    GLuint i;
    bool success = true;

    /* glGetBufferSubData may need to loop over a set of enums doing something with them */
    for (i = 0; ea_GetBufferSubData[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::GetBufferSubData", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glMapBuffer */

/* Enums to test for glMapBuffer (may not be required) */
struct enumTestRec const ea_MapBuffer[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallMapBuffer(void)
{
    GLuint i;
    bool success = true;

    /* glMapBuffer may need to loop over a set of enums doing something with them */
    for (i = 0; ea_MapBuffer[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::MapBuffer", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glGetVertexAttribdv */

/* Enums to test for glGetVertexAttribdv (may not be required) */
struct enumTestRec const ea_GetVertexAttribdv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallGetVertexAttribdv(void)
{
    GLuint i;
    bool success = true;

    /* glGetVertexAttribdv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_GetVertexAttribdv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::GetVertexAttribdv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttrib1d */

/* Enums to test for glVertexAttrib1d (may not be required) */
struct enumTestRec const ea_VertexAttrib1d[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttrib1d(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttrib1d may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttrib1d[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttrib1d", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttrib1dv */

/* Enums to test for glVertexAttrib1dv (may not be required) */
struct enumTestRec const ea_VertexAttrib1dv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttrib1dv(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttrib1dv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttrib1dv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttrib1dv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttrib1s */

/* Enums to test for glVertexAttrib1s (may not be required) */
struct enumTestRec const ea_VertexAttrib1s[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttrib1s(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttrib1s may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttrib1s[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttrib1s", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttrib1sv */

/* Enums to test for glVertexAttrib1sv (may not be required) */
struct enumTestRec const ea_VertexAttrib1sv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttrib1sv(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttrib1sv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttrib1sv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttrib1sv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttrib2d */

/* Enums to test for glVertexAttrib2d (may not be required) */
struct enumTestRec const ea_VertexAttrib2d[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttrib2d(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttrib2d may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttrib2d[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttrib2d", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttrib2dv */

/* Enums to test for glVertexAttrib2dv (may not be required) */
struct enumTestRec const ea_VertexAttrib2dv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttrib2dv(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttrib2dv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttrib2dv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttrib2dv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttrib2s */

/* Enums to test for glVertexAttrib2s (may not be required) */
struct enumTestRec const ea_VertexAttrib2s[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttrib2s(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttrib2s may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttrib2s[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttrib2s", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttrib2sv */

/* Enums to test for glVertexAttrib2sv (may not be required) */
struct enumTestRec const ea_VertexAttrib2sv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttrib2sv(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttrib2sv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttrib2sv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttrib2sv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttrib3d */

/* Enums to test for glVertexAttrib3d (may not be required) */
struct enumTestRec const ea_VertexAttrib3d[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttrib3d(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttrib3d may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttrib3d[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttrib3d", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttrib3dv */

/* Enums to test for glVertexAttrib3dv (may not be required) */
struct enumTestRec const ea_VertexAttrib3dv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttrib3dv(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttrib3dv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttrib3dv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttrib3dv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttrib3s */

/* Enums to test for glVertexAttrib3s (may not be required) */
struct enumTestRec const ea_VertexAttrib3s[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttrib3s(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttrib3s may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttrib3s[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttrib3s", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttrib3sv */

/* Enums to test for glVertexAttrib3sv (may not be required) */
struct enumTestRec const ea_VertexAttrib3sv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttrib3sv(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttrib3sv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttrib3sv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttrib3sv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttrib4Nbv */

/* Enums to test for glVertexAttrib4Nbv (may not be required) */
struct enumTestRec const ea_VertexAttrib4Nbv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttrib4Nbv(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttrib4Nbv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttrib4Nbv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttrib4Nbv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttrib4Niv */

/* Enums to test for glVertexAttrib4Niv (may not be required) */
struct enumTestRec const ea_VertexAttrib4Niv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttrib4Niv(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttrib4Niv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttrib4Niv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttrib4Niv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttrib4Nsv */

/* Enums to test for glVertexAttrib4Nsv (may not be required) */
struct enumTestRec const ea_VertexAttrib4Nsv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttrib4Nsv(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttrib4Nsv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttrib4Nsv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttrib4Nsv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttrib4Nub */

/* Enums to test for glVertexAttrib4Nub (may not be required) */
struct enumTestRec const ea_VertexAttrib4Nub[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttrib4Nub(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttrib4Nub may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttrib4Nub[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttrib4Nub", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttrib4Nubv */

/* Enums to test for glVertexAttrib4Nubv (may not be required) */
struct enumTestRec const ea_VertexAttrib4Nubv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttrib4Nubv(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttrib4Nubv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttrib4Nubv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttrib4Nubv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttrib4Nuiv */

/* Enums to test for glVertexAttrib4Nuiv (may not be required) */
struct enumTestRec const ea_VertexAttrib4Nuiv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttrib4Nuiv(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttrib4Nuiv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttrib4Nuiv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttrib4Nuiv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttrib4Nusv */

/* Enums to test for glVertexAttrib4Nusv (may not be required) */
struct enumTestRec const ea_VertexAttrib4Nusv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttrib4Nusv(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttrib4Nusv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttrib4Nusv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttrib4Nusv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttrib4bv */

/* Enums to test for glVertexAttrib4bv (may not be required) */
struct enumTestRec const ea_VertexAttrib4bv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttrib4bv(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttrib4bv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttrib4bv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttrib4bv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttrib4d */

/* Enums to test for glVertexAttrib4d (may not be required) */
struct enumTestRec const ea_VertexAttrib4d[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttrib4d(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttrib4d may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttrib4d[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttrib4d", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttrib4dv */

/* Enums to test for glVertexAttrib4dv (may not be required) */
struct enumTestRec const ea_VertexAttrib4dv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttrib4dv(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttrib4dv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttrib4dv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttrib4dv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttrib4iv */

/* Enums to test for glVertexAttrib4iv (may not be required) */
struct enumTestRec const ea_VertexAttrib4iv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttrib4iv(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttrib4iv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttrib4iv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttrib4iv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttrib4s */

/* Enums to test for glVertexAttrib4s (may not be required) */
struct enumTestRec const ea_VertexAttrib4s[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttrib4s(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttrib4s may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttrib4s[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttrib4s", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttrib4sv */

/* Enums to test for glVertexAttrib4sv (may not be required) */
struct enumTestRec const ea_VertexAttrib4sv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttrib4sv(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttrib4sv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttrib4sv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttrib4sv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttrib4ubv */

/* Enums to test for glVertexAttrib4ubv (may not be required) */
struct enumTestRec const ea_VertexAttrib4ubv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttrib4ubv(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttrib4ubv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttrib4ubv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttrib4ubv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttrib4uiv */

/* Enums to test for glVertexAttrib4uiv (may not be required) */
struct enumTestRec const ea_VertexAttrib4uiv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttrib4uiv(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttrib4uiv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttrib4uiv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttrib4uiv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttrib4usv */

/* Enums to test for glVertexAttrib4usv (may not be required) */
struct enumTestRec const ea_VertexAttrib4usv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttrib4usv(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttrib4usv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttrib4usv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttrib4usv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glUniformMatrix2x3fv */

/* Enums to test for glUniformMatrix2x3fv (may not be required) */
struct enumTestRec const ea_UniformMatrix2x3fv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallUniformMatrix2x3fv(void)
{
    GLuint i;
    bool success = true;

    /* glUniformMatrix2x3fv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_UniformMatrix2x3fv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::UniformMatrix2x3fv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glUniformMatrix3x2fv */

/* Enums to test for glUniformMatrix3x2fv (may not be required) */
struct enumTestRec const ea_UniformMatrix3x2fv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallUniformMatrix3x2fv(void)
{
    GLuint i;
    bool success = true;

    /* glUniformMatrix3x2fv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_UniformMatrix3x2fv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::UniformMatrix3x2fv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glUniformMatrix4x2fv */

/* Enums to test for glUniformMatrix4x2fv (may not be required) */
struct enumTestRec const ea_UniformMatrix4x2fv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallUniformMatrix4x2fv(void)
{
    GLuint i;
    bool success = true;

    /* glUniformMatrix4x2fv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_UniformMatrix4x2fv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::UniformMatrix4x2fv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glUniformMatrix3x4fv */

/* Enums to test for glUniformMatrix3x4fv (may not be required) */
struct enumTestRec const ea_UniformMatrix3x4fv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallUniformMatrix3x4fv(void)
{
    GLuint i;
    bool success = true;

    /* glUniformMatrix3x4fv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_UniformMatrix3x4fv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::UniformMatrix3x4fv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glUniformMatrix4x3fv */

/* Enums to test for glUniformMatrix4x3fv (may not be required) */
struct enumTestRec const ea_UniformMatrix4x3fv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallUniformMatrix4x3fv(void)
{
    GLuint i;
    bool success = true;

    /* glUniformMatrix4x3fv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_UniformMatrix4x3fv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::UniformMatrix4x3fv", "Coverage test not implemented yet");

    return success;
}

/* OpenGL 3.0 entry points */

/* Coverage test for glColorMaski */

/* Enums to test for glColorMaski (may not be required) */
struct enumTestRec const ea_ColorMaski[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallColorMaski(void)
{
    GLuint i;
    bool success = true;

    /* glColorMaski may need to loop over a set of enums doing something with them */
    for (i = 0; ea_ColorMaski[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::ColorMaski", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glGetBooleani_v */

/* Enums to test for glGetBooleani_v (may not be required) */
struct enumTestRec const ea_GetBooleani_v[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallGetBooleani_v(void)
{
    GLuint i;
    bool success = true;

    /* glGetBooleani_v may need to loop over a set of enums doing something with them */
    for (i = 0; ea_GetBooleani_v[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::GetBooleani_v", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glGetIntegeri_v */

/* Enums to test for glGetIntegeri_v (may not be required) */
struct enumTestRec const ea_GetIntegeri_v[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallGetIntegeri_v(void)
{
    GLuint i;
    bool success = true;

    /* glGetIntegeri_v may need to loop over a set of enums doing something with them */
    for (i = 0; ea_GetIntegeri_v[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::GetIntegeri_v", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glEnablei */

/* Enums to test for glEnablei (may not be required) */
struct enumTestRec const ea_Enablei[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallEnablei(void)
{
    GLuint i;
    bool success = true;

    /* glEnablei may need to loop over a set of enums doing something with them */
    for (i = 0; ea_Enablei[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::Enablei", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glDisablei */

/* Enums to test for glDisablei (may not be required) */
struct enumTestRec const ea_Disablei[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallDisablei(void)
{
    GLuint i;
    bool success = true;

    /* glDisablei may need to loop over a set of enums doing something with them */
    for (i = 0; ea_Disablei[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::Disablei", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glIsEnabledi */

/* Enums to test for glIsEnabledi (may not be required) */
struct enumTestRec const ea_IsEnabledi[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallIsEnabledi(void)
{
    GLuint i;
    bool success = true;

    /* glIsEnabledi may need to loop over a set of enums doing something with them */
    for (i = 0; ea_IsEnabledi[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::IsEnabledi", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glClampColor */

/* Enums to test for glClampColor (may not be required) */
struct enumTestRec const ea_ClampColor[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallClampColor(void)
{
    GLuint i;
    bool success = true;

    /* glClampColor may need to loop over a set of enums doing something with them */
    for (i = 0; ea_ClampColor[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::ClampColor", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glBeginConditionalRender */

/* Enums to test for glBeginConditionalRender (may not be required) */
struct enumTestRec const ea_BeginConditionalRender[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallBeginConditionalRender(void)
{
    GLuint i;
    bool success = true;

    /* glBeginConditionalRender may need to loop over a set of enums doing something with them */
    for (i = 0; ea_BeginConditionalRender[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::BeginConditionalRender", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glEndConditionalRender */

/* Enums to test for glEndConditionalRender (may not be required) */
struct enumTestRec const ea_EndConditionalRender[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallEndConditionalRender(void)
{
    GLuint i;
    bool success = true;

    /* glEndConditionalRender may need to loop over a set of enums doing something with them */
    for (i = 0; ea_EndConditionalRender[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::EndConditionalRender", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttribI1i */

/* Enums to test for glVertexAttribI1i (may not be required) */
struct enumTestRec const ea_VertexAttribI1i[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttribI1i(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttribI1i may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttribI1i[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttribI1i", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttribI2i */

/* Enums to test for glVertexAttribI2i (may not be required) */
struct enumTestRec const ea_VertexAttribI2i[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttribI2i(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttribI2i may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttribI2i[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttribI2i", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttribI3i */

/* Enums to test for glVertexAttribI3i (may not be required) */
struct enumTestRec const ea_VertexAttribI3i[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttribI3i(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttribI3i may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttribI3i[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttribI3i", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttribI1ui */

/* Enums to test for glVertexAttribI1ui (may not be required) */
struct enumTestRec const ea_VertexAttribI1ui[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttribI1ui(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttribI1ui may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttribI1ui[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttribI1ui", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttribI2ui */

/* Enums to test for glVertexAttribI2ui (may not be required) */
struct enumTestRec const ea_VertexAttribI2ui[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttribI2ui(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttribI2ui may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttribI2ui[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttribI2ui", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttribI3ui */

/* Enums to test for glVertexAttribI3ui (may not be required) */
struct enumTestRec const ea_VertexAttribI3ui[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttribI3ui(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttribI3ui may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttribI3ui[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttribI3ui", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttribI1iv */

/* Enums to test for glVertexAttribI1iv (may not be required) */
struct enumTestRec const ea_VertexAttribI1iv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttribI1iv(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttribI1iv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttribI1iv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttribI1iv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttribI2iv */

/* Enums to test for glVertexAttribI2iv (may not be required) */
struct enumTestRec const ea_VertexAttribI2iv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttribI2iv(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttribI2iv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttribI2iv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttribI2iv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttribI3iv */

/* Enums to test for glVertexAttribI3iv (may not be required) */
struct enumTestRec const ea_VertexAttribI3iv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttribI3iv(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttribI3iv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttribI3iv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttribI3iv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttribI1uiv */

/* Enums to test for glVertexAttribI1uiv (may not be required) */
struct enumTestRec const ea_VertexAttribI1uiv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttribI1uiv(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttribI1uiv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttribI1uiv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttribI1uiv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttribI2uiv */

/* Enums to test for glVertexAttribI2uiv (may not be required) */
struct enumTestRec const ea_VertexAttribI2uiv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttribI2uiv(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttribI2uiv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttribI2uiv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttribI2uiv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttribI3uiv */

/* Enums to test for glVertexAttribI3uiv (may not be required) */
struct enumTestRec const ea_VertexAttribI3uiv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttribI3uiv(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttribI3uiv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttribI3uiv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttribI3uiv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttribI4bv */

/* Enums to test for glVertexAttribI4bv (may not be required) */
struct enumTestRec const ea_VertexAttribI4bv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttribI4bv(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttribI4bv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttribI4bv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttribI4bv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttribI4sv */

/* Enums to test for glVertexAttribI4sv (may not be required) */
struct enumTestRec const ea_VertexAttribI4sv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttribI4sv(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttribI4sv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttribI4sv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttribI4sv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttribI4ubv */

/* Enums to test for glVertexAttribI4ubv (may not be required) */
struct enumTestRec const ea_VertexAttribI4ubv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttribI4ubv(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttribI4ubv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttribI4ubv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttribI4ubv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttribI4usv */

/* Enums to test for glVertexAttribI4usv (may not be required) */
struct enumTestRec const ea_VertexAttribI4usv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttribI4usv(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttribI4usv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttribI4usv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttribI4usv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glBindFragDataLocation */

/* Enums to test for glBindFragDataLocation (may not be required) */
struct enumTestRec const ea_BindFragDataLocation[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallBindFragDataLocation(void)
{
    GLuint i;
    bool success = true;

    /* glBindFragDataLocation may need to loop over a set of enums doing something with them */
    for (i = 0; ea_BindFragDataLocation[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::BindFragDataLocation", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glUniform1ui */

/* Enums to test for glUniform1ui (may not be required) */
struct enumTestRec const ea_Uniform1ui[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallUniform1ui(void)
{
    GLuint i;
    bool success = true;

    /* glUniform1ui may need to loop over a set of enums doing something with them */
    for (i = 0; ea_Uniform1ui[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::Uniform1ui", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glUniform3ui */

/* Enums to test for glUniform3ui (may not be required) */
struct enumTestRec const ea_Uniform3ui[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallUniform3ui(void)
{
    GLuint i;
    bool success = true;

    /* glUniform3ui may need to loop over a set of enums doing something with them */
    for (i = 0; ea_Uniform3ui[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::Uniform3ui", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glUniform4ui */

/* Enums to test for glUniform4ui (may not be required) */
struct enumTestRec const ea_Uniform4ui[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallUniform4ui(void)
{
    GLuint i;
    bool success = true;

    /* glUniform4ui may need to loop over a set of enums doing something with them */
    for (i = 0; ea_Uniform4ui[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::Uniform4ui", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glUniform1uiv */

/* Enums to test for glUniform1uiv (may not be required) */
struct enumTestRec const ea_Uniform1uiv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallUniform1uiv(void)
{
    GLuint i;
    bool success = true;

    /* glUniform1uiv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_Uniform1uiv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::Uniform1uiv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glUniform3uiv */

/* Enums to test for glUniform3uiv (may not be required) */
struct enumTestRec const ea_Uniform3uiv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallUniform3uiv(void)
{
    GLuint i;
    bool success = true;

    /* glUniform3uiv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_Uniform3uiv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::Uniform3uiv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glUniform4uiv */

/* Enums to test for glUniform4uiv (may not be required) */
struct enumTestRec const ea_Uniform4uiv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallUniform4uiv(void)
{
    GLuint i;
    bool success = true;

    /* glUniform4uiv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_Uniform4uiv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::Uniform4uiv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glTexParameterIiv */

/* Enums to test for glTexParameterIiv (may not be required) */
struct enumTestRec const ea_TexParameterIiv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallTexParameterIiv(void)
{
    GLuint i;
    bool success = true;

    /* glTexParameterIiv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_TexParameterIiv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::TexParameterIiv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glTexParameterIuiv */

/* Enums to test for glTexParameterIuiv (may not be required) */
struct enumTestRec const ea_TexParameterIuiv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallTexParameterIuiv(void)
{
    GLuint i;
    bool success = true;

    /* glTexParameterIuiv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_TexParameterIuiv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::TexParameterIuiv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glGetTexParameterIiv */

/* Enums to test for glGetTexParameterIiv (may not be required) */
struct enumTestRec const ea_GetTexParameterIiv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallGetTexParameterIiv(void)
{
    GLuint i;
    bool success = true;

    /* glGetTexParameterIiv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_GetTexParameterIiv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::GetTexParameterIiv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glGetTexParameterIuiv */

/* Enums to test for glGetTexParameterIuiv (may not be required) */
struct enumTestRec const ea_GetTexParameterIuiv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallGetTexParameterIuiv(void)
{
    GLuint i;
    bool success = true;

    /* glGetTexParameterIuiv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_GetTexParameterIuiv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::GetTexParameterIuiv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glFramebufferTexture1D */

/* Enums to test for glFramebufferTexture1D (may not be required) */
struct enumTestRec const ea_FramebufferTexture1D[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallFramebufferTexture1D(void)
{
    GLuint i;
    bool success = true;

    /* glFramebufferTexture1D may need to loop over a set of enums doing something with them */
    for (i = 0; ea_FramebufferTexture1D[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::FramebufferTexture1D", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glFramebufferTexture3D */

/* Enums to test for glFramebufferTexture3D (may not be required) */
struct enumTestRec const ea_FramebufferTexture3D[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallFramebufferTexture3D(void)
{
    GLuint i;
    bool success = true;

    /* glFramebufferTexture3D may need to loop over a set of enums doing something with them */
    for (i = 0; ea_FramebufferTexture3D[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::FramebufferTexture3D", "Coverage test not implemented yet");

    return success;
}

/* OpenGL 3.1 entry points */

/* Coverage test for glTexBuffer */

/* Enums to test for glTexBuffer (may not be required) */
struct enumTestRec const ea_TexBuffer[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallTexBuffer(void)
{
    GLuint i;
    bool success = true;

    /* glTexBuffer may need to loop over a set of enums doing something with them */
    for (i = 0; ea_TexBuffer[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::TexBuffer", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glPrimitiveRestartIndex */

/* Enums to test for glPrimitiveRestartIndex (may not be required) */
struct enumTestRec const ea_PrimitiveRestartIndex[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallPrimitiveRestartIndex(void)
{
    GLuint i;
    bool success = true;

    /* glPrimitiveRestartIndex may need to loop over a set of enums doing something with them */
    for (i = 0; ea_PrimitiveRestartIndex[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::PrimitiveRestartIndex", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glGetActiveUniformName */

/* Enums to test for glGetActiveUniformName (may not be required) */
struct enumTestRec const ea_GetActiveUniformName[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallGetActiveUniformName(void)
{
    GLuint i;
    bool success = true;

    /* glGetActiveUniformName may need to loop over a set of enums doing something with them */
    for (i = 0; ea_GetActiveUniformName[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::GetActiveUniformName", "Coverage test not implemented yet");

    return success;
}

/* OpenGL 3.2 entry points */

/* Coverage test for glGetInteger64i_v */

/* Enums to test for glGetInteger64i_v (may not be required) */
struct enumTestRec const ea_GetInteger64i_v[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallGetInteger64i_v(void)
{
    GLuint i;
    bool success = true;

    /* glGetInteger64i_v may need to loop over a set of enums doing something with them */
    for (i = 0; ea_GetInteger64i_v[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::GetInteger64i_v", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glFramebufferTexture */

/* Enums to test for glFramebufferTexture (may not be required) */
struct enumTestRec const ea_FramebufferTexture[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallFramebufferTexture(void)
{
    GLuint i;
    bool success = true;

    /* glFramebufferTexture may need to loop over a set of enums doing something with them */
    for (i = 0; ea_FramebufferTexture[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::FramebufferTexture", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glDrawElementsBaseVertex */

/* Enums to test for glDrawElementsBaseVertex (may not be required) */
struct enumTestRec const ea_DrawElementsBaseVertex[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallDrawElementsBaseVertex(void)
{
    GLuint i;
    bool success = true;

    /* glDrawElementsBaseVertex may need to loop over a set of enums doing something with them */
    for (i = 0; ea_DrawElementsBaseVertex[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::DrawElementsBaseVertex", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glDrawRangeElementsBaseVertex */

/* Enums to test for glDrawRangeElementsBaseVertex (may not be required) */
struct enumTestRec const ea_DrawRangeElementsBaseVertex[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallDrawRangeElementsBaseVertex(void)
{
    GLuint i;
    bool success = true;

    /* glDrawRangeElementsBaseVertex may need to loop over a set of enums doing something with them */
    for (i = 0; ea_DrawRangeElementsBaseVertex[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::DrawRangeElementsBaseVertex", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glDrawElementsInstancedBaseVertex */

/* Enums to test for glDrawElementsInstancedBaseVertex (may not be required) */
struct enumTestRec const ea_DrawElementsInstancedBaseVertex[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallDrawElementsInstancedBaseVertex(void)
{
    GLuint i;
    bool success = true;

    /* glDrawElementsInstancedBaseVertex may need to loop over a set of enums doing something with them */
    for (i = 0; ea_DrawElementsInstancedBaseVertex[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::DrawElementsInstancedBaseVertex", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glMultiDrawElementsBaseVertex */

/* Enums to test for glMultiDrawElementsBaseVertex (may not be required) */
struct enumTestRec const ea_MultiDrawElementsBaseVertex[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallMultiDrawElementsBaseVertex(void)
{
    GLuint i;
    bool success = true;

    /* glMultiDrawElementsBaseVertex may need to loop over a set of enums doing something with them */
    for (i = 0; ea_MultiDrawElementsBaseVertex[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::MultiDrawElementsBaseVertex", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glProvokingVertex */

/* Enums to test for glProvokingVertex (may not be required) */
struct enumTestRec const ea_ProvokingVertex[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallProvokingVertex(void)
{
    GLuint i;
    bool success = true;

    /* glProvokingVertex may need to loop over a set of enums doing something with them */
    for (i = 0; ea_ProvokingVertex[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::ProvokingVertex", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glTexImage2DMultisample */

/* Enums to test for glTexImage2DMultisample (may not be required) */
struct enumTestRec const ea_TexImage2DMultisample[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallTexImage2DMultisample(void)
{
    GLuint i;
    bool success = true;

    /* glTexImage2DMultisample may need to loop over a set of enums doing something with them */
    for (i = 0; ea_TexImage2DMultisample[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::TexImage2DMultisample", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glTexImage3DMultisample */

/* Enums to test for glTexImage3DMultisample (may not be required) */
struct enumTestRec const ea_TexImage3DMultisample[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallTexImage3DMultisample(void)
{
    GLuint i;
    bool success = true;

    /* glTexImage3DMultisample may need to loop over a set of enums doing something with them */
    for (i = 0; ea_TexImage3DMultisample[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::TexImage3DMultisample", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glGetMultisamplefv */

/* Enums to test for glGetMultisamplefv (may not be required) */
struct enumTestRec const ea_GetMultisamplefv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallGetMultisamplefv(void)
{
    GLuint i;
    bool success = true;

    /* glGetMultisamplefv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_GetMultisamplefv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::GetMultisamplefv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glSampleMaski */

/* Enums to test for glSampleMaski (may not be required) */
struct enumTestRec const ea_SampleMaski[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallSampleMaski(void)
{
    GLuint i;
    bool success = true;

    /* glSampleMaski may need to loop over a set of enums doing something with them */
    for (i = 0; ea_SampleMaski[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::SampleMaski", "Coverage test not implemented yet");

    return success;
}

/* OpenGL 3.3 entry points */

/* Coverage test for glBindFragDataLocationIndexed */

/* Enums to test for glBindFragDataLocationIndexed (may not be required) */
struct enumTestRec const ea_BindFragDataLocationIndexed[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallBindFragDataLocationIndexed(void)
{
    GLuint i;
    bool success = true;

    /* glBindFragDataLocationIndexed may need to loop over a set of enums doing something with them */
    for (i = 0; ea_BindFragDataLocationIndexed[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::BindFragDataLocationIndexed", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glGetFragDataIndex */

/* Enums to test for glGetFragDataIndex (may not be required) */
struct enumTestRec const ea_GetFragDataIndex[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallGetFragDataIndex(void)
{
    GLuint i;
    bool success = true;

    /* glGetFragDataIndex may need to loop over a set of enums doing something with them */
    for (i = 0; ea_GetFragDataIndex[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::GetFragDataIndex", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glSamplerParameterIiv */

/* Enums to test for glSamplerParameterIiv (may not be required) */
struct enumTestRec const ea_SamplerParameterIiv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallSamplerParameterIiv(void)
{
    GLuint i;
    bool success = true;

    /* glSamplerParameterIiv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_SamplerParameterIiv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::SamplerParameterIiv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glSamplerParameterIuiv */

/* Enums to test for glSamplerParameterIuiv (may not be required) */
struct enumTestRec const ea_SamplerParameterIuiv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallSamplerParameterIuiv(void)
{
    GLuint i;
    bool success = true;

    /* glSamplerParameterIuiv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_SamplerParameterIuiv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::SamplerParameterIuiv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glGetSamplerParameterIiv */

/* Enums to test for glGetSamplerParameterIiv (may not be required) */
struct enumTestRec const ea_GetSamplerParameterIiv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallGetSamplerParameterIiv(void)
{
    GLuint i;
    bool success = true;

    /* glGetSamplerParameterIiv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_GetSamplerParameterIiv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::GetSamplerParameterIiv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glGetSamplerParameterIfv */

/* Enums to test for glGetSamplerParameterIfv (may not be required) */
struct enumTestRec const ea_GetSamplerParameterIfv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallGetSamplerParameterIfv(void)
{
    GLuint i;
    bool success = true;

    /* glGetSamplerParameterIfv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_GetSamplerParameterIfv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::GetSamplerParameterIfv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glQueryCounter */

/* Enums to test for glQueryCounter (may not be required) */
struct enumTestRec const ea_QueryCounter[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallQueryCounter(void)
{
    GLuint i;
    bool success = true;

    /* glQueryCounter may need to loop over a set of enums doing something with them */
    for (i = 0; ea_QueryCounter[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::QueryCounter", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glGetQueryObjecti64v */

/* Enums to test for glGetQueryObjecti64v (may not be required) */
struct enumTestRec const ea_GetQueryObjecti64v[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallGetQueryObjecti64v(void)
{
    GLuint i;
    bool success = true;

    /* glGetQueryObjecti64v may need to loop over a set of enums doing something with them */
    for (i = 0; ea_GetQueryObjecti64v[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::GetQueryObjecti64v", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glGetQueryObjectui64v */

/* Enums to test for glGetQueryObjectui64v (may not be required) */
struct enumTestRec const ea_GetQueryObjectui64v[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallGetQueryObjectui64v(void)
{
    GLuint i;
    bool success = true;

    /* glGetQueryObjectui64v may need to loop over a set of enums doing something with them */
    for (i = 0; ea_GetQueryObjectui64v[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::GetQueryObjectui64v", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexP2ui */

/* Enums to test for glVertexP2ui (may not be required) */
struct enumTestRec const ea_VertexP2ui[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexP2ui(void)
{
    GLuint i;
    bool success = true;

    /* glVertexP2ui may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexP2ui[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexP2ui", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexP2uiv */

/* Enums to test for glVertexP2uiv (may not be required) */
struct enumTestRec const ea_VertexP2uiv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexP2uiv(void)
{
    GLuint i;
    bool success = true;

    /* glVertexP2uiv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexP2uiv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexP2uiv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexP3ui */

/* Enums to test for glVertexP3ui (may not be required) */
struct enumTestRec const ea_VertexP3ui[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexP3ui(void)
{
    GLuint i;
    bool success = true;

    /* glVertexP3ui may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexP3ui[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexP3ui", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexP3uiv */

/* Enums to test for glVertexP3uiv (may not be required) */
struct enumTestRec const ea_VertexP3uiv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexP3uiv(void)
{
    GLuint i;
    bool success = true;

    /* glVertexP3uiv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexP3uiv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexP3uiv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexP4ui */

/* Enums to test for glVertexP4ui (may not be required) */
struct enumTestRec const ea_VertexP4ui[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexP4ui(void)
{
    GLuint i;
    bool success = true;

    /* glVertexP4ui may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexP4ui[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexP4ui", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexP4uiv */

/* Enums to test for glVertexP4uiv (may not be required) */
struct enumTestRec const ea_VertexP4uiv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexP4uiv(void)
{
    GLuint i;
    bool success = true;

    /* glVertexP4uiv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexP4uiv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexP4uiv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glTexCoordP1ui */

/* Enums to test for glTexCoordP1ui (may not be required) */
struct enumTestRec const ea_TexCoordP1ui[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallTexCoordP1ui(void)
{
    GLuint i;
    bool success = true;

    /* glTexCoordP1ui may need to loop over a set of enums doing something with them */
    for (i = 0; ea_TexCoordP1ui[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::TexCoordP1ui", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glTexCoordP1uiv */

/* Enums to test for glTexCoordP1uiv (may not be required) */
struct enumTestRec const ea_TexCoordP1uiv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallTexCoordP1uiv(void)
{
    GLuint i;
    bool success = true;

    /* glTexCoordP1uiv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_TexCoordP1uiv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::TexCoordP1uiv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glTexCoordP2ui */

/* Enums to test for glTexCoordP2ui (may not be required) */
struct enumTestRec const ea_TexCoordP2ui[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallTexCoordP2ui(void)
{
    GLuint i;
    bool success = true;

    /* glTexCoordP2ui may need to loop over a set of enums doing something with them */
    for (i = 0; ea_TexCoordP2ui[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::TexCoordP2ui", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glTexCoordP2uiv */

/* Enums to test for glTexCoordP2uiv (may not be required) */
struct enumTestRec const ea_TexCoordP2uiv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallTexCoordP2uiv(void)
{
    GLuint i;
    bool success = true;

    /* glTexCoordP2uiv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_TexCoordP2uiv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::TexCoordP2uiv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glTexCoordP3ui */

/* Enums to test for glTexCoordP3ui (may not be required) */
struct enumTestRec const ea_TexCoordP3ui[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallTexCoordP3ui(void)
{
    GLuint i;
    bool success = true;

    /* glTexCoordP3ui may need to loop over a set of enums doing something with them */
    for (i = 0; ea_TexCoordP3ui[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::TexCoordP3ui", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glTexCoordP3uiv */

/* Enums to test for glTexCoordP3uiv (may not be required) */
struct enumTestRec const ea_TexCoordP3uiv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallTexCoordP3uiv(void)
{
    GLuint i;
    bool success = true;

    /* glTexCoordP3uiv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_TexCoordP3uiv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::TexCoordP3uiv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glTexCoordP4ui */

/* Enums to test for glTexCoordP4ui (may not be required) */
struct enumTestRec const ea_TexCoordP4ui[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallTexCoordP4ui(void)
{
    GLuint i;
    bool success = true;

    /* glTexCoordP4ui may need to loop over a set of enums doing something with them */
    for (i = 0; ea_TexCoordP4ui[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::TexCoordP4ui", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glTexCoordP4uiv */

/* Enums to test for glTexCoordP4uiv (may not be required) */
struct enumTestRec const ea_TexCoordP4uiv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallTexCoordP4uiv(void)
{
    GLuint i;
    bool success = true;

    /* glTexCoordP4uiv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_TexCoordP4uiv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::TexCoordP4uiv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glMultiTexCoordP1ui */

/* Enums to test for glMultiTexCoordP1ui (may not be required) */
struct enumTestRec const ea_MultiTexCoordP1ui[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallMultiTexCoordP1ui(void)
{
    GLuint i;
    bool success = true;

    /* glMultiTexCoordP1ui may need to loop over a set of enums doing something with them */
    for (i = 0; ea_MultiTexCoordP1ui[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::MultiTexCoordP1ui", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glMultiTexCoordP1uiv */

/* Enums to test for glMultiTexCoordP1uiv (may not be required) */
struct enumTestRec const ea_MultiTexCoordP1uiv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallMultiTexCoordP1uiv(void)
{
    GLuint i;
    bool success = true;

    /* glMultiTexCoordP1uiv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_MultiTexCoordP1uiv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::MultiTexCoordP1uiv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glMultiTexCoordP2ui */

/* Enums to test for glMultiTexCoordP2ui (may not be required) */
struct enumTestRec const ea_MultiTexCoordP2ui[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallMultiTexCoordP2ui(void)
{
    GLuint i;
    bool success = true;

    /* glMultiTexCoordP2ui may need to loop over a set of enums doing something with them */
    for (i = 0; ea_MultiTexCoordP2ui[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::MultiTexCoordP2ui", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glMultiTexCoordP2uiv */

/* Enums to test for glMultiTexCoordP2uiv (may not be required) */
struct enumTestRec const ea_MultiTexCoordP2uiv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallMultiTexCoordP2uiv(void)
{
    GLuint i;
    bool success = true;

    /* glMultiTexCoordP2uiv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_MultiTexCoordP2uiv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::MultiTexCoordP2uiv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glMultiTexCoordP3ui */

/* Enums to test for glMultiTexCoordP3ui (may not be required) */
struct enumTestRec const ea_MultiTexCoordP3ui[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallMultiTexCoordP3ui(void)
{
    GLuint i;
    bool success = true;

    /* glMultiTexCoordP3ui may need to loop over a set of enums doing something with them */
    for (i = 0; ea_MultiTexCoordP3ui[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::MultiTexCoordP3ui", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glMultiTexCoordP3uiv */

/* Enums to test for glMultiTexCoordP3uiv (may not be required) */
struct enumTestRec const ea_MultiTexCoordP3uiv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallMultiTexCoordP3uiv(void)
{
    GLuint i;
    bool success = true;

    /* glMultiTexCoordP3uiv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_MultiTexCoordP3uiv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::MultiTexCoordP3uiv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glMultiTexCoordP4ui */

/* Enums to test for glMultiTexCoordP4ui (may not be required) */
struct enumTestRec const ea_MultiTexCoordP4ui[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallMultiTexCoordP4ui(void)
{
    GLuint i;
    bool success = true;

    /* glMultiTexCoordP4ui may need to loop over a set of enums doing something with them */
    for (i = 0; ea_MultiTexCoordP4ui[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::MultiTexCoordP4ui", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glMultiTexCoordP4uiv */

/* Enums to test for glMultiTexCoordP4uiv (may not be required) */
struct enumTestRec const ea_MultiTexCoordP4uiv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallMultiTexCoordP4uiv(void)
{
    GLuint i;
    bool success = true;

    /* glMultiTexCoordP4uiv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_MultiTexCoordP4uiv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::MultiTexCoordP4uiv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glNormalP3ui */

/* Enums to test for glNormalP3ui (may not be required) */
struct enumTestRec const ea_NormalP3ui[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallNormalP3ui(void)
{
    GLuint i;
    bool success = true;

    /* glNormalP3ui may need to loop over a set of enums doing something with them */
    for (i = 0; ea_NormalP3ui[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::NormalP3ui", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glNormalP3uiv */

/* Enums to test for glNormalP3uiv (may not be required) */
struct enumTestRec const ea_NormalP3uiv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallNormalP3uiv(void)
{
    GLuint i;
    bool success = true;

    /* glNormalP3uiv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_NormalP3uiv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::NormalP3uiv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glColorP3ui */

/* Enums to test for glColorP3ui (may not be required) */
struct enumTestRec const ea_ColorP3ui[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallColorP3ui(void)
{
    GLuint i;
    bool success = true;

    /* glColorP3ui may need to loop over a set of enums doing something with them */
    for (i = 0; ea_ColorP3ui[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::ColorP3ui", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glColorP3uiv */

/* Enums to test for glColorP3uiv (may not be required) */
struct enumTestRec const ea_ColorP3uiv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallColorP3uiv(void)
{
    GLuint i;
    bool success = true;

    /* glColorP3uiv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_ColorP3uiv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::ColorP3uiv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glColorP4ui */

/* Enums to test for glColorP4ui (may not be required) */
struct enumTestRec const ea_ColorP4ui[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallColorP4ui(void)
{
    GLuint i;
    bool success = true;

    /* glColorP4ui may need to loop over a set of enums doing something with them */
    for (i = 0; ea_ColorP4ui[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::ColorP4ui", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glColorP4uiv */

/* Enums to test for glColorP4uiv (may not be required) */
struct enumTestRec const ea_ColorP4uiv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallColorP4uiv(void)
{
    GLuint i;
    bool success = true;

    /* glColorP4uiv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_ColorP4uiv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::ColorP4uiv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glSecondaryColorP3ui */

/* Enums to test for glSecondaryColorP3ui (may not be required) */
struct enumTestRec const ea_SecondaryColorP3ui[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallSecondaryColorP3ui(void)
{
    GLuint i;
    bool success = true;

    /* glSecondaryColorP3ui may need to loop over a set of enums doing something with them */
    for (i = 0; ea_SecondaryColorP3ui[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::SecondaryColorP3ui", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glSecondaryColorP3uiv */

/* Enums to test for glSecondaryColorP3uiv (may not be required) */
struct enumTestRec const ea_SecondaryColorP3uiv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallSecondaryColorP3uiv(void)
{
    GLuint i;
    bool success = true;

    /* glSecondaryColorP3uiv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_SecondaryColorP3uiv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::SecondaryColorP3uiv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttribP1ui */

/* Enums to test for glVertexAttribP1ui (may not be required) */
struct enumTestRec const ea_VertexAttribP1ui[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttribP1ui(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttribP1ui may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttribP1ui[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttribP1ui", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttribP1uiv */

/* Enums to test for glVertexAttribP1uiv (may not be required) */
struct enumTestRec const ea_VertexAttribP1uiv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttribP1uiv(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttribP1uiv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttribP1uiv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttribP1uiv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttribP2ui */

/* Enums to test for glVertexAttribP2ui (may not be required) */
struct enumTestRec const ea_VertexAttribP2ui[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttribP2ui(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttribP2ui may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttribP2ui[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttribP2ui", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttribP2uiv */

/* Enums to test for glVertexAttribP2uiv (may not be required) */
struct enumTestRec const ea_VertexAttribP2uiv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttribP2uiv(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttribP2uiv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttribP2uiv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttribP2uiv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttribP3ui */

/* Enums to test for glVertexAttribP3ui (may not be required) */
struct enumTestRec const ea_VertexAttribP3ui[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttribP3ui(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttribP3ui may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttribP3ui[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttribP3ui", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttribP3uiv */

/* Enums to test for glVertexAttribP3uiv (may not be required) */
struct enumTestRec const ea_VertexAttribP3uiv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttribP3uiv(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttribP3uiv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttribP3uiv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttribP3uiv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttribP4ui */

/* Enums to test for glVertexAttribP4ui (may not be required) */
struct enumTestRec const ea_VertexAttribP4ui[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttribP4ui(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttribP4ui may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttribP4ui[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttribP4ui", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glVertexAttribP4uiv */

/* Enums to test for glVertexAttribP4uiv (may not be required) */
struct enumTestRec const ea_VertexAttribP4uiv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallVertexAttribP4uiv(void)
{
    GLuint i;
    bool success = true;

    /* glVertexAttribP4uiv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_VertexAttribP4uiv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::VertexAttribP4uiv", "Coverage test not implemented yet");

    return success;
}

/* OpenGL 4.0 entry points */

/* Coverage test for glDrawArraysIndirect */

/* Enums to test for glDrawArraysIndirect (may not be required) */
struct enumTestRec const ea_DrawArraysIndirect[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallDrawArraysIndirect(void)
{
    GLuint i;
    bool success = true;

    /* glDrawArraysIndirect may need to loop over a set of enums doing something with them */
    for (i = 0; ea_DrawArraysIndirect[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::DrawArraysIndirect", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glDrawElementsIndirect */

/* Enums to test for glDrawElementsIndirect (may not be required) */
struct enumTestRec const ea_DrawElementsIndirect[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallDrawElementsIndirect(void)
{
    GLuint i;
    bool success = true;

    /* glDrawElementsIndirect may need to loop over a set of enums doing something with them */
    for (i = 0; ea_DrawElementsIndirect[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::DrawElementsIndirect", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glUniform1d */

/* Enums to test for glUniform1d (may not be required) */
struct enumTestRec const ea_Uniform1d[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallUniform1d(void)
{
    GLuint i;
    bool success = true;

    /* glUniform1d may need to loop over a set of enums doing something with them */
    for (i = 0; ea_Uniform1d[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::Uniform1d", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glUniform2d */

/* Enums to test for glUniform2d (may not be required) */
struct enumTestRec const ea_Uniform2d[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallUniform2d(void)
{
    GLuint i;
    bool success = true;

    /* glUniform2d may need to loop over a set of enums doing something with them */
    for (i = 0; ea_Uniform2d[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::Uniform2d", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glUniform3d */

/* Enums to test for glUniform3d (may not be required) */
struct enumTestRec const ea_Uniform3d[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallUniform3d(void)
{
    GLuint i;
    bool success = true;

    /* glUniform3d may need to loop over a set of enums doing something with them */
    for (i = 0; ea_Uniform3d[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::Uniform3d", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glUniform4d */

/* Enums to test for glUniform4d (may not be required) */
struct enumTestRec const ea_Uniform4d[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallUniform4d(void)
{
    GLuint i;
    bool success = true;

    /* glUniform4d may need to loop over a set of enums doing something with them */
    for (i = 0; ea_Uniform4d[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::Uniform4d", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glUniform1dv */

/* Enums to test for glUniform1dv (may not be required) */
struct enumTestRec const ea_Uniform1dv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallUniform1dv(void)
{
    GLuint i;
    bool success = true;

    /* glUniform1dv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_Uniform1dv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::Uniform1dv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glUniform2dv */

/* Enums to test for glUniform2dv (may not be required) */
struct enumTestRec const ea_Uniform2dv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallUniform2dv(void)
{
    GLuint i;
    bool success = true;

    /* glUniform2dv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_Uniform2dv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::Uniform2dv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glUniform3dv */

/* Enums to test for glUniform3dv (may not be required) */
struct enumTestRec const ea_Uniform3dv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallUniform3dv(void)
{
    GLuint i;
    bool success = true;

    /* glUniform3dv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_Uniform3dv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::Uniform3dv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glUniform4dv */

/* Enums to test for glUniform4dv (may not be required) */
struct enumTestRec const ea_Uniform4dv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallUniform4dv(void)
{
    GLuint i;
    bool success = true;

    /* glUniform4dv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_Uniform4dv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::Uniform4dv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glUniformMatrix2dv */

/* Enums to test for glUniformMatrix2dv (may not be required) */
struct enumTestRec const ea_UniformMatrix2dv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallUniformMatrix2dv(void)
{
    GLuint i;
    bool success = true;

    /* glUniformMatrix2dv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_UniformMatrix2dv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::UniformMatrix2dv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glUniformMatrix3dv */

/* Enums to test for glUniformMatrix3dv (may not be required) */
struct enumTestRec const ea_UniformMatrix3dv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallUniformMatrix3dv(void)
{
    GLuint i;
    bool success = true;

    /* glUniformMatrix3dv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_UniformMatrix3dv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::UniformMatrix3dv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glUniformMatrix4dv */

/* Enums to test for glUniformMatrix4dv (may not be required) */
struct enumTestRec const ea_UniformMatrix4dv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallUniformMatrix4dv(void)
{
    GLuint i;
    bool success = true;

    /* glUniformMatrix4dv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_UniformMatrix4dv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::UniformMatrix4dv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glUniformMatrix2x3dv */

/* Enums to test for glUniformMatrix2x3dv (may not be required) */
struct enumTestRec const ea_UniformMatrix2x3dv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallUniformMatrix2x3dv(void)
{
    GLuint i;
    bool success = true;

    /* glUniformMatrix2x3dv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_UniformMatrix2x3dv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::UniformMatrix2x3dv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glUniformMatrix2x4dv */

/* Enums to test for glUniformMatrix2x4dv (may not be required) */
struct enumTestRec const ea_UniformMatrix2x4dv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallUniformMatrix2x4dv(void)
{
    GLuint i;
    bool success = true;

    /* glUniformMatrix2x4dv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_UniformMatrix2x4dv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::UniformMatrix2x4dv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glUniformMatrix3x2dv */

/* Enums to test for glUniformMatrix3x2dv (may not be required) */
struct enumTestRec const ea_UniformMatrix3x2dv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallUniformMatrix3x2dv(void)
{
    GLuint i;
    bool success = true;

    /* glUniformMatrix3x2dv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_UniformMatrix3x2dv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::UniformMatrix3x2dv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glUniformMatrix3x4dv */

/* Enums to test for glUniformMatrix3x4dv (may not be required) */
struct enumTestRec const ea_UniformMatrix3x4dv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallUniformMatrix3x4dv(void)
{
    GLuint i;
    bool success = true;

    /* glUniformMatrix3x4dv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_UniformMatrix3x4dv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::UniformMatrix3x4dv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glUniformMatrix4x2dv */

/* Enums to test for glUniformMatrix4x2dv (may not be required) */
struct enumTestRec const ea_UniformMatrix4x2dv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallUniformMatrix4x2dv(void)
{
    GLuint i;
    bool success = true;

    /* glUniformMatrix4x2dv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_UniformMatrix4x2dv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::UniformMatrix4x2dv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glUniformMatrix4x3dv */

/* Enums to test for glUniformMatrix4x3dv (may not be required) */
struct enumTestRec const ea_UniformMatrix4x3dv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallUniformMatrix4x3dv(void)
{
    GLuint i;
    bool success = true;

    /* glUniformMatrix4x3dv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_UniformMatrix4x3dv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::UniformMatrix4x3dv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glGetUniformdv */

/* Enums to test for glGetUniformdv (may not be required) */
struct enumTestRec const ea_GetUniformdv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallGetUniformdv(void)
{
    GLuint i;
    bool success = true;

    /* glGetUniformdv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_GetUniformdv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::GetUniformdv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glProgramUniform1dEXT */

/* Enums to test for glProgramUniform1dEXT (may not be required) */
struct enumTestRec const ea_ProgramUniform1dEXT[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallProgramUniform1dEXT(void)
{
    GLuint i;
    bool success = true;

    /* glProgramUniform1dEXT may need to loop over a set of enums doing something with them */
    for (i = 0; ea_ProgramUniform1dEXT[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::ProgramUniform1dEXT", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glProgramUniform2dEXT */

/* Enums to test for glProgramUniform2dEXT (may not be required) */
struct enumTestRec const ea_ProgramUniform2dEXT[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallProgramUniform2dEXT(void)
{
    GLuint i;
    bool success = true;

    /* glProgramUniform2dEXT may need to loop over a set of enums doing something with them */
    for (i = 0; ea_ProgramUniform2dEXT[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::ProgramUniform2dEXT", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glProgramUniform3dEXT */

/* Enums to test for glProgramUniform3dEXT (may not be required) */
struct enumTestRec const ea_ProgramUniform3dEXT[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallProgramUniform3dEXT(void)
{
    GLuint i;
    bool success = true;

    /* glProgramUniform3dEXT may need to loop over a set of enums doing something with them */
    for (i = 0; ea_ProgramUniform3dEXT[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::ProgramUniform3dEXT", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glProgramUniform4dEXT */

/* Enums to test for glProgramUniform4dEXT (may not be required) */
struct enumTestRec const ea_ProgramUniform4dEXT[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallProgramUniform4dEXT(void)
{
    GLuint i;
    bool success = true;

    /* glProgramUniform4dEXT may need to loop over a set of enums doing something with them */
    for (i = 0; ea_ProgramUniform4dEXT[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::ProgramUniform4dEXT", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glProgramUniform1dvEXT */

/* Enums to test for glProgramUniform1dvEXT (may not be required) */
struct enumTestRec const ea_ProgramUniform1dvEXT[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallProgramUniform1dvEXT(void)
{
    GLuint i;
    bool success = true;

    /* glProgramUniform1dvEXT may need to loop over a set of enums doing something with them */
    for (i = 0; ea_ProgramUniform1dvEXT[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::ProgramUniform1dvEXT", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glProgramUniform2dvEXT */

/* Enums to test for glProgramUniform2dvEXT (may not be required) */
struct enumTestRec const ea_ProgramUniform2dvEXT[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallProgramUniform2dvEXT(void)
{
    GLuint i;
    bool success = true;

    /* glProgramUniform2dvEXT may need to loop over a set of enums doing something with them */
    for (i = 0; ea_ProgramUniform2dvEXT[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::ProgramUniform2dvEXT", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glProgramUniform3dvEXT */

/* Enums to test for glProgramUniform3dvEXT (may not be required) */
struct enumTestRec const ea_ProgramUniform3dvEXT[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallProgramUniform3dvEXT(void)
{
    GLuint i;
    bool success = true;

    /* glProgramUniform3dvEXT may need to loop over a set of enums doing something with them */
    for (i = 0; ea_ProgramUniform3dvEXT[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::ProgramUniform3dvEXT", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glProgramUniform4dvEXT */

/* Enums to test for glProgramUniform4dvEXT (may not be required) */
struct enumTestRec const ea_ProgramUniform4dvEXT[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallProgramUniform4dvEXT(void)
{
    GLuint i;
    bool success = true;

    /* glProgramUniform4dvEXT may need to loop over a set of enums doing something with them */
    for (i = 0; ea_ProgramUniform4dvEXT[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::ProgramUniform4dvEXT", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glProgramUniformMatrix2dvEXT */

/* Enums to test for glProgramUniformMatrix2dvEXT (may not be required) */
struct enumTestRec const ea_ProgramUniformMatrix2dvEXT[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallProgramUniformMatrix2dvEXT(void)
{
    GLuint i;
    bool success = true;

    /* glProgramUniformMatrix2dvEXT may need to loop over a set of enums doing something with them */
    for (i = 0; ea_ProgramUniformMatrix2dvEXT[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::ProgramUniformMatrix2dvEXT", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glProgramUniformMatrix3dvEXT */

/* Enums to test for glProgramUniformMatrix3dvEXT (may not be required) */
struct enumTestRec const ea_ProgramUniformMatrix3dvEXT[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallProgramUniformMatrix3dvEXT(void)
{
    GLuint i;
    bool success = true;

    /* glProgramUniformMatrix3dvEXT may need to loop over a set of enums doing something with them */
    for (i = 0; ea_ProgramUniformMatrix3dvEXT[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::ProgramUniformMatrix3dvEXT", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glProgramUniformMatrix4dvEXT */

/* Enums to test for glProgramUniformMatrix4dvEXT (may not be required) */
struct enumTestRec const ea_ProgramUniformMatrix4dvEXT[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallProgramUniformMatrix4dvEXT(void)
{
    GLuint i;
    bool success = true;

    /* glProgramUniformMatrix4dvEXT may need to loop over a set of enums doing something with them */
    for (i = 0; ea_ProgramUniformMatrix4dvEXT[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::ProgramUniformMatrix4dvEXT", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glProgramUniformMatrix2x3dvEXT */

/* Enums to test for glProgramUniformMatrix2x3dvEXT (may not be required) */
struct enumTestRec const ea_ProgramUniformMatrix2x3dvEXT[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallProgramUniformMatrix2x3dvEXT(void)
{
    GLuint i;
    bool success = true;

    /* glProgramUniformMatrix2x3dvEXT may need to loop over a set of enums doing something with them */
    for (i = 0; ea_ProgramUniformMatrix2x3dvEXT[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::ProgramUniformMatrix2x3dvEXT", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glProgramUniformMatrix2x4dvEXT */

/* Enums to test for glProgramUniformMatrix2x4dvEXT (may not be required) */
struct enumTestRec const ea_ProgramUniformMatrix2x4dvEXT[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallProgramUniformMatrix2x4dvEXT(void)
{
    GLuint i;
    bool success = true;

    /* glProgramUniformMatrix2x4dvEXT may need to loop over a set of enums doing something with them */
    for (i = 0; ea_ProgramUniformMatrix2x4dvEXT[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::ProgramUniformMatrix2x4dvEXT", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glProgramUniformMatrix3x2dvEXT */

/* Enums to test for glProgramUniformMatrix3x2dvEXT (may not be required) */
struct enumTestRec const ea_ProgramUniformMatrix3x2dvEXT[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallProgramUniformMatrix3x2dvEXT(void)
{
    GLuint i;
    bool success = true;

    /* glProgramUniformMatrix3x2dvEXT may need to loop over a set of enums doing something with them */
    for (i = 0; ea_ProgramUniformMatrix3x2dvEXT[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::ProgramUniformMatrix3x2dvEXT", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glProgramUniformMatrix3x4dvEXT */

/* Enums to test for glProgramUniformMatrix3x4dvEXT (may not be required) */
struct enumTestRec const ea_ProgramUniformMatrix3x4dvEXT[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallProgramUniformMatrix3x4dvEXT(void)
{
    GLuint i;
    bool success = true;

    /* glProgramUniformMatrix3x4dvEXT may need to loop over a set of enums doing something with them */
    for (i = 0; ea_ProgramUniformMatrix3x4dvEXT[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::ProgramUniformMatrix3x4dvEXT", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glProgramUniformMatrix4x2dvEXT */

/* Enums to test for glProgramUniformMatrix4x2dvEXT (may not be required) */
struct enumTestRec const ea_ProgramUniformMatrix4x2dvEXT[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallProgramUniformMatrix4x2dvEXT(void)
{
    GLuint i;
    bool success = true;

    /* glProgramUniformMatrix4x2dvEXT may need to loop over a set of enums doing something with them */
    for (i = 0; ea_ProgramUniformMatrix4x2dvEXT[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::ProgramUniformMatrix4x2dvEXT", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glProgramUniformMatrix4x3dvEXT */

/* Enums to test for glProgramUniformMatrix4x3dvEXT (may not be required) */
struct enumTestRec const ea_ProgramUniformMatrix4x3dvEXT[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallProgramUniformMatrix4x3dvEXT(void)
{
    GLuint i;
    bool success = true;

    /* glProgramUniformMatrix4x3dvEXT may need to loop over a set of enums doing something with them */
    for (i = 0; ea_ProgramUniformMatrix4x3dvEXT[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::ProgramUniformMatrix4x3dvEXT", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glGetSubroutineUniformLocation */

/* Enums to test for glGetSubroutineUniformLocation (may not be required) */
struct enumTestRec const ea_GetSubroutineUniformLocation[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallGetSubroutineUniformLocation(void)
{
    GLuint i;
    bool success = true;

    /* glGetSubroutineUniformLocation may need to loop over a set of enums doing something with them */
    for (i = 0; ea_GetSubroutineUniformLocation[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::GetSubroutineUniformLocation", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glGetSubroutineIndex */

/* Enums to test for glGetSubroutineIndex (may not be required) */
struct enumTestRec const ea_GetSubroutineIndex[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallGetSubroutineIndex(void)
{
    GLuint i;
    bool success = true;

    /* glGetSubroutineIndex may need to loop over a set of enums doing something with them */
    for (i = 0; ea_GetSubroutineIndex[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::GetSubroutineIndex", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glGetActiveSubroutineUniformiv */

/* Enums to test for glGetActiveSubroutineUniformiv (may not be required) */
struct enumTestRec const ea_GetActiveSubroutineUniformiv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallGetActiveSubroutineUniformiv(void)
{
    GLuint i;
    bool success = true;

    /* glGetActiveSubroutineUniformiv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_GetActiveSubroutineUniformiv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::GetActiveSubroutineUniformiv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glGetActiveSubroutineUniformName */

/* Enums to test for glGetActiveSubroutineUniformName (may not be required) */
struct enumTestRec const ea_GetActiveSubroutineUniformName[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallGetActiveSubroutineUniformName(void)
{
    GLuint i;
    bool success = true;

    /* glGetActiveSubroutineUniformName may need to loop over a set of enums doing something with them */
    for (i = 0; ea_GetActiveSubroutineUniformName[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::GetActiveSubroutineUniformName", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glGetActiveSubroutineName */

/* Enums to test for glGetActiveSubroutineName (may not be required) */
struct enumTestRec const ea_GetActiveSubroutineName[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallGetActiveSubroutineName(void)
{
    GLuint i;
    bool success = true;

    /* glGetActiveSubroutineName may need to loop over a set of enums doing something with them */
    for (i = 0; ea_GetActiveSubroutineName[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::GetActiveSubroutineName", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glUniformSubroutinesuiv */

/* Enums to test for glUniformSubroutinesuiv (may not be required) */
struct enumTestRec const ea_UniformSubroutinesuiv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallUniformSubroutinesuiv(void)
{
    GLuint i;
    bool success = true;

    /* glUniformSubroutinesuiv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_UniformSubroutinesuiv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::UniformSubroutinesuiv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glGetUniformSubroutineuiv */

/* Enums to test for glGetUniformSubroutineuiv (may not be required) */
struct enumTestRec const ea_GetUniformSubroutineuiv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallGetUniformSubroutineuiv(void)
{
    GLuint i;
    bool success = true;

    /* glGetUniformSubroutineuiv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_GetUniformSubroutineuiv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::GetUniformSubroutineuiv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glGetProgramStageiv */

/* Enums to test for glGetProgramStageiv (may not be required) */
struct enumTestRec const ea_GetProgramStageiv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallGetProgramStageiv(void)
{
    GLuint i;
    bool success = true;

    /* glGetProgramStageiv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_GetProgramStageiv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::GetProgramStageiv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glPatchParameteri */

/* Enums to test for glPatchParameteri (may not be required) */
struct enumTestRec const ea_PatchParameteri[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallPatchParameteri(void)
{
    GLuint i;
    bool success = true;

    /* glPatchParameteri may need to loop over a set of enums doing something with them */
    for (i = 0; ea_PatchParameteri[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::PatchParameteri", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glPatchParameterfv */

/* Enums to test for glPatchParameterfv (may not be required) */
struct enumTestRec const ea_PatchParameterfv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallPatchParameterfv(void)
{
    GLuint i;
    bool success = true;

    /* glPatchParameterfv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_PatchParameterfv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::PatchParameterfv", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glDrawTransformFeedback */

/* Enums to test for glDrawTransformFeedback (may not be required) */
struct enumTestRec const ea_DrawTransformFeedback[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallDrawTransformFeedback(void)
{
    GLuint i;
    bool success = true;

    /* glDrawTransformFeedback may need to loop over a set of enums doing something with them */
    for (i = 0; ea_DrawTransformFeedback[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::DrawTransformFeedback", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glDrawTransformFeedbackStream */

/* Enums to test for glDrawTransformFeedbackStream (may not be required) */
struct enumTestRec const ea_DrawTransformFeedbackStream[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallDrawTransformFeedbackStream(void)
{
    GLuint i;
    bool success = true;

    /* glDrawTransformFeedbackStream may need to loop over a set of enums doing something with them */
    for (i = 0; ea_DrawTransformFeedbackStream[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::DrawTransformFeedbackStream", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glBeginQueryIndexed */

/* Enums to test for glBeginQueryIndexed (may not be required) */
struct enumTestRec const ea_BeginQueryIndexed[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallBeginQueryIndexed(void)
{
    GLuint i;
    bool success = true;

    /* glBeginQueryIndexed may need to loop over a set of enums doing something with them */
    for (i = 0; ea_BeginQueryIndexed[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::BeginQueryIndexed", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glEndQueryIndexed */

/* Enums to test for glEndQueryIndexed (may not be required) */
struct enumTestRec const ea_EndQueryIndexed[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallEndQueryIndexed(void)
{
    GLuint i;
    bool success = true;

    /* glEndQueryIndexed may need to loop over a set of enums doing something with them */
    for (i = 0; ea_EndQueryIndexed[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::EndQueryIndexed", "Coverage test not implemented yet");

    return success;
}

/* Coverage test for glGetQueryIndexediv */

/* Enums to test for glGetQueryIndexediv (may not be required) */
struct enumTestRec const ea_GetQueryIndexediv[] = {{"End of List", -1}};

bool ApiCoverageTestCase::TestCoverageGLCallGetQueryIndexediv(void)
{
    GLuint i;
    bool success = true;

    /* glGetQueryIndexediv may need to loop over a set of enums doing something with them */
    for (i = 0; ea_GetQueryIndexediv[i].value != -1; i++)
    {
    }

    tcu_msg("ApiCoverageTestCase::GetQueryIndexediv", "Coverage test not implemented yet");

    return success;
}

/** Constructor.
 *
 *  @param context Rendering context.
 */
ApiCoverageTests::ApiCoverageTests(deqp::Context &context)
    : TestCaseGroup(context, "api", "Verifies OpenGL API coverage functionality")
{
}

/** Initializes the test group contents. */
void ApiCoverageTests::init()
{
    addChild(new ApiCoverageTestCase(m_context));
}

} // namespace glcts
