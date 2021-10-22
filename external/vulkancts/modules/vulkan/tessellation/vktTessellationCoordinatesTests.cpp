/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2014 The Android Open Source Project
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
 *//*!
 * \file
 * \brief Tessellation Coordinates Tests
 *//*--------------------------------------------------------------------*/

#include "vktTessellationCoordinatesTests.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTessellationUtil.hpp"

#include "tcuTestLog.hpp"
#include "tcuRGBA.hpp"
#include "tcuSurface.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuVectorUtil.hpp"

#include "vkDefs.hpp"
#include "vkBarrierUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"

#include "deUniquePtr.hpp"

#include <string>
#include <vector>

namespace vkt
{
namespace tessellation
{

using namespace vk;

namespace
{

template <typename T>
class SizeLessThan
{
public:
	bool operator() (const T& a, const T& b) const { return a.size() < b.size(); }
};

std::string getCaseName (const TessPrimitiveType primitiveType, const SpacingMode spacingMode, bool executionModeInEvaluationShader)
{
	std::ostringstream str;
	str << getTessPrimitiveTypeShaderName(primitiveType) << "_" << getSpacingModeShaderName(spacingMode);
	if (!executionModeInEvaluationShader)
		str << "_execution_mode_in_tesc";
	return str.str();
}

std::vector<TessLevels> genTessLevelCases (const TessPrimitiveType	primitiveType,
										   const SpacingMode		spacingMode)
{
	static const TessLevels rawTessLevelCases[] =
	{
		{ { 1.0f,	1.0f	},	{ 1.0f,		1.0f,	1.0f,	1.0f	} },
		{ { 63.0f,	24.0f	},	{ 15.0f,	42.0f,	10.0f,	12.0f	} },
		{ { 3.0f,	2.0f	},	{ 6.0f,		8.0f,	7.0f,	9.0f	} },
		{ { 4.0f,	6.0f	},	{ 2.0f,		3.0f,	1.0f,	4.0f	} },
		{ { 2.0f,	2.0f	},	{ 6.0f,		8.0f,	7.0f,	9.0f	} },
		{ { 5.0f,	6.0f	},	{ 1.0f,		1.0f,	1.0f,	1.0f	} },
		{ { 1.0f,	6.0f	},	{ 2.0f,		3.0f,	1.0f,	4.0f	} },
		{ { 5.0f,	1.0f	},	{ 2.0f,		3.0f,	1.0f,	4.0f	} },
		{ { 5.2f,	1.6f	},	{ 2.9f,		3.4f,	1.5f,	4.1f	} }
	};

	if (spacingMode == SPACINGMODE_EQUAL)
		return std::vector<TessLevels>(DE_ARRAY_BEGIN(rawTessLevelCases), DE_ARRAY_END(rawTessLevelCases));
	else
	{
		std::vector<TessLevels> result;
		result.reserve(DE_LENGTH_OF_ARRAY(rawTessLevelCases));

		for (int tessLevelCaseNdx = 0; tessLevelCaseNdx < DE_LENGTH_OF_ARRAY(rawTessLevelCases); ++tessLevelCaseNdx)
		{
			TessLevels curTessLevelCase = rawTessLevelCases[tessLevelCaseNdx];

			float* const inner = &curTessLevelCase.inner[0];
			float* const outer = &curTessLevelCase.outer[0];

			for (int j = 0; j < 2; ++j) inner[j] = static_cast<float>(getClampedRoundedTessLevel(spacingMode, inner[j]));
			for (int j = 0; j < 4; ++j) outer[j] = static_cast<float>(getClampedRoundedTessLevel(spacingMode, outer[j]));

			if (primitiveType == TESSPRIMITIVETYPE_TRIANGLES)
			{
				if (outer[0] > 1.0f || outer[1] > 1.0f || outer[2] > 1.0f)
				{
					if (inner[0] == 1.0f)
						inner[0] = static_cast<float>(getClampedRoundedTessLevel(spacingMode, inner[0] + 0.1f));
				}
			}
			else if (primitiveType == TESSPRIMITIVETYPE_QUADS)
			{
				if (outer[0] > 1.0f || outer[1] > 1.0f || outer[2] > 1.0f || outer[3] > 1.0f)
				{
					if (inner[0] == 1.0f) inner[0] = static_cast<float>(getClampedRoundedTessLevel(spacingMode, inner[0] + 0.1f));
					if (inner[1] == 1.0f) inner[1] = static_cast<float>(getClampedRoundedTessLevel(spacingMode, inner[1] + 0.1f));
				}
			}

			result.push_back(curTessLevelCase);
		}

		DE_ASSERT(static_cast<int>(result.size()) == DE_LENGTH_OF_ARRAY(rawTessLevelCases));
		return result;
	}
}

std::vector<tcu::Vec3> generateReferenceTessCoords (const TessPrimitiveType	primitiveType,
													const SpacingMode		spacingMode,
													const float*			innerLevels,
													const float*			outerLevels)
{
	if (isPatchDiscarded(primitiveType, outerLevels))
		return std::vector<tcu::Vec3>();

	switch (primitiveType)
	{
		case TESSPRIMITIVETYPE_TRIANGLES:
		{
			int inner;
			int outer[3];
			getClampedRoundedTriangleTessLevels(spacingMode, innerLevels, outerLevels, &inner, &outer[0]);

			if (spacingMode != SPACINGMODE_EQUAL)
			{
				// \note For fractional spacing modes, exact results are implementation-defined except in special cases.
				DE_ASSERT(de::abs(innerLevels[0] - static_cast<float>(inner)) < 0.001f);
				for (int i = 0; i < 3; ++i)
					DE_ASSERT(de::abs(outerLevels[i] - static_cast<float>(outer[i])) < 0.001f);
				DE_ASSERT(inner > 1 || (outer[0] == 1 && outer[1] == 1 && outer[2] == 1));
			}

			return generateReferenceTriangleTessCoords(spacingMode, inner, outer[0], outer[1], outer[2]);
		}

		case TESSPRIMITIVETYPE_QUADS:
		{
			int inner[2];
			int outer[4];
			getClampedRoundedQuadTessLevels(spacingMode, innerLevels, outerLevels, &inner[0], &outer[0]);

			if (spacingMode != SPACINGMODE_EQUAL)
			{
				// \note For fractional spacing modes, exact results are implementation-defined except in special cases.
				for (int i = 0; i < 2; ++i)
					DE_ASSERT(de::abs(innerLevels[i] - static_cast<float>(inner[i])) < 0.001f);
				for (int i = 0; i < 4; ++i)
					DE_ASSERT(de::abs(outerLevels[i] - static_cast<float>(outer[i])) < 0.001f);

				DE_ASSERT((inner[0] > 1 && inner[1] > 1) || (inner[0] == 1 && inner[1] == 1 && outer[0] == 1 && outer[1] == 1 && outer[2] == 1 && outer[3] == 1));
			}

			return generateReferenceQuadTessCoords(spacingMode, inner[0], inner[1], outer[0], outer[1], outer[2], outer[3]);
		}

		case TESSPRIMITIVETYPE_ISOLINES:
		{
			int outer[2];
			getClampedRoundedIsolineTessLevels(spacingMode, &outerLevels[0], &outer[0]);

			if (spacingMode != SPACINGMODE_EQUAL)
			{
				// \note For fractional spacing modes, exact results are implementation-defined except in special cases.
				DE_ASSERT(de::abs(outerLevels[1] - static_cast<float>(outer[1])) < 0.001f);
			}

			return generateReferenceIsolineTessCoords(outer[0], outer[1]);
		}

		default:
			DE_ASSERT(false);
			return std::vector<tcu::Vec3>();
	}
}

void drawPoint (tcu::Surface& dst, const int centerX, const int centerY, const tcu::RGBA& color, const int size)
{
	const int width		= dst.getWidth();
	const int height	= dst.getHeight();
	DE_ASSERT(de::inBounds(centerX, 0, width) && de::inBounds(centerY, 0, height));
	DE_ASSERT(size > 0);

	for (int yOff = -((size-1)/2); yOff <= size/2; ++yOff)
	for (int xOff = -((size-1)/2); xOff <= size/2; ++xOff)
	{
		const int pixX = centerX + xOff;
		const int pixY = centerY + yOff;
		if (de::inBounds(pixX, 0, width) && de::inBounds(pixY, 0, height))
			dst.setPixel(pixX, pixY, color);
	}
}

void drawTessCoordPoint (tcu::Surface& dst, const TessPrimitiveType primitiveType, const tcu::Vec3& pt, const tcu::RGBA& color, const int size)
{
	// \note These coordinates should match the description in the log message in TessCoordTestInstance::iterate.

	static const tcu::Vec2 triangleCorners[3] =
	{
		tcu::Vec2(0.95f, 0.95f),
		tcu::Vec2(0.5f,  0.95f - 0.9f*deFloatSqrt(3.0f/4.0f)),
		tcu::Vec2(0.05f, 0.95f)
	};

	static const float quadIsolineLDRU[4] =
	{
		0.1f, 0.9f, 0.9f, 0.1f
	};

	const tcu::Vec2 dstPos = primitiveType == TESSPRIMITIVETYPE_TRIANGLES ? pt.x()*triangleCorners[0]
																		  + pt.y()*triangleCorners[1]
																		  + pt.z()*triangleCorners[2]

					  : primitiveType == TESSPRIMITIVETYPE_QUADS ||
						primitiveType == TESSPRIMITIVETYPE_ISOLINES ? tcu::Vec2((1.0f - pt.x())*quadIsolineLDRU[0] + pt.x()*quadIsolineLDRU[2],
																			    (1.0f - pt.y())*quadIsolineLDRU[1] + pt.y()*quadIsolineLDRU[3])

					  : tcu::Vec2(-1.0f);

	drawPoint(dst,
			  static_cast<int>(dstPos.x() * (float)dst.getWidth()),
			  static_cast<int>(dstPos.y() * (float)dst.getHeight()),
			  color,
			  size);
}

void drawTessCoordVisualization (tcu::Surface& dst, const TessPrimitiveType primitiveType, const std::vector<tcu::Vec3>& coords)
{
	const int imageWidth  = 256;
	const int imageHeight = 256;
	dst.setSize(imageWidth, imageHeight);

	tcu::clear(dst.getAccess(), tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

	for (int i = 0; i < static_cast<int>(coords.size()); ++i)
		drawTessCoordPoint(dst, primitiveType, coords[i], tcu::RGBA::white(), 2);
}

inline bool vec3XLessThan (const tcu::Vec3& a, const tcu::Vec3& b)
{
	return a.x() < b.x();
}

int binarySearchFirstVec3WithXAtLeast (const std::vector<tcu::Vec3>& sorted, float x)
{
	const tcu::Vec3 ref(x, 0.0f, 0.0f);
	const std::vector<tcu::Vec3>::const_iterator first = std::lower_bound(sorted.begin(), sorted.end(), ref, vec3XLessThan);
	if (first == sorted.end())
		return -1;
	return static_cast<int>(std::distance(sorted.begin(), first));
}

// Check that all points in subset are (approximately) present also in superset.
bool oneWayComparePointSets (tcu::TestLog&					log,
							 tcu::Surface&					errorDst,
							 const TessPrimitiveType		primitiveType,
							 const std::vector<tcu::Vec3>&	subset,
							 const std::vector<tcu::Vec3>&	superset,
							 const char*					subsetName,
							 const char*					supersetName,
							 const tcu::RGBA&				errorColor)
{
	const std::vector<tcu::Vec3> supersetSorted		 = sorted(superset, vec3XLessThan);
	const float					 epsilon			 = 0.01f;
	const int					 maxNumFailurePrints = 5;
	int							 numFailuresDetected = 0;

	for (int subNdx = 0; subNdx < static_cast<int>(subset.size()); ++subNdx)
	{
		const tcu::Vec3& subPt = subset[subNdx];

		bool matchFound = false;

		{
			// Binary search the index of the first point in supersetSorted with x in the [subPt.x() - epsilon, subPt.x() + epsilon] range.
			const tcu::Vec3	matchMin			= subPt - epsilon;
			const tcu::Vec3	matchMax			= subPt + epsilon;
			const int		firstCandidateNdx	= binarySearchFirstVec3WithXAtLeast(supersetSorted, matchMin.x());

			if (firstCandidateNdx >= 0)
			{
				// Compare subPt to all points in supersetSorted with x in the [subPt.x() - epsilon, subPt.x() + epsilon] range.
				for (int superNdx = firstCandidateNdx; superNdx < static_cast<int>(supersetSorted.size()) && supersetSorted[superNdx].x() <= matchMax.x(); ++superNdx)
				{
					const tcu::Vec3& superPt = supersetSorted[superNdx];

					if (tcu::boolAll(tcu::greaterThanEqual	(superPt, matchMin)) &&
						tcu::boolAll(tcu::lessThanEqual		(superPt, matchMax)))
					{
						matchFound = true;
						break;
					}
				}
			}
		}

		if (!matchFound)
		{
			++numFailuresDetected;
			if (numFailuresDetected < maxNumFailurePrints)
				log << tcu::TestLog::Message << "Failure: no matching " << supersetName << " point found for " << subsetName << " point " << subPt << tcu::TestLog::EndMessage;
			else if (numFailuresDetected == maxNumFailurePrints)
				log << tcu::TestLog::Message << "Note: More errors follow" << tcu::TestLog::EndMessage;

			drawTessCoordPoint(errorDst, primitiveType, subPt, errorColor, 4);
		}
	}

	return numFailuresDetected == 0;
}

//! Returns true on matching coordinate sets.
bool compareTessCoords (tcu::TestLog&					log,
						TessPrimitiveType				primitiveType,
						const std::vector<tcu::Vec3>&	refCoords,
						const std::vector<tcu::Vec3>&	resCoords)
{
	tcu::Surface	refVisual;
	tcu::Surface	resVisual;
	bool			success = true;

	drawTessCoordVisualization(refVisual, primitiveType, refCoords);
	drawTessCoordVisualization(resVisual, primitiveType, resCoords);

	// Check that all points in reference also exist in result.
	success = oneWayComparePointSets(log, refVisual, primitiveType, refCoords, resCoords, "reference", "result", tcu::RGBA::blue()) && success;
	// Check that all points in result also exist in reference.
	success = oneWayComparePointSets(log, resVisual, primitiveType, resCoords, refCoords, "result", "reference", tcu::RGBA::red()) && success;

	if (!success)
	{
		log << tcu::TestLog::Message << "Note: in the following reference visualization, points that are missing in result point set are blue (if any)" << tcu::TestLog::EndMessage
			<< tcu::TestLog::Image("RefTessCoordVisualization", "Reference tessCoord visualization", refVisual)
			<< tcu::TestLog::Message << "Note: in the following result visualization, points that are missing in reference point set are red (if any)" << tcu::TestLog::EndMessage;
	}

	log << tcu::TestLog::Image("ResTessCoordVisualization", "Result tessCoord visualization", resVisual);

	return success;
}

class TessCoordTest : public TestCase
{
public:
								TessCoordTest	(tcu::TestContext&			testCtx,
												 const TessPrimitiveType	primitiveType,
												 const SpacingMode			spacingMode,
												 const bool					executionModeInEvaluationShader = true);

	void						initPrograms	(SourceCollections&			programCollection) const;
	void						checkSupport	(Context&					context) const;
	TestInstance*				createInstance	(Context&					context) const;

private:
	const TessPrimitiveType		m_primitiveType;
	const SpacingMode			m_spacingMode;
	const bool					m_executionModeInEvaluationShader;
};

TessCoordTest::TessCoordTest (tcu::TestContext&			testCtx,
							  const TessPrimitiveType	primitiveType,
							  const SpacingMode			spacingMode,
							  const bool				executionModeInEvaluationShader)
	: TestCase							(testCtx, getCaseName(primitiveType, spacingMode, executionModeInEvaluationShader), "")
	, m_primitiveType					(primitiveType)
	, m_spacingMode						(spacingMode)
	, m_executionModeInEvaluationShader	(executionModeInEvaluationShader)
{
}

void TessCoordTest::checkSupport (Context& context) const
{
	if (const vk::VkPhysicalDevicePortabilitySubsetFeaturesKHR* const features = getPortability(context))
	{
		checkPointMode(*features);
		checkPrimitive(*features, m_primitiveType);
	}
}

void TessCoordTest::initPrograms (SourceCollections& programCollection) const
{
	if (m_executionModeInEvaluationShader)
	{
		// Vertex shader - no inputs
		{
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_310_ES) << "\n"
				<< "\n"
				<< "void main (void)\n"
				<< "{\n"
				<< "}\n";

			programCollection.glslSources.add("vert") << glu::VertexSource(src.str());
		}

		// Tessellation control shader
		{
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_310_ES) << "\n"
				<< "#extension GL_EXT_tessellation_shader : require\n"
				<< "\n"
				<< "layout(vertices = 1) out;\n"
				<< "\n"
				<< "layout(set = 0, binding = 0, std430) readonly restrict buffer TessLevels {\n"
				<< "    float inner0;\n"
				<< "    float inner1;\n"
				<< "    float outer0;\n"
				<< "    float outer1;\n"
				<< "    float outer2;\n"
				<< "    float outer3;\n"
				<< "} sb_levels;\n"
				<< "\n"
				<< "void main (void)\n"
				<< "{\n"
				<< "    gl_TessLevelInner[0] = sb_levels.inner0;\n"
				<< "    gl_TessLevelInner[1] = sb_levels.inner1;\n"
				<< "\n"
				<< "    gl_TessLevelOuter[0] = sb_levels.outer0;\n"
				<< "    gl_TessLevelOuter[1] = sb_levels.outer1;\n"
				<< "    gl_TessLevelOuter[2] = sb_levels.outer2;\n"
				<< "    gl_TessLevelOuter[3] = sb_levels.outer3;\n"
				<< "}\n";

			programCollection.glslSources.add("tesc") << glu::TessellationControlSource(src.str());
		}

		// Tessellation evaluation shader
		{
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_310_ES) << "\n"
				<< "#extension GL_EXT_tessellation_shader : require\n"
				<< "\n"
				<< "layout(" << getTessPrimitiveTypeShaderName(m_primitiveType) << ", "
				<< getSpacingModeShaderName(m_spacingMode) << ", point_mode) in;\n" << "\n"
				<< "layout(set = 0, binding = 1, std430) coherent restrict buffer Output {\n"
				<< "    int  numInvocations;\n"
				<< "    vec3 tessCoord[];\n"		// alignment is 16 bytes, same as vec4
				<< "} sb_out;\n"
				<< "\n"
				<< "void main (void)\n"
				<< "{\n"
				<< "    int index = atomicAdd(sb_out.numInvocations, 1);\n"
				<< "    sb_out.tessCoord[index] = gl_TessCoord;\n"
				<< "}\n";

			programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(src.str());
		}
	}
	else
	{
		// note: spirv code for all stages coresponds to glsl version above

		programCollection.spirvAsmSources.add("vert")
			<< "OpCapability Shader\n"
			   "%glsl_ext_inst = OpExtInstImport \"GLSL.std.450\"\n"
			   "OpMemoryModel Logical GLSL450\n"
			   "OpEntryPoint Vertex %main_fun \"main\"\n"
			   "%type_void       = OpTypeVoid\n"
			   "%type_void_f     = OpTypeFunction %type_void\n"
			   "%main_fun        = OpFunction %type_void None %type_void_f\n"
			   "%main_label      = OpLabel\n"
			   "OpReturn\n"
			   "OpFunctionEnd\n";

		// glsl requires primitive_mode, vertex_spacing, ordering and point_mode layout qualifiers to be defined in
		// tessellation evaluation shader while spirv allows corresponding execution modes to be defined in TES and/or
		// TCS; here we test using execution modes only in TCS as TES is tested with glsl version of tests

		const std::string executionMode =
			std::string("OpExecutionMode %main_fun ") + getTessPrimitiveTypeShaderName(m_primitiveType, true) + "\n"
			"OpExecutionMode %main_fun " + getSpacingModeShaderName(m_spacingMode, true) + "\n" +
			"OpExecutionMode %main_fun PointMode\n"
			"OpExecutionMode %main_fun VertexOrderCcw\n";

		std::string tescSrc =
			   "OpCapability Tessellation\n"
			   "%glsl_ext_inst = OpExtInstImport \"GLSL.std.450\"\n"
			   "OpMemoryModel Logical GLSL450\n"
			   "OpEntryPoint TessellationControl %main_fun \"main\" %var_tess_level_inner %var_tess_level_outer\n"
			   "OpExecutionMode %main_fun OutputVertices 1\n";
		tescSrc += executionMode +
			   "OpDecorate %var_tess_level_inner Patch\n"
			   "OpDecorate %var_tess_level_inner BuiltIn TessLevelInner\n"
			   "OpMemberDecorate %type_struct_sb_levels 0 NonWritable\n"
			   "OpMemberDecorate %type_struct_sb_levels 0 Offset 0\n"
			   "OpMemberDecorate %type_struct_sb_levels 1 NonWritable\n"
			   "OpMemberDecorate %type_struct_sb_levels 1 Offset 4\n"
			   "OpMemberDecorate %type_struct_sb_levels 2 NonWritable\n"
			   "OpMemberDecorate %type_struct_sb_levels 2 Offset 8\n"
			   "OpMemberDecorate %type_struct_sb_levels 3 NonWritable\n"
			   "OpMemberDecorate %type_struct_sb_levels 3 Offset 12\n"
			   "OpMemberDecorate %type_struct_sb_levels 4 NonWritable\n"
			   "OpMemberDecorate %type_struct_sb_levels 4 Offset 16\n"
			   "OpMemberDecorate %type_struct_sb_levels 5 NonWritable\n"
			   "OpMemberDecorate %type_struct_sb_levels 5 Offset 20\n"
			   "OpDecorate %type_struct_sb_levels BufferBlock\n"
			   "OpDecorate %var_struct_sb_levels DescriptorSet 0\n"
			   "OpDecorate %var_struct_sb_levels Binding 0\n"
			   "OpDecorate %var_struct_sb_levels Restrict\n"
			   "OpDecorate %var_tess_level_outer Patch\n"
			   "OpDecorate %var_tess_level_outer BuiltIn TessLevelOuter\n"
			   "%type_void                 = OpTypeVoid\n"
			   "%type_void_f               = OpTypeFunction %type_void\n"
			   "%type_f32                  = OpTypeFloat 32\n"
			   "%type_u32                  = OpTypeInt 32 0\n"
			   "%c_u32_2                   = OpConstant %type_u32 2\n"
			   "%type_arr_f32_2            = OpTypeArray %type_f32 %c_u32_2\n"
			   "%type_arr_f32_2_ptr        = OpTypePointer Output %type_arr_f32_2\n"
			   "%type_i32                  = OpTypeInt 32 1\n"
			   "%type_struct_sb_levels     = OpTypeStruct %type_f32 %type_f32 %type_f32 %type_f32 %type_f32 %type_f32\n"
			   "%type_struct_sb_levels_ptr = OpTypePointer Uniform %type_struct_sb_levels\n"
			   "%var_struct_sb_levels      = OpVariable %type_struct_sb_levels_ptr Uniform\n"
			   "%type_uni_f32_ptr          = OpTypePointer Uniform %type_f32\n"
			   "%type_out_f32_ptr          = OpTypePointer Output %type_f32\n"
			   "%c_i32_0                   = OpConstant %type_i32 0\n"
			   "%c_i32_1                   = OpConstant %type_i32 1\n"
			   "%c_u32_4                   = OpConstant %type_u32 4\n"
			   "%c_i32_2                   = OpConstant %type_i32 2\n"
			   "%c_i32_3                   = OpConstant %type_i32 3\n"
			   "%c_i32_4                   = OpConstant %type_i32 4\n"
			   "%c_i32_5                   = OpConstant %type_i32 5\n"
			   "%type_arr_f32_4            = OpTypeArray %type_f32 %c_u32_4\n"
			   "%type_arr_f32_4_ptr        = OpTypePointer Output %type_arr_f32_4\n"
			   "%var_tess_level_inner      = OpVariable %type_arr_f32_2_ptr Output\n"
			   "%var_tess_level_outer      = OpVariable %type_arr_f32_4_ptr Output\n"
			   "%main_fun                  = OpFunction %type_void None %type_void_f\n"
			   "%main_label                = OpLabel\n"
			   "%tess_inner_0_ptr          = OpAccessChain %type_uni_f32_ptr %var_struct_sb_levels %c_i32_0\n"
			   "%tess_inner_0              = OpLoad %type_f32 %tess_inner_0_ptr\n"
			   "%gl_tess_inner_0           = OpAccessChain %type_out_f32_ptr %var_tess_level_inner %c_i32_0\n"
			   "                             OpStore %gl_tess_inner_0 %tess_inner_0\n"
			   "%tess_inner_1_ptr          = OpAccessChain %type_uni_f32_ptr %var_struct_sb_levels %c_i32_1\n"
			   "%tess_inner_1              = OpLoad %type_f32 %tess_inner_1_ptr\n"
			   "%gl_tess_inner_1           = OpAccessChain %type_out_f32_ptr %var_tess_level_inner %c_i32_1\n"
			   "                             OpStore %gl_tess_inner_1 %tess_inner_1\n"
			   "%tess_outer_0_ptr          = OpAccessChain %type_uni_f32_ptr %var_struct_sb_levels %c_i32_2\n"
			   "%tess_outer_0              = OpLoad %type_f32 %tess_outer_0_ptr\n"
			   "%gl_tess_outer_0           = OpAccessChain %type_out_f32_ptr %var_tess_level_outer %c_i32_0\n"
			   "                             OpStore %gl_tess_outer_0 %tess_outer_0\n"
			   "%tess_outer_1_ptr          = OpAccessChain %type_uni_f32_ptr %var_struct_sb_levels %c_i32_3\n"
			   "%tess_outer_1              = OpLoad %type_f32 %tess_outer_1_ptr\n"
			   "%gl_tess_outer_1           = OpAccessChain %type_out_f32_ptr %var_tess_level_outer %c_i32_1\n"
			   "                             OpStore %gl_tess_outer_1 %tess_outer_1\n"
			   "%tess_outer_2_ptr          = OpAccessChain %type_uni_f32_ptr %var_struct_sb_levels %c_i32_4\n"
			   "%tess_outer_2              = OpLoad %type_f32 %tess_outer_2_ptr\n"
			   "%gl_tess_outer_2           = OpAccessChain %type_out_f32_ptr %var_tess_level_outer %c_i32_2\n"
			   "                             OpStore %gl_tess_outer_2 %tess_outer_2\n"
			   "%tess_outer_3_ptr          = OpAccessChain %type_uni_f32_ptr %var_struct_sb_levels %c_i32_5\n"
			   "%tess_outer_3              = OpLoad %type_f32 %tess_outer_3_ptr\n"
			   "%gl_tess_outer_3           = OpAccessChain %type_out_f32_ptr %var_tess_level_outer %c_i32_3\n"
			   "                             OpStore %gl_tess_outer_3 %tess_outer_3\n"
			   "OpReturn\n"
			   "OpFunctionEnd\n";
		programCollection.spirvAsmSources.add("tesc") << tescSrc;

		std::string teseSrc =
			   "OpCapability Tessellation\n"
			   "%glsl_ext_inst = OpExtInstImport \"GLSL.std.450\"\n"
			   "OpMemoryModel Logical GLSL450\n"
			   "OpEntryPoint TessellationEvaluation %main_fun \"main\" %var_gl_tess_coord\n"
			   "OpDecorate %type_run_arr_v3_f32 ArrayStride 16\n"
			   "OpMemberDecorate %type_struct 0 Coherent\n"
			   "OpMemberDecorate %type_struct 0 Offset 0\n"
			   "OpMemberDecorate %type_struct 1 Coherent\n"
			   "OpMemberDecorate %type_struct 1 Offset 16\n"
			   "OpDecorate %type_struct BufferBlock\n"
			   "OpDecorate %var_struct_ptr DescriptorSet 0\n"
			   "OpDecorate %var_struct_ptr Restrict\n"
			   "OpDecorate %var_struct_ptr Binding 1\n"
			   "OpDecorate %var_gl_tess_coord BuiltIn TessCoord\n"
			   "%type_void             = OpTypeVoid\n"
			   "%type_void_f           = OpTypeFunction %type_void\n"
			   "%type_i32              = OpTypeInt 32 1\n"
			   "%type_u32              = OpTypeInt 32 0\n"
			   "%type_i32_fp           = OpTypePointer Function %type_i32\n"
			   "%type_f32              = OpTypeFloat 32\n"
			   "%type_v3_f32           = OpTypeVector %type_f32 3\n"
			   "%type_run_arr_v3_f32   = OpTypeRuntimeArray %type_v3_f32\n"
			   "%type_struct           = OpTypeStruct %type_i32 %type_run_arr_v3_f32\n"
			   "%type_uni_struct_ptr   = OpTypePointer Uniform %type_struct\n"
			   "%type_uni_i32_ptr      = OpTypePointer Uniform %type_i32\n"
			   "%type_uni_v3_f32_ptr   = OpTypePointer Uniform %type_v3_f32\n"
			   "%type_in_v3_f32_ptr    = OpTypePointer Input %type_v3_f32\n"
			   "%c_i32_0               = OpConstant %type_i32 0\n"
			   "%c_i32_1               = OpConstant %type_i32 1\n"
			   "%c_u32_0               = OpConstant %type_u32 1\n"
			   "%c_u32_1               = OpConstant %type_u32 0\n"
			   "%var_struct_ptr        = OpVariable %type_uni_struct_ptr Uniform\n"
			   "%var_gl_tess_coord     = OpVariable %type_in_v3_f32_ptr Input\n"
			   "%main_fun              = OpFunction %type_void None %type_void_f\n"
			   "%main_label            = OpLabel\n"
			   "%var_i32_ptr           = OpVariable %type_i32_fp Function\n"
			   "%num_invocations       = OpAccessChain %type_uni_i32_ptr %var_struct_ptr %c_i32_0\n"
			   "%index_0               = OpAtomicIAdd %type_i32 %num_invocations %c_u32_0 %c_u32_1 %c_i32_1\n"
			   "                         OpStore %var_i32_ptr %index_0\n"
			   "%index_1               = OpLoad %type_i32 %var_i32_ptr\n"
			   "%gl_tess_coord         = OpLoad %type_v3_f32 %var_gl_tess_coord\n"
			   "%out_tess_coord        = OpAccessChain %type_uni_v3_f32_ptr %var_struct_ptr %c_i32_1 %index_1\n"
			   "                         OpStore %out_tess_coord %gl_tess_coord\n"
			   "OpReturn\n"
			   "OpFunctionEnd\n";
		programCollection.spirvAsmSources.add("tese") << teseSrc;
	}
}

class TessCoordTestInstance : public TestInstance
{
public:
								TessCoordTestInstance (Context&					context,
													   const TessPrimitiveType	primitiveType,
													   const SpacingMode		spacingMode);

