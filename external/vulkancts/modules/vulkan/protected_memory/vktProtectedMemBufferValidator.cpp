/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 The Khronos Group Inc.
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
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
 * \brief Protected content buffer validator helper
 *//*--------------------------------------------------------------------*/

#include "vktProtectedMemBufferValidator.hpp"

#include "tcuTestLog.hpp"

#include "vkBuilderUtil.hpp"
#include "vkPrograms.hpp"
#include "vkTypeUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestGroupUtil.hpp"
#include "tcuStringTemplate.hpp"

#include "vktProtectedMemUtils.hpp"

namespace vkt
{
namespace ProtectedMem
{
namespace
{

const char* generateShaderVarString (TestType testType)
{
	switch (testType)
	{
		case TYPE_UINT:		return "uvec4";
		case TYPE_INT:		return "ivec4";
		case TYPE_FLOAT:	return "vec4";

		default: DE_FATAL("Incorrect vector type format"); return "";
	}
}

const char* generateShaderBufferString (TestType testType, BufferType bufferType)
{
	if (bufferType == STORAGE_BUFFER)
		return "buffer";

	DE_ASSERT(bufferType == SAMPLER_BUFFER);

	switch (testType) {
		case TYPE_UINT:		return "uniform usamplerBuffer";
		case TYPE_INT:		return "uniform isamplerBuffer";
		case TYPE_FLOAT:	return "uniform samplerBuffer";

		default: DE_FATAL("Incorrect sampler buffer format"); return "";
	}
}

} // anonymous

void initBufferValidatorPrograms (vk::SourceCollections& programCollection, TestType testType, BufferType bufferType)
{
	// Layout:
	//  set = 0, location = 0 -> buffer|uniform usamplerBuffer|uniform isamplerBuffer|uniform samplerBuffersampler2D u_protectedBuffer
	//  set = 0, location = 1 -> buffer ProtectedHelper (2 * uint)
	//  set = 0, location = 2 -> uniform Data (2 * vec2 + 4 * vec4|ivec4|uvec4)
	const char* validatorShaderTemplateSamplerBuffer = "#version 450\n"
					  "layout(local_size_x = 1) in;\n"
					  "\n"
					  "layout(set=0, binding=0) ${BUFFER_TYPE} u_protectedBuffer;\n"
					  "\n"
					  "layout(set=0, binding=1) buffer ProtectedHelper\n"
					  "{\n"
					  "    highp uint zero; // set to 0\n"
					  "    highp uint unusedOut;\n"
					  "} helper;\n"
					  "\n"
					  "layout(set=0, binding=2) uniform Data\n"
					  "{\n"
					  "    highp ivec4 protectedBufferPosition[4];\n"
					  "    highp ${VAR_TYPE} protectedBufferRef[4];\n"
					  "};\n"
					  "\n"
					  "void error ()\n"
					  "{\n"
					  "    for (uint x = 0; x < 10; x += helper.zero)\n"
					  "        atomicAdd(helper.unusedOut, 1u);\n"
					  "}\n"
					  "\n"
					  "bool compare (${VAR_TYPE} a, ${VAR_TYPE} b, float threshold)\n"
					  "{\n"
					  "    return all(lessThanEqual(abs(a - b), ${VAR_TYPE}(threshold)));\n"
					  "}\n"
					  "\n"
					  "void main (void)\n"
					  "{\n"
					  "    float threshold = 0.1;\n"
					  "    for (uint i = 0; i < 4; i++)\n"
					  "    {\n"
					  "        ${VAR_TYPE} v = texelFetch(u_protectedBuffer, protectedBufferPosition[i].x);\n"
					  "        if (!compare(v, protectedBufferRef[i], threshold))\n"
					  "            error();\n"
					  "    }\n"
					  "}\n";

	const char* validatorShaderTemplateStorageBuffer = "#version 450\n"
					  "layout(local_size_x = 1) in;\n"
					  "\n"
					  "layout(set=0, binding=0) ${BUFFER_TYPE} u_protectedBuffer\n"
					  "{\n"
					  "    highp ${VAR_TYPE} protectedTestValues;\n"
					  "} testBuffer;\n"
					  "\n"
					  "layout(set=0, binding=1) buffer ProtectedHelper\n"
					  "{\n"
					  "    highp uint zero; // set to 0\n"
					  "    highp uint unusedOut;\n"
					  "} helper;\n"
					  "\n"
					  "layout(set=0, binding=2) uniform Data\n"
					  "{\n"
					  "    highp ${VAR_TYPE} protectedReferenceValues;\n"
					  "};\n"
					  "\n"
					  "void error ()\n"
					  "{\n"
					  "    for (uint x = 0; x < 10; x += helper.zero)\n"
					  "        atomicAdd(helper.unusedOut, 1u);\n"
					  "}\n"
					  "\n"
					  "bool compare (${VAR_TYPE} a, ${VAR_TYPE} b, float threshold)\n"
					  "{\n"
					  "    return all(lessThanEqual(abs(a - b), ${VAR_TYPE}(threshold)));\n"
					  "}\n"
					  "\n"
					  "void main (void)\n"
					  "{\n"
					  "    float threshold = 0.1;\n"
					  "    if (!compare(testBuffer.protectedTestValues, protectedReferenceValues, threshold))\n"
					  "        error();\n"
					  "}\n";

	tcu::StringTemplate validatorShaderTemplate(bufferType == SAMPLER_BUFFER ? validatorShaderTemplateSamplerBuffer : validatorShaderTemplateStorageBuffer);

	std::map<std::string, std::string> validatorParams;
	validatorParams["VAR_TYPE"]		= generateShaderVarString(testType);
	validatorParams["BUFFER_TYPE"]	= generateShaderBufferString(testType, bufferType);
	std::string validatorShader		= validatorShaderTemplate.specialize(validatorParams);

	const char* resetSSBOShader = "#version 450\n"
					  "layout(local_size_x = 1) in;\n"
					  "\n"
					  "layout(set=0, binding=1) buffer ProtectedHelper\n"
					  "{\n"
					  "    highp uint zero; // set to 0\n"
					  "    highp uint unusedOut;\n"
					  "} helper;\n"
					  "\n"
					  "void main (void)\n"
					  "{\n"
					  "    helper.zero = 0;\n"
					  "}\n";

	programCollection.glslSources.add("ResetSSBO") << glu::ComputeSource(resetSSBOShader);
	programCollection.glslSources.add("BufferValidator") << glu::ComputeSource(validatorShader);
}

vk::VkDescriptorType getDescriptorType (BufferType bufferType)
{
	switch (bufferType)
	{
		case STORAGE_BUFFER: return vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		case SAMPLER_BUFFER: return vk::VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
		default: DE_FATAL("Incorrect buffer type specified"); return (vk::VkDescriptorType)0;
	}
}

} // ProtectedMem
} // vkt
