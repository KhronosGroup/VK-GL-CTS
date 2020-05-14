/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2019 The Khronos Group Inc.
 * Copyright (c) 2019 Google Inc.
 * Copyright (c) 2017 Codeplay Software Ltd.
 * Copyright (c) 2018 NVIDIA Corporation
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

#include "vktSubgroupsPartitionedTests.hpp"
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
	OPTYPE_ADD = 0,
	OPTYPE_MUL,
	OPTYPE_MIN,
	OPTYPE_MAX,
	OPTYPE_AND,
	OPTYPE_OR,
	OPTYPE_XOR,
	OPTYPE_INCLUSIVE_ADD,
	OPTYPE_INCLUSIVE_MUL,
	OPTYPE_INCLUSIVE_MIN,
	OPTYPE_INCLUSIVE_MAX,
	OPTYPE_INCLUSIVE_AND,
	OPTYPE_INCLUSIVE_OR,
	OPTYPE_INCLUSIVE_XOR,
	OPTYPE_EXCLUSIVE_ADD,
	OPTYPE_EXCLUSIVE_MUL,
	OPTYPE_EXCLUSIVE_MIN,
	OPTYPE_EXCLUSIVE_MAX,
	OPTYPE_EXCLUSIVE_AND,
	OPTYPE_EXCLUSIVE_OR,
	OPTYPE_EXCLUSIVE_XOR,
	OPTYPE_LAST
};