	tcu::TestStatus				iterate				  (void);

private:
	const TessPrimitiveType		m_primitiveType;
	const SpacingMode			m_spacingMode;
};

TessCoordTestInstance::TessCoordTestInstance (Context&					context,
											  const TessPrimitiveType	primitiveType,
											  const SpacingMode			spacingMode)
	: TestInstance		(context)
	, m_primitiveType	(primitiveType)
	, m_spacingMode		(spacingMode)
{
}

tcu::TestStatus TessCoordTestInstance::iterate (void)
{
	const DeviceInterface&	vk					= m_context.getDeviceInterface();
	const VkDevice			device				= m_context.getDevice();
	const VkQueue			queue				= m_context.getUniversalQueue();
	const deUint32			queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	Allocator&				allocator			= m_context.getDefaultAllocator();

	// Test data

	const std::vector<TessLevels>		 tessLevelCases			= genTessLevelCases(m_primitiveType, m_spacingMode);
	std::vector<std::vector<tcu::Vec3> > allReferenceTessCoords	(tessLevelCases.size());

	for (deUint32 i = 0; i < tessLevelCases.size(); ++i)
		allReferenceTessCoords[i] = generateReferenceTessCoords(m_primitiveType, m_spacingMode, &tessLevelCases[i].inner[0], &tessLevelCases[i].outer[0]);

	const size_t maxNumVertices = static_cast<int>(std::max_element(allReferenceTessCoords.begin(), allReferenceTessCoords.end(), SizeLessThan<std::vector<tcu::Vec3> >())->size());

	// Input buffer: tessellation levels. Data is filled in later.

	const Buffer tessLevelsBuffer(vk, device, allocator,
		makeBufferCreateInfo(sizeof(TessLevels), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), MemoryRequirement::HostVisible);

	// Output buffer: number of invocations + padding + tessellation coordinates. Initialized later.

	const int          resultBufferTessCoordsOffset	 = 4 * (int)sizeof(deInt32);
	const int          extraneousVertices			 = 16;	// allow some room for extraneous vertices from duplicate shader invocations (number is arbitrary)
	const VkDeviceSize resultBufferSizeBytes		 = resultBufferTessCoordsOffset + (maxNumVertices + extraneousVertices)*sizeof(tcu::Vec4);
	const Buffer       resultBuffer					 (vk, device, allocator, makeBufferCreateInfo(resultBufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), MemoryRequirement::HostVisible);

	// Descriptors

	const Unique<VkDescriptorSetLayout> descriptorSetLayout(DescriptorSetLayoutBuilder()
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
		.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
		.build(vk, device));

	const Unique<VkDescriptorPool> descriptorPool(DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));

