/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 The Khronos Group Inc.
 * Copyright (c) 2017 Codeplay Software Ltd.
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

#include "vktSubgroupsClusteredTests.hpp"
#include "vktSubgroupsTestsUtils.hpp"

#include <string>
#include <vector>

using namespace tcu;
using namespace std;
using namespace vk;
using namespace vkt;

namespace
{
enum OpType
{
	OPTYPE_CLUSTERED_ADD = 0,
	OPTYPE_CLUSTERED_MUL,
	OPTYPE_CLUSTERED_MIN,
	OPTYPE_CLUSTERED_MAX,
	OPTYPE_CLUSTERED_AND,
	OPTYPE_CLUSTERED_OR,
	OPTYPE_CLUSTERED_XOR,
	OPTYPE_CLUSTERED_LAST
};

static bool checkVertexPipelineStages(std::vector<const void*> datas,
									  deUint32 width, deUint32)
{
	const deUint32* data =
		reinterpret_cast<const deUint32*>(datas[0]);
	for (deUint32 x = 0; x < width; ++x)
	{
		deUint32 val = data[x];

		if (0x1 != val)
		{
			return false;
		}
	}

	return true;
}

static bool checkFragment(std::vector<const void*> datas,
						  deUint32 width, deUint32 height, deUint32)
{
	const deUint32* data =
		reinterpret_cast<const deUint32*>(datas[0]);
	for (deUint32 x = 0; x < width; ++x)
	{
		for (deUint32 y = 0; y < height; ++y)
		{
			deUint32 val = data[x * height + y];

			if (0x1 != val)
			{
				return false;
			}
		}
	}

	return true;
}

static bool checkCompute(std::vector<const void*> datas,
						 const deUint32 numWorkgroups[3], const deUint32 localSize[3],
						 deUint32)
{
	const deUint32* data =
		reinterpret_cast<const deUint32*>(datas[0]);

	for (deUint32 nX = 0; nX < numWorkgroups[0]; ++nX)
	{
		for (deUint32 nY = 0; nY < numWorkgroups[1]; ++nY)
		{
			for (deUint32 nZ = 0; nZ < numWorkgroups[2]; ++nZ)
			{
				for (deUint32 lX = 0; lX < localSize[0]; ++lX)
				{
					for (deUint32 lY = 0; lY < localSize[1]; ++lY)
					{
						for (deUint32 lZ = 0; lZ < localSize[2];
								++lZ)
						{
							const deUint32 globalInvocationX =
								nX * localSize[0] + lX;
							const deUint32 globalInvocationY =
								nY * localSize[1] + lY;
							const deUint32 globalInvocationZ =
								nZ * localSize[2] + lZ;

							const deUint32 globalSizeX =
								numWorkgroups[0] * localSize[0];
							const deUint32 globalSizeY =
								numWorkgroups[1] * localSize[1];

							const deUint32 offset =
								globalSizeX *
								((globalSizeY *
								  globalInvocationZ) +
								 globalInvocationY) +
								globalInvocationX;

							if (0x1 != data[offset])
							{
								return false;
							}
						}
					}
				}
			}
		}
	}

	return true;
}

std::string getOpTypeName(int opType)
{
	switch (opType)
	{
		default:
			DE_FATAL("Unsupported op type");
		case OPTYPE_CLUSTERED_ADD:
			return "subgroupClusteredAdd";
		case OPTYPE_CLUSTERED_MUL:
			return "subgroupClusteredMul";
		case OPTYPE_CLUSTERED_MIN:
			return "subgroupClusteredMin";
		case OPTYPE_CLUSTERED_MAX:
			return "subgroupClusteredMax";
		case OPTYPE_CLUSTERED_AND:
			return "subgroupClusteredAnd";
		case OPTYPE_CLUSTERED_OR:
			return "subgroupClusteredOr";
		case OPTYPE_CLUSTERED_XOR:
			return "subgroupClusteredXor";
	}
}

std::string getOpTypeOperation(int opType, vk::VkFormat format, std::string lhs, std::string rhs)
{
	switch (opType)
	{
		default:
			DE_FATAL("Unsupported op type");
		case OPTYPE_CLUSTERED_ADD:
			return lhs + " + " + rhs;
		case OPTYPE_CLUSTERED_MUL:
			return lhs + " * " + rhs;
		case OPTYPE_CLUSTERED_MIN:
			switch (format)
			{
				default:
					return "min(" + lhs + ", " + rhs + ")";
				case VK_FORMAT_R32_SFLOAT:
				case VK_FORMAT_R64_SFLOAT:
					return "(isnan(" + lhs + ") ? " + rhs + " : (isnan(" + rhs + ") ? " + lhs + " : min(" + lhs + ", " + rhs + ")))";
				case VK_FORMAT_R32G32_SFLOAT:
				case VK_FORMAT_R32G32B32_SFLOAT:
				case VK_FORMAT_R32G32B32A32_SFLOAT:
				case VK_FORMAT_R64G64_SFLOAT:
				case VK_FORMAT_R64G64B64_SFLOAT:
				case VK_FORMAT_R64G64B64A64_SFLOAT:
					return "mix(mix(min(" + lhs + ", " + rhs + "), " + lhs + ", isnan(" + rhs + ")), " + rhs + ", isnan(" + lhs + "))";
			}
		case OPTYPE_CLUSTERED_MAX:
			switch (format)
			{
				default:
					return "max(" + lhs + ", " + rhs + ")";
				case VK_FORMAT_R32_SFLOAT:
				case VK_FORMAT_R64_SFLOAT:
					return "(isnan(" + lhs + ") ? " + rhs + " : (isnan(" + rhs + ") ? " + lhs + " : max(" + lhs + ", " + rhs + ")))";
				case VK_FORMAT_R32G32_SFLOAT:
				case VK_FORMAT_R32G32B32_SFLOAT:
				case VK_FORMAT_R32G32B32A32_SFLOAT:
				case VK_FORMAT_R64G64_SFLOAT:
				case VK_FORMAT_R64G64B64_SFLOAT:
				case VK_FORMAT_R64G64B64A64_SFLOAT:
					return "mix(mix(max(" + lhs + ", " + rhs + "), " + lhs + ", isnan(" + rhs + ")), " + rhs + ", isnan(" + lhs + "))";
			}
		case OPTYPE_CLUSTERED_AND:
			switch (format)
			{
				default:
					return lhs + " & " + rhs;
				case VK_FORMAT_R8_USCALED:
					return lhs + " && " + rhs;
				case VK_FORMAT_R8G8_USCALED:
					return "bvec2(" + lhs + ".x && " + rhs + ".x, " + lhs + ".y && " + rhs + ".y)";
				case VK_FORMAT_R8G8B8_USCALED:
					return "bvec3(" + lhs + ".x && " + rhs + ".x, " + lhs + ".y && " + rhs + ".y, " + lhs + ".z && " + rhs + ".z)";
				case VK_FORMAT_R8G8B8A8_USCALED:
					return "bvec4(" + lhs + ".x && " + rhs + ".x, " + lhs + ".y && " + rhs + ".y, " + lhs + ".z && " + rhs + ".z, " + lhs + ".w && " + rhs + ".w)";
			}
		case OPTYPE_CLUSTERED_OR:
			switch (format)
			{
				default:
					return lhs + " | " + rhs;
				case VK_FORMAT_R8_USCALED:
					return lhs + " || " + rhs;
				case VK_FORMAT_R8G8_USCALED:
					return "bvec2(" + lhs + ".x || " + rhs + ".x, " + lhs + ".y || " + rhs + ".y)";
				case VK_FORMAT_R8G8B8_USCALED:
					return "bvec3(" + lhs + ".x || " + rhs + ".x, " + lhs + ".y || " + rhs + ".y, " + lhs + ".z || " + rhs + ".z)";
				case VK_FORMAT_R8G8B8A8_USCALED:
					return "bvec4(" + lhs + ".x || " + rhs + ".x, " + lhs + ".y || " + rhs + ".y, " + lhs + ".z || " + rhs + ".z, " + lhs + ".w || " + rhs + ".w)";
			}
		case OPTYPE_CLUSTERED_XOR:
			switch (format)
			{
				default:
					return lhs + " ^ " + rhs;
				case VK_FORMAT_R8_USCALED:
					return lhs + " ^^ " + rhs;
				case VK_FORMAT_R8G8_USCALED:
					return "bvec2(" + lhs + ".x ^^ " + rhs + ".x, " + lhs + ".y ^^ " + rhs + ".y)";
				case VK_FORMAT_R8G8B8_USCALED:
					return "bvec3(" + lhs + ".x ^^ " + rhs + ".x, " + lhs + ".y ^^ " + rhs + ".y, " + lhs + ".z ^^ " + rhs + ".z)";
				case VK_FORMAT_R8G8B8A8_USCALED:
					return "bvec4(" + lhs + ".x ^^ " + rhs + ".x, " + lhs + ".y ^^ " + rhs + ".y, " + lhs + ".z ^^ " + rhs + ".z, " + lhs + ".w ^^ " + rhs + ".w)";
			}
	}
}


std::string getIdentity(int opType, vk::VkFormat format)
{
	bool isFloat = false;
	bool isInt = false;
	bool isUnsigned = false;

	switch (format)
	{
		default:
			DE_FATAL("Unhandled format!");
		case VK_FORMAT_R32_SINT:
		case VK_FORMAT_R32G32_SINT:
		case VK_FORMAT_R32G32B32_SINT:
		case VK_FORMAT_R32G32B32A32_SINT:
			isInt = true;
			break;
		case VK_FORMAT_R32_UINT:
		case VK_FORMAT_R32G32_UINT:
		case VK_FORMAT_R32G32B32_UINT:
		case VK_FORMAT_R32G32B32A32_UINT:
			isUnsigned = true;
			break;
		case VK_FORMAT_R32_SFLOAT:
		case VK_FORMAT_R32G32_SFLOAT:
		case VK_FORMAT_R32G32B32_SFLOAT:
		case VK_FORMAT_R32G32B32A32_SFLOAT:
		case VK_FORMAT_R64_SFLOAT:
		case VK_FORMAT_R64G64_SFLOAT:
		case VK_FORMAT_R64G64B64_SFLOAT:
		case VK_FORMAT_R64G64B64A64_SFLOAT:
			isFloat = true;
			break;
		case VK_FORMAT_R8_USCALED:
		case VK_FORMAT_R8G8_USCALED:
		case VK_FORMAT_R8G8B8_USCALED:
		case VK_FORMAT_R8G8B8A8_USCALED:
			break; // bool types are not anything
	}

	switch (opType)
	{
		default:
			DE_FATAL("Unsupported op type");
		case OPTYPE_CLUSTERED_ADD:
			return subgroups::getFormatNameForGLSL(format) + "(0)";
		case OPTYPE_CLUSTERED_MUL:
			return subgroups::getFormatNameForGLSL(format) + "(1)";
		case OPTYPE_CLUSTERED_MIN:
			if (isFloat)
			{
				return subgroups::getFormatNameForGLSL(format) + "(intBitsToFloat(0x7f800000))";
			}
			else if (isInt)
			{
				return subgroups::getFormatNameForGLSL(format) + "(0x7fffffff)";
			}
			else if (isUnsigned)
			{
				return subgroups::getFormatNameForGLSL(format) + "(0xffffffffu)";
			}
			else
			{
				DE_FATAL("Unhandled case");
			}
		case OPTYPE_CLUSTERED_MAX:
			if (isFloat)
			{
				return subgroups::getFormatNameForGLSL(format) + "(intBitsToFloat(0xff800000))";
			}
			else if (isInt)
			{
				return subgroups::getFormatNameForGLSL(format) + "(0x80000000)";
			}
			else if (isUnsigned)
			{
				return subgroups::getFormatNameForGLSL(format) + "(0)";
			}
			else
			{
				DE_FATAL("Unhandled case");
			}
		case OPTYPE_CLUSTERED_AND:
			return subgroups::getFormatNameForGLSL(format) + "(~0)";
		case OPTYPE_CLUSTERED_OR:
			return subgroups::getFormatNameForGLSL(format) + "(0)";
		case OPTYPE_CLUSTERED_XOR:
			return subgroups::getFormatNameForGLSL(format) + "(0)";
	}
}

std::string getCompare(int opType, vk::VkFormat format, std::string lhs, std::string rhs)
{
	std::string formatName = subgroups::getFormatNameForGLSL(format);
	switch (format)
	{
		default:
			return "all(equal(" + lhs + ", " + rhs + "))";
		case VK_FORMAT_R8_USCALED:
		case VK_FORMAT_R32_UINT:
		case VK_FORMAT_R32_SINT:
			return "(" + lhs + " == " + rhs + ")";
		case VK_FORMAT_R32_SFLOAT:
		case VK_FORMAT_R64_SFLOAT:
			switch (opType)
			{
				default:
					return "(abs(" + lhs + " - " + rhs + ") < 0.00001)";
				case OPTYPE_CLUSTERED_MIN:
				case OPTYPE_CLUSTERED_MAX:
					return "(" + lhs + " == " + rhs + ")";
			}
		case VK_FORMAT_R32G32_SFLOAT:
		case VK_FORMAT_R32G32B32_SFLOAT:
		case VK_FORMAT_R32G32B32A32_SFLOAT:
		case VK_FORMAT_R64G64_SFLOAT:
		case VK_FORMAT_R64G64B64_SFLOAT:
		case VK_FORMAT_R64G64B64A64_SFLOAT:
			switch (opType)
			{
				default:
					return "all(lessThan(abs(" + lhs + " - " + rhs + "), " + formatName + "(0.00001)))";
				case OPTYPE_CLUSTERED_MIN:
				case OPTYPE_CLUSTERED_MAX:
					return "all(equal(" + lhs + ", " + rhs + "))";
			}
	}
}

struct CaseDefinition
{
	int					opType;
	VkShaderStageFlags	shaderStage;
	VkFormat			format;
	bool				noSSBO;
};

void initFrameBufferPrograms (SourceCollections& programCollection, CaseDefinition caseDef)
{
	std::ostringstream bdy;

	bdy << "  bool tempResult = true;\n";

	for (deUint32 i = 1; i <= subgroups::maxSupportedSubgroupSize(); i *= 2)
	{
		bdy	<< "  {\n"
			<< "    const uint clusterSize = " << i << ";\n"
			<< "    if (clusterSize <= gl_SubgroupSize)\n"
			<< "    {\n"
			<< "      " << subgroups::getFormatNameForGLSL(caseDef.format) << " op = "
			<< getOpTypeName(caseDef.opType) + "(data[gl_SubgroupInvocationID], clusterSize);\n"
			<< "      for (uint clusterOffset = 0; clusterOffset < gl_SubgroupSize; clusterOffset += clusterSize)\n"
			<< "      {\n"
			<< "        " << subgroups::getFormatNameForGLSL(caseDef.format) << " ref = "
			<< getIdentity(caseDef.opType, caseDef.format) << ";\n"
			<< "        for (uint index = clusterOffset; index < (clusterOffset + clusterSize); index++)\n"
			<< "        {\n"
			<< "          if (subgroupBallotBitExtract(mask, index))\n"
			<< "          {\n"
			<< "            ref = " << getOpTypeOperation(caseDef.opType, caseDef.format, "ref", "data[index]") << ";\n"
			<< "          }\n"
			<< "        }\n"
			<< "        if ((clusterOffset <= gl_SubgroupInvocationID) && (gl_SubgroupInvocationID < (clusterOffset + clusterSize)))\n"
			<< "        {\n"
			<< "          if (!" << getCompare(caseDef.opType, caseDef.format, "ref", "op") << ")\n"
			<< "          {\n"
			<< "            tempResult = false;\n"
			<< "          }\n"
			<< "        }\n"
			<< "      }\n"
			<< "    }\n"
			<< "  }\n";
	}

	if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		std::ostringstream src;
		std::ostringstream	fragmentSrc;

		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450 )<< "\n"
			<< "#extension GL_KHR_shader_subgroup_clustered: enable\n"
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< "layout(location = 0) in highp vec4 in_position;\n"
			<< "layout(location = 0) out float out_color;\n"
			<< "layout(set = 0, binding = 0) uniform Buffer1\n"
			<< "{\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data[" << subgroups::maxSupportedSubgroupSize() << "];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  uvec4 mask = subgroupBallot(true);\n"
			<< bdy.str()
			<< "  out_color = float(tempResult ? 1 : 0);\n"
			<< "  gl_Position = in_position;\n"
			<< "}\n";

