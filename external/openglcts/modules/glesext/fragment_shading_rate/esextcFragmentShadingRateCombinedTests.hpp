#ifndef _ESEXTCFRAGMENTSHADINGRATECOMBINEDTESTS_HPP
#define _ESEXTCFRAGMENTSHADINGRATECOMBINEDTESTS_HPP
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
 */

/*!
 * \file  esextcesextcFragmentShadingRateCombinedTests.hpp
 * \brief FragmentShadingRateEXT tests for combination of conditions
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
class FragmentShadingRateCombined : public TestCaseBase
{
public:
    struct TestcaseParam
    {
        bool useShadingRateAPI;
        bool useShadingRatePrimitive;
        bool useShadingRateAttachment;
        glw::GLenum combinerOp0;
        glw::GLenum combinerOp1;
        bool msaa;
        int32_t framebufferSize;
    };

    struct Extent2D
    {
        uint32_t width;
        uint32_t height;
    };

public:
    FragmentShadingRateCombined(Context &context, const ExtParameters &extParams,
                                const FragmentShadingRateCombined::TestcaseParam &testcaseParam, const char *name,
                                const char *description);

    virtual ~FragmentShadingRateCombined()
    {
    }

    void init(void) override;
    void deinit(void) override;
    IterateResult iterate(void) override;

private:
    std::string genVS();
    std::string genFS();
    std::string genCS();
    glw::GLenum translateDrawIDToShadingRate(uint32_t drawID) const;
    glw::GLenum translatePrimIDToShadingRate(uint32_t primID) const;
    glw::GLenum translateCoordsToShadingRate(uint32_t srx, uint32_t sry) const;
    uint32_t getPrimitiveID(uint32_t drawID) const;
    uint32_t simulate(uint32_t drawID, uint32_t primID, uint32_t x, uint32_t y);
    Extent2D packedShadingRateToExtent(uint32_t packedRate) const;
    uint32_t shadingRateExtentToClampedMask(Extent2D ext, bool allowSwap) const;

    void setupTest(void);
    Extent2D combine(Extent2D extent0, Extent2D extent1, glw::GLenum combineOp) const;

private:
    TestcaseParam m_tcParam;
    glu::ShaderProgram *m_renderProgram;
    glu::ShaderProgram *m_computeProgram;
    glw::GLuint m_to_id;
    glw::GLuint m_sr_to_id;
    glw::GLuint m_fbo_id;
    glw::GLuint m_vbo_id;
    std::vector<glw::GLenum> m_availableShadingRates;
    glw::GLint m_srTexelWidth;
    glw::GLint m_srTexelHeight;
    std::vector<uint32_t> m_simulationCache;
};

} // namespace glcts

#endif // _ESEXTCFRAGMENTSHADINGRATECOMBINEDTESTS_HPP
