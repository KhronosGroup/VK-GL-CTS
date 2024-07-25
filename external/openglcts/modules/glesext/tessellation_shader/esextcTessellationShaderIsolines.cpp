/*-------------------------------------------------------------------------
 * OpenGL Conformance Test Suite
 * -----------------------------
 *
 * Copyright (c) 2014-2016 The Khronos Group Inc.
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

#include "esextcTessellationShaderIsolines.hpp"
#include "esextcTessellationShaderUtils.hpp"
#include "gluContextInfo.hpp"
#include "gluDefs.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "tcuTestLog.hpp"

namespace glcts
{

/** Constructor
 *
 * @param context Test context
 **/
TessellationShadersIsolines::TessellationShadersIsolines(Context &context, const ExtParameters &extParams)
    : TestCaseBase(context, extParams, "isolines_tessellation",
                   "Verifies that the number of isolines generated during tessellation is "
                   "derived from the first outer tessellation level.\n"
                   "Makes sure that the number of segments in each isoline is derived from "
                   "the second outer tessellation level.\n"
                   "Makes sure that both inner tessellation levels and the 3rd and the 4th "
                   "outer tessellation levels do not affect the tessellation process.\n"
                   "Makes sure that equal_spacing vertex spacing mode does not affect amount"
                   " of generated isolines.\n"
                   "Makes sure no line is drawn between (0, 1) and (1, 1) in (u, v) domain.")
    , m_irrelevant_tess_value_1(0.0f)
    , m_irrelevant_tess_value_2(0.0f)
    , m_utils_ptr(DE_NULL)
    , m_vao_id(0)
{
    /* Left blank on purpose */
}

/** Checks that amount of isolines generated during tessellation corresponds to the
 *  first outer tessellation level.
 *
 *  This check needs not to operate over all test results generated for a particular
 *  vertex spacing mode.
 *
 *  @param test_result Value of MAX_TESS_GEN_LEVEL token. For ES3.1 it will be equal to
 *                     GL_MAX_TESS_GEN_LEVEL_EXT and for ES3.2 to GL_MAX_TESS_GEN_LEVEL.
 *
 **/
void TessellationShadersIsolines::checkFirstOuterTessellationLevelEffect(_test_result &test_result,
                                                                         const glw::GLenum glMaxTessGenLevelToken)
{
    glcts::Context &context                = test_result.parent->parent->getContext();
    const glw::Functions &gl               = context.getRenderContext().getFunctions();
    glw::GLint gl_max_tess_gen_level_value = 0;
    unsigned int n_isolines_expected       = 0;

    if (test_result.n_vertices != 0)
    {
        /* Calculate how many isolines we're expecting */
        gl.getIntegerv(glMaxTessGenLevelToken, &gl_max_tess_gen_level_value);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glGetIntegerv() failed for GL_MAX_TESS_GEN_LEVEL_EXT pname");

        /* NOTE: Amount of isolines should always be based on TESSELLATION_SHADER_VERTEX_SPACING_EQUAL
         *       vertex spacing mode, even if a different one is defined in TE stage.
         */
        float outer_zero_tess_level_clamped_rounded = 0.0f;

        TessellationShaderUtils::getTessellationLevelAfterVertexSpacing(
            TESSELLATION_SHADER_VERTEX_SPACING_EQUAL, test_result.parent->outer_tess_levels[0],
            gl_max_tess_gen_level_value, DE_NULL, /* out_clamped */
            &outer_zero_tess_level_clamped_rounded);

        n_isolines_expected = (unsigned int)outer_zero_tess_level_clamped_rounded;

        if (test_result.n_isolines != n_isolines_expected)
        {
            tcu::TestContext &test = test_result.parent->parent->getTestContext();

            test.getLog() << tcu::TestLog::Message
                          << "Tessellator generated an invalid amount of isolines:" << test_result.n_isolines
                          << " instead of the expected amount:" << n_isolines_expected
                          << " for the following inner tessellation level configuration:"
                          << " (" << test_result.parent->inner_tess_levels[0] << ", "
                          << test_result.parent->inner_tess_levels[1] << ")"
                          << " and the following outer tesellation level configuration:"
                          << " (" << test_result.parent->outer_tess_levels[0] << ", "
                          << test_result.parent->outer_tess_levels[1] << ", "
                          << test_result.parent->outer_tess_levels[2] << ", "
                          << test_result.parent->outer_tess_levels[3] << ")" << tcu::TestLog::EndMessage;

            TCU_FAIL("Invalid amount of isolines generated by tessellator");
        }
    } /* if (test_run.n_vertices != 0) */
}