	const Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

	const VkDescriptorBufferInfo tessLevelsBufferInfo = makeDescriptorBufferInfo(tessLevelsBuffer.get(), 0ull, sizeof(TessLevels));
	const VkDescriptorBufferInfo resultBufferInfo     = makeDescriptorBufferInfo(resultBuffer.get(), 0ull, resultBufferSizeBytes);

	DescriptorSetUpdateBuilder()
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &tessLevelsBufferInfo)
		.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &resultBufferInfo)
		.update(vk, device);

	// Pipeline: set up vertex processing without rasterization

	const Unique<VkRenderPass>		renderPass		(makeRenderPassWithoutAttachments (vk, device));
	const Unique<VkFramebuffer>		framebuffer		(makeFramebuffer(vk, device, *renderPass, 0u, DE_NULL, 1u, 1u));
	const Unique<VkPipelineLayout>	pipelineLayout	(makePipelineLayout(vk, device, *descriptorSetLayout));
	const Unique<VkCommandPool>		cmdPool			(makeCommandPool(vk, device, queueFamilyIndex));
	const Unique<VkCommandBuffer>	cmdBuffer		(allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY));

	const Unique<VkPipeline> pipeline(GraphicsPipelineBuilder()
		.setShader(vk, device, VK_SHADER_STAGE_VERTEX_BIT,					m_context.getBinaryCollection().get("vert"), DE_NULL)
		.setShader(vk, device, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,	m_context.getBinaryCollection().get("tesc"), DE_NULL)
		.setShader(vk, device, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, m_context.getBinaryCollection().get("tese"), DE_NULL)
		.build    (vk, device, *pipelineLayout, *renderPass));

	deUint32 numPassedCases = 0;

	// Repeat the test for all tessellation coords cases
	for (deUint32 tessLevelCaseNdx = 0; tessLevelCaseNdx < tessLevelCases.size(); ++tessLevelCaseNdx)
	{
		m_context.getTestContext().getLog()
			<< tcu::TestLog::Message
			<< "Tessellation levels: " << getTessellationLevelsString(tessLevelCases[tessLevelCaseNdx], m_primitiveType)
			<< tcu::TestLog::EndMessage;

		// Upload tessellation levels data to the input buffer
		{
			const Allocation& alloc				= tessLevelsBuffer.getAllocation();
			TessLevels* const bufferTessLevels	= static_cast<TessLevels*>(alloc.getHostPtr());

			*bufferTessLevels = tessLevelCases[tessLevelCaseNdx];
			flushAlloc(vk, device, alloc);
		}

		// Clear the results buffer
		{
			const Allocation& alloc = resultBuffer.getAllocation();

			deMemset(alloc.getHostPtr(), 0, static_cast<std::size_t>(resultBufferSizeBytes));
			flushAlloc(vk, device, alloc);
		}

		// Reset the command buffer and begin recording.
		beginCommandBuffer(vk, *cmdBuffer);
		beginRenderPassWithRasterizationDisabled(vk, *cmdBuffer, *renderPass, *framebuffer);

		vk.cmdBindPipeline(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);
		vk.cmdBindDescriptorSets(*cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0u, 1u, &descriptorSet.get(), 0u, DE_NULL);

		// Process a single abstract vertex.
		vk.cmdDraw(*cmdBuffer, 1u, 1u, 0u, 0u);
		endRenderPass(vk, *cmdBuffer);

		{
			const VkBufferMemoryBarrier shaderWriteBarrier = makeBufferMemoryBarrier(
				VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, *resultBuffer, 0ull, resultBufferSizeBytes);

			vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u,
				0u, DE_NULL, 1u, &shaderWriteBarrier, 0u, DE_NULL);
		}

		endCommandBuffer(vk, *cmdBuffer);
		submitCommandsAndWait(vk, device, queue, *cmdBuffer);

		// Verify results
		{
			const Allocation&				resultAlloc			= resultBuffer.getAllocation();

			invalidateAlloc(vk, device, resultAlloc);

			const deInt32					numResults			= *static_cast<deInt32*>(resultAlloc.getHostPtr());
			const std::vector<tcu::Vec3>	resultTessCoords	= readInterleavedData<tcu::Vec3>(numResults, resultAlloc.getHostPtr(), resultBufferTessCoordsOffset, sizeof(tcu::Vec4));
			const std::vector<tcu::Vec3>&	referenceTessCoords	= allReferenceTessCoords[tessLevelCaseNdx];
			const int						numExpectedResults	= static_cast<int>(referenceTessCoords.size());
			tcu::TestLog&					log					= m_context.getTestContext().getLog();

			if (numResults < numExpectedResults)
			{
				log << tcu::TestLog::Message
					<< "Failure: generated " << numResults << " coordinates, but the expected reference value is " << numExpectedResults
					<< tcu::TestLog::EndMessage;
			}
			else if (numResults == numExpectedResults)
				log << tcu::TestLog::Message << "Note: generated " << numResults << " tessellation coordinates" << tcu::TestLog::EndMessage;
			else
			{
				log << tcu::TestLog::Message
					<< "Note: generated " << numResults << " coordinates (out of which " << numExpectedResults << " must be unique)"
					<< tcu::TestLog::EndMessage;
			}

			if (m_primitiveType == TESSPRIMITIVETYPE_TRIANGLES)
				log << tcu::TestLog::Message << "Note: in the following visualization(s), the u=1, v=1, w=1 corners are at the right, top, and left corners, respectively" << tcu::TestLog::EndMessage;
			else if (m_primitiveType == TESSPRIMITIVETYPE_QUADS || m_primitiveType == TESSPRIMITIVETYPE_ISOLINES)
				log << tcu::TestLog::Message << "Note: in the following visualization(s), u and v coordinate go left-to-right and bottom-to-top, respectively" << tcu::TestLog::EndMessage;
			else
				DE_ASSERT(false);

			if (compareTessCoords(log, m_primitiveType, referenceTessCoords, resultTessCoords) && (numResults >= numExpectedResults))
				++numPassedCases;
		}
	}  // for tessLevelCaseNdx

	return (numPassedCases == tessLevelCases.size() ? tcu::TestStatus::pass("OK") : tcu::TestStatus::fail("Some cases have failed"));
}