		programCollection.glslSources.add("vert") << glu::VertexSource(src.str());

		fragmentSrc << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
			<< "layout(location = 0) in float in_color;\n"
			<< "layout(location = 0) out uint out_color;\n"
			<< "void main()\n"
			<<"{\n"
			<< "	out_color = uint(in_color);\n"
			<< "}\n";
		programCollection.glslSources.add("fragment") << glu::FragmentSource(fragmentSrc.str());
	}
	else
	{
		DE_FATAL("Unsupported shader stage");
	}
}

void initPrograms(SourceCollections& programCollection, CaseDefinition caseDef)
{
	std::ostringstream bdy;

	bdy << "  bool tempResult = true;\n";

	for (deUint32 i = 1; i <= subgroups::maxSupportedSubgroupSize(); i *= 2)
	{
		bdy	<< "  {\n"
			<< "    const uint clusterSize = " << i << ";\n"
			<< "    if (clusterSize <= gl_SubgroupSize)\n"
			<< "    {\n"
			<< "      " << subgroups::getFormatNameForGLSL(caseDef.format) << " op = "
			<< getOpTypeName(caseDef.opType) + "(data[gl_SubgroupInvocationID], clusterSize);\n"
			<< "      for (uint clusterOffset = 0; clusterOffset < gl_SubgroupSize; clusterOffset += clusterSize)\n"
			<< "      {\n"
			<< "        " << subgroups::getFormatNameForGLSL(caseDef.format) << " ref = "
			<< getIdentity(caseDef.opType, caseDef.format) << ";\n"
			<< "        for (uint index = clusterOffset; index < (clusterOffset + clusterSize); index++)\n"
			<< "        {\n"
			<< "          if (subgroupBallotBitExtract(mask, index))\n"
			<< "          {\n"
			<< "            ref = " << getOpTypeOperation(caseDef.opType, caseDef.format, "ref", "data[index]") << ";\n"
			<< "          }\n"
			<< "        }\n"
			<< "        if ((clusterOffset <= gl_SubgroupInvocationID) && (gl_SubgroupInvocationID < (clusterOffset + clusterSize)))\n"
			<< "        {\n"
			<< "          if (!" << getCompare(caseDef.opType, caseDef.format, "ref", "op") << ")\n"
			<< "          {\n"
			<< "            tempResult = false;\n"
			<< "          }\n"
			<< "        }\n"
			<< "      }\n"
			<< "    }\n"
			<< "  }\n";
	}

	if (VK_SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
	{
		std::ostringstream src;

		src << "#version 450\n"
			<< "#extension GL_KHR_shader_subgroup_clustered: enable\n"
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< "layout (local_size_x_id = 0, local_size_y_id = 1, "
			"local_size_z_id = 2) in;\n"
			<< "layout(set = 0, binding = 0, std430) buffer Buffer1\n"
			<< "{\n"
			<< "  uint result[];\n"
			<< "};\n"
			<< "layout(set = 0, binding = 1, std430) buffer Buffer2\n"
			<< "{\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data[];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  uvec3 globalSize = gl_NumWorkGroups * gl_WorkGroupSize;\n"
			<< "  highp uint offset = globalSize.x * ((globalSize.y * "
			"gl_GlobalInvocationID.z) + gl_GlobalInvocationID.y) + "
			"gl_GlobalInvocationID.x;\n"
			<< "  uvec4 mask = subgroupBallot(true);\n"
			<< bdy.str()
			<< "  result[offset] = tempResult ? 1 : 0;\n"
			<< "}\n";

		programCollection.glslSources.add("comp")
				<< glu::ComputeSource(src.str());
	}
	else if (VK_SHADER_STAGE_FRAGMENT_BIT == caseDef.shaderStage)
	{
		programCollection.glslSources.add("vert")
				<< glu::VertexSource(subgroups::getVertShaderForStage(caseDef.shaderStage));

		std::ostringstream frag;

		frag << "#version 450\n"
			 << "#extension GL_KHR_shader_subgroup_clustered: enable\n"
			 << "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			 << "layout(location = 0) out uint result;\n"
			 << "layout(set = 0, binding = 0, std430) readonly buffer Buffer2\n"
			 << "{\n"
			 << "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data[];\n"
			 << "};\n"
			 << "void main (void)\n"
			 << "{\n"
			 << "  uvec4 mask = subgroupBallot(true);\n"
			 << bdy.str()
			 << "  result = tempResult ? 1 : 0;\n"
			 << "}\n";

		programCollection.glslSources.add("frag")
				<< glu::FragmentSource(frag.str());
	}
	else if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		std::ostringstream src;

		src << "#version 450\n"
			<< "#extension GL_KHR_shader_subgroup_clustered: enable\n"
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< "layout(set = 0, binding = 0, std430) buffer Buffer1\n"
			<< "{\n"
			<< "  uint result[];\n"
			<< "};\n"
			<< "layout(set = 0, binding = 1, std430) buffer Buffer2\n"
			<< "{\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data[];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  uvec4 mask = subgroupBallot(true);\n"
			<< bdy.str()
			<< "  result[gl_VertexIndex] = tempResult ? 1 : 0;\n"
			<< "}\n";

		programCollection.glslSources.add("vert")
				<< glu::VertexSource(src.str());
	}
	else if (VK_SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
	{
		programCollection.glslSources.add("vert")
				<< glu::VertexSource(subgroups::getVertShaderForStage(caseDef.shaderStage));

		std::ostringstream src;

		src << "#version 450\n"
			<< "#extension GL_KHR_shader_subgroup_clustered: enable\n"
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< "layout(points) in;\n"
			<< "layout(points, max_vertices = 1) out;\n"
			<< "layout(set = 0, binding = 0, std430) buffer Buffer1\n"
			<< "{\n"
			<< "  uint result[];\n"
			<< "};\n"
			<< "layout(set = 0, binding = 1, std430) buffer Buffer2\n"
			<< "{\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data[];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  uvec4 mask = subgroupBallot(true);\n"
			<< bdy.str()
			<< "  result[gl_PrimitiveIDIn] = tempResult ? 1 : 0;\n"
			<< "}\n";

		programCollection.glslSources.add("geom")
				<< glu::GeometrySource(src.str());
	}
	else if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
	{
		programCollection.glslSources.add("vert")
				<< glu::VertexSource(subgroups::getVertShaderForStage(caseDef.shaderStage));

		programCollection.glslSources.add("tese")
				<< glu::TessellationEvaluationSource("#version 450\nlayout(isolines) in;\nvoid main (void) {}\n");

		std::ostringstream src;

		src << "#version 450\n"
			<< "#extension GL_KHR_shader_subgroup_clustered: enable\n"
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< "layout(vertices=1) out;\n"
			<< "layout(set = 0, binding = 0, std430) buffer Buffer1\n"
			<< "{\n"
			<< "  uint result[];\n"
			<< "};\n"
			<< "layout(set = 0, binding = 1, std430) buffer Buffer2\n"
			<< "{\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data[];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  uvec4 mask = subgroupBallot(true);\n"
			<< bdy.str()
			<< "  result[gl_PrimitiveID] = tempResult ? 1 : 0;\n"
			<< "}\n";

		programCollection.glslSources.add("tesc")
				<< glu::TessellationControlSource(src.str());
	}
	else if (VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT == caseDef.shaderStage)
	{
		programCollection.glslSources.add("vert")
				<< glu::VertexSource(subgroups::getVertShaderForStage(caseDef.shaderStage));

		programCollection.glslSources.add("tesc")
				<< glu::TessellationControlSource("#version 450\nlayout(vertices=1) out;\nvoid main (void) { for(uint i = 0; i < 4; i++) { gl_TessLevelOuter[i] = 1.0f; } }\n");

		std::ostringstream src;

		src << "#version 450\n"
			<< "#extension GL_KHR_shader_subgroup_clustered: enable\n"
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< "layout(isolines) in;\n"
			<< "layout(set = 0, binding = 0, std430) buffer Buffer1\n"
			<< "{\n"
			<< "  uint result[];\n"
			<< "};\n"
			<< "layout(set = 0, binding = 1, std430) buffer Buffer2\n"
			<< "{\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data[];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  uvec4 mask = subgroupBallot(true);\n"
			<< bdy.str()
			<< "  result[gl_PrimitiveID * 2 + uint(gl_TessCoord.x + 0.5)] = tempResult ? 1 : 0;\n"
			<< "}\n";

		programCollection.glslSources.add("tese")
				<< glu::TessellationEvaluationSource(src.str());
	}
	else
	{
		DE_FATAL("Unsupported shader stage");
	}
}

tcu::TestStatus test(Context& context, const CaseDefinition caseDef)
{
	if (!subgroups::isSubgroupSupported(context))
		TCU_THROW(NotSupportedError, "Subgroup operations are not supported");

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

	if (!subgroups::isSubgroupFeatureSupportedForDevice(context, VK_SUBGROUP_FEATURE_CLUSTERED_BIT))
	{
		TCU_THROW(NotSupportedError, "Device does not support subgroup clustered operations");
	}

	if (subgroups::isDoubleFormat(caseDef.format) &&
			!subgroups::isDoubleSupportedForDevice(context))
	{
		TCU_THROW(NotSupportedError, "Device does not support subgroup double operations");
	}

	//Tests which don't use the SSBO
	if (caseDef.noSSBO && VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		subgroups::SSBOData inputData;
		inputData.format = caseDef.format;
		inputData.numElements = subgroups::maxSupportedSubgroupSize();
		inputData.initializeType = subgroups::SSBOData::InitializeNonZero;

		return subgroups::makeVertexFrameBufferTest(context, VK_FORMAT_R32_UINT, &inputData, 1, checkVertexPipelineStages);
	}

	if ((VK_SHADER_STAGE_FRAGMENT_BIT != caseDef.shaderStage) &&
			(VK_SHADER_STAGE_COMPUTE_BIT != caseDef.shaderStage))
	{
		if (!subgroups::isVertexSSBOSupportedForDevice(context))
		{
			TCU_THROW(NotSupportedError, "Device does not support vertex stage SSBO writes");
		}
	}

	if (VK_SHADER_STAGE_FRAGMENT_BIT == caseDef.shaderStage)
	{
		subgroups::SSBOData inputData;
		inputData.format = caseDef.format;
		inputData.numElements = subgroups::maxSupportedSubgroupSize();
		inputData.initializeType = subgroups::SSBOData::InitializeNonZero;

		return subgroups::makeFragmentTest(context, VK_FORMAT_R32_UINT,
										   &inputData, 1, checkFragment);
	}
	else if (VK_SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
	{
		subgroups::SSBOData inputData;
		inputData.format = caseDef.format;
		inputData.numElements = subgroups::maxSupportedSubgroupSize();
		inputData.initializeType = subgroups::SSBOData::InitializeNonZero;

		return subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, &inputData,
										  1, checkCompute);
	}
	else if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		subgroups::SSBOData inputData;
		inputData.format = caseDef.format;
		inputData.numElements = subgroups::maxSupportedSubgroupSize();
		inputData.initializeType = subgroups::SSBOData::InitializeNonZero;

		return subgroups::makeVertexTest(context, VK_FORMAT_R32_UINT, &inputData,
										 1, checkVertexPipelineStages);
	}
	else if (VK_SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
	{
		subgroups::SSBOData inputData;
		inputData.format = caseDef.format;
		inputData.numElements = subgroups::maxSupportedSubgroupSize();
		inputData.initializeType = subgroups::SSBOData::InitializeNonZero;

		return subgroups::makeGeometryTest(context, VK_FORMAT_R32_UINT, &inputData,
										   1, checkVertexPipelineStages);
	}
	else if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
	{
		subgroups::SSBOData inputData;
		inputData.format = caseDef.format;
		inputData.numElements = subgroups::maxSupportedSubgroupSize();
		inputData.initializeType = subgroups::SSBOData::InitializeNonZero;

		return subgroups::makeTessellationControlTest(context, VK_FORMAT_R32_UINT, &inputData,
				1, checkVertexPipelineStages);
	}
	else if (VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT == caseDef.shaderStage)
	{
		subgroups::SSBOData inputData;
		inputData.format = caseDef.format;
		inputData.numElements = subgroups::maxSupportedSubgroupSize();
		inputData.initializeType = subgroups::SSBOData::InitializeNonZero;

		return subgroups::makeTessellationEvaluationTest(context, VK_FORMAT_R32_UINT, &inputData,
				1, checkVertexPipelineStages);
	}
	else
	{
		return tcu::TestStatus::pass("Unhandled shader stage!");
	}
}
}