/** Makes sure that tessellation coordinates generated for inner+outer tessellation level
 *  configurations, between which irrelevant levels have been defined, are exactly the same.
 *
 *  This check needs to operate over all test results generated for a particular
 *  vertex spacing mode.
 *
 *  This function throws a TestError exception if the check fails.
 **/
void TessellationShadersIsolines::checkIrrelevantTessellationLevelsHaveNoEffect()
{
    /* Make sure that two example data sets, for which irrelevant tessellation levels have
     * been changed, are exactly the same
     */
    DE_ASSERT(m_test_results.find(TESSELLATION_SHADER_VERTEX_SPACING_EQUAL) != m_test_results.end());

    const float epsilon                          = 1e-5f;
    float irrelevant_tess_level1_rounded_clamped = 0.0f;
    int irrelevant_tess_level1                   = 0;
    float irrelevant_tess_level2_rounded_clamped = 0.0f;
    int irrelevant_tess_level2                   = 0;
    _test_results_iterator test_result_iterator_start =
        m_test_results[TESSELLATION_SHADER_VERTEX_SPACING_EQUAL].begin();
    _test_results_iterator test_result_iterator_end = m_test_results[TESSELLATION_SHADER_VERTEX_SPACING_EQUAL].end();

    /* Calculate two tessellation level values that we've used in init() */
    const glw::Functions &gl               = m_context.getRenderContext().getFunctions();
    glw::GLint gl_max_tess_gen_level_value = 0;

    gl.getIntegerv(m_glExtTokens.MAX_TESS_GEN_LEVEL, &gl_max_tess_gen_level_value);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGetIntegerv() failed for GL_MAX_TESS_GEN_LEVEL_EXT pname");

    TessellationShaderUtils::getTessellationLevelAfterVertexSpacing(
        TESSELLATION_SHADER_VERTEX_SPACING_EQUAL, m_irrelevant_tess_value_1, gl_max_tess_gen_level_value,
        DE_NULL, /* out_clamped */
        &irrelevant_tess_level1_rounded_clamped);
    TessellationShaderUtils::getTessellationLevelAfterVertexSpacing(
        TESSELLATION_SHADER_VERTEX_SPACING_EQUAL, m_irrelevant_tess_value_2, gl_max_tess_gen_level_value,
        DE_NULL, /* out_clamped */
        &irrelevant_tess_level2_rounded_clamped);

    irrelevant_tess_level1 = (int)irrelevant_tess_level1_rounded_clamped;
    irrelevant_tess_level2 = (int)irrelevant_tess_level2_rounded_clamped;

    DE_ASSERT(de::abs(irrelevant_tess_level1 - irrelevant_tess_level2) > 0);

    /* Iterate through all test runs for equal spacing */
    for (_test_results_iterator test_result_iterator = test_result_iterator_start;
         test_result_iterator != test_result_iterator_end; test_result_iterator++)
    {
        _test_result test_result = *test_result_iterator;

        if (test_result.irrelevant_tess_level == irrelevant_tess_level1)
        {
            _test_result test_result_reference =
                findTestResult(irrelevant_tess_level2, test_result.outer1_tess_level, test_result.outer2_tess_level,
                               TESSELLATION_SHADER_VERTEX_SPACING_EQUAL);

            /* data for current test run and the reference one should match */
            DE_ASSERT(test_result.n_vertices == test_result_reference.n_vertices);

            for (unsigned int n_vertex = 0; n_vertex < test_result.n_vertices; ++n_vertex)
            {
                const float *vertex_data_1 = (&test_result.rendered_data[0]) + n_vertex * 3;           /* components */
                const float *vertex_data_2 = (&test_result_reference.rendered_data[0]) + n_vertex * 3; /* components */

                if (de::abs(vertex_data_1[0] - vertex_data_2[0]) > epsilon ||
                    de::abs(vertex_data_1[1] - vertex_data_2[1]) > epsilon ||
                    de::abs(vertex_data_1[2] - vertex_data_2[2]) > epsilon)
                {
                    tcu::TestContext &test = test_result_iterator->parent->parent->getTestContext();

                    test.getLog()
                        << tcu::TestLog::Message
                        << "Tessellator generated non-matching data for different tessellation level configurations, "
                           "where only irrelevant tessellation levels have been changed; "
                        << " data generated for {inner:"
                        << " (" << test_result.parent->inner_tess_levels[0] << ", "
                        << test_result.parent->inner_tess_levels[1] << ")"
                        << " outer:"
                        << " (" << test_result.parent->outer_tess_levels[0] << ", "
                        << test_result.parent->outer_tess_levels[1] << ", " << test_result.parent->outer_tess_levels[2]
                        << ", " << test_result.parent->outer_tess_levels[3] << ")"
                        << "}:"
                        << " (" << vertex_data_1[0] << ", " << vertex_data_1[1] << ", " << vertex_data_1[2] << ")"
                        << ", data generated for {inner:"
                        << " (" << test_result_reference.parent->inner_tess_levels[0] << ", "
                        << test_result_reference.parent->inner_tess_levels[1] << ")"
                        << " outer:"
                        << " (" << test_result_reference.parent->outer_tess_levels[0] << ", "
                        << test_result_reference.parent->outer_tess_levels[1] << ", "
                        << test_result_reference.parent->outer_tess_levels[2] << ", "
                        << test_result_reference.parent->outer_tess_levels[3] << ")"
                        << "}:"
                        << " (" << vertex_data_2[0] << ", " << vertex_data_2[1] << ", " << vertex_data_2[2] << ")"
                        << tcu::TestLog::EndMessage;

                    TCU_FAIL("Invalid amount of unique line segments generated by tessellator");
                } /* if (equal and fractional_even data mismatch) */
            }     /* for (all vertices) */
        }         /* if (current test result's irrelelvant tessellation levels match what we're after) */
    }             /* for (all test runs) */
}

