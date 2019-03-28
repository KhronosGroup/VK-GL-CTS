/*------------------------------------------------------------------------
 * OpenGL Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017-2019 The Khronos Group Inc.
 * Copyright (c) 2017 Codeplay Software Ltd.
 * Copyright (c) 2019 NVIDIA Corporation.
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
 * \brief Subgroups Tests
 */ /*--------------------------------------------------------------------*/

#include "glcSubgroupsBasicTests.hpp"
#include "glcSubgroupsTestsUtils.hpp"
#include "gluContextInfo.hpp"

#include <string>
#include <vector>
#include "tcuStringTemplate.hpp"

using namespace tcu;
using namespace std;

namespace glc
{
namespace subgroups
{

namespace
{
static const deUint32			ELECTED_VALUE		= 42u;
static const deUint32			UNELECTED_VALUE		= 13u;
static const deUint64			SHADER_BUFFER_SIZE	= 4096ull;

static bool checkFragmentSubgroupBarriersNoSSBO(std::vector<const void*> datas,
		deUint32 width, deUint32 height, deUint32)
{
	const float* const	resultData	= reinterpret_cast<const float*>(datas[0]);

	for (deUint32 x = 0u; x < width; ++x)
	{
		for (deUint32 y = 0u; y < height; ++y)
		{
			const deUint32 ndx = (x * height + y) * 4u;
			if (1.0f == resultData[ndx +2])
			{
				if(resultData[ndx] != resultData[ndx +1])
				{
					return false;
				}
			}
			else if (resultData[ndx] != resultData[ndx +3])
			{
				return false;
			}
		}
	}

	return true;
}

static bool checkVertexPipelineStagesSubgroupElectNoSSBO(std::vector<const void*> datas,
		deUint32 width, deUint32)
{
	const float* const	resultData			= reinterpret_cast<const float*>(datas[0]);
	float				poisonValuesFound	= 0.0f;
	float				numSubgroupsUsed	= 0.0f;

	for (deUint32 x = 0; x < width; ++x)
	{
		deUint32 val = static_cast<deUint32>(resultData[x * 2]);
		numSubgroupsUsed += resultData[x * 2 + 1];

		switch (val)
		{
			default:
				// some garbage value was found!
				return false;
			case UNELECTED_VALUE:
				break;
			case ELECTED_VALUE:
				poisonValuesFound += 1.0f;
				break;
		}
	}
	return numSubgroupsUsed == poisonValuesFound;
}

static bool checkVertexPipelineStagesSubgroupElect(std::vector<const void*> datas,
		deUint32 width, deUint32)
{
	const deUint32* const resultData =
		reinterpret_cast<const deUint32*>(datas[0]);
	deUint32 poisonValuesFound = 0;

	for (deUint32 x = 0; x < width; ++x)
	{
		deUint32 val = resultData[x];

		switch (val)
		{
			default:
				// some garbage value was found!
				return false;
			case UNELECTED_VALUE:
				break;
			case ELECTED_VALUE:
				poisonValuesFound++;
				break;
		}
	}

	// we used an atomicly incremented counter to note how many subgroups we used for the vertex shader
	const deUint32 numSubgroupsUsed =
		*reinterpret_cast<const deUint32*>(datas[1]);

	return numSubgroupsUsed == poisonValuesFound;
}

static bool checkVertexPipelineStagesSubgroupBarriers(std::vector<const void*> datas,
		deUint32 width, deUint32)
{
	const deUint32* const resultData = reinterpret_cast<const deUint32*>(datas[0]);

	// We used this SSBO to generate our unique value!
	const deUint32 ref = *reinterpret_cast<const deUint32*>(datas[3]);

	for (deUint32 x = 0; x < width; ++x)
	{
		deUint32 val = resultData[x];

		if (val != ref)
			return false;
	}

	return true;
}

static bool checkVertexPipelineStagesSubgroupBarriersNoSSBO(std::vector<const void*> datas,
		deUint32 width, deUint32)
{
	const float* const	resultData	= reinterpret_cast<const float*>(datas[0]);

	for (deUint32 x = 0u; x < width; ++x)
	{
		const deUint32 ndx = x*4u;
		if (1.0f == resultData[ndx +2])
		{
			if(resultData[ndx] != resultData[ndx +1])
				return false;
		}
		else if (resultData[ndx] != resultData[ndx +3])
		{
			return false;
		}
	}
	return true;
}

static bool checkTessellationEvaluationSubgroupBarriersNoSSBO(std::vector<const void*> datas,
		deUint32 width, deUint32)
{
	const float* const	resultData	= reinterpret_cast<const float*>(datas[0]);

	for (deUint32 x = 0u; x < width; ++x)
	{
		const deUint32 ndx = x*4u;
		if (0.0f == resultData[ndx +2] && resultData[ndx] != resultData[ndx +3])
		{
			return false;
		}
	}
	return true;
}

static bool checkComputeSubgroupElect(std::vector<const void*> datas,
									  const deUint32 numWorkgroups[3], const deUint32 localSize[3],
									  deUint32)
{
	return glc::subgroups::checkCompute(datas, numWorkgroups, localSize, 1);
}

static bool checkComputeSubgroupBarriers(std::vector<const void*> datas,
		const deUint32 numWorkgroups[3], const deUint32 localSize[3],
		deUint32)
{
	// We used this SSBO to generate our unique value!
	const deUint32 ref = *reinterpret_cast<const deUint32*>(datas[2]);
	return glc::subgroups::checkCompute(datas, numWorkgroups, localSize, ref);
}

enum OpType
{
	OPTYPE_ELECT = 0,
	OPTYPE_SUBGROUP_BARRIER,
	OPTYPE_SUBGROUP_MEMORY_BARRIER,
	OPTYPE_SUBGROUP_MEMORY_BARRIER_BUFFER,
	OPTYPE_SUBGROUP_MEMORY_BARRIER_SHARED,
	OPTYPE_SUBGROUP_MEMORY_BARRIER_IMAGE,
	OPTYPE_LAST
};

std::string getOpTypeName(int opType)
{
	switch (opType)
	{
		default:
			DE_FATAL("Unsupported op type");
			return "";
		case OPTYPE_ELECT:
			return "subgroupElect";
		case OPTYPE_SUBGROUP_BARRIER:
			return "subgroupBarrier";
		case OPTYPE_SUBGROUP_MEMORY_BARRIER:
			return "subgroupMemoryBarrier";
		case OPTYPE_SUBGROUP_MEMORY_BARRIER_BUFFER:
			return "subgroupMemoryBarrierBuffer";
		case OPTYPE_SUBGROUP_MEMORY_BARRIER_SHARED:
			return "subgroupMemoryBarrierShared";
		case OPTYPE_SUBGROUP_MEMORY_BARRIER_IMAGE:
			return "subgroupMemoryBarrierImage";
	}
}

struct CaseDefinition
{
	int							opType;
	subgroups::ShaderStageFlags	shaderStage;
};

void initFrameBufferPrograms(SourceCollections& programCollection, CaseDefinition caseDef)
{
	if(subgroups::SHADER_STAGE_FRAGMENT_BIT != caseDef.shaderStage)
	{
		const string fragmentGLSL =
			"#version 450\n"
			"layout(location = 0) in vec4 in_color;\n"
			"layout(location = 0) out vec4 out_color;\n"
			"void main()\n"
			"{\n"
			"	out_color = in_color;\n"
			"}\n";

		programCollection.add("fragment") << glu::FragmentSource(fragmentGLSL);
	}
	if (subgroups::SHADER_STAGE_FRAGMENT_BIT == caseDef.shaderStage)
	{
		const string vertexGLSL =
			"#version 450\n"
			"void main (void)\n"
			"{\n"
			"  vec2 uv = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);\n"
			"  gl_Position = vec4(uv * 2.0f + -1.0f, 0.0f, 1.0f);\n"
			"  gl_PointSize = 1.0f;\n"
			"}\n";

		programCollection.add("vert") << glu::VertexSource(vertexGLSL);
	}
	else if (subgroups::SHADER_STAGE_VERTEX_BIT != caseDef.shaderStage)
		subgroups::setVertexShaderFrameBuffer(programCollection);

	if (OPTYPE_ELECT == caseDef.opType)
	{
		std::ostringstream electedValue ;
		std::ostringstream unelectedValue;
		electedValue << ELECTED_VALUE;
		unelectedValue << UNELECTED_VALUE;

		if (subgroups::SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
		{
			const string vertexGLSL =
				"#version 450\n"
				"#extension GL_KHR_shader_subgroup_basic: enable\n"
				"layout(location = 0) out vec4 out_color;\n"
				"layout(location = 0) in highp vec4 in_position;\n"
				"\n"
				"void main (void)\n"
				"{\n"
				"  if (subgroupElect())\n"
				"  {\n"
				"    out_color.r = " + electedValue.str() + ";\n"
				"    out_color.g = 1.0f;\n"
				"  }\n"
				"  else\n"
				"  {\n"
				"    out_color.r = " + unelectedValue.str() + ";\n"
				"    out_color.g = 0.0f;\n"
				"  }\n"
				"  gl_Position = in_position;\n"
				"  gl_PointSize = 1.0f;\n"
				"}\n";

			programCollection.add("vert") << glu::VertexSource(vertexGLSL);
		}
		else if (subgroups::SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
		{
			const string geometryGLSL =
				"#version 450\n"
				"#extension GL_KHR_shader_subgroup_basic: enable\n"
				"layout(points) in;\n"
				"layout(points, max_vertices = 1) out;\n"
				"layout(location = 0) out vec4 out_color;\n"
				"void main (void)\n"
				"{\n"
				"  if (subgroupElect())\n"
				"  {\n"
				"    out_color.r = " + electedValue.str() + ";\n"
				"    out_color.g = 1.0f;\n"
				"  }\n"
				"  else\n"
				"  {\n"
				"    out_color.r = " + unelectedValue.str() + ";\n"
				"    out_color.g = 0.0f;\n"
				"  }\n"
				"  gl_Position = gl_in[0].gl_Position;\n"
				"  EmitVertex();\n"
				"  EndPrimitive();\n"
				"}\n";

			programCollection.add("geometry") << glu::GeometrySource(geometryGLSL);
		}
		else if (subgroups::SHADER_STAGE_TESS_EVALUATION_BIT == caseDef.shaderStage)
		{
			const string controlSourceGLSL =
				"#version 450\n"
				"#extension GL_KHR_shader_subgroup_basic: enable\n"
				"#extension GL_EXT_tessellation_shader : require\n"
				"layout(vertices = 2) out;\n"
				"void main (void)\n"
				"{\n"
				"  if (gl_InvocationID == 0)\n"
				"  {\n"
				"    gl_TessLevelOuter[0] = 1.0f;\n"
				"    gl_TessLevelOuter[1] = 1.0f;\n"
				"  }\n"
				"  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
				"}\n";

			programCollection.add("tesc") << glu::TessellationControlSource(controlSourceGLSL);

			const string evaluationSourceGLSL =
				"#version 450\n"
				"#extension GL_KHR_shader_subgroup_basic: enable\n"
				"#extension GL_EXT_tessellation_shader : require\n"
				"layout(isolines, equal_spacing, ccw ) in;\n"
				"layout(location = 0) out vec4 out_color;\n"
				"\n"
				"void main (void)\n"
				"{\n"
				"  if (subgroupElect())\n"
				"  {\n"
				"    out_color.r = 71.f;\n" // << 2 * ELECTED_VALUE - UNELECTED_VALUE << ";\n"
				"    out_color.g = 2.0f;\n"
				"  }\n"
				"  else\n"
				"  {\n"
				"    out_color.r = " + unelectedValue.str() + ";\n"
				"    out_color.g = 0.0f;\n"
				"  }\n"
				"  gl_Position = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x);\n"
				"}\n";

			programCollection.add("tese") << glu::TessellationEvaluationSource(evaluationSourceGLSL);
		}
		else if (subgroups::SHADER_STAGE_TESS_CONTROL_BIT == caseDef.shaderStage)
		{
			const string  controlSourceGLSL =
				"#version 450\n"
				"#extension GL_KHR_shader_subgroup_basic: enable\n"
				"#extension GL_EXT_tessellation_shader : require\n"
				"layout(vertices = 2) out;\n"
				"layout(location = 0) out vec4 out_color[];\n"
				"void main (void)\n"
				"{\n"
				"  if (gl_InvocationID == 0)\n"
				"  {\n"
				"    gl_TessLevelOuter[0] = 1.0f;\n"
				"    gl_TessLevelOuter[1] = 1.0f;\n"
				"  }\n"
				"  if (subgroupElect())\n"
				"  {\n"
				"    out_color[gl_InvocationID].r = " + electedValue.str() + ";\n"
				"    out_color[gl_InvocationID].g = 1.0f;\n"
				"  }\n"
				"  else\n"
				"  {\n"
				"    out_color[gl_InvocationID].r = " + unelectedValue.str() + ";\n"
				"    out_color[gl_InvocationID].g = 0.0f;\n"
				"  }\n"
				"  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
				"}\n";

			programCollection.add("tesc") << glu::TessellationControlSource(controlSourceGLSL);

			const string evaluationSourceGLSL =
				"#version 450\n"
				"#extension GL_KHR_shader_subgroup_ballot: enable\n"
				"#extension GL_EXT_tessellation_shader : require\n"
				"layout(isolines, equal_spacing, ccw ) in;\n"
				"layout(location = 0) in vec4 in_color[];\n"
				"layout(location = 0) out vec4 out_color;\n"
				"\n"
				"void main (void)\n"
				"{\n"
				"  gl_Position = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x);\n"
				"  out_color = in_color[0];\n"
				"}\n";

			programCollection.add("tese") << glu::TessellationEvaluationSource(evaluationSourceGLSL);
		}
		else
		{
			DE_FATAL("Unsupported shader stage");
		}
	}
	else
	{
		std::ostringstream bdy;
		string color = (subgroups::SHADER_STAGE_TESS_CONTROL_BIT == caseDef.shaderStage) ? "out_color[gl_InvocationID].b = 1.0f;\n" : "out_color.b = 1.0f;\n";
		switch (caseDef.opType)
		{
			default:
				DE_FATAL("Unhandled op type!");
				break;
			case OPTYPE_SUBGROUP_BARRIER:
			case OPTYPE_SUBGROUP_MEMORY_BARRIER:
			case OPTYPE_SUBGROUP_MEMORY_BARRIER_BUFFER:
			{
				bdy << "  tempResult2 = tempBuffer[id];\n"
					<< "  if (subgroupElect())\n"
					<< "  {\n"
					<< "    tempResult = value;\n"
					<< "    " << color
					<< "  }\n"
					<< "  else\n"
					<< "  {\n"
					<< "    tempResult = tempBuffer[id];\n"
					<< "  }\n"
					<< "  " << getOpTypeName(caseDef.opType) << "();\n";
				break;
			}
			case OPTYPE_SUBGROUP_MEMORY_BARRIER_IMAGE:
				bdy << "  tempResult2 = imageLoad(tempImage, ivec2(id, 0)).x;\n"
					<< "  if (subgroupElect())\n"
					<< "  {\n"
					<< "    tempResult = value;\n"
					<< "    " << color
					<< "  }\n"
					<< "  else\n"
					<< "  {\n"
					<< "    tempResult = imageLoad(tempImage, ivec2(id, 0)).x;\n"
					<< "  }\n"
					<< "  subgroupMemoryBarrierImage();\n";

				break;
		}

		if (subgroups::SHADER_STAGE_FRAGMENT_BIT == caseDef.shaderStage)
		{
			std::ostringstream	fragment;
			fragment	<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
				<< "#extension GL_KHR_shader_subgroup_basic: enable\n"
				<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
				<< "layout(location = 0) out vec4 out_color;\n"
				<< "\n"
				<< "layout(binding = 0) uniform Buffer1\n"
				<< "{\n"
				<< "  uint tempBuffer["<<SHADER_BUFFER_SIZE/4ull<<"];\n"
				<< "};\n"
				<< "\n"
				<< "layout(binding = 1) uniform Buffer2\n"
				<< "{\n"
				<< "  uint value;\n"
				<< "};\n"
				<< (OPTYPE_SUBGROUP_MEMORY_BARRIER_IMAGE == caseDef.opType ? "layout(binding = 0, r32ui) readonly uniform highp uimage2D tempImage;\n" : "\n")
				<< "void main (void)\n"
				<< "{\n"
				<< "  if (gl_HelperInvocation) return;\n"
				<< "  uint id = 0;\n"
				<< "  if (subgroupElect())\n"
				<< "  {\n"
				<< "    id = uint(gl_FragCoord.x);\n"
				<< "  }\n"
				<< "  id = subgroupBroadcastFirst(id);\n"
				<< "  uint localId = id;\n"
				<< "  uint tempResult = 0u;\n"
				<< "  uint tempResult2 = 0u;\n"
				<< "  out_color.b = 0.0f;\n"
				<< bdy.str()
				<< "  out_color.r = float(tempResult);\n"
				<< "  out_color.g = float(value);\n"
				<< "  out_color.a = float(tempResult2);\n"
				<< "}\n";
			programCollection.add("fragment") << glu::FragmentSource(fragment.str());
		}
		else if (subgroups::SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
		{
			std::ostringstream	vertex;
			vertex	<< glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
				<< "#extension GL_KHR_shader_subgroup_basic: enable\n"
				<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
				<<"\n"
				<< "layout(location = 0) out vec4 out_color;\n"
				<< "layout(location = 0) in highp vec4 in_position;\n"
				<< "\n"
				<< "layout(binding = 0) uniform Buffer1\n"
				<< "{\n"
				<< "  uint tempBuffer["<<SHADER_BUFFER_SIZE/4ull<<"];\n"
				<< "};\n"
				<< "\n"
				<< "layout(binding = 1) uniform Buffer2\n"
				<< "{\n"
				<< "  uint value;\n"
				<< "};\n"
				<< (OPTYPE_SUBGROUP_MEMORY_BARRIER_IMAGE == caseDef.opType ? "layout(binding = 0, r32ui) readonly uniform highp uimage2D tempImage;\n" : "\n")
				<< "void main (void)\n"
				<< "{\n"
				<< "  uint id = 0;\n"
				<< "  if (subgroupElect())\n"
				<< "  {\n"
				<< "    id = gl_VertexID;\n"
				<< "  }\n"
				<< "  id = subgroupBroadcastFirst(id);\n"
				<< "  uint tempResult = 0u;\n"
				<< "  uint tempResult2 = 0u;\n"
				<< "  out_color.b = 0.0f;\n"
				<< bdy.str()
				<< "  out_color.r = float(tempResult);\n"
				<< "  out_color.g = float(value);\n"
				<< "  out_color.a = float(tempResult2);\n"
				<< "  gl_Position = in_position;\n"
				<< "  gl_PointSize = 1.0f;\n"
				<< "}\n";
			programCollection.add("vert") << glu::VertexSource(vertex.str());
		}
	else if (subgroups::SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
		{
			std::ostringstream geometry;

			geometry << "#version 450\n"
					<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
					<< "#extension GL_KHR_shader_subgroup_basic: enable\n"
					<< "layout(points) in;\n"
					<< "layout(points, max_vertices = 1) out;\n"
					<< "layout(location = 0) out vec4 out_color;\n"
					<< "layout(binding = 0) uniform Buffer1\n"
					<< "{\n"
					<< "  uint tempBuffer["<<SHADER_BUFFER_SIZE/4ull<<"];\n"
					<< "};\n"
					<< "\n"
					<< "layout(binding = 1) uniform Buffer2\n"
					<< "{\n"
					<< "  uint value;\n"
					<< "};\n"
					<< (OPTYPE_SUBGROUP_MEMORY_BARRIER_IMAGE == caseDef.opType ? "layout(binding = 0, r32ui) readonly uniform highp uimage2D tempImage;\n" : "\n")
					<< "void main (void)\n"
					<< "{\n"
					<< "  uint id = 0;\n"
					<< "  if (subgroupElect())\n"
					<< "  {\n"
					<< "    id = gl_InvocationID;\n"
					<< "  }\n"
					<< "  id = subgroupBroadcastFirst(id);\n"
					<< "  uint tempResult = 0u;\n"
					<< "  uint tempResult2 = 0u;\n"
					<< "  out_color.b = 0.0f;\n"
					<< bdy.str()
					<< "  out_color.r = float(tempResult);\n"
					<< "  out_color.g = float(value);\n"
					<< "  out_color.a = float(tempResult2);\n"
					<< "  gl_Position = gl_in[0].gl_Position;\n"
					<< "  EmitVertex();\n"
					<< "  EndPrimitive();\n"
					<< "}\n";

			programCollection.add("geometry") << glu::GeometrySource(geometry.str());
		}
		else if (subgroups::SHADER_STAGE_TESS_EVALUATION_BIT == caseDef.shaderStage)
		{
			std::ostringstream controlSource;
			std::ostringstream evaluationSource;

			controlSource << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
				<< "#extension GL_EXT_tessellation_shader : require\n"
				<< "layout(vertices = 2) out;\n"
				<< "void main (void)\n"
				<< "{\n"
				<< "  if (gl_InvocationID == 0)\n"
				<<"  {\n"
				<< "    gl_TessLevelOuter[0] = 1.0f;\n"
				<< "    gl_TessLevelOuter[1] = 1.0f;\n"
				<< "  }\n"
				<< "  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
				<< "}\n";

			evaluationSource << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
				<< "#extension GL_KHR_shader_subgroup_basic: enable\n"
				<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
				<< "#extension GL_EXT_tessellation_shader : require\n"
				<< "layout(isolines, equal_spacing, ccw ) in;\n"
				<< "layout(location = 0) out vec4 out_color;\n"
				<< "layout(binding = 0) uniform Buffer1\n"
				<< "{\n"
				<< "  uint tempBuffer["<<SHADER_BUFFER_SIZE/4ull<<"];\n"
				<< "};\n"
				<< "\n"
				<< "layout(binding = 1) uniform Buffer2\n"
				<< "{\n"
				<< "  uint value;\n"
				<< "};\n"
				<< (OPTYPE_SUBGROUP_MEMORY_BARRIER_IMAGE == caseDef.opType ? "layout(binding = 0, r32ui) readonly uniform highp uimage2D tempImage;\n" : "\n")
				<< "void main (void)\n"
				<< "{\n"
				<< "  uint id = 0;\n"
				<< "  if (subgroupElect())\n"
				<< "  {\n"
				<< "    id = gl_PrimitiveID;\n"
				<< "  }\n"
				<< "  id = subgroupBroadcastFirst(id);\n"
				<< "  uint tempResult = 0u;\n"
				<< "  uint tempResult2 = 0u;\n"
				<< "  out_color.b = 0.0f;\n"
				<< bdy.str()
				<< "  out_color.r = float(tempResult);\n"
				<< "  out_color.g = float(value);\n"
				<< "  out_color.a = float(tempResult2);\n"
				<< "  gl_Position = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x);\n"
				<< "}\n";

			programCollection.add("tesc") << glu::TessellationControlSource(controlSource.str());
			programCollection.add("tese") << glu::TessellationEvaluationSource(evaluationSource.str());
		}
		else if (subgroups::SHADER_STAGE_TESS_CONTROL_BIT == caseDef.shaderStage)
		{
			std::ostringstream controlSource;
			std::ostringstream evaluationSource;

			controlSource  << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
				<< "#extension GL_KHR_shader_subgroup_basic: enable\n"
				<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
				<< "#extension GL_EXT_tessellation_shader : require\n"
				<< "layout(vertices = 2) out;\n"
				<< "layout(location = 0) out vec4 out_color[];\n"
				<< "layout(binding = 0) uniform Buffer1\n"
				<< "{\n"
				<< "  uint tempBuffer["<<SHADER_BUFFER_SIZE/4ull<<"];\n"
				<< "};\n"
				<< "\n"
				<< "layout(binding = 1) uniform Buffer2\n"
				<< "{\n"
				<< "  uint value;\n"
				<< "};\n"
				<< (OPTYPE_SUBGROUP_MEMORY_BARRIER_IMAGE == caseDef.opType ? "layout(binding = 0, r32ui) readonly uniform highp uimage2D tempImage;\n" : "\n")
				<< "void main (void)\n"
				<< "{\n"
				<< "  uint id = 0;\n"
				<< "  if (gl_InvocationID == 0)\n"
				<<"  {\n"
				<< "    gl_TessLevelOuter[0] = 1.0f;\n"
				<< "    gl_TessLevelOuter[1] = 1.0f;\n"
				<< "  }\n"
				<< "  if (subgroupElect())\n"
				<< "  {\n"
				<< "    id = gl_InvocationID;\n"
				<< "  }\n"
				<< "  id = subgroupBroadcastFirst(id);\n"
				<< "  uint tempResult = 0u;\n"
				<< "  uint tempResult2 = 0u;\n"
				<< "  out_color[gl_InvocationID].b = 0.0f;\n"
				<< bdy.str()
				<< "  out_color[gl_InvocationID].r = float(tempResult);\n"
				<< "  out_color[gl_InvocationID].g = float(value);\n"
				<< "  out_color[gl_InvocationID].a = float(tempResult2);\n"
				<< "  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
				<< "}\n";

			evaluationSource << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
				<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
				<< "#extension GL_EXT_tessellation_shader : require\n"
				<< "layout(isolines, equal_spacing, ccw ) in;\n"
				<< "layout(location = 0) in vec4 in_color[];\n"
				<< "layout(location = 0) out vec4 out_color;\n"
				<< "\n"
				<< "void main (void)\n"
				<< "{\n"
				<< "  gl_Position = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x);\n"
				<< "  out_color = in_color[0];\n"
				<< "}\n";

			programCollection.add("tesc") << glu::TessellationControlSource(controlSource.str());
			programCollection.add("tese") << glu::TessellationEvaluationSource(evaluationSource.str());
		}
		else
		{
			DE_FATAL("Unsupported shader stage");
		}
	}
}

void initPrograms(SourceCollections& programCollection, CaseDefinition caseDef)
{
	if (OPTYPE_ELECT == caseDef.opType)
	{
		if (subgroups::SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
		{
			std::ostringstream src;

			src << "#version 450\n"
				<< "#extension GL_KHR_shader_subgroup_basic: enable\n"
				<< "layout (${LOCAL_SIZE_X}, ${LOCAL_SIZE_Y}, ${LOCAL_SIZE_Z}) in;\n"
				<< "layout(binding = 0, std430) buffer Buffer1\n"
				<< "{\n"
				<< "  uint result[];\n"
				<< "};\n"
				<< "\n"
				<< subgroups::getSharedMemoryBallotHelper()
				<< "void main (void)\n"
				<< "{\n"
				<< "  uvec3 globalSize = gl_NumWorkGroups * gl_WorkGroupSize;\n"
				<< "  highp uint offset = globalSize.x * ((globalSize.y * "
				"gl_GlobalInvocationID.z) + gl_GlobalInvocationID.y) + "
				"gl_GlobalInvocationID.x;\n"
				<< "  uint value = " << UNELECTED_VALUE << ";\n"
				<< "  if (subgroupElect())\n"
				<< "  {\n"
				<< "    value = " << ELECTED_VALUE << ";\n"
				<< "  }\n"
				<< "  uvec4 bits = bitCount(sharedMemoryBallot(value == " << ELECTED_VALUE << "));\n"
				<< "  result[offset] = bits.x + bits.y + bits.z + bits.w;\n"
				<< "}\n";

			programCollection.add("comp") << glu::ComputeSource(src.str());
		}
		else
		{
			{
				std::ostringstream  vertex;
				vertex	<< "#version 450\n"
						<< "#extension GL_KHR_shader_subgroup_basic: enable\n"
						<< "layout(binding = 0, std430) buffer Buffer0\n"
						<< "{\n"
						<< "  uint result[];\n"
						<< "} b0;\n"
						<< "layout(binding = 4, std430) buffer Buffer4\n"
						<< "{\n"
						<< "  uint numSubgroupsExecuted;\n"
						<< "} b4;\n"
						<< "\n"
						<< "void main (void)\n"
						<< "{\n"
						<< "  if (subgroupElect())\n"
						<< "  {\n"
						<< "    b0.result[gl_VertexID] = " << ELECTED_VALUE << ";\n"
						<< "    atomicAdd(b4.numSubgroupsExecuted, 1);\n"
						<< "  }\n"
						<< "  else\n"
						<< "  {\n"
						<< "    b0.result[gl_VertexID] = " << UNELECTED_VALUE << ";\n"
						<< "  }\n"
						<< "  float pixelSize = 2.0f/1024.0f;\n"
						<< "  float pixelPosition = pixelSize/2.0f - 1.0f;\n"
						<< "  gl_Position = vec4(float(gl_VertexID) * pixelSize + pixelPosition, 0.0f, 0.0f, 1.0f);\n"
						<< "  gl_PointSize = 1.0f;\n"
						<< "}\n";
				programCollection.add("vert") << glu::VertexSource(vertex.str());
			}

			{
				std::ostringstream tesc;
				tesc	<< "#version 450\n"
						<< "#extension GL_KHR_shader_subgroup_basic: enable\n"
						<< "layout(vertices=1) out;\n"
						<< "layout(binding = 1, std430) buffer Buffer1\n"
						<< "{\n"
						<< "  uint result[];\n"
						<< "} b1;\n"
						<< "layout(binding = 5, std430) buffer Buffer5\n"
						<< "{\n"
						<< "  uint numSubgroupsExecuted;\n"
						<< "} b5;\n"
						<< "\n"
						<< "void main (void)\n"
						<< "{\n"
						<< "  if (subgroupElect())\n"
						<< "  {\n"
						<< "    b1.result[gl_PrimitiveID] = " << ELECTED_VALUE << ";\n"
						<< "    atomicAdd(b5.numSubgroupsExecuted, 1);\n"
						<< "  }\n"
						<< "  else\n"
						<< "  {\n"
						<< "    b1.result[gl_PrimitiveID] = " << UNELECTED_VALUE << ";\n"
						<< "  }\n"
						<< "  if (gl_InvocationID == 0)\n"
						<< "  {\n"
						<< "    gl_TessLevelOuter[0] = 1.0f;\n"
						<< "    gl_TessLevelOuter[1] = 1.0f;\n"
						<< "  }\n"
						<< "  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
						<< "}\n";
				programCollection.add("tesc") << glu::TessellationControlSource(tesc.str());
			}

			{
				std::ostringstream tese;
				tese	<< "#version 450\n"
						<< "#extension GL_KHR_shader_subgroup_basic: enable\n"
						<< "layout(isolines) in;\n"
						<< "layout(binding = 2, std430) buffer Buffer2\n"
						<< "{\n"
						<< "  uint result[];\n"
						<< "} b2;\n"
						<< "layout(binding = 6, std430) buffer Buffer6\n"
						<< "{\n"
						<< "  uint numSubgroupsExecuted;\n"
						<< "} b6;\n"
						<< "\n"
						<< "void main (void)\n"
						<< "{\n"
						<< "  if (subgroupElect())\n"
						<< "  {\n"
						<< "    b2.result[gl_PrimitiveID * 2 + uint(gl_TessCoord.x + 0.5)] = " << ELECTED_VALUE << ";\n"
						<< "    atomicAdd(b6.numSubgroupsExecuted, 1);\n"
						<< "  }\n"
						<< "  else\n"
						<< "  {\n"
						<< "    b2.result[gl_PrimitiveID * 2 + uint(gl_TessCoord.x + 0.5)] = " << UNELECTED_VALUE << ";\n"
						<< "  }\n"
						<< "  float pixelSize = 2.0f/1024.0f;\n"
						<< "  gl_Position = gl_in[0].gl_Position + gl_TessCoord.x * pixelSize / 2.0f;\n"
						<< "}\n";
				programCollection.add("tese") << glu::TessellationEvaluationSource(tese.str());
			}
			{
				std::ostringstream geometry;
				geometry	<< "#version 450\n"
							<< "#extension GL_KHR_shader_subgroup_basic: enable\n"
							<< "layout(${TOPOLOGY}) in;\n"
							<< "layout(points, max_vertices = 1) out;\n"
							<< "layout(binding = 3, std430) buffer Buffer3\n"
							<< "{\n"
							<< "  uint result[];\n"
							<< "} b3;\n"
							<< "layout(binding = 7, std430) buffer Buffer7\n"
							<< "{\n"
							<< "  uint numSubgroupsExecuted;\n"
							<< "} b7;\n"
							<< "\n"
							<< "void main (void)\n"
							<< "{\n"
							<< "  if (subgroupElect())\n"
							<< "  {\n"
							<< "    b3.result[gl_PrimitiveIDIn] = " << ELECTED_VALUE << ";\n"
							<< "    atomicAdd(b7.numSubgroupsExecuted, 1);\n"
							<< "  }\n"
							<< "  else\n"
							<< "  {\n"
							<< "    b3.result[gl_PrimitiveIDIn] = " << UNELECTED_VALUE << ";\n"
							<< "  }\n"
							<< "  gl_Position = gl_in[0].gl_Position;\n"
							<< "  EmitVertex();\n"
							<< "  EndPrimitive();\n"
							<< "}\n";
				subgroups::addGeometryShadersFromTemplate(geometry.str(), programCollection);
			}

			{
				std::ostringstream fragment;
				fragment	<< "#version 450\n"
							<< "#extension GL_KHR_shader_subgroup_basic: enable\n"
							<< "layout(location = 0) out uint data;\n"
							<< "layout(binding = 8, std430) buffer Buffer8\n"
							<< "{\n"
							<< "  uint numSubgroupsExecuted;\n"
							<< "} b8;\n"
							<< "void main (void)\n"
							<< "{\n"
							<< "  if (gl_HelperInvocation) return;\n"
							<< "  if (subgroupElect())\n"
							<< "  {\n"
							<< "    data = " << ELECTED_VALUE << ";\n"
							<< "    atomicAdd(b8.numSubgroupsExecuted, 1);\n"
							<< "  }\n"
							<< "  else\n"
							<< "  {\n"
							<< "    data = " << UNELECTED_VALUE << ";\n"
							<< "  }\n"
							<< "}\n";
				programCollection.add("fragment") << glu::FragmentSource(fragment.str());
			}
			subgroups::addNoSubgroupShader(programCollection);
		}
	}
	else
	{
		std::ostringstream bdy;

		switch (caseDef.opType)
		{
			default:
				DE_FATAL("Unhandled op type!");
				break;
			case OPTYPE_SUBGROUP_BARRIER:
			case OPTYPE_SUBGROUP_MEMORY_BARRIER:
			case OPTYPE_SUBGROUP_MEMORY_BARRIER_BUFFER:
				bdy << "  if (subgroupElect())\n"
					<< "  {\n"
					<< "    b${SSBO1}.tempBuffer[id] = b${SSBO2}.value;\n"
					<< "  }\n"
					<< "  " << getOpTypeName(caseDef.opType) << "();\n"
					<< "  tempResult = b${SSBO1}.tempBuffer[id];\n";
				break;
			case OPTYPE_SUBGROUP_MEMORY_BARRIER_SHARED:
				bdy << "  if (subgroupElect())\n"
					<< "  {\n"
					<< "    tempShared[localId] = b${SSBO2}.value;\n"
					<< "  }\n"
					<< "  subgroupMemoryBarrierShared();\n"
					<< "  tempResult = tempShared[localId];\n";
				break;
			case OPTYPE_SUBGROUP_MEMORY_BARRIER_IMAGE:
				bdy << "  if (subgroupElect())\n"
					<< "  {\n"
					<< "    imageStore(tempImage${IMG1}, ivec2(id, 0), ivec4(b${SSBO2}.value));\n"
					<< "  }\n"
					<< "  subgroupMemoryBarrierImage();\n"
					<< "  tempResult = imageLoad(tempImage${IMG1}, ivec2(id, 0)).x;\n";
				break;
		}

		tcu::StringTemplate bdyTemplate(bdy.str());

		if (subgroups::SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
		{
			std::ostringstream src;
			map<string, string> bufferNameMapping;
			bufferNameMapping.insert(pair<string, string>("SSBO1", "1"));
			bufferNameMapping.insert(pair<string, string>("SSBO2", "2"));
			bufferNameMapping.insert(pair<string, string>("IMG1", "0"));

			src << "#version 450\n"
				<< "#extension GL_KHR_shader_subgroup_basic: enable\n"
				<< "layout (${LOCAL_SIZE_X}, ${LOCAL_SIZE_Y}, ${LOCAL_SIZE_Z}) in;\n"
				<< "layout(binding = 0, std430) buffer Buffer0\n"
				<< "{\n"
				<< "  uint result[];\n"
				<< "} b0;\n"
				<< "layout(binding = 1, std430) buffer Buffer1\n"
				<< "{\n"
				<< "  uint tempBuffer[];\n"
				<< "} b1;\n"
				<< "layout(binding = 2, std430) buffer Buffer2\n"
				<< "{\n"
				<< "  uint value;\n"
				<< "} b2;\n"
				<< "layout(binding = 0, r32ui) uniform uimage2D tempImage0;\n"
				<< "shared uint tempShared[gl_WorkGroupSize.x * gl_WorkGroupSize.y * gl_WorkGroupSize.z];\n"
				<< "\n"
				<< "void main (void)\n"
				<< "{\n"
				<< "  uvec3 globalSize = gl_NumWorkGroups * gl_WorkGroupSize;\n"
				<< "  highp uint offset = globalSize.x * ((globalSize.y * "
				"gl_GlobalInvocationID.z) + gl_GlobalInvocationID.y) + "
				"gl_GlobalInvocationID.x;\n"
				<< "  uint localId = gl_SubgroupID;\n"
				<< "  uint id = globalSize.x * ((globalSize.y * "
				"gl_WorkGroupID.z) + gl_WorkGroupID.y) + "
				"gl_WorkGroupID.x + localId;\n"
				<< "  uint tempResult = 0;\n"
				<< bdyTemplate.specialize(bufferNameMapping)
				<< "  b0.result[offset] = tempResult;\n"
				<< "}\n";

			programCollection.add("comp") << glu::ComputeSource(src.str());
		}
		else
		{
			{
				map<string, string> bufferNameMapping;
				bufferNameMapping.insert(pair<string, string>("SSBO1", "4"));
				bufferNameMapping.insert(pair<string, string>("SSBO2", "6"));
				bufferNameMapping.insert(pair<string, string>("IMG1", "0"));

				const string vertex =
					"#version 450\n"
					"#extension GL_KHR_shader_subgroup_basic: enable\n"
					"#extension GL_KHR_shader_subgroup_ballot: enable\n"
					"layout(binding = 0, std430) buffer Buffer0\n"
					"{\n"
					"  uint result[];\n"
					"} b0;\n"
					"layout(binding = 4, std430) buffer Buffer4\n"
					"{\n"
					"  uint tempBuffer[];\n"
					"} b4;\n"
					"layout(binding = 5, std430) buffer Buffer5\n"
					"{\n"
					"  uint subgroupID;\n"
					"} b5;\n"
					"layout(binding = 6, std430) buffer Buffer6\n"
					"{\n"
					"  uint value;\n"
					"} b6;\n"
					"layout(binding = 0, r32ui) uniform uimage2D tempImage0;\n"
					"void main (void)\n"
					"{\n"
					"  uint id = 0;\n"
					"  if (subgroupElect())\n"
					"  {\n"
					"    id = atomicAdd(b5.subgroupID, 1);\n"
					"  }\n"
					"  id = subgroupBroadcastFirst(id);\n"
					"  uint localId = id;\n"
					"  uint tempResult = 0;\n"
					+ bdyTemplate.specialize(bufferNameMapping) +
					"  b0.result[gl_VertexID] = tempResult;\n"
					"  float pixelSize = 2.0f/1024.0f;\n"
					"  float pixelPosition = pixelSize/2.0f - 1.0f;\n"
					"  gl_Position = vec4(float(gl_VertexID) * pixelSize + pixelPosition, 0.0f, 0.0f, 1.0f);\n"
					"  gl_PointSize = 1.0f;\n"
					"}\n";
				programCollection.add("vert") << glu::VertexSource(vertex);
			}

			{
				map<string, string> bufferNameMapping;
				bufferNameMapping.insert(pair<string, string>("SSBO1", "7"));
				bufferNameMapping.insert(pair<string, string>("SSBO2", "9"));
				bufferNameMapping.insert(pair<string, string>("IMG1", "1"));

				const string tesc =
					"#version 450\n"
					"#extension GL_KHR_shader_subgroup_basic: enable\n"
					"#extension GL_KHR_shader_subgroup_ballot: enable\n"
					"layout(vertices=1) out;\n"
					"layout(binding = 1, std430) buffer Buffer1\n"
					"{\n"
					"  uint result[];\n"
					"} b1;\n"
					"layout(binding = 7, std430) buffer Buffer7\n"
					"{\n"
					"  uint tempBuffer[];\n"
					"} b7;\n"
					"layout(binding = 8, std430) buffer Buffer8\n"
					"{\n"
					"  uint subgroupID;\n"
					"} b8;\n"
					"layout(binding = 9, std430) buffer Buffer9\n"
					"{\n"
					"  uint value;\n"
					"} b9;\n"
					"layout(binding = 1, r32ui) uniform uimage2D tempImage1;\n"
					"void main (void)\n"
					"{\n"
					"  uint id = 0;\n"
					"  if (subgroupElect())\n"
					"  {\n"
					"    id = atomicAdd(b8.subgroupID, 1);\n"
					"  }\n"
					"  id = subgroupBroadcastFirst(id);\n"
					"  uint localId = id;\n"
					"  uint tempResult = 0;\n"
					+ bdyTemplate.specialize(bufferNameMapping) +
					"  b1.result[gl_PrimitiveID] = tempResult;\n"
					"  if (gl_InvocationID == 0)\n"
					"  {\n"
					"    gl_TessLevelOuter[0] = 1.0f;\n"
					"    gl_TessLevelOuter[1] = 1.0f;\n"
					"  }\n"
					"  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
					"}\n";
				programCollection.add("tesc") << glu::TessellationControlSource(tesc);
			}

			{
				map<string, string> bufferNameMapping;
				bufferNameMapping.insert(pair<string, string>("SSBO1", "10"));
				bufferNameMapping.insert(pair<string, string>("SSBO2", "12"));
				bufferNameMapping.insert(pair<string, string>("IMG1", "2"));

				const string tese =
					"#version 450\n"
					"#extension GL_KHR_shader_subgroup_basic: enable\n"
					"#extension GL_KHR_shader_subgroup_ballot: enable\n"
					"layout(isolines) in;\n"
					"layout(binding = 2, std430) buffer Buffer2\n"
					"{\n"
					"  uint result[];\n"
					"} b2;\n"
					"layout(binding = 10, std430) buffer Buffer10\n"
					"{\n"
					"  uint tempBuffer[];\n"
					"} b10;\n"
					"layout(binding = 11, std430) buffer Buffer11\n"
					"{\n"
					"  uint subgroupID;\n"
					"} b11;\n"
					"layout(binding = 12, std430) buffer Buffer12\n"
					"{\n"
					"  uint value;\n"
					"} b12;\n"
					"layout(binding = 2, r32ui) uniform uimage2D tempImage2;\n"
					"void main (void)\n"
					"{\n"
					"  uint id = 0;\n"
					"  if (subgroupElect())\n"
					"  {\n"
					"    id = atomicAdd(b11.subgroupID, 1);\n"
					"  }\n"
					"  id = subgroupBroadcastFirst(id);\n"
					"  uint localId = id;\n"
					"  uint tempResult = 0;\n"
					+ bdyTemplate.specialize(bufferNameMapping) +
					"  b2.result[gl_PrimitiveID * 2 + uint(gl_TessCoord.x + 0.5)] = tempResult;\n"
					"  float pixelSize = 2.0f/1024.0f;\n""  gl_Position = gl_in[0].gl_Position + gl_TessCoord.x * pixelSize / 2.0f;\n"
					"}\n";
				programCollection.add("tese") << glu::TessellationEvaluationSource(tese);
			}
			{
				map<string, string> bufferNameMapping;
				bufferNameMapping.insert(pair<string, string>("SSBO1", "13"));
				bufferNameMapping.insert(pair<string, string>("SSBO2", "15"));
				bufferNameMapping.insert(pair<string, string>("IMG1", "3"));

				const string geometry =
					"#version 450\n"
					"#extension GL_KHR_shader_subgroup_basic: enable\n"
					"#extension GL_KHR_shader_subgroup_ballot: enable\n"
					"layout(${TOPOLOGY}) in;\n"
					"layout(points, max_vertices = 1) out;\n"
					"layout(binding = 3, std430) buffer Buffer3\n"
					"{\n"
					"  uint result[];\n"
					"} b3;\n"
					"layout(binding = 13, std430) buffer Buffer13\n"
					"{\n"
					"  uint tempBuffer[];\n"
					"} b13;\n"
					"layout(binding = 14, std430) buffer Buffer14\n"
					"{\n"
					"  uint subgroupID;\n"
					"} b14;\n"
					"layout(binding = 15, std430) buffer Buffer15\n"
					"{\n"
					"  uint value;\n"
					"} b15;\n"
					"layout(binding = 3, r32ui) uniform uimage2D tempImage3;\n"
					"void main (void)\n"
					"{\n"
					"  uint id = 0;\n"
					"  if (subgroupElect())\n"
					"  {\n"
					"    id = atomicAdd(b14.subgroupID, 1);\n"
					"  }\n"
					"  id = subgroupBroadcastFirst(id);\n"
					"  uint localId = id;\n"
					"  uint tempResult = 0;\n"
					 + bdyTemplate.specialize(bufferNameMapping) +
					"  b3.result[gl_PrimitiveIDIn] = tempResult;\n"
					"  gl_Position = gl_in[0].gl_Position;\n"
					"  EmitVertex();\n"
					"  EndPrimitive();\n"
					"}\n";
				subgroups::addGeometryShadersFromTemplate(geometry, programCollection);
			}

			{
				map<string, string> bufferNameMapping;
				bufferNameMapping.insert(pair<string, string>("SSBO1", "16"));
				bufferNameMapping.insert(pair<string, string>("SSBO2", "18"));
				bufferNameMapping.insert(pair<string, string>("IMG1", "4"));

				const string fragment =
					"#version 450\n"
					"#extension GL_KHR_shader_subgroup_basic: enable\n"
					"#extension GL_KHR_shader_subgroup_ballot: enable\n"
					"layout(location = 0) out uint result;\n"
					"layout(binding = 16, std430) buffer Buffer16\n"
					"{\n"
					"  uint tempBuffer[];\n"
					"} b16;\n"
					"layout(binding = 17, std430) buffer Buffer17\n"
					"{\n"
					"  uint subgroupID;\n"
					"} b17;\n"
					"layout(binding = 18, std430) buffer Buffer18\n"
					"{\n"
					"  uint value;\n"
					"} b18;\n"
					"layout(binding = 4, r32ui) uniform uimage2D tempImage4;\n"
					"void main (void)\n"
					"{\n"
					"  if (gl_HelperInvocation) return;\n"
					"  uint id = 0;\n"
					"  if (subgroupElect())\n"
					"  {\n"
					"    id = atomicAdd(b17.subgroupID, 1);\n"
					"  }\n"
					"  id = subgroupBroadcastFirst(id);\n"
					"  uint localId = id;\n"
					"  uint tempResult = 0;\n"
					+ bdyTemplate.specialize(bufferNameMapping) +
					"  result = tempResult;\n"
					"}\n";
				programCollection.add("fragment") << glu::FragmentSource(fragment);
			}

		subgroups::addNoSubgroupShader(programCollection);
		}
	}
}

void supportedCheck (Context& context, CaseDefinition caseDef)
{
	DE_UNREF(caseDef);
	if (!subgroups::isSubgroupSupported(context))
		TCU_THROW(NotSupportedError, "Subgroup operations are not supported");
}

tcu::TestStatus noSSBOtest (Context& context, const CaseDefinition caseDef)
{
	if (!subgroups::areSubgroupOperationsSupportedForStage(
				context, caseDef.shaderStage))
	{
		if (subgroups::areSubgroupOperationsRequiredForStage(
					caseDef.shaderStage))
		{
			return tcu::TestStatus::fail(
					   "Shader stage " +
					   subgroups::getShaderStageName(caseDef.shaderStage) +
					   " is required to support subgroup operations!");
		}
		else
		{
			TCU_THROW(NotSupportedError, "Device does not support subgroup operations for this stage");
		}
	}

	if (!subgroups::isSubgroupFeatureSupportedForDevice(context, subgroups::SUBGROUP_FEATURE_BASIC_BIT))
	{
		return tcu::TestStatus::fail(
				   "Subgroup feature " +
				   subgroups::getSubgroupFeatureName(subgroups::SUBGROUP_FEATURE_BASIC_BIT) +
				   " is a required capability!");
	}

	if (OPTYPE_ELECT != caseDef.opType && subgroups::SHADER_STAGE_COMPUTE_BIT != caseDef.shaderStage)
	{
		if (!subgroups::isSubgroupFeatureSupportedForDevice(context, subgroups::SUBGROUP_FEATURE_BALLOT_BIT))
		{
			TCU_THROW(NotSupportedError, "Subgroup basic operation non-compute stage test required that ballot operations are supported!");
		}
	}

	const deUint32						inputDatasCount	= OPTYPE_SUBGROUP_MEMORY_BARRIER_IMAGE == caseDef.opType ? 3u : 2u;
	std::vector<subgroups::SSBOData>	inputDatas		(inputDatasCount);

	inputDatas[0].format = subgroups::FORMAT_R32_UINT;
	inputDatas[0].numElements = SHADER_BUFFER_SIZE/4ull;
	inputDatas[0].initializeType = subgroups::SSBOData::InitializeNonZero;
	inputDatas[0].binding = 0u;

	inputDatas[1].format = subgroups::FORMAT_R32_UINT;
	inputDatas[1].numElements = 1ull;
	inputDatas[1].initializeType = subgroups::SSBOData::InitializeNonZero;
	inputDatas[1].binding = 1u;

	if(OPTYPE_SUBGROUP_MEMORY_BARRIER_IMAGE == caseDef.opType )
	{
		inputDatas[2].format = subgroups::FORMAT_R32_UINT;
		inputDatas[2].numElements = SHADER_BUFFER_SIZE;
		inputDatas[2].initializeType = subgroups::SSBOData::InitializeNone;
		inputDatas[2].isImage = true;
		inputDatas[2].binding = 0u;
	}

	if (subgroups::SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		if (OPTYPE_ELECT == caseDef.opType)
			return subgroups::makeVertexFrameBufferTest(context, subgroups::FORMAT_R32G32_SFLOAT, DE_NULL, 0u, checkVertexPipelineStagesSubgroupElectNoSSBO);
		else
			return subgroups::makeVertexFrameBufferTest(context, subgroups::FORMAT_R32G32B32A32_SFLOAT, &inputDatas[0], inputDatasCount, checkVertexPipelineStagesSubgroupBarriersNoSSBO);
	}
	else if (subgroups::SHADER_STAGE_FRAGMENT_BIT == caseDef.shaderStage)
	{
		return subgroups::makeFragmentFrameBufferTest(context, subgroups::FORMAT_R32G32B32A32_SFLOAT, &inputDatas[0], inputDatasCount, checkFragmentSubgroupBarriersNoSSBO);
	}
	else if (subgroups::SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
	{
		if (OPTYPE_ELECT == caseDef.opType)
			return subgroups::makeGeometryFrameBufferTest(context, subgroups::FORMAT_R32G32_SFLOAT, DE_NULL, 0u, checkVertexPipelineStagesSubgroupElectNoSSBO);
		else
			return subgroups::makeGeometryFrameBufferTest(context, subgroups::FORMAT_R32G32B32A32_SFLOAT, &inputDatas[0], inputDatasCount, checkVertexPipelineStagesSubgroupBarriersNoSSBO);
	}

	if (OPTYPE_ELECT == caseDef.opType)
		return subgroups::makeTessellationEvaluationFrameBufferTest(context, subgroups::FORMAT_R32G32_SFLOAT, DE_NULL, 0u, checkVertexPipelineStagesSubgroupElectNoSSBO, caseDef.shaderStage);

	return subgroups::makeTessellationEvaluationFrameBufferTest(context, subgroups::FORMAT_R32G32B32A32_SFLOAT, &inputDatas[0], inputDatasCount,
		(subgroups::SHADER_STAGE_TESS_CONTROL_BIT == caseDef.shaderStage)? checkVertexPipelineStagesSubgroupBarriersNoSSBO : checkTessellationEvaluationSubgroupBarriersNoSSBO,
		caseDef.shaderStage);
}

tcu::TestStatus test(Context& context, const CaseDefinition caseDef)
{
	if (!subgroups::isSubgroupFeatureSupportedForDevice(context, subgroups::SUBGROUP_FEATURE_BASIC_BIT))
	{
		return tcu::TestStatus::fail(
					"Subgroup feature " +
					subgroups::getSubgroupFeatureName(subgroups::SUBGROUP_FEATURE_BASIC_BIT) +
					" is a required capability!");
	}

	if (OPTYPE_ELECT != caseDef.opType && subgroups::SHADER_STAGE_COMPUTE_BIT != caseDef.shaderStage)
	{
		if (!subgroups::isSubgroupFeatureSupportedForDevice(context, subgroups::SUBGROUP_FEATURE_BALLOT_BIT))
		{
			TCU_THROW(NotSupportedError, "Subgroup basic operation non-compute stage test required that ballot operations are supported!");
		}
	}

	if (subgroups::SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
	{
		if (!subgroups::areSubgroupOperationsSupportedForStage(context, caseDef.shaderStage))
		{
				return tcu::TestStatus::fail("Shader stage " +
										subgroups::getShaderStageName(caseDef.shaderStage) +
										" is required to support subgroup operations!");
		}

		if (OPTYPE_ELECT == caseDef.opType)
		{
			return subgroups::makeComputeTest(context, subgroups::FORMAT_R32_UINT, DE_NULL, 0, checkComputeSubgroupElect);
		}
		else
		{
			const deUint32 inputDatasCount = 3;
			subgroups::SSBOData inputDatas[inputDatasCount];
			inputDatas[0].format = subgroups::FORMAT_R32_UINT;
			inputDatas[0].numElements = SHADER_BUFFER_SIZE;
			inputDatas[0].initializeType = subgroups::SSBOData::InitializeNone;
			inputDatas[0].binding = 1u;

			inputDatas[1].format = subgroups::FORMAT_R32_UINT;
			inputDatas[1].numElements = 1;
			inputDatas[1].initializeType = subgroups::SSBOData::InitializeNonZero;
			inputDatas[1].binding = 2u;

			inputDatas[2].format = subgroups::FORMAT_R32_UINT;
			inputDatas[2].numElements = SHADER_BUFFER_SIZE;
			inputDatas[2].initializeType = subgroups::SSBOData::InitializeNone;
			inputDatas[2].isImage = true;
			inputDatas[2].binding = 0u;

			return subgroups::makeComputeTest(context, subgroups::FORMAT_R32_UINT, inputDatas, inputDatasCount, checkComputeSubgroupBarriers);
		}
	}
	else
	{
		if (!subgroups::isFragmentSSBOSupportedForDevice(context))
		{
			TCU_THROW(NotSupportedError, "Subgroup basic operation require that the fragment stage be able to write to SSBOs!");
		}

		int supportedStages = context.getDeqpContext().getContextInfo().getInt(GL_SUBGROUP_SUPPORTED_STAGES_KHR);

		subgroups::ShaderStageFlags stages = (subgroups::ShaderStageFlags)(caseDef.shaderStage & supportedStages);

		if ( subgroups::SHADER_STAGE_FRAGMENT_BIT != stages && !subgroups::isVertexSSBOSupportedForDevice(context))
		{
			if ( (stages & subgroups::SHADER_STAGE_FRAGMENT_BIT) == 0)
				TCU_THROW(NotSupportedError, "Device does not support vertex stage SSBO writes");
			else
				stages = subgroups::SHADER_STAGE_FRAGMENT_BIT;
		}

		if ((subgroups::ShaderStageFlags)0u == stages)
			TCU_THROW(NotSupportedError, "Subgroup operations are not supported for any graphic shader");

		if (OPTYPE_ELECT == caseDef.opType)
		{
			const deUint32 inputCount = 5u;
			subgroups::SSBOData inputData[inputCount];

			inputData[0].format			= subgroups::FORMAT_R32_UINT;
			inputData[0].numElements	= 1;
			inputData[0].initializeType	= subgroups::SSBOData::InitializeZero;
			inputData[0].binding		= 4u;
			inputData[0].stages			= subgroups::SHADER_STAGE_VERTEX_BIT;

			inputData[1].format			= subgroups::FORMAT_R32_UINT;
			inputData[1].numElements	= 1;
			inputData[1].initializeType	= subgroups::SSBOData::InitializeZero;
			inputData[1].binding		= 5u;
			inputData[1].stages			= subgroups::SHADER_STAGE_TESS_CONTROL_BIT;

			inputData[2].format			= subgroups::FORMAT_R32_UINT;
			inputData[2].numElements	= 1;
			inputData[2].initializeType	= subgroups::SSBOData::InitializeZero;
			inputData[2].binding		= 6u;
			inputData[2].stages			= subgroups::SHADER_STAGE_TESS_EVALUATION_BIT;

			inputData[3].format			= subgroups::FORMAT_R32_UINT;
			inputData[3].numElements	= 1;
			inputData[3].initializeType	= subgroups::SSBOData::InitializeZero;
			inputData[3].binding		= 7u;
			inputData[3].stages			= subgroups::SHADER_STAGE_GEOMETRY_BIT;

			inputData[4].format			= subgroups::FORMAT_R32_UINT;
			inputData[4].numElements	= 1;
			inputData[4].initializeType	= subgroups::SSBOData::InitializeZero;
			inputData[4].binding		= 8u;
			inputData[4].stages			= subgroups::SHADER_STAGE_FRAGMENT_BIT;

			return subgroups::allStages(context, subgroups::FORMAT_R32_UINT, inputData, inputCount, checkVertexPipelineStagesSubgroupElect, stages);
		}
		else
		{
			const subgroups::ShaderStageFlags stagesBits[] =
			{
				subgroups::SHADER_STAGE_VERTEX_BIT,
				subgroups::SHADER_STAGE_TESS_CONTROL_BIT,
				subgroups::SHADER_STAGE_TESS_EVALUATION_BIT,
				subgroups::SHADER_STAGE_GEOMETRY_BIT,
				subgroups::SHADER_STAGE_FRAGMENT_BIT,
			};

			const deUint32 inputDatasCount = DE_LENGTH_OF_ARRAY(stagesBits) * 4u;
			subgroups::SSBOData inputDatas[inputDatasCount];

			for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(stagesBits); ++ndx)
			{
				const deUint32 index = ndx*4;
				const deUint32 ssboIndex = ndx*3;
				const deUint32 imgIndex = ndx;
				inputDatas[index].format				= subgroups::FORMAT_R32_UINT;
				inputDatas[index].numElements			= SHADER_BUFFER_SIZE;
				inputDatas[index].initializeType		= subgroups::SSBOData::InitializeNonZero;
				inputDatas[index].binding				= ssboIndex + 4u;
				inputDatas[index].stages				= stagesBits[ndx];

				inputDatas[index + 1].format			= subgroups::FORMAT_R32_UINT;
				inputDatas[index + 1].numElements		= 1;
				inputDatas[index + 1].initializeType	= subgroups::SSBOData::InitializeZero;
				inputDatas[index + 1].binding			= ssboIndex + 5u;
				inputDatas[index + 1].stages			= stagesBits[ndx];

				inputDatas[index + 2].format			= subgroups::FORMAT_R32_UINT;
				inputDatas[index + 2].numElements		= 1;
				inputDatas[index + 2].initializeType	= subgroups::SSBOData::InitializeNonZero;
				inputDatas[index + 2].binding			= ssboIndex + 6u;
				inputDatas[index + 2].stages			= stagesBits[ndx];

				inputDatas[index + 3].format			= subgroups::FORMAT_R32_UINT;
				inputDatas[index + 3].numElements		= SHADER_BUFFER_SIZE;
				inputDatas[index + 3].initializeType	= subgroups::SSBOData::InitializeNone;
				inputDatas[index + 3].isImage			= true;
				inputDatas[index + 3].binding			= imgIndex;
				inputDatas[index + 3].stages			= stagesBits[ndx];
			}

			return subgroups::allStages(context, subgroups::FORMAT_R32_UINT, inputDatas, inputDatasCount, checkVertexPipelineStagesSubgroupBarriers, stages);
		}
	}
}
}

deqp::TestCaseGroup* createSubgroupsBasicTests(deqp::Context& testCtx)
{
	de::MovePtr<deqp::TestCaseGroup> graphicGroup(new deqp::TestCaseGroup(
		testCtx, "graphics", "Subgroup basic category tests: graphics"));
	de::MovePtr<deqp::TestCaseGroup> computeGroup(new deqp::TestCaseGroup(
		testCtx, "compute", "Subgroup basic category tests: compute"));
	de::MovePtr<deqp::TestCaseGroup> framebufferGroup(new deqp::TestCaseGroup(
		testCtx, "framebuffer", "Subgroup basic category tests: framebuffer"));

	const subgroups::ShaderStageFlags stages[] =
	{
		SHADER_STAGE_FRAGMENT_BIT,
		SHADER_STAGE_VERTEX_BIT,
		SHADER_STAGE_TESS_EVALUATION_BIT,
		SHADER_STAGE_TESS_CONTROL_BIT,
		SHADER_STAGE_GEOMETRY_BIT,
	};

	for (int opTypeIndex = 0; opTypeIndex < OPTYPE_LAST; ++opTypeIndex)
	{
		const std::string op = de::toLower(getOpTypeName(opTypeIndex));

		{
			const CaseDefinition caseDef = {opTypeIndex, SHADER_STAGE_COMPUTE_BIT};
			SubgroupFactory<CaseDefinition>::addFunctionCaseWithPrograms(computeGroup.get(), op, "",
										supportedCheck, initPrograms, test, caseDef);
		}

		if (OPTYPE_SUBGROUP_MEMORY_BARRIER_SHARED == opTypeIndex)
		{
			// Shared isn't available in non compute shaders.
			continue;
		}

		{
			const CaseDefinition caseDef = {opTypeIndex, SHADER_STAGE_ALL_GRAPHICS};
			SubgroupFactory<CaseDefinition>::addFunctionCaseWithPrograms(graphicGroup.get(),
										op, "",
										supportedCheck, initPrograms, test, caseDef);
		}

		if (OPTYPE_ELECT == opTypeIndex)
		{
			for (int stageIndex = 1; stageIndex < DE_LENGTH_OF_ARRAY(stages); ++stageIndex)
			{
				const CaseDefinition caseDef = {opTypeIndex, stages[stageIndex]};
				SubgroupFactory<CaseDefinition>::addFunctionCaseWithPrograms(framebufferGroup.get(),
							op + "_" + getShaderStageName(caseDef.shaderStage), "",
							supportedCheck, initFrameBufferPrograms, noSSBOtest, caseDef);
			}
		}
		else
		{
			for (int stageIndex = 0; stageIndex < DE_LENGTH_OF_ARRAY(stages); ++stageIndex)
			{
				const CaseDefinition caseDefFrag = {opTypeIndex, stages[stageIndex]};
				SubgroupFactory<CaseDefinition>::addFunctionCaseWithPrograms(framebufferGroup.get(),
							op + "_" + getShaderStageName(caseDefFrag.shaderStage), "",
							supportedCheck, initFrameBufferPrograms, noSSBOtest, caseDefFrag);
			}
		}

	}

	de::MovePtr<deqp::TestCaseGroup> group(new deqp::TestCaseGroup(
		testCtx, "basic", "Subgroup basic category tests"));

	group->addChild(graphicGroup.release());
	group->addChild(computeGroup.release());
	group->addChild(framebufferGroup.release());

	return group.release();
}

} // subgroups
} // glc
