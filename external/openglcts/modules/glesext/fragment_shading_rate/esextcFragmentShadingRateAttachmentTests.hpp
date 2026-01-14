#ifndef _ESEXTCFRAGMENTSHADINGRATEATTACHMENTTESTS_HPP
#define _ESEXTCFRAGMENTSHADINGRATEATTACHMENTTESTS_HPP
/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2022-2022 The Khronos Group Inc.
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
 */

/*!
 * \file
 * \brief
 */ /*-------------------------------------------------------------------*/

/*!
 * \file  esextcesextcFragmentShadingRateAttachment.hpp
 * \brief FragmentShadingRateEXT attachment related tests
 */ /*-------------------------------------------------------------------*/

#include "../esextcTestCaseBase.hpp"
#include "gluShaderUtil.hpp"
#include "tcuDefs.hpp"

#include <vector>

namespace glu
{
class ShaderProgram;
}

namespace glcts
{
class FragmentShadingRateAttachment : public TestCaseBase
{
public:
    enum class TestKind
    {
        Scissor   = 0,
        MultiView = 1,
        Count,
    };

    struct TestcaseParam
    {
        TestKind testKind;
        bool attachmentShadingRate;
        bool multiShadingRate;
        uint32_t framebufferSize;
        uint32_t layerCount;
    };

    struct Box
    {
        bool in(uint32_t xIn, uint32_t yIn)
        {
            return (xIn >= x && xIn < (x + width) && yIn >= y && yIn < (y + height));
        }
        uint32_t x;
        uint32_t y;
        uint32_t width;
        uint32_t height;
    };

public:
    FragmentShadingRateAttachment(Context &context, const ExtParameters &extParams, const TestcaseParam &testcaseParam,
                                  const char *name, const char *description);

    ~FragmentShadingRateAttachment() override
    {
    }

    void init(void) override;
    void deinit(void) override;
    IterateResult iterate(void) override;

private:
    std::string genVS() const;
    std::string genFS() const;
    glw::GLenum translateCoordsToShadingRate(uint32_t srLayer, uint32_t srx, uint32_t sry) const;
    glw::GLenum translateDrawIDToShadingRate(uint32_t drawID) const;
    uint32_t drawIDToViewID(uint32_t drawID) const;

    void setupTest(void);

private:
    TestcaseParam m_tcParam;
    glu::ShaderProgram *m_program;
    glw::GLuint m_to_id;
    glw::GLuint m_sr_to_id;
    glw::GLuint m_fbo_id;
    glw::GLuint m_vao_id;
    glw::GLuint m_vbo_id;
    Box m_scissorBox;
    std::vector<glw::GLenum> m_availableShadingRates;
    glw::GLint m_srTexelWidth;
    glw::GLint m_srTexelHeight;
};

} // namespace glcts

#endif // _ESEXTCFRAGMENTSHADINGRATEATTACHMENTTESTS_HPP