/** Checks that the amount of line segments generated per isoline is as defined by
 *  second outer tessellation level.
 *
 *  This check needs not to operate over all test results generated for a particular
 *  vertex spacing mode.
 *
 *  This function throws a TestError exception if the check fails.
 *
 *  @param test_result Test result descriptor to perform the check on.
 *
 **/
void TessellationShadersIsolines::checkSecondOuterTessellationLevelEffect(_test_result &test_result,
                                                                          const glw::GLenum glMaxTessGenLevelToken)
{
    typedef float _line_segment_x;
    typedef std::pair<_line_segment_x, _line_segment_x> _line_segment;
    typedef std::vector<_line_segment> _line_segments;
    typedef _line_segments::iterator _line_segments_iterator;

    glcts::Context &context = test_result.parent->parent->getContext();
    const float epsilon     = 1e-5f;
    _line_segments found_line_segments;
    const glw::Functions &gl                          = context.getRenderContext().getFunctions();
    glw::GLint gl_max_tess_gen_level_value            = 0;
    float outer_tess_levels1_clamped_rounded          = 0.0f;
    unsigned int n_line_segments_per_isoline_expected = 0;
    unsigned int n_unique_line_segments_found         = 0;

    if (test_result.n_vertices != 0)
    {
        /* Calculate how many isolines we're expecting */
        gl.getIntegerv(glMaxTessGenLevelToken, &gl_max_tess_gen_level_value);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glGetIntegerv() failed for GL_MAX_TESS_GEN_LEVEL_EXT pname");

        TessellationShaderUtils::getTessellationLevelAfterVertexSpacing(
            test_result.parent->vertex_spacing_mode, test_result.parent->outer_tess_levels[1],
            gl_max_tess_gen_level_value, DE_NULL, /* out_clamped */
            &outer_tess_levels1_clamped_rounded);

        n_line_segments_per_isoline_expected = (unsigned int)outer_tess_levels1_clamped_rounded;

        /* Count unique line segments found in all the line segments making up the result data set.  */
        for (unsigned int n_vertex = 0; n_vertex < test_result.n_vertices;
             n_vertex += 2 /* vertices per line segment */)
        {
            bool was_line_segment_found = false;
            const float *vertex1        = (&test_result.rendered_data[0]) + n_vertex * 3; /* components */
            float vertex1_x             = vertex1[0];
            const float *vertex2        = (&test_result.rendered_data[0]) + (n_vertex + 1) * 3; /* components */
            float vertex2_x             = vertex2[0];

            for (_line_segments_iterator found_line_segments_iterator = found_line_segments.begin();
                 found_line_segments_iterator != found_line_segments.end(); found_line_segments_iterator++)
            {
                float &found_vertex1_x = found_line_segments_iterator->first;
                float &found_vertex2_x = found_line_segments_iterator->second;

                if (de::abs(found_vertex1_x - vertex1_x) < epsilon && de::abs(found_vertex2_x - vertex2_x) < epsilon)
                {
                    was_line_segment_found = true;

                    break;
                }
            } /* for (all found Ys) */

            if (!was_line_segment_found)
            {
                found_line_segments.push_back(_line_segment(vertex1_x, vertex2_x));
            }
        } /* for (all vertices) */

        /* Compare the values */
        n_unique_line_segments_found = (unsigned int)found_line_segments.size();

        if (n_unique_line_segments_found != n_line_segments_per_isoline_expected)
        {
            tcu::TestContext &test = test_result.parent->parent->getTestContext();

            test.getLog() << tcu::TestLog::Message << "Tessellator generated an invalid amount of unique line segments:"
                          << n_unique_line_segments_found
                          << " instead of the expected amount:" << n_line_segments_per_isoline_expected
                          << " for the following inner tessellation level configuration:"
                          << " (" << test_result.parent->inner_tess_levels[0] << ", "
                          << test_result.parent->inner_tess_levels[1] << ")"
                          << " and the following outer tesellation level configuration:"
                          << " (" << test_result.parent->outer_tess_levels[0] << ", "
                          << test_result.parent->outer_tess_levels[1] << ", "
                          << test_result.parent->outer_tess_levels[2] << ", "
                          << test_result.parent->outer_tess_levels[3] << ")"
                          << " and the following vertex spacing mode: " << test_result.parent->vertex_spacing_mode
                          << tcu::TestLog::EndMessage;

            TCU_FAIL("Invalid amount of unique line segments generated by tessellator");
        }
    } /* if (test_run.n_vertices != 0) */
}

