/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2016 Google Inc.
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \brief OpenGL 3.x Test Packages.
 */ /*-------------------------------------------------------------------*/

#include "gl3cTestPackages.hpp"
#include "gl3cCommonBugsTests.hpp"
#include "gl3cGLSLnoperspectiveTests.hpp"
#include "gl3cGPUShader5Tests.hpp"
#include "gl3cTextureSizePromotion.hpp"
#include "gl3cTextureSwizzleTests.hpp"
#include "gl3cTransformFeedbackOverflowQueryTests.hpp"
#include "gl3cTransformFeedbackTests.hpp"
#include "gl3cTransformFeedback3Tests.hpp"
#include "gl4cPipelineStatisticsQueryTests.hpp"
#include "glcApiCoverageTests.hpp"
#include "glcClipDistance.hpp"
#include "glcCullDistance.hpp"
#include "glcBufferObjectsTests.hpp"
#include "glcFragDepthTests.hpp"
#include "glcFramebufferBlitTests.hpp"
#include "glcGLSLVectorConstructorTests.hpp"
#include "glcInfoTests.hpp"
#include "glcNearestEdgeTests.hpp"
#include "glcPackedDepthStencilTests.hpp"
#include "glcPackedPixelsTests.hpp"
#include "glcPixelStorageModesTests.hpp"
#include "glcShaderFunctionTests.hpp"
#include "glcShaderIndexingTests.hpp"
#include "glcShaderIntegerMixTests.hpp"
#include "glcShaderLibrary.hpp"
#include "glcShaderLoopTests.hpp"
#include "glcShaderNegativeTests.hpp"
#include "glcShaderStructTests.hpp"
#include "glcTextureLodBasicTests.hpp"
#include "glcTextureLodBiasTests.hpp"
#include "glcTextureRepeatModeTests.hpp"
#include "glcTransformFeedbackTests.hpp"
#include "glcUniformBlockTests.hpp"
#include "glcUniformBlockNegativeTests.hpp"
#include "gluStateReset.hpp"
#include "qpTestLog.h"
#include "tcuTestLog.hpp"
#include "tcuCommandLine.hpp"
#include "tcuWaiverUtil.hpp"

#include "../glesext/texture_shadow_lod/esextcTextureShadowLodFunctionsTest.hpp"

namespace gl3cts
{

TestCaseWrapper::TestCaseWrapper(deqp::TestPackage &package, de::SharedPtr<tcu::WaiverUtil> waiverMechanism)
    : m_testPackage(package)
    , m_waiverMechanism(waiverMechanism)
{
}

TestCaseWrapper::~TestCaseWrapper(void)
{
}

void TestCaseWrapper::init(tcu::TestCase *testCase, const std::string &path)
{
    if (m_waiverMechanism->isOnWaiverList(path))
        throw tcu::TestException("Waived test", QP_TEST_RESULT_WAIVER);

    testCase->init();
}

void TestCaseWrapper::deinit(tcu::TestCase *testCase)
{
    testCase->deinit();

    deqp::Context &context = m_testPackage.getContext();
    glu::resetState(context.getRenderContext(), context.getContextInfo());
}

tcu::TestNode::IterateResult TestCaseWrapper::iterate(tcu::TestCase *testCase)
{
    tcu::TestContext &testCtx     = m_testPackage.getTestContext();
    glu::RenderContext &renderCtx = m_testPackage.getContext().getRenderContext();

    // Clear to black
    {
        const glw::Functions &gl = renderCtx.getFunctions();
        gl.clearColor(0.0, 0.0f, 0.0f, 1.0f);
        gl.clear(GL_COLOR_BUFFER_BIT);
    }

    const tcu::TestCase::IterateResult result = testCase->iterate();

    {
        if (testCtx.getCommandLine().isTerminateOnDeviceLostEnabled())
        {
            const glw::Functions &gl = renderCtx.getFunctions();
            auto res                 = gl.getGraphicsResetStatus();
            if (res != GL_NO_ERROR)
            {
                testCtx.setTestResult(QP_TEST_RESULT_DEVICE_LOST, "Device Lost");
            }
        }
    }

    // Call implementation specific post-iterate routine (usually handles native events and swaps buffers)
    try
    {
        deqp::Context &context = m_testPackage.getContext();
        context.getRenderContext().postIterate();
        return result;
    }
    catch (const tcu::ResourceError &)
    {
        throw tcu::ResourceError(std::string("Resource error in context post-iteration routine"));
        testCtx.setTerminateAfter(true);
        return tcu::TestNode::STOP;
    }
    catch (const std::exception &)
    {
        testCtx.getLog().endCase(QP_TEST_RESULT_FAIL, "Error in context post-iteration routine");
        return tcu::TestNode::STOP;
    }
}

// GL30TestPackage

class GL30ShaderTests : public glcts::TestCaseGroup
{
public:
    GL30ShaderTests(deqp::Context &context) : TestCaseGroup(context, "shaders30", "Shading Language Tests")
    {
    }

