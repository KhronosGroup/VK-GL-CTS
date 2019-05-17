/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 Google Inc.
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
 * \brief Compute Shader Based Test Case Utility Structs/Functions
 *//*--------------------------------------------------------------------*/

#include "vktSpvAsmComputeShaderTestUtil.hpp"
#include "tcuStringTemplate.hpp"

namespace vkt
{
namespace SpirVAssembly
{
namespace
{
bool verifyOutputWithEpsilon (const std::vector<AllocationSp>& outputAllocs, const std::vector<Resource>& expectedOutputs, tcu::TestLog& log, const float epsilon)
{
	DE_ASSERT(outputAllocs.size() != 0);
	DE_ASSERT(outputAllocs.size() == expectedOutputs.size());

	for (size_t outputNdx = 0; outputNdx < outputAllocs.size(); ++outputNdx)
	{
		std::vector<deUint8>	expectedBytes;
		expectedOutputs[outputNdx].getBytes(expectedBytes);

		std::vector<float>	expectedFloats	(expectedBytes.size() / sizeof (float));
		std::vector<float>	actualFloats	(expectedBytes.size() / sizeof (float));

		memcpy(&expectedFloats[0], &expectedBytes.front(), expectedBytes.size());
		memcpy(&actualFloats[0], outputAllocs[outputNdx]->getHostPtr(), expectedBytes.size());
		for (size_t floatNdx = 0; floatNdx < actualFloats.size(); ++floatNdx)
		{
			// Use custom epsilon because of the float->string conversion
			if (fabs(expectedFloats[floatNdx] - actualFloats[floatNdx]) > epsilon)
			{
				log << tcu::TestLog::Message << "Error: The actual and expected values not matching."
					<< " Expected: " << expectedFloats[floatNdx] << " Actual: " << actualFloats[floatNdx] << " Epsilon: " << epsilon << tcu::TestLog::EndMessage;
				return false;
			}
		}
	}
	return true;
}
}

std::string getComputeAsmShaderPreamble (const std::string& capabilities,
										 const std::string& extensions,
										 const std::string& exeModes,
										 const std::string& extraEntryPoints,
										 const std::string& extraEntryPointsArguments)
{
	return
		std::string("OpCapability Shader\n") +
		capabilities +
		extensions +
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint GLCompute %main \"main\" %id " + extraEntryPointsArguments + "\n" +
		extraEntryPoints +
		"OpExecutionMode %main LocalSize 1 1 1\n" +
		exeModes;
}

const char* getComputeAsmShaderPreambleWithoutLocalSize (void)
{
	return
		"OpCapability Shader\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint GLCompute %main \"main\" %id\n";
}

std::string getComputeAsmCommonTypes (std::string blockStorageClass)
{
	return std::string(
		"%bool      = OpTypeBool\n"
		"%void      = OpTypeVoid\n"
		"%voidf     = OpTypeFunction %void\n"
		"%u32       = OpTypeInt 32 0\n"
		"%i32       = OpTypeInt 32 1\n"
		"%f32       = OpTypeFloat 32\n"
		"%uvec3     = OpTypeVector %u32 3\n"
		"%fvec3     = OpTypeVector %f32 3\n"
		"%uvec3ptr  = OpTypePointer Input %uvec3\n") +
		"%i32ptr    = OpTypePointer " + blockStorageClass + " %i32\n"
		"%f32ptr    = OpTypePointer " + blockStorageClass + " %f32\n"
		"%i32arr    = OpTypeRuntimeArray %i32\n"
		"%f32arr    = OpTypeRuntimeArray %f32\n";
}

const char* getComputeAsmCommonInt64Types (void)
{
	return
		"%i64       = OpTypeInt 64 1\n"
		"%i64ptr    = OpTypePointer Uniform %i64\n"
		"%i64arr    = OpTypeRuntimeArray %i64\n";
}

std::string getComputeAsmInputOutputBuffer (std::string blockStorageClass)
{	// Uniform | StorageBuffer
	return std::string() +
		"%buf     = OpTypeStruct %f32arr\n"
		"%bufptr  = OpTypePointer " + blockStorageClass + " %buf\n"
		"%indata    = OpVariable %bufptr " + blockStorageClass + "\n"
		"%outdata   = OpVariable %bufptr " + blockStorageClass + "\n";
}

std::string getComputeAsmInputOutputBufferTraits (std::string blockStorageClass)
{	// BufferBlock | Block
	return std::string() +
		"OpDecorate %buf " + blockStorageClass + "\n"
		"OpDecorate %indata DescriptorSet 0\n"
		"OpDecorate %indata Binding 0\n"
		"OpDecorate %outdata DescriptorSet 0\n"
		"OpDecorate %outdata Binding 1\n"
		"OpDecorate %f32arr ArrayStride 4\n"
		"OpMemberDecorate %buf 0 Offset 0\n";
}

bool verifyOutput (const std::vector<Resource>&, const std::vector<AllocationSp>& outputAllocs, const std::vector<Resource>& expectedOutputs, tcu::TestLog& log)
{
	const float	epsilon	= 0.001f;
	return verifyOutputWithEpsilon(outputAllocs, expectedOutputs, log, epsilon);
}

// Creates compute-shader assembly by specializing a boilerplate StringTemplate
// on fragments, which must (at least) map "testfun" to an OpFunction definition
// for %test_code that takes and returns a %v4f32.  Boilerplate IDs are prefixed
// with "BP_" to avoid collisions with fragments.
//
// It corresponds roughly to this GLSL:
//;
// void main (void) { test_func(vec4(gl_GlobalInvocationID)); }
std::string makeComputeShaderAssembly(const std::map<std::string, std::string>& fragments)
{
	static const char computeShaderBoilerplate[] =
		"OpCapability Shader\n"

		"${capability:opt}\n"
		"${extension:opt}\n"

		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint GLCompute %BP_main \"main\" %BP_id3u\n"
		"OpExecutionMode %BP_main LocalSize 1 1 1\n"
		"${execution_mode:opt}\n"
		"OpSource GLSL 430\n"
		"OpDecorate %BP_id3u BuiltIn GlobalInvocationId\n"

		"${decoration:opt}\n"

		SPIRV_ASSEMBLY_TYPES
		SPIRV_ASSEMBLY_CONSTANTS
		SPIRV_ASSEMBLY_ARRAYS

		"%ip_v3u32  = OpTypePointer Input %v3u32\n"
		"%BP_id3u   = OpVariable %ip_v3u32 Input\n"

		"${pre_main:opt}\n"

		"%BP_main   = OpFunction %void None %voidf\n"
		"%BP_label  = OpLabel\n"
		"%BP_id3ul  = OpLoad %v3u32 %BP_id3u\n"
		"%BP_id4u   = OpCompositeConstruct %v4u32 %BP_id3ul %c_u32_0\n"
		"%BP_id4f   = OpConvertUToF %v4f32 %BP_id4u\n"
		"%BP_result = OpFunctionCall %v4f32 %test_code %BP_id4f\n"
		"             OpReturn\n"
		"             OpFunctionEnd\n"
		"\n"
		"${testfun}\n"
		"\n"

		"%isUniqueIdZero = OpFunction %bool None %bool_function\n"
		"%BP_getId_label = OpLabel\n"
		"%BP_id_0_ptr = OpAccessChain %ip_u32 %BP_id3u %c_u32_0\n"
		"%BP_id_1_ptr = OpAccessChain %ip_u32 %BP_id3u %c_u32_1\n"
		"%BP_id_2_ptr = OpAccessChain %ip_u32 %BP_id3u %c_u32_2\n"
		"%BP_id_0_val = OpLoad %u32 %BP_id_0_ptr\n"
		"%BP_id_1_val = OpLoad %u32 %BP_id_1_ptr\n"
		"%BP_id_2_val = OpLoad %u32 %BP_id_2_ptr\n"
		"%BP_id_uni_0 = OpBitwiseOr %u32 %BP_id_0_val %BP_id_1_val\n"
		"  %BP_id_uni = OpBitwiseOr %u32 %BP_id_2_val %BP_id_uni_0\n"
		" %is_id_zero = OpIEqual %bool %BP_id_uni %c_u32_0\n"
		"               OpReturnValue %is_id_zero\n"
		"               OpFunctionEnd\n";

	return tcu::StringTemplate(computeShaderBoilerplate).specialize(fragments);
}

} // SpirVAssembly
} // vkt