/** Verifies that no vertex making up any of the line segments outputted by the
 *  tessellator is located at height equal to -1.
 *
 *  This check needs not to operate over all test results generated for a particular
 *  vertex spacing mode.
 *
 *  This function throws a TestError exception if the check fails.
 *
 *  @param test_result Test result descriptor to perform the check on.
 *
 **/
void TessellationShadersIsolines::checkNoLineSegmentIsDefinedAtHeightOne(_test_result &test_result, glw::GLenum unused)
{
    (void)unused; // suppress warning

    const float epsilon = 1e-5f;

    for (unsigned int n_vertex = 0; n_vertex < test_result.n_vertices; ++n_vertex)
    {
        const float *vertex = (&test_result.rendered_data[0]) + n_vertex * 3; /* components */

        if (de::abs(vertex[1] - 1.0f) < epsilon)
        {
            tcu::TestContext &test = test_result.parent->parent->getTestContext();

            test.getLog() << tcu::TestLog::Message << "Tessellator generated the following coordinate:"
                          << " (" << vertex[0] << ", " << vertex[1] << ", " << vertex[2] << ")"
                          << " for the following inner tessellation level configuration:"
                          << " (" << test_result.parent->inner_tess_levels[0] << ", "
                          << test_result.parent->inner_tess_levels[1] << ")"
                          << " and the following outer tesellation level configuration:"
                          << " (" << test_result.parent->outer_tess_levels[0] << ", "
                          << test_result.parent->outer_tess_levels[1] << ", "
                          << test_result.parent->outer_tess_levels[2] << ", "
                          << test_result.parent->outer_tess_levels[3] << ")"
                          << " which is invalid: Y must never be equal to 1." << tcu::TestLog::EndMessage;

            TCU_FAIL("Invalid line segment generated by tessellator");
        } /* If the Y coordinate is set at 1 */
    }     /* for (all vertices) */
}