TestInstance* TessCoordTest::createInstance (Context& context) const
{
	requireFeatures(context.getInstanceInterface(), context.getPhysicalDevice(), FEATURE_TESSELLATION_SHADER | FEATURE_VERTEX_PIPELINE_STORES_AND_ATOMICS);

	return new TessCoordTestInstance(context, m_primitiveType, m_spacingMode);
}

} // anonymous

//! Based on dEQP-GLES31.functional.tessellation.tesscoord.*
//! \note Transform feedback is replaced with SSBO. Because of that, this version allows duplicate coordinates from shader invocations.
//! The test still fails if not enough coordinates are generated, or if coordinates don't match the reference data.
tcu::TestCaseGroup* createCoordinatesTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group (new tcu::TestCaseGroup(testCtx, "tesscoord", "Tessellation coordinates tests"));

	for (int primitiveTypeNdx = 0; primitiveTypeNdx < TESSPRIMITIVETYPE_LAST; ++primitiveTypeNdx)
		for (int spacingModeNdx = 0; spacingModeNdx < SPACINGMODE_LAST; ++spacingModeNdx)
		{
			group->addChild(new TessCoordTest(testCtx, (TessPrimitiveType)primitiveTypeNdx, (SpacingMode)spacingModeNdx));

			// test if TessCoord builtin has correct value in Evaluation shader when execution mode is set only in Control shader
			group->addChild(new TessCoordTest(testCtx, (TessPrimitiveType)primitiveTypeNdx, (SpacingMode)spacingModeNdx, false));
		}

	return group.release();
}

} // tessellation
} // vkt