static bool checkVertexPipelineStages(std::vector<const void*> datas,
									  deUint32 width, deUint32)
{
	const deUint32* data =
		reinterpret_cast<const deUint32*>(datas[0]);
	for (deUint32 x = 0; x < width; ++x)
	{
		deUint32 val = data[x];

		if (0xFFFFFF != val)
		{
			return false;
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

							if (0xFFFFFF != data[offset])
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
			return "";
		case OPTYPE_ADD:
			return "subgroupAdd";
		case OPTYPE_MUL:
			return "subgroupMul";
		case OPTYPE_MIN:
			return "subgroupMin";
		case OPTYPE_MAX:
			return "subgroupMax";
		case OPTYPE_AND:
			return "subgroupAnd";
		case OPTYPE_OR:
			return "subgroupOr";
		case OPTYPE_XOR:
			return "subgroupXor";
		case OPTYPE_INCLUSIVE_ADD:
			return "subgroupInclusiveAdd";
		case OPTYPE_INCLUSIVE_MUL:
			return "subgroupInclusiveMul";
		case OPTYPE_INCLUSIVE_MIN:
			return "subgroupInclusiveMin";
		case OPTYPE_INCLUSIVE_MAX:
			return "subgroupInclusiveMax";
		case OPTYPE_INCLUSIVE_AND:
			return "subgroupInclusiveAnd";
		case OPTYPE_INCLUSIVE_OR:
			return "subgroupInclusiveOr";
		case OPTYPE_INCLUSIVE_XOR:
			return "subgroupInclusiveXor";
		case OPTYPE_EXCLUSIVE_ADD:
			return "subgroupExclusiveAdd";
		case OPTYPE_EXCLUSIVE_MUL:
			return "subgroupExclusiveMul";
		case OPTYPE_EXCLUSIVE_MIN:
			return "subgroupExclusiveMin";
		case OPTYPE_EXCLUSIVE_MAX:
			return "subgroupExclusiveMax";
		case OPTYPE_EXCLUSIVE_AND:
			return "subgroupExclusiveAnd";
		case OPTYPE_EXCLUSIVE_OR:
			return "subgroupExclusiveOr";
		case OPTYPE_EXCLUSIVE_XOR:
			return "subgroupExclusiveXor";
	}
}

std::string getOpTypeNamePartitioned(int opType)
{
	switch (opType)
	{
		default:
			DE_FATAL("Unsupported op type");
			return "";
		case OPTYPE_ADD:
			return "subgroupPartitionedAddNV";
		case OPTYPE_MUL:
			return "subgroupPartitionedMulNV";
		case OPTYPE_MIN:
			return "subgroupPartitionedMinNV";
		case OPTYPE_MAX:
			return "subgroupPartitionedMaxNV";
		case OPTYPE_AND:
			return "subgroupPartitionedAndNV";
		case OPTYPE_OR:
			return "subgroupPartitionedOrNV";
		case OPTYPE_XOR:
			return "subgroupPartitionedXorNV";
		case OPTYPE_INCLUSIVE_ADD:
			return "subgroupPartitionedInclusiveAddNV";
		case OPTYPE_INCLUSIVE_MUL:
			return "subgroupPartitionedInclusiveMulNV";
		case OPTYPE_INCLUSIVE_MIN:
			return "subgroupPartitionedInclusiveMinNV";
		case OPTYPE_INCLUSIVE_MAX:
			return "subgroupPartitionedInclusiveMaxNV";
		case OPTYPE_INCLUSIVE_AND:
			return "subgroupPartitionedInclusiveAndNV";
		case OPTYPE_INCLUSIVE_OR:
			return "subgroupPartitionedInclusiveOrNV";
		case OPTYPE_INCLUSIVE_XOR:
			return "subgroupPartitionedInclusiveXorNV";
		case OPTYPE_EXCLUSIVE_ADD:
			return "subgroupPartitionedExclusiveAddNV";
		case OPTYPE_EXCLUSIVE_MUL:
			return "subgroupPartitionedExclusiveMulNV";
		case OPTYPE_EXCLUSIVE_MIN:
			return "subgroupPartitionedExclusiveMinNV";
		case OPTYPE_EXCLUSIVE_MAX:
			return "subgroupPartitionedExclusiveMaxNV";
		case OPTYPE_EXCLUSIVE_AND:
			return "subgroupPartitionedExclusiveAndNV";
		case OPTYPE_EXCLUSIVE_OR:
			return "subgroupPartitionedExclusiveOrNV";
		case OPTYPE_EXCLUSIVE_XOR:
			return "subgroupPartitionedExclusiveXorNV";
	}
}

std::string getIdentity(int opType, vk::VkFormat format)
{
	const bool isFloat = subgroups::isFormatFloat(format);
	const bool isInt = subgroups::isFormatSigned(format);
	const bool isUnsigned = subgroups::isFormatUnsigned(format);

	switch (opType)
	{
		default:
			DE_FATAL("Unsupported op type");
			return "";
		case OPTYPE_ADD:
		case OPTYPE_INCLUSIVE_ADD:
		case OPTYPE_EXCLUSIVE_ADD:
			return subgroups::getFormatNameForGLSL(format) + "(0)";
		case OPTYPE_MUL:
		case OPTYPE_INCLUSIVE_MUL:
		case OPTYPE_EXCLUSIVE_MUL:
			return subgroups::getFormatNameForGLSL(format) + "(1)";
		case OPTYPE_MIN:
		case OPTYPE_INCLUSIVE_MIN:
		case OPTYPE_EXCLUSIVE_MIN:
			if (isFloat)
			{
				return subgroups::getFormatNameForGLSL(format) + "(intBitsToFloat(0x7f800000))";
			}
			else if (isInt)
			{
				switch (format)
				{
					default:
						return subgroups::getFormatNameForGLSL(format) + "(0x7fffffff)";
					case VK_FORMAT_R8_SINT:
					case VK_FORMAT_R8G8_SINT:
					case VK_FORMAT_R8G8B8_SINT:
					case VK_FORMAT_R8G8B8A8_SINT:
					case VK_FORMAT_R8_UINT:
					case VK_FORMAT_R8G8_UINT:
					case VK_FORMAT_R8G8B8_UINT:
					case VK_FORMAT_R8G8B8A8_UINT:
						return subgroups::getFormatNameForGLSL(format) + "(0x7f)";
					case VK_FORMAT_R16_SINT:
					case VK_FORMAT_R16G16_SINT:
					case VK_FORMAT_R16G16B16_SINT:
					case VK_FORMAT_R16G16B16A16_SINT:
					case VK_FORMAT_R16_UINT:
					case VK_FORMAT_R16G16_UINT:
					case VK_FORMAT_R16G16B16_UINT:
					case VK_FORMAT_R16G16B16A16_UINT:
						return subgroups::getFormatNameForGLSL(format) + "(0x7fff)";
			        case VK_FORMAT_R64_SINT:
			        case VK_FORMAT_R64G64_SINT:
			        case VK_FORMAT_R64G64B64_SINT:
			        case VK_FORMAT_R64G64B64A64_SINT:
			        case VK_FORMAT_R64_UINT:
			        case VK_FORMAT_R64G64_UINT:
			        case VK_FORMAT_R64G64B64_UINT:
			        case VK_FORMAT_R64G64B64A64_UINT:
						return subgroups::getFormatNameForGLSL(format) + "(0x7fffffffffffffffUL)";
				}
			}
			else if (isUnsigned)
			{
				return subgroups::getFormatNameForGLSL(format) + "(-1)";
			}
			else
			{
				DE_FATAL("Unhandled case");
				return "";
			}
		case OPTYPE_MAX:
		case OPTYPE_INCLUSIVE_MAX:
		case OPTYPE_EXCLUSIVE_MAX:
			if (isFloat)
			{
				return subgroups::getFormatNameForGLSL(format) + "(intBitsToFloat(0xff800000))";
			}
			else if (isInt)
			{
				switch (format)
				{
					default:
						return subgroups::getFormatNameForGLSL(format) + "(0x80000000)";
					case VK_FORMAT_R8_SINT:
					case VK_FORMAT_R8G8_SINT:
					case VK_FORMAT_R8G8B8_SINT:
					case VK_FORMAT_R8G8B8A8_SINT:
					case VK_FORMAT_R8_UINT:
					case VK_FORMAT_R8G8_UINT:
					case VK_FORMAT_R8G8B8_UINT:
					case VK_FORMAT_R8G8B8A8_UINT:
						return subgroups::getFormatNameForGLSL(format) + "(0x80)";
					case VK_FORMAT_R16_SINT:
					case VK_FORMAT_R16G16_SINT:
					case VK_FORMAT_R16G16B16_SINT:
					case VK_FORMAT_R16G16B16A16_SINT:
					case VK_FORMAT_R16_UINT:
					case VK_FORMAT_R16G16_UINT:
					case VK_FORMAT_R16G16B16_UINT:
					case VK_FORMAT_R16G16B16A16_UINT:
						return subgroups::getFormatNameForGLSL(format) + "(0x8000)";
			        case VK_FORMAT_R64_SINT:
			        case VK_FORMAT_R64G64_SINT:
			        case VK_FORMAT_R64G64B64_SINT:
			        case VK_FORMAT_R64G64B64A64_SINT:
			        case VK_FORMAT_R64_UINT:
			        case VK_FORMAT_R64G64_UINT:
			        case VK_FORMAT_R64G64B64_UINT:
			        case VK_FORMAT_R64G64B64A64_UINT:
						return subgroups::getFormatNameForGLSL(format) + "(0x8000000000000000UL)";
				}
			}
			else if (isUnsigned)
			{
				return subgroups::getFormatNameForGLSL(format) + "(0)";
			}
			else
			{
				DE_FATAL("Unhandled case");
				return "";
			}
		case OPTYPE_AND:
		case OPTYPE_INCLUSIVE_AND:
		case OPTYPE_EXCLUSIVE_AND:
			return subgroups::getFormatNameForGLSL(format) + "(~0)";
		case OPTYPE_OR:
		case OPTYPE_INCLUSIVE_OR:
		case OPTYPE_EXCLUSIVE_OR:
			return subgroups::getFormatNameForGLSL(format) + "(0)";
		case OPTYPE_XOR:
		case OPTYPE_INCLUSIVE_XOR:
		case OPTYPE_EXCLUSIVE_XOR:
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
		case VK_FORMAT_R8_UINT:
		case VK_FORMAT_R8_SINT:
		case VK_FORMAT_R16_UINT:
		case VK_FORMAT_R16_SINT:
		case VK_FORMAT_R32_UINT:
		case VK_FORMAT_R32_SINT:
		case VK_FORMAT_R64_UINT:
		case VK_FORMAT_R64_SINT:
			return "(" + lhs + " == " + rhs + ")";
		case VK_FORMAT_R16_SFLOAT:
			switch (opType)
			{
				default:
					return "(abs(" + lhs + " - " + rhs + ") < 0.1)";
				case OPTYPE_MIN:
				case OPTYPE_INCLUSIVE_MIN:
				case OPTYPE_EXCLUSIVE_MIN:
				case OPTYPE_MAX:
				case OPTYPE_INCLUSIVE_MAX:
				case OPTYPE_EXCLUSIVE_MAX:
					return "(" + lhs + " == " + rhs + ")";
			}
		case VK_FORMAT_R32_SFLOAT:
		case VK_FORMAT_R64_SFLOAT:
			switch (opType)
			{
				default:
					return "(abs(" + lhs + " - " + rhs + ") < 0.00001)";
				case OPTYPE_MIN:
				case OPTYPE_INCLUSIVE_MIN:
				case OPTYPE_EXCLUSIVE_MIN:
				case OPTYPE_MAX:
				case OPTYPE_INCLUSIVE_MAX:
				case OPTYPE_EXCLUSIVE_MAX:
					return "(" + lhs + " == " + rhs + ")";
			}
		case VK_FORMAT_R16G16_SFLOAT:
		case VK_FORMAT_R16G16B16_SFLOAT:
		case VK_FORMAT_R16G16B16A16_SFLOAT:
			switch (opType)
			{
				default:
					return "all(lessThan(abs(" + lhs + " - " + rhs + "), " + formatName + "(0.1)))";
				case OPTYPE_MIN:
				case OPTYPE_INCLUSIVE_MIN:
				case OPTYPE_EXCLUSIVE_MIN:
				case OPTYPE_MAX:
				case OPTYPE_INCLUSIVE_MAX:
				case OPTYPE_EXCLUSIVE_MAX:
					return "all(equal(" + lhs + ", " + rhs + "))";
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
				case OPTYPE_MIN:
				case OPTYPE_INCLUSIVE_MIN:
				case OPTYPE_EXCLUSIVE_MIN:
				case OPTYPE_MAX:
				case OPTYPE_INCLUSIVE_MAX:
				case OPTYPE_EXCLUSIVE_MAX:
					return "all(equal(" + lhs + ", " + rhs + "))";
			}
	}
}

struct CaseDefinition
{
	int					opType;
	VkShaderStageFlags	shaderStage;
	VkFormat			format;
	de::SharedPtr<bool>	geometryPointSizeSupported;
};

string getTestString(const CaseDefinition &caseDef)
{
    // NOTE: tempResult can't have anything in bits 31:24 to avoid int->float
    // conversion overflow in framebuffer tests.
    string fmt = subgroups::getFormatNameForGLSL(caseDef.format);
	string bdy =
		"  uint tempResult = 0;\n"
		"  uint id = gl_SubgroupInvocationID;\n";

    // Test the case where the partition has a single subset with all invocations in it.
    // This should generate the same result as the non-partitioned function.
    bdy +=
        "  uvec4 allBallot = mask;\n"
        "  " + fmt + " allResult = " + getOpTypeNamePartitioned(caseDef.opType) + "(data[gl_SubgroupInvocationID], allBallot);\n"
        "  " + fmt + " refResult = " + getOpTypeName(caseDef.opType) + "(data[gl_SubgroupInvocationID]);\n"
        "  if (" + getCompare(caseDef.opType, caseDef.format, "allResult", "refResult") + ") {\n"
        "      tempResult |= 0x1;\n"
        "  }\n";

    // The definition of a partition doesn't forbid bits corresponding to inactive
    // invocations being in the subset with active invocations. In other words, test that
    // bits corresponding to inactive invocations are ignored.
    bdy +=
	    "  if (0 == (gl_SubgroupInvocationID % 2)) {\n"
        "    " + fmt + " allResult = " + getOpTypeNamePartitioned(caseDef.opType) + "(data[gl_SubgroupInvocationID], allBallot);\n"
        "    " + fmt + " refResult = " + getOpTypeName(caseDef.opType) + "(data[gl_SubgroupInvocationID]);\n"
        "    if (" + getCompare(caseDef.opType, caseDef.format, "allResult", "refResult") + ") {\n"
        "        tempResult |= 0x2;\n"
        "    }\n"
        "  } else {\n"
        "    tempResult |= 0x2;\n"
        "  }\n";

    // Test the case where the partition has each invocation in a unique subset. For
    // exclusive ops, the result is identity. For reduce/inclusive, it's the original value.
    string expectedSelfResult = "data[gl_SubgroupInvocationID]";
    if (caseDef.opType >= OPTYPE_EXCLUSIVE_ADD &&
        caseDef.opType <= OPTYPE_EXCLUSIVE_XOR) {
        expectedSelfResult = getIdentity(caseDef.opType, caseDef.format);
    }

    bdy +=
        "  uvec4 selfBallot = subgroupPartitionNV(gl_SubgroupInvocationID);\n"
        "  " + fmt + " selfResult = " + getOpTypeNamePartitioned(caseDef.opType) + "(data[gl_SubgroupInvocationID], selfBallot);\n"
        "  if (" + getCompare(caseDef.opType, caseDef.format, "selfResult", expectedSelfResult) + ") {\n"
        "      tempResult |= 0x4;\n"
        "  }\n";

    // Test "random" partitions based on a hash of the invocation id.
    // This "hash" function produces interesting/randomish partitions.
    static const char *idhash = "((id%N)+(id%(N+1))-(id%2)+(id/2))%((N+1)/2)";

    bdy +=
		"  for (uint N = 1; N < 16; ++N) {\n"
		"    " + fmt + " idhashFmt = " + fmt + "(" + idhash + ");\n"
		"    uvec4 partitionBallot = subgroupPartitionNV(idhashFmt) & mask;\n"
		"    " + fmt + " partitionedResult = " + getOpTypeNamePartitioned(caseDef.opType) + "(data[gl_SubgroupInvocationID], partitionBallot);\n"
		"      for (uint i = 0; i < N; ++i) {\n"
		"        " + fmt + " iFmt = " + fmt + "(i);\n"
        "        if (" + getCompare(caseDef.opType, caseDef.format, "idhashFmt", "iFmt") + ") {\n"
        "          " + fmt + " subsetResult = " + getOpTypeName(caseDef.opType) + "(data[gl_SubgroupInvocationID]);\n"
        "          tempResult |= " + getCompare(caseDef.opType, caseDef.format, "partitionedResult", "subsetResult") + " ? (0x4 << N) : 0;\n"
        "        }\n"
        "      }\n"
        "  }\n"
        // tests in flow control:
		"  if (1 == (gl_SubgroupInvocationID % 2)) {\n"
        "    for (uint N = 1; N < 7; ++N) {\n"
		"      " + fmt + " idhashFmt = " + fmt + "(" + idhash + ");\n"
		"      uvec4 partitionBallot = subgroupPartitionNV(idhashFmt) & mask;\n"
        "      " + fmt + " partitionedResult = " + getOpTypeNamePartitioned(caseDef.opType) + "(data[gl_SubgroupInvocationID], partitionBallot);\n"
        "        for (uint i = 0; i < N; ++i) {\n"
		"          " + fmt + " iFmt = " + fmt + "(i);\n"
        "          if (" + getCompare(caseDef.opType, caseDef.format, "idhashFmt", "iFmt") + ") {\n"
        "            " + fmt + " subsetResult = " + getOpTypeName(caseDef.opType) + "(data[gl_SubgroupInvocationID]);\n"
        "            tempResult |= " + getCompare(caseDef.opType, caseDef.format, "partitionedResult", "subsetResult") + " ? (0x20000 << N) : 0;\n"
        "          }\n"
        "        }\n"
        "    }\n"
        "  } else {\n"
        "    tempResult |= 0xFC0000;\n"
        "  }\n"
        ;

    return bdy;
}

void initFrameBufferPrograms (SourceCollections& programCollection, CaseDefinition caseDef)
{
	const vk::ShaderBuildOptions	buildOptions	(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
	std::ostringstream				bdy;

	subgroups::setFragmentShaderFrameBuffer(programCollection);

	if (VK_SHADER_STAGE_VERTEX_BIT != caseDef.shaderStage)
		subgroups::setVertexShaderFrameBuffer(programCollection);

	bdy << getTestString(caseDef);

	if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
	{
		std::ostringstream vertexSrc;
		vertexSrc << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
			<< "#extension GL_NV_shader_subgroup_partitioned: enable\n"
			<< "#extension GL_KHR_shader_subgroup_arithmetic: enable\n"
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< subgroups::getAdditionalExtensionForFormat(caseDef.format)
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
			<< "  out_color = float(tempResult);\n"
			<< "  gl_Position = in_position;\n"
			<< "  gl_PointSize = 1.0f;\n"
			<< "}\n";
		programCollection.glslSources.add("vert")
			<< glu::VertexSource(vertexSrc.str()) << buildOptions;
	}
	else if (VK_SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
	{
		std::ostringstream geometry;

		geometry << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
			<< "#extension GL_NV_shader_subgroup_partitioned: enable\n"
			<< "#extension GL_KHR_shader_subgroup_arithmetic: enable\n"
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< subgroups::getAdditionalExtensionForFormat(caseDef.format)
			<< "layout(points) in;\n"
			<< "layout(points, max_vertices = 1) out;\n"
			<< "layout(location = 0) out float out_color;\n"
			<< "layout(set = 0, binding = 0) uniform Buffer\n"
			<< "{\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data[" << subgroups::maxSupportedSubgroupSize() << "];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  uvec4 mask = subgroupBallot(true);\n"
			<< bdy.str()
			<< "  out_color = float(tempResult);\n"
			<< "  gl_Position = gl_in[0].gl_Position;\n"
			<< (*caseDef.geometryPointSizeSupported ? "  gl_PointSize = gl_in[0].gl_PointSize;\n" : "")
			<< "  EmitVertex();\n"
			<< "  EndPrimitive();\n"
			<< "}\n";

		programCollection.glslSources.add("geometry")
				<< glu::GeometrySource(geometry.str()) << buildOptions;
	}
	else if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
	{
		std::ostringstream controlSource;
		controlSource  << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
			<< "#extension GL_NV_shader_subgroup_partitioned: enable\n"
			<< "#extension GL_KHR_shader_subgroup_arithmetic: enable\n"
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< subgroups::getAdditionalExtensionForFormat(caseDef.format)
			<< "layout(vertices = 2) out;\n"
			<< "layout(location = 0) out float out_color[];\n"
			<< "layout(set = 0, binding = 0) uniform Buffer1\n"
			<< "{\n"
			<< "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data[" << subgroups::maxSupportedSubgroupSize() << "];\n"
			<< "};\n"
			<< "\n"
			<< "void main (void)\n"
			<< "{\n"
			<< "  if (gl_InvocationID == 0)\n"
			<<"  {\n"
			<< "    gl_TessLevelOuter[0] = 1.0f;\n"
			<< "    gl_TessLevelOuter[1] = 1.0f;\n"
			<< "  }\n"
			<< "  uvec4 mask = subgroupBallot(true);\n"
			<< bdy.str()
			<< "  out_color[gl_InvocationID] = float(tempResult);"
			<< "  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
			<< (*caseDef.geometryPointSizeSupported ? "  gl_out[gl_InvocationID].gl_PointSize = gl_in[gl_InvocationID].gl_PointSize;\n" : "")
			<< "}\n";


		programCollection.glslSources.add("tesc")
			<< glu::TessellationControlSource(controlSource.str()) << buildOptions;
		subgroups::setTesEvalShaderFrameBuffer(programCollection);
	}
	else if (VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT == caseDef.shaderStage)
	{

		std::ostringstream evaluationSource;
		evaluationSource << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_450)<<"\n"
			<< "#extension GL_NV_shader_subgroup_partitioned: enable\n"
			<< "#extension GL_KHR_shader_subgroup_arithmetic: enable\n"
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< subgroups::getAdditionalExtensionForFormat(caseDef.format)
			<< "layout(isolines, equal_spacing, ccw ) in;\n"
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
			<< "  out_color = float(tempResult);\n"
			<< "  gl_Position = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x);\n"
			<< (*caseDef.geometryPointSizeSupported ? "  gl_PointSize = gl_in[0].gl_PointSize;\n" : "")
			<< "}\n";

		subgroups::setTesCtrlShaderFrameBuffer(programCollection);
		programCollection.glslSources.add("tese") << glu::TessellationEvaluationSource(evaluationSource.str()) << buildOptions;
	}
	else
	{
		DE_FATAL("Unsupported shader stage");
	}
}

void initPrograms(SourceCollections& programCollection, CaseDefinition caseDef)
{
	const string bdy = getTestString(caseDef);

	if (VK_SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
	{
		std::ostringstream src;

		src << "#version 450\n"
			<< "#extension GL_NV_shader_subgroup_partitioned: enable\n"
			<< "#extension GL_KHR_shader_subgroup_arithmetic: enable\n"
			<< "#extension GL_KHR_shader_subgroup_ballot: enable\n"
			<< subgroups::getAdditionalExtensionForFormat(caseDef.format)
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
			<< bdy
			<< "  result[offset] = tempResult;\n"
			<< "}\n";

		programCollection.glslSources.add("comp")
				<< glu::ComputeSource(src.str()) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
	}
	else
	{
		{
			const std::string vertex =
				"#version 450\n"
				"#extension GL_NV_shader_subgroup_partitioned: enable\n"
			    "#extension GL_KHR_shader_subgroup_arithmetic: enable\n"
				"#extension GL_KHR_shader_subgroup_ballot: enable\n"
				+ subgroups::getAdditionalExtensionForFormat(caseDef.format) +
				"layout(set = 0, binding = 0, std430) buffer Buffer1\n"
				"{\n"
				"  uint result[];\n"
				"};\n"
				"layout(set = 0, binding = 4, std430) readonly buffer Buffer2\n"
				"{\n"
				"  " + subgroups::getFormatNameForGLSL(caseDef.format) + " data[];\n"
				"};\n"
				"\n"
				"void main (void)\n"
				"{\n"
				"  uvec4 mask = subgroupBallot(true);\n"
				+ bdy+
				"  result[gl_VertexIndex] = tempResult;\n"
				"  float pixelSize = 2.0f/1024.0f;\n"
				"  float pixelPosition = pixelSize/2.0f - 1.0f;\n"
				"  gl_Position = vec4(float(gl_VertexIndex) * pixelSize + pixelPosition, 0.0f, 0.0f, 1.0f);\n"
				"  gl_PointSize = 1.0f;\n"
				"}\n";
			programCollection.glslSources.add("vert")
					<< glu::VertexSource(vertex) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
		}

		{
			const std::string tesc =
				"#version 450\n"
				"#extension GL_NV_shader_subgroup_partitioned: enable\n"
			    "#extension GL_KHR_shader_subgroup_arithmetic: enable\n"
				"#extension GL_KHR_shader_subgroup_ballot: enable\n"
				+ subgroups::getAdditionalExtensionForFormat(caseDef.format) +
				"layout(vertices=1) out;\n"
				"layout(set = 0, binding = 1, std430) buffer Buffer1\n"
				"{\n"
				"  uint result[];\n"
				"};\n"
				"layout(set = 0, binding = 4, std430) readonly buffer Buffer2\n"
				"{\n"
				"  " + subgroups::getFormatNameForGLSL(caseDef.format) + " data[];\n"
				"};\n"
				"\n"
				"void main (void)\n"
				"{\n"
				"  uvec4 mask = subgroupBallot(true);\n"
				+ bdy +
				"  result[gl_PrimitiveID] = tempResult;\n"
				"  if (gl_InvocationID == 0)\n"
				"  {\n"
				"    gl_TessLevelOuter[0] = 1.0f;\n"
				"    gl_TessLevelOuter[1] = 1.0f;\n"
				"  }\n"
				"  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
				"}\n";
			programCollection.glslSources.add("tesc")
				<< glu::TessellationControlSource(tesc) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
		}

		{
			const std::string tese =
				"#version 450\n"
				"#extension GL_NV_shader_subgroup_partitioned: enable\n"
			    "#extension GL_KHR_shader_subgroup_arithmetic: enable\n"
				"#extension GL_KHR_shader_subgroup_ballot: enable\n"
				+ subgroups::getAdditionalExtensionForFormat(caseDef.format) +
				"layout(isolines) in;\n"
				"layout(set = 0, binding = 2, std430) buffer Buffer1\n"
				"{\n"
				"  uint result[];\n"
				"};\n"
				"layout(set = 0, binding = 4, std430) readonly buffer Buffer2\n"
				"{\n"
				"  " + subgroups::getFormatNameForGLSL(caseDef.format) + " data[];\n"
				"};\n"
				"\n"
				"void main (void)\n"
				"{\n"
				"  uvec4 mask = subgroupBallot(true);\n"
				+ bdy +
				"  result[gl_PrimitiveID * 2 + uint(gl_TessCoord.x + 0.5)] = tempResult;\n"
				"  float pixelSize = 2.0f/1024.0f;\n"
				"  gl_Position = gl_in[0].gl_Position + gl_TessCoord.x * pixelSize / 2.0f;\n"
				"}\n";
			programCollection.glslSources.add("tese")
				<< glu::TessellationEvaluationSource(tese) << vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
		}

		{
			const std::string geometry =
				"#version 450\n"
				"#extension GL_NV_shader_subgroup_partitioned: enable\n"
			    "#extension GL_KHR_shader_subgroup_arithmetic: enable\n"
				"#extension GL_KHR_shader_subgroup_ballot: enable\n"
				+ subgroups::getAdditionalExtensionForFormat(caseDef.format) +
				"layout(${TOPOLOGY}) in;\n"
				"layout(points, max_vertices = 1) out;\n"
				"layout(set = 0, binding = 3, std430) buffer Buffer1\n"
				"{\n"
				"  uint result[];\n"
				"};\n"
				"layout(set = 0, binding = 4, std430) readonly buffer Buffer2\n"
				"{\n"
				"  " + subgroups::getFormatNameForGLSL(caseDef.format) + " data[];\n"
				"};\n"
				"\n"
				"void main (void)\n"
				"{\n"
				"  uvec4 mask = subgroupBallot(true);\n"
				 + bdy +
				"  result[gl_PrimitiveIDIn] = tempResult;\n"
				"  gl_Position = gl_in[0].gl_Position;\n"
				"  EmitVertex();\n"
				"  EndPrimitive();\n"
				"}\n";
			subgroups::addGeometryShadersFromTemplate(geometry, vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u),
													  programCollection.glslSources);
		}

		{
			const std::string fragment =
				"#version 450\n"
				"#extension GL_NV_shader_subgroup_partitioned: enable\n"
			    "#extension GL_KHR_shader_subgroup_arithmetic: enable\n"
				"#extension GL_KHR_shader_subgroup_ballot: enable\n"
				+ subgroups::getAdditionalExtensionForFormat(caseDef.format) +
				"layout(location = 0) out uint result;\n"
				"layout(set = 0, binding = 4, std430) readonly buffer Buffer2\n"
				"{\n"
				"  " + subgroups::getFormatNameForGLSL(caseDef.format) + " data[];\n"
				"};\n"
				"void main (void)\n"
				"{\n"
				"  uvec4 mask = subgroupBallot(true);\n"
				+ bdy +
				"  result = tempResult;\n"
				"}\n";
			programCollection.glslSources.add("fragment")
				<< glu::FragmentSource(fragment)<< vk::ShaderBuildOptions(programCollection.usedVulkanVersion, vk::SPIRV_VERSION_1_3, 0u);
		}
		subgroups::addNoSubgroupShader(programCollection);
	}
}

void supportedCheck (Context& context, CaseDefinition caseDef)
{
	if (!subgroups::isSubgroupSupported(context))
		TCU_THROW(NotSupportedError, "Subgroup operations are not supported");

	if (!subgroups::isSubgroupFeatureSupportedForDevice(context, VK_SUBGROUP_FEATURE_PARTITIONED_BIT_NV))
	{
		TCU_THROW(NotSupportedError, "Device does not support subgroup partitioned operations");
	}

	if (!subgroups::isFormatSupportedForDevice(context, caseDef.format))
		TCU_THROW(NotSupportedError, "Device does not support the specified format in subgroup operations");

	*caseDef.geometryPointSizeSupported = subgroups::isTessellationAndGeometryPointSizeSupported(context);
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

	subgroups::SSBOData inputData;
	inputData.format = caseDef.format;
	inputData.layout = subgroups::SSBOData::LayoutStd140;
	inputData.numElements = subgroups::maxSupportedSubgroupSize();
	inputData.initializeType = subgroups::SSBOData::InitializeNonZero;

	if (VK_SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
		return subgroups::makeVertexFrameBufferTest(context, VK_FORMAT_R32_UINT, &inputData, 1, checkVertexPipelineStages);
	else if (VK_SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
		return subgroups::makeGeometryFrameBufferTest(context, VK_FORMAT_R32_UINT, &inputData, 1, checkVertexPipelineStages);
	else if (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT == caseDef.shaderStage)
		return subgroups::makeTessellationEvaluationFrameBufferTest(context, VK_FORMAT_R32_UINT, &inputData, 1, checkVertexPipelineStages, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
	else if (VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT == caseDef.shaderStage)
		return subgroups::makeTessellationEvaluationFrameBufferTest(context,  VK_FORMAT_R32_UINT, &inputData, 1, checkVertexPipelineStages, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
	else
		TCU_THROW(InternalError, "Unhandled shader stage");
}

bool checkShaderStages (Context& context, const CaseDefinition& caseDef)
{
	if (!subgroups::areSubgroupOperationsSupportedForStage(
				context, caseDef.shaderStage))
	{
		if (subgroups::areSubgroupOperationsRequiredForStage(
					caseDef.shaderStage))
		{
			return false;
		}
		else
		{
			TCU_THROW(NotSupportedError, "Device does not support subgroup operations for this stage");
		}
	}
	return true;
}

tcu::TestStatus test(Context& context, const CaseDefinition caseDef)
{
	if (VK_SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
	{
		if(!checkShaderStages(context,caseDef))
		{
			return tcu::TestStatus::fail(
							"Shader stage " +
							subgroups::getShaderStageName(caseDef.shaderStage) +
							" is required to support subgroup operations!");
		}
		subgroups::SSBOData inputData;
		inputData.format = caseDef.format;
		inputData.layout = subgroups::SSBOData::LayoutStd430;
		inputData.numElements = subgroups::maxSupportedSubgroupSize();
		inputData.initializeType = subgroups::SSBOData::InitializeNonZero;

		return subgroups::makeComputeTest(context, VK_FORMAT_R32_UINT, &inputData, 1, checkCompute);
	}
	else
	{
		VkPhysicalDeviceSubgroupProperties subgroupProperties;
		subgroupProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
		subgroupProperties.pNext = DE_NULL;

		VkPhysicalDeviceProperties2 properties;
		properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		properties.pNext = &subgroupProperties;

		context.getInstanceInterface().getPhysicalDeviceProperties2(context.getPhysicalDevice(), &properties);

		VkShaderStageFlagBits stages = (VkShaderStageFlagBits)(caseDef.shaderStage  & subgroupProperties.supportedStages);

		if ( VK_SHADER_STAGE_FRAGMENT_BIT != stages && !subgroups::isVertexSSBOSupportedForDevice(context))
		{
			if ( (stages & VK_SHADER_STAGE_FRAGMENT_BIT) == 0)
				TCU_THROW(NotSupportedError, "Device does not support vertex stage SSBO writes");
			else
				stages = VK_SHADER_STAGE_FRAGMENT_BIT;
		}

		if ((VkShaderStageFlagBits)0u == stages)
			TCU_THROW(NotSupportedError, "Subgroup operations are not supported for any graphic shader");

		subgroups::SSBOData inputData;
		inputData.format			= caseDef.format;
		inputData.layout			= subgroups::SSBOData::LayoutStd430;
		inputData.numElements		= subgroups::maxSupportedSubgroupSize();
		inputData.initializeType	= subgroups::SSBOData::InitializeNonZero;
		inputData.binding			= 4u;
		inputData.stages			= stages;

		return subgroups::allStages(context, VK_FORMAT_R32_UINT, &inputData,
										 1, checkVertexPipelineStages, stages);
	}
}
}

namespace vkt
{
namespace subgroups
{
tcu::TestCaseGroup* createSubgroupsPartitionedTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> graphicGroup(new tcu::TestCaseGroup(
		testCtx, "graphics", "Subgroup partitioned category tests: graphics"));
	de::MovePtr<tcu::TestCaseGroup> computeGroup(new tcu::TestCaseGroup(
		testCtx, "compute", "Subgroup partitioned category tests: compute"));
	de::MovePtr<tcu::TestCaseGroup> framebufferGroup(new tcu::TestCaseGroup(
		testCtx, "framebuffer", "Subgroup partitioned category tests: framebuffer"));

	const VkShaderStageFlags stages[] =
	{
		VK_SHADER_STAGE_VERTEX_BIT,
		VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
		VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
		VK_SHADER_STAGE_GEOMETRY_BIT,
	};

	const std::vector<VkFormat> formats = subgroups::getAllFormats();

	for (size_t formatIndex = 0; formatIndex < formats.size(); ++formatIndex)
	{
		const VkFormat format = formats[formatIndex];

		for (int opTypeIndex = 0; opTypeIndex < OPTYPE_LAST; ++opTypeIndex)
		{
			bool isBool = false;
			bool isFloat = false;

			switch (format)
			{
				default:
					break;
				case VK_FORMAT_R16_SFLOAT:
				case VK_FORMAT_R16G16_SFLOAT:
				case VK_FORMAT_R16G16B16_SFLOAT:
				case VK_FORMAT_R16G16B16A16_SFLOAT:
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
				case OPTYPE_AND:
				case OPTYPE_INCLUSIVE_AND:
				case OPTYPE_EXCLUSIVE_AND:
				case OPTYPE_OR:
				case OPTYPE_INCLUSIVE_OR:
				case OPTYPE_EXCLUSIVE_OR:
				case OPTYPE_XOR:
				case OPTYPE_INCLUSIVE_XOR:
				case OPTYPE_EXCLUSIVE_XOR:
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
			std::string op = getOpTypeName(opTypeIndex);

			{
				const CaseDefinition caseDef = {opTypeIndex, VK_SHADER_STAGE_COMPUTE_BIT, format, de::SharedPtr<bool>(new bool)};
				addFunctionCaseWithPrograms(computeGroup.get(),
											de::toLower(op) + "_" +
											subgroups::getFormatNameForGLSL(format),
											"", supportedCheck, initPrograms, test, caseDef);
			}

			{
				const CaseDefinition caseDef = {opTypeIndex, VK_SHADER_STAGE_ALL_GRAPHICS, format, de::SharedPtr<bool>(new bool)};
				addFunctionCaseWithPrograms(graphicGroup.get(),
											de::toLower(op) + "_" +
											subgroups::getFormatNameForGLSL(format),
											"", supportedCheck, initPrograms, test, caseDef);
			}

			for (int stageIndex = 0; stageIndex < DE_LENGTH_OF_ARRAY(stages); ++stageIndex)
			{
				const CaseDefinition caseDef = {opTypeIndex, stages[stageIndex], format, de::SharedPtr<bool>(new bool)};
				addFunctionCaseWithPrograms(framebufferGroup.get(), de::toLower(op) + "_" + subgroups::getFormatNameForGLSL(format) +
											"_" + getShaderStageName(caseDef.shaderStage), "",
											supportedCheck, initFrameBufferPrograms, noSSBOtest, caseDef);
			}
		}
	}

	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(
		testCtx, "partitioned", "Subgroup partitioned category tests"));

	group->addChild(graphicGroup.release());
	group->addChild(computeGroup.release());
	group->addChild(framebufferGroup.release());

	return group.release();
}

} // subgroups
} // vkt