/** Verifies that amount of isolines generated for the same inner+outer level
 *  configurations but for different vertex spacing modes is exactly the same.
 *
 *  This check needs to operate over all test results generated for a particular
 *  vertex spacing mode.
 *
 *  This function throws a TestError exception if the check fails.
 *
 **/
void TessellationShadersIsolines::checkVertexSpacingDoesNotAffectAmountOfGeneratedIsolines()
{
    DE_ASSERT(m_tests.find(TESSELLATION_SHADER_VERTEX_SPACING_EQUAL) != m_tests.end());

    _test_results_iterator test_result_iterator_start =
        m_test_results[TESSELLATION_SHADER_VERTEX_SPACING_EQUAL].begin();
    _test_results_iterator test_result_iterator_end = m_test_results[TESSELLATION_SHADER_VERTEX_SPACING_EQUAL].end();

    for (_test_results_iterator test_result_iterator = test_result_iterator_start;
         test_result_iterator != test_result_iterator_end; test_result_iterator++)
    {
        _test_result &test_result_equal = *test_result_iterator;
        _test_result test_result_fe;
        _test_result test_result_fo;

        /* Find a corresponding fractional_even test run descriptor */
        test_result_fe =
            findTestResult(test_result_equal.irrelevant_tess_level, test_result_equal.outer1_tess_level,
                           test_result_equal.outer2_tess_level, TESSELLATION_SHADER_VERTEX_SPACING_FRACTIONAL_EVEN);
        test_result_fo =
            findTestResult(test_result_equal.irrelevant_tess_level, test_result_equal.outer1_tess_level,
                           test_result_equal.outer2_tess_level, TESSELLATION_SHADER_VERTEX_SPACING_FRACTIONAL_ODD);

        /* Make sure the amounts match */
        if (test_result_equal.n_isolines != test_result_fe.n_isolines ||
            test_result_fe.n_isolines != test_result_fo.n_isolines)
        {
            tcu::TestContext &test = test_result_iterator->parent->parent->getTestContext();

            test.getLog() << tcu::TestLog::Message
                          << "Tessellator generated different amount of isolines for EQUAL/"
                             "FRACTIONAL_EVEN/FRACTIONAL_ODD vertex spacing modes which is "
                             "invalid."
                          << tcu::TestLog::EndMessage;

            TCU_FAIL("Invalid amount of unique isolines generated by tessellator");
        } /* if (amount of generated isolines does not match) */
    }     /* for (all test runs) */
}

/** Counts amount of unique isolines in the captured data set and updates
 *  n_isolines field of user-provided @param test_result instance.
 *
 *  @param test_result Test result instance to update.
 */
void TessellationShadersIsolines::countIsolines(_test_result &test_result)
{
    const float epsilon = 1e-5f;
    std::vector<float> found_ys;

    for (unsigned int n_vertex = 0; n_vertex < test_result.n_vertices; ++n_vertex)
    {
        bool was_y_found    = false;
        const float *vertex = (&test_result.rendered_data[0]) + n_vertex * 3; /* components */
        float vertex_y      = vertex[1];

        for (std::vector<float>::iterator found_ys_iterator = found_ys.begin(); found_ys_iterator != found_ys.end();
             found_ys_iterator++)
        {
            float &found_y = *found_ys_iterator;

            if (de::abs(vertex_y - found_y) < epsilon)
            {
                was_y_found = true;

                break;
            }
        } /* for (all found Ys) */

        if (!was_y_found)
        {
            found_ys.push_back(vertex_y);
        }
    } /* for (all vertices) */

    /* Store the value */
    test_result.n_isolines = (unsigned int)found_ys.size();
}

/** Deinitializes ES objects created for the test. */
void TessellationShadersIsolines::deinit()
{
    /* Call base class' deinit() */
    TestCaseBase::deinit();

    if (!m_is_tessellation_shader_supported)
    {
        return;
    }

    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    /* Reset GL_PATCH_VERTICES_EXT to default value */
    gl.patchParameteri(m_glExtTokens.PATCH_VERTICES, 3);

    /* Disable GL_RASTERIZER_DISCARD mode */
    gl.disable(GL_RASTERIZER_DISCARD);

    /* Unbind vertex array object */
    gl.bindVertexArray(0);

    /* Release Utilities instance */
    if (m_utils_ptr != NULL)
    {
        delete m_utils_ptr;

        m_utils_ptr = DE_NULL;
    }

    if (m_vao_id != 0)
    {
        gl.deleteVertexArrays(1, &m_vao_id);

        m_vao_id = 0;
    }

    /* Free the data structures we allocated for the test */
    m_tests.clear();
}