namespace vkt
{
namespace subgroups
{
tcu::TestCaseGroup* createSubgroupsClusteredTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(
			testCtx, "clustered", "Subgroup clustered category tests"));

	const VkShaderStageFlags stages[] =
	{
		VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
		VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
		VK_SHADER_STAGE_GEOMETRY_BIT,
		VK_SHADER_STAGE_VERTEX_BIT,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		VK_SHADER_STAGE_COMPUTE_BIT
	};

	const VkFormat formats[] =
	{
		VK_FORMAT_R32_SINT, VK_FORMAT_R32G32_SINT, VK_FORMAT_R32G32B32_SINT,
		VK_FORMAT_R32G32B32A32_SINT, VK_FORMAT_R32_UINT, VK_FORMAT_R32G32_UINT,
		VK_FORMAT_R32G32B32_UINT, VK_FORMAT_R32G32B32A32_UINT,
		VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT,
		VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_FORMAT_R64_SFLOAT, VK_FORMAT_R64G64_SFLOAT,
		VK_FORMAT_R64G64B64_SFLOAT, VK_FORMAT_R64G64B64A64_SFLOAT,
		VK_FORMAT_R8_USCALED, VK_FORMAT_R8G8_USCALED,
		VK_FORMAT_R8G8B8_USCALED, VK_FORMAT_R8G8B8A8_USCALED,
	};

