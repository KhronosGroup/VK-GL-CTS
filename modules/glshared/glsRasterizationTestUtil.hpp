#ifndef _GLSRASTERIZATIONTESTUTIL_HPP
#define _GLSRASTERIZATIONTESTUTIL_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL (ES) Module
 * -----------------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
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
 * \brief rasterization test utils.
 *//*--------------------------------------------------------------------*/

#include "deMath.h"
#include "tcuDefs.hpp"
#include "tcuTestLog.hpp"

#include <vector>

namespace deqp
{
namespace gls
{
namespace RasterizationTestUtil
{

enum CoverageType
{
	COVERAGE_FULL = 0,		// !< primitive fully covers the queried area
	COVERAGE_PARTIAL,		// !< primitive coverage is either partial, or could be full, partial or none depending on rounding and/or fill rules
	COVERAGE_NONE,			// !< primitive does not cover area at all

	COVERAGE_LAST
};

enum VerificationMode
{
	VERIFICATIONMODE_STRICT = 0,	// !< do not allow even a single bad pixel
	VERIFICATIONMODE_WEAK,			// !< allow some bad pixels

	VERIFICATIONMODE_LAST
};

enum LineInterpolationMethod
{
	LINEINTERPOLATION_STRICTLY_CORRECT = 0,	// !< line interpolation matches the specification
	LINEINTERPOLATION_PROJECTED,			// !< line interpolation weights are otherwise correct, but they are projected onto major axis
	LINEINTERPOLATION_INCORRECT				// !< line interpolation is incorrect
};

struct TriangleSceneSpec
{
	struct SceneTriangle
	{
		tcu::Vec4	positions[3];
		tcu::Vec4	colors[3];
		bool		sharedEdge[3]; // !< is the edge i -> i+1 shared with another scene triangle
	};

	std::vector<SceneTriangle> triangles;
};

struct LineSceneSpec
{
	struct SceneLine
	{
		tcu::Vec4	positions[2];
		tcu::Vec4	colors[2];
	};

	std::vector<SceneLine>	lines;
	float					lineWidth;
};

struct PointSceneSpec
{
	struct ScenePoint
	{
		tcu::Vec4	position;
		tcu::Vec4	color;
		float		pointSize;
	};

	std::vector<ScenePoint> points;
};

struct RasterizationArguments
{
	int numSamples;
	int subpixelBits;
	int redBits;
	int greenBits;
	int blueBits;
};

/*--------------------------------------------------------------------*//*!
 * \brief Calculates triangle coverage at given pixel
 * Calculates the coverage of a triangle given by three vertices. The
 * triangle should not be z-clipped. If multisample is false, the pixel
 * center is compared against the triangle. If multisample is true, the
 * whole pixel area is compared.
 *//*--------------------------------------------------------------------*/
CoverageType calculateTriangleCoverage (const tcu::Vec4& p0, const tcu::Vec4& p1, const tcu::Vec4& p2, const tcu::IVec2& pixel, const tcu::IVec2& viewportSize, int subpixelBits, bool multisample);

/*--------------------------------------------------------------------*//*!
 * \brief Verify triangle rasterization result
 * Verifies pixels in the surface are rasterized within the bounds given
 * by RasterizationArguments. Triangles should not be z-clipped.
 *
 * Triangle colors are not used. The triangle is expected to be white.
 *
 * Returns false if invalid rasterization is found.
 *//*--------------------------------------------------------------------*/
bool verifyTriangleGroupRasterization (const tcu::Surface& surface, const TriangleSceneSpec& scene, const RasterizationArguments& args, tcu::TestLog& log, VerificationMode mode = VERIFICATIONMODE_STRICT);

/*--------------------------------------------------------------------*//*!
 * \brief Verify line rasterization result
 * Verifies pixels in the surface are rasterized within the bounds given
 * by RasterizationArguments. Lines should not be z-clipped.
 *
 * Line colors are not used. The line is expected to be white.
 *
 * Returns false if invalid rasterization is found.
 *//*--------------------------------------------------------------------*/
bool verifyLineGroupRasterization (const tcu::Surface& surface, const LineSceneSpec& scene, const RasterizationArguments& args, tcu::TestLog& log);

/*--------------------------------------------------------------------*//*!
 * \brief Verify point rasterization result
 * Verifies points in the surface are rasterized within the bounds given
 * by RasterizationArguments. Points should not be z-clipped.
 *
 * Point colors are not used. The point is expected to be white.
 *
 * Returns false if invalid rasterization is found.
 *//*--------------------------------------------------------------------*/
bool verifyPointGroupRasterization (const tcu::Surface& surface, const PointSceneSpec& scene, const RasterizationArguments& args, tcu::TestLog& log);

/*--------------------------------------------------------------------*//*!
 * \brief Verify triangle color interpolation is valid
 * Verifies the color of a fragments of a colored triangle is in the
 * valid range. Triangles should not be z-clipped.
 *
 * The background is expected to be black.
 *
 * Returns false if invalid rasterization interpolation is found.
 *//*--------------------------------------------------------------------*/
bool verifyTriangleGroupInterpolation (const tcu::Surface& surface, const TriangleSceneSpec& scene, const RasterizationArguments& args, tcu::TestLog& log);

/*--------------------------------------------------------------------*//*!
 * \brief Verify line color interpolation is valid
 * Verifies the color of a fragments of a colored line is in the
 * valid range. Lines should not be z-clipped.
 *
 * The background is expected to be black.
 *
 * Returns the detected interpolation method of the input image.
 *//*--------------------------------------------------------------------*/
LineInterpolationMethod verifyLineGroupInterpolation (const tcu::Surface& surface, const LineSceneSpec& scene, const RasterizationArguments& args, tcu::TestLog& log);

} // StateQueryUtil
} // gls
} // deqp

#endif // _GLSRASTERIZATIONTESTUTIL_HPP