/** Retrieves test result structure for a particular set of properties.
 *
 *  @param irrelevant_tess_level Irrelevant tessellation level the test result descriptor should be using.
 *  @param outer1_tess_level     First outer tessellation level value  the test result descriptor should be using.
 *  @param outer2_tess_level     Second outer tessellation level value the test result descriptor should be using.
 *  @param vertex_spacing_mode   Vertex spacing mode the test result descriptor should be using.
 *
 *  This function throws a TestError exception if the test result descriptor the caller is after is not found.
 *
 *  @return Test result descriptor of interest.
 **/
TessellationShadersIsolines::_test_result TessellationShadersIsolines::findTestResult(
    _irrelevant_tess_level irrelevant_tess_level, _outer1_tess_level outer1_tess_level,
    _outer2_tess_level outer2_tess_level, _tessellation_shader_vertex_spacing vertex_spacing_mode)
{
    DE_ASSERT(m_tests.find(vertex_spacing_mode) != m_tests.end());

    _test_results &test_results = m_test_results[vertex_spacing_mode];
    bool has_found              = false;
    TessellationShadersIsolines::_test_result result;

    for (_test_results_iterator test_results_iterator = test_results.begin();
         test_results_iterator != test_results.end(); test_results_iterator++)
    {
        if (test_results_iterator->irrelevant_tess_level == irrelevant_tess_level &&
            test_results_iterator->outer1_tess_level == outer1_tess_level &&
            test_results_iterator->outer2_tess_level == outer2_tess_level)
        {
            has_found = true;
            result    = *test_results_iterator;

            break;
        }
    } /* for (all test runs) */

    if (!has_found)
    {
        TCU_FAIL("Requested test run was not found.");
    }

    return result;
}

/** Retrieves rendering context associated with the test instance.
 *
 *  @return Rendering context.
 *
 **/
Context &TessellationShadersIsolines::getContext()
{
    return m_context;
}

/** Initializes ES objects necessary to run the test. */
void TessellationShadersIsolines::initTest()
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    /* Skip if required extensions are not supported. */
    if (!m_is_tessellation_shader_supported)
    {
        throw tcu::NotSupportedError(TESSELLATION_SHADER_EXTENSION_NOT_SUPPORTED);
    }

    /* Generate Utilities instance */
    m_utils_ptr = new TessellationShaderUtils(gl, this);

    /* Set up vertex array object */
    gl.genVertexArrays(1, &m_vao_id);
    GLU_EXPECT_NO_ERROR(gl.getError(), "Could not generate vertex array object");

    gl.bindVertexArray(m_vao_id);
    GLU_EXPECT_NO_ERROR(gl.getError(), "Error binding vertex array object!");

    /* Retrieve GL_MAX_TESS_GEN_LEVEL_EXT value */
    glw::GLint gl_max_tess_gen_level_value = 0;

    gl.getIntegerv(m_glExtTokens.MAX_TESS_GEN_LEVEL, &gl_max_tess_gen_level_value);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGetIntegerv() failed for GL_MAX_TESS_GEN_LEVEL_EXT pname");

    /* Initialize reference tessellation values */
    const glw::GLfloat tess_levels[] = {-1.0f, 4.0f, float(gl_max_tess_gen_level_value) * 0.5f,
                                        float(gl_max_tess_gen_level_value)};
    const unsigned int n_tess_levels = sizeof(tess_levels) / sizeof(tess_levels[0]);

    m_irrelevant_tess_value_1 = tess_levels[0];
    m_irrelevant_tess_value_2 = tess_levels[1];

    /* Initialize all test passes.
     *
     * Make sure each relevant outer tessellation level iterates through values
     * of our interest
     */
    for (unsigned int outer1_tess_level_index = 0; outer1_tess_level_index < n_tess_levels; ++outer1_tess_level_index)
    {
        for (unsigned int outer2_tess_level_index = 0; outer2_tess_level_index < n_tess_levels;
             ++outer2_tess_level_index)
        {
            /* To make the test execute in a reasonable time frame, just use
             * two different levels for the outer tessellation levels */
            DE_STATIC_ASSERT(n_tess_levels >= 2);

            for (unsigned int other_tess_level_index = 0; other_tess_level_index < 2 /* see comment */;
                 ++other_tess_level_index)
            {
                float inner_tess_levels[2] = {tess_levels[other_tess_level_index], tess_levels[other_tess_level_index]};
                float outer_tess_levels[4] = {tess_levels[outer1_tess_level_index],
                                              tess_levels[outer2_tess_level_index], tess_levels[other_tess_level_index],
                                              tess_levels[other_tess_level_index]};

                /* Finally, iterate over three vertex spacing modes */
                _tessellation_shader_vertex_spacing vertex_spacing_mode;

                const _tessellation_shader_vertex_spacing vs_modes[] = {
                    TESSELLATION_SHADER_VERTEX_SPACING_EQUAL, TESSELLATION_SHADER_VERTEX_SPACING_FRACTIONAL_EVEN,
                    TESSELLATION_SHADER_VERTEX_SPACING_FRACTIONAL_ODD};
                const int n_vs_modes = sizeof(vs_modes) / sizeof(vs_modes[0]);

                for (int n_vs_mode = 0; n_vs_mode < n_vs_modes; ++n_vs_mode)
                {
                    vertex_spacing_mode = vs_modes[n_vs_mode];

                    _test_descriptor test;

                    initTestDescriptor(vertex_spacing_mode, inner_tess_levels, outer_tess_levels,
                                       tess_levels[other_tess_level_index], test);

                    m_tests[vertex_spacing_mode].push_back(test);
                } /* for (all available vertex spacing modes) */
            }     /* for (all irrelevant tessellation levels) */
        }         /* for (all defined second outer tessellation levels) */
    }             /* for (all defined first outer tessellation levels) */
}