	for (int stageIndex = 0; stageIndex < DE_LENGTH_OF_ARRAY(stages); ++stageIndex)
	{
		const VkShaderStageFlags stage = stages[stageIndex];

		for (int formatIndex = 0; formatIndex < DE_LENGTH_OF_ARRAY(formats); ++formatIndex)
		{
			const VkFormat format = formats[formatIndex];

			for (int opTypeIndex = 0; opTypeIndex < OPTYPE_CLUSTERED_LAST; ++opTypeIndex)
			{
				bool isBool = false;
				bool isFloat = false;

				switch (format)
				{
					default:
						break;
					case VK_FORMAT_R32_SFLOAT:
					case VK_FORMAT_R32G32_SFLOAT:
					case VK_FORMAT_R32G32B32_SFLOAT:
					case VK_FORMAT_R32G32B32A32_SFLOAT:
					case VK_FORMAT_R64_SFLOAT:
					case VK_FORMAT_R64G64_SFLOAT:
					case VK_FORMAT_R64G64B64_SFLOAT:
					case VK_FORMAT_R64G64B64A64_SFLOAT:
						isFloat = true;
						break;
					case VK_FORMAT_R8_USCALED:
					case VK_FORMAT_R8G8_USCALED:
					case VK_FORMAT_R8G8B8_USCALED:
					case VK_FORMAT_R8G8B8A8_USCALED:
						isBool = true;
						break;
				}

				bool isBitwiseOp = false;

				switch (opTypeIndex)
				{
					default:
						break;
					case OPTYPE_CLUSTERED_AND:
					case OPTYPE_CLUSTERED_OR:
					case OPTYPE_CLUSTERED_XOR:
						isBitwiseOp = true;
						break;
				}

				if (isFloat && isBitwiseOp)
				{
					// Skip float with bitwise category.
					continue;
				}

				if (isBool && !isBitwiseOp)
				{
					// Skip bool when its not the bitwise category.
					continue;
				}

				CaseDefinition caseDef = {opTypeIndex, stage, format, false};

				std::ostringstream name;

				std::string op = getOpTypeName(opTypeIndex);

				name << de::toLower(op)
					 << "_" << subgroups::getFormatNameForGLSL(format)
					 << "_" << getShaderStageName(stage);

				addFunctionCaseWithPrograms(group.get(), name.str(),
											"", initPrograms, test, caseDef);

				if (VK_SHADER_STAGE_VERTEX_BIT == stage)
				{
					caseDef.noSSBO = true;
					addFunctionCaseWithPrograms(group.get(), name.str()+"_framebuffer", "",
												initFrameBufferPrograms, test, caseDef);
				}
			}
		}
	}

	return group.release();
}

} // subgroups
} // vkt