    void init(void)
    {
        addChild(
            new deqp::ShaderLibraryGroup(m_context, "declarations", "Declaration Tests", "gl30/declarations.test"));
        addChild(new deqp::GLSLVectorConstructorTests(m_context, glu::GLSL_VERSION_130));
        addChild(new deqp::UniformBlockNegativeTests(m_context, glu::GLSL_VERSION_150));
    }
};

GL30TestPackage::GL30TestPackage(tcu::TestContext &testCtx, const char *packageName, const char *description,
                                 glu::ContextType renderContextType)
    : TestPackage(testCtx, packageName, packageName, renderContextType, "gl_cts/data/")
{
    (void)description;
}

GL30TestPackage::~GL30TestPackage(void)
{
}

void GL30TestPackage::init(void)
{
    // Call init() in parent - this creates context.
    TestPackage::init();

    try
    {
        addChild(new deqp::InfoTests(getContext()));
        addChild(new glcts::ClipDistance::Tests(getContext()));
        addChild(new gl3cts::GLSLnoperspectiveTests(getContext()));
        addChild(new gl3cts::TransformFeedback::Tests(getContext()));
        addChild(new gl3cts::TransformFeedback3Tests(getContext()));
        addChild(new glcts::TextureRepeatModeTests(getContext()));
        addChild(new glcts::TextureLodBasicTests(getContext()));
        addChild(new GL30ShaderTests(getContext()));
        addChild(new deqp::Functional::TextureShadowLodTest(getContext()));
        addChild(new glcts::FramebufferBlitTests(getContext()));
        addChild(new glcts::TextureLodBiasTests(getContext()));
        addChild(new glcts::BufferObjectsTests(getContext()));
        addChild(new glcts::TransformFeedbackTests(getContext()));
        addChild(new glcts::ApiCoverageTests(getContext()));
    }
    catch (...)
    {
        // Destroy context.
        TestPackage::deinit();
        throw;
    }
}

tcu::TestCaseExecutor *GL30TestPackage::createExecutor(void) const
{
    return new TestCaseWrapper(const_cast<GL30TestPackage &>(*this), m_waiverMechanism);
}

// GL31TestPackage

GL31TestPackage::GL31TestPackage(tcu::TestContext &testCtx, const char *packageName, const char *description,
                                 glu::ContextType renderContextType)
    : GL30TestPackage(testCtx, packageName, packageName, renderContextType)
{
    (void)description;
}

GL31TestPackage::~GL31TestPackage(void)
{
}

void GL31TestPackage::init(void)
{
    // Call init() in parent - this creates context.
    GL30TestPackage::init();

    try
    {
        addChild(new gl3cts::CommonBugsTests(getContext()));
        addChild(new gl3cts::TextureSizePromotion::Tests(getContext()));
    }
    catch (...)
    {
        // Destroy context.
        TestPackage::deinit();
        throw;
    }
}

// GL32TestPackage

GL32TestPackage::GL32TestPackage(tcu::TestContext &testCtx, const char *packageName, const char *description,
                                 glu::ContextType renderContextType)
    : GL31TestPackage(testCtx, packageName, packageName, renderContextType)
{
    (void)description;
}

GL32TestPackage::~GL32TestPackage(void)
{
}

void GL32TestPackage::init(void)
{
    // Call init() in parent - this creates context.
    GL31TestPackage::init();

    try
    {
        addChild(new gl3cts::GPUShader5Tests(getContext()));
        addChild(new gl3cts::TransformFeedbackOverflowQueryTests(
            getContext(), gl3cts::TransformFeedbackOverflowQueryTests::API_GL_ARB_transform_feedback_overflow_query));
        addChild(new glcts::PackedPixelsTests(getContext()));
        addChild(new glcts::PackedDepthStencilTests(getContext()));
    }
    catch (...)
    {
        // Destroy context.
        TestPackage::deinit();
        throw;
    }
}

// OpenGL 3.3 test groups

class GL33ShaderTests : public glcts::TestCaseGroup
{
public:
    GL33ShaderTests(deqp::Context &context) : TestCaseGroup(context, "shaders", "Shading Language Tests")
    {
    }

    void init(void)
    {
        addChild(new deqp::ShaderLibraryGroup(m_context, "arrays", "Array Tests", "gl33/arrays.test"));
        addChild(
            new deqp::ShaderLibraryGroup(m_context, "declarations", "Declaration Tests", "gl33/declarations.test"));
        addChild(new deqp::FragDepthTests(m_context, glu::GLSL_VERSION_330));
        addChild(new deqp::ShaderIndexingTests(m_context, glu::GLSL_VERSION_330));
        addChild(new deqp::ShaderLoopTests(m_context, glu::GLSL_VERSION_330));
        addChild(
            new deqp::ShaderLibraryGroup(m_context, "preprocessor", "Preprocessor Tests", "gl33/preprocessor.test"));
        addChild(new deqp::ShaderFunctionTests(m_context, glu::GLSL_VERSION_330));
        addChild(new deqp::ShaderStructTests(m_context, glu::GLSL_VERSION_330));
        addChild(new deqp::UniformBlockTests(m_context, glu::GLSL_VERSION_330));
        addChild(new deqp::ShaderIntegerMixTests(m_context, glu::GLSL_VERSION_330));
        addChild(new deqp::ShaderNegativeTests(m_context, glu::GLSL_VERSION_330));
    }
};

// GL33TestPackage

GL33TestPackage::GL33TestPackage(tcu::TestContext &testCtx, const char *packageName, const char *description,
                                 glu::ContextType renderContextType)
    : GL32TestPackage(testCtx, packageName, packageName, renderContextType)
{
    (void)description;
}

GL33TestPackage::~GL33TestPackage(void)
{
}

void GL33TestPackage::init(void)
{
    // Call init() in parent - this creates context.
    GL32TestPackage::init();

    try
    {
        addChild(new GL33ShaderTests(getContext()));
        addChild(new glcts::PipelineStatisticsQueryTests(getContext()));
        addChild(new glcts::CullDistance::Tests(getContext()));
        addChild(new gl3cts::TextureSwizzleTests(getContext()));
        addChild(new glcts::NearestEdgeCases(getContext()));
        addChild(new glcts::PixelStorageModesTests(getContext(), glu::GLSL_VERSION_330));
    }
    catch (...)
    {
        // Destroy context.
        TestPackage::deinit();
        throw;
    }
}

} // namespace gl3cts