/** Initializes all ES objects necessary to run a specific test pass.
 *
 *  @param vertex_spacing        Vertex spacing mode to initialize the test descriptor with.
 *  @param inner_tess_levels     Two FP values defining subsequent inner tessellation levels
 *                               to be used for initializing the test descriptor. Must NOT be
 *                               NULL.
 *  @param outer_tess_levels     Four FP values defining subsequent outer tessellation levels
 *                               to be used for initializing the test descriptor. Must NOT be
 *                               NULL.
 *  @param irrelevant_tess_level Value to be used to set irrelevant tessellation level values.
 *  @param test                  Test descriptor to fill with IDs of initialized objects.
 **/
void TessellationShadersIsolines::initTestDescriptor(_tessellation_shader_vertex_spacing vertex_spacing,
                                                     const float *inner_tess_levels, const float *outer_tess_levels,
                                                     float irrelevant_tess_level, _test_descriptor &test)
{
    memcpy(test.inner_tess_levels, inner_tess_levels, sizeof(test.inner_tess_levels));
    memcpy(test.outer_tess_levels, outer_tess_levels, sizeof(test.outer_tess_levels));

    test.parent                = this;
    test.irrelevant_tess_level = irrelevant_tess_level;
    test.vertex_spacing_mode   = vertex_spacing;
}

/** Executes the test.
 *
 *  Sets the test result to QP_TEST_RESULT_FAIL if the test failed, QP_TEST_RESULT_PASS otherwise.
 *
 *  Note the function throws exception should an error occur!
 *
 *  @return STOP if the test has finished, CONTINUE to indicate iterate() should be called once again.
 **/
