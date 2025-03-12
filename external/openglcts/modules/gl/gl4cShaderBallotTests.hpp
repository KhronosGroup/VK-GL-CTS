#ifndef _GL4CSHADERBALLOTTESTS_HPP
#define _GL4CSHADERBALLOTTESTS_HPP
/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2014-2017 The Khronos Group Inc.
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
* \file  gl4cShaderBallotTests.hpp
* \brief Conformance tests for the ARB_shader_ballot functionality.
*/ /*-------------------------------------------------------------------*/

#include "esextcTestCaseBase.hpp"
#include "glcTestCase.hpp"
#include "gluShaderProgram.hpp"

#include <map>
#include <vector>

namespace gl4cts
{
class ShaderBallotBaseTestCase : public glcts::TestCaseBase
{
public:
    class ShaderPipeline
    {
    private:
        glu::ShaderProgram *m_programRender;
        glu::ShaderProgram *m_programCompute;
        const glu::ShaderType m_testedShader;
        const uint32_t m_fileNameSuffixOffset;

        std::vector<std::string> m_shaders[glu::SHADERTYPE_LAST];
        const char **m_shaderChunks[glu::SHADERTYPE_LAST];

        std::map<std::string, std::string> m_specializationMap;

        virtual void renderQuad(deqp::Context &context);
        virtual void executeComputeShader(deqp::Context &context);

    public:
        ShaderPipeline(glu::ShaderType testedShader, const std::string &contentSnippet,
                       const std::map<std::string, std::string> &specMap = std::map<std::string, std::string>(),
                       const std::string &additionalLayout               = std::string(),
                       const std::string &additionalFunctions            = std::string(),
                       const uint32_t &fileNameSuffixOffset              = 0u);
        virtual ~ShaderPipeline();

        const char *const *getShaderParts(glu::ShaderType shaderType) const;
        unsigned int getShaderPartsCount(glu::ShaderType shaderType) const;

        void use(deqp::Context &context);

        // methods defined in the class body are treated as inline by default
        uint32_t getRenderProgram() const
        {
            DE_ASSERT(m_programRender);
            return m_programRender->getProgram();
        }

        uint32_t getComputeProgram() const
        {
            DE_ASSERT(m_programCompute);
            return m_programCompute->getProgram();
        }

        inline void setShaderPrograms(glu::ShaderProgram *programRender, glu::ShaderProgram *programCompute)
        {
            m_programRender  = programRender;
            m_programCompute = programCompute;
        }

        inline const std::map<std::string, std::string> &getSpecializationMap() const
        {
            return m_specializationMap;
        }

        glu::ShaderType getTestedShader() const
        {
            return m_testedShader;
        }

        uint32_t getFileNameSuffixOffset()
        {
            return m_fileNameSuffixOffset;
        }

        void test(deqp::Context &context);
    };

protected:
    /* Protected methods */
    void createShaderPrograms(ShaderPipeline &pipeline, const std::string &name, uint32_t index);

    /* Protected members*/
    std::vector<ShaderPipeline *> m_shaderPipelines;

    typedef std::vector<ShaderPipeline *>::iterator ShaderPipelineIter;

public:
    /* Public methods */
    ShaderBallotBaseTestCase(deqp::Context &context, const char *name, const char *description)
        : TestCaseBase(context, glcts::ExtParameters(glu::GLSL_VERSION_450, glcts::EXTENSIONTYPE_EXT), name,
                       description)
    {
    }

    virtual ~ShaderBallotBaseTestCase();

    static bool validateScreenPixels(deqp::Context &context, tcu::Vec4 desiredColor, const tcu::Vec4 &ignoredColor);
    static bool validateScreenPixelsSameColor(deqp::Context &context, const tcu::Vec4 &ignoredColor);
    static bool validateColor(tcu::Vec4 testedColor, const tcu::Vec4 &desiredColor);
};

/** Test verifies availability of new build-in features
 **/
class ShaderBallotAvailabilityTestCase : public ShaderBallotBaseTestCase
{
public:
    /* Public methods */
    ShaderBallotAvailabilityTestCase(deqp::Context &context);

    void init();

    tcu::TestNode::IterateResult iterate();
};

/** Test verifies values of gl_SubGroup*MaskARB variables
 **/
class ShaderBallotBitmasksTestCase : public ShaderBallotBaseTestCase
{
public:
    /* Public methods */
    ShaderBallotBitmasksTestCase(deqp::Context &context);

    void init();

    tcu::TestNode::IterateResult iterate();

protected:
    /* Protected members*/
    std::map<std::string, std::string> m_maskVars;

    typedef std::map<std::string, std::string>::iterator MaskVarIter;
};

/** Test verifies ballotARB calls and returned results
 **/
class ShaderBallotFunctionBallotTestCase : public ShaderBallotBaseTestCase
{
public:
    /* Public methods */
    ShaderBallotFunctionBallotTestCase(deqp::Context &context);

    void init();

    tcu::TestNode::IterateResult iterate();
};

/** Test verifies readInvocationARB and readFirstInvocationARB function calls
 **/
class ShaderBallotFunctionReadTestCase : public ShaderBallotBaseTestCase
{
public:
    typedef ShaderBallotBaseTestCase super;

    class ShaderPipeline : public super::ShaderPipeline
    {
        glw::GLuint m_buffer;

        void createAndBindBuffer(deqp::Context &context);
        void destroyBuffer(deqp::Context &context);

        virtual void renderQuad(deqp::Context &context) override;
        virtual void executeComputeShader(deqp::Context &context) override;

    public:
        ShaderPipeline(glu::ShaderType testedShader, const std::string &additionalLayout,
                       const std::string &additionalFunctions, const std::string &contentSnippet,
                       const uint32_t &fileNameSuffixOffset,
                       const std::map<std::string, std::string> &specMap = std::map<std::string, std::string>())
            : ShaderBallotBaseTestCase::ShaderPipeline(testedShader, contentSnippet, specMap, additionalLayout,
                                                       additionalFunctions, fileNameSuffixOffset)
            , m_buffer(0u)
        {
        }
    };

    /* Public methods */
    ShaderBallotFunctionReadTestCase(deqp::Context &context);

    virtual void init() override
    {
    }

    tcu::TestNode::IterateResult iterate() override;

    static inline const uint32_t readInvocationSuffix      = 0u;
    static inline const uint32_t readFirstInvocationSuffix = 10u;
};

class ShaderBallotTests : public deqp::TestCaseGroup
{
public:
    ShaderBallotTests(deqp::Context &context);
    void init(void);

private:
    ShaderBallotTests(const ShaderBallotTests &other);
    ShaderBallotTests &operator=(const ShaderBallotTests &other);
};

} // namespace gl4cts

#endif // _GL4CSHADERBALLOTTESTS_HPP