tcu::TestNode::IterateResult TessellationShadersIsolines::iterate(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();

    initTest();

    /* We only need to use one vertex per so go for it */
    gl.patchParameteri(m_glExtTokens.PATCH_VERTICES, 1);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glPatchParameteriEXT() failed for GL_PATCH_VERTICES_EXT pname");

    /* We don't need to rasterize anything in this test */
    gl.enable(GL_RASTERIZER_DISCARD);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glEnable(GL_RASTERIZER_DISCARD) failed");

    /* Retrieve GL_MAX_TESS_GEN_LEVEL_EXT value before we continue */
    glw::GLint gl_max_tess_gen_level_value = 0;

    gl.getIntegerv(m_glExtTokens.MAX_TESS_GEN_LEVEL, &gl_max_tess_gen_level_value);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGetIntegerv() failed for GL_MAX_TESS_GEN_LEVEL_EXT pname.");

    /* To perform actual tests, we need to first retrieve the tessellated coordinates data.
     * Run all tests configured and fill per-test buffer with the information.
     **/
    for (_tests_per_vertex_spacing_map_iterator vs_key_iterator = m_tests.begin(); vs_key_iterator != m_tests.end();
         vs_key_iterator++)
    {
        for (_tests_const_iterator test_iterator = vs_key_iterator->second.begin();
             test_iterator != vs_key_iterator->second.end(); test_iterator++)
        {
            const _test_descriptor &test = *test_iterator;

            /* Capture tessellation data for considered configuration */
            unsigned int n_rendered_vertices = m_utils_ptr->getAmountOfVerticesGeneratedByTessellator(
                TESSELLATION_SHADER_PRIMITIVE_MODE_ISOLINES, test.inner_tess_levels, test.outer_tess_levels,
                test.vertex_spacing_mode, false); /* is_point_mode_enabled */
            std::vector<char> rendered_data = m_utils_ptr->getDataGeneratedByTessellator(
                test.inner_tess_levels, false, /* point mode */
                TESSELLATION_SHADER_PRIMITIVE_MODE_ISOLINES, TESSELLATION_SHADER_VERTEX_ORDERING_CCW,
                test.vertex_spacing_mode, test.outer_tess_levels);

            /* Store the data in a test result descriptor */
            _test_result result;

            result.n_vertices            = n_rendered_vertices;
            result.parent                = &test;
            result.irrelevant_tess_level = (int)test.irrelevant_tess_level;
            result.outer1_tess_level     = (int)test.outer_tess_levels[0];
            result.outer2_tess_level     = (int)test.outer_tess_levels[1];
            result.rendered_data.resize(rendered_data.size() / sizeof(float));
            if (0 != rendered_data.size())
            {
                memcpy(&result.rendered_data[0], &rendered_data[0], rendered_data.size());
            }
            if (result.rendered_data.size() > 0)
            {
                countIsolines(result);
            }

            /* Store the test run descriptor. */
            m_test_results[test.vertex_spacing_mode].push_back(result);
        }
    }

    /* Now we can proceed with actual tests */
    /* (test 1): Make sure amount of isolines is determined by first outer tessellation level */
    runForAllTestResults(checkFirstOuterTessellationLevelEffect);

    /* (test 2): Make sure amount of line segments per height is determined by second outer
     *           tessellation level.
     */
    runForAllTestResults(checkSecondOuterTessellationLevelEffect);

    /* (test 3): Make sure 3rd, 4th outer tessellation levels, as well as all inner tessellation
     *           levels have no impact on the tessellated coordinates */
    checkIrrelevantTessellationLevelsHaveNoEffect();

    /* (test 4): Make sure no matter what vertex spacing is requested in TC stage, it is always
     *           equal_spacing that is applied.
     */
    checkVertexSpacingDoesNotAffectAmountOfGeneratedIsolines();

    /* (test 5): Make sure that no data set features a line segment at height of 1. */
    runForAllTestResults(checkNoLineSegmentIsDefinedAtHeightOne);

    /* All done */
    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    return STOP;
}

/** Calls the caller-provided function provided for each test result descriptor
 *  created during pre-computation stage.
 *
 *  @param pProcessTestRun Function pointer to call. The function will be called
 *                         exactly once for each cached test result descriptor.
 *
 **/
void TessellationShadersIsolines::runForAllTestResults(PFNTESTRESULTPROCESSORPROC pProcessTestResult)
{
    for (_test_results_per_vertex_spacing_map_iterator vs_key_iterator = m_test_results.begin();
         vs_key_iterator != m_test_results.end(); vs_key_iterator++)
    {
        for (_test_results_iterator test_results_iterator = vs_key_iterator->second.begin();
             test_results_iterator != vs_key_iterator->second.end(); test_results_iterator++)
        {
            _test_result &test_result = *test_results_iterator;

            pProcessTestResult(test_result, m_glExtTokens.MAX_TESS_GEN_LEVEL);
        } /* for (all level3 keys) */
    }     /* for (all vertex spacing modes) */
}

} /* namespace glcts */
