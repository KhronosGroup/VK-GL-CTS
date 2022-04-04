/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2020 The Khronos Group Inc.
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
 * \brief Test copying struct which contains an empty struct.
		  Test pointer comparisons of empty struct members.
 *//*--------------------------------------------------------------------*/

#include "vktSpvAsmComputeShaderTestUtil.hpp"
#include "vktSpvAsmComputeShaderCase.hpp"
#include "vktSpvAsmEmptyStructTests.hpp"
#include "tcuStringTemplate.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktSpvAsmUtils.hpp"

namespace vkt
{
namespace SpirVAssembly
{
namespace
{

bool verifyResult(const std::vector<Resource>&,
				  const std::vector<AllocationSp>&	outputAllocs,
				  const std::vector<Resource>&		expectedOutputs,
				  tcu::TestLog&)
{
	for (deUint32 outputNdx = 0; outputNdx < static_cast<deUint32>(outputAllocs.size()); ++outputNdx)
	{
		std::vector<deUint8> expectedBytes;
		expectedOutputs[outputNdx].getBytes(expectedBytes);

		const deUint32  itemCount	= static_cast<deUint32>(expectedBytes.size()) / 4u;
		const deUint32* returned	= static_cast<const deUint32*>(outputAllocs[outputNdx]->getHostPtr());
		const deUint32* expected	= reinterpret_cast<const deUint32*>(&expectedBytes.front());

		for (deUint32 i = 0; i < itemCount; ++i)
		{
			// skip items with 0 as this is used to mark empty structure
			if (expected[i] == 0)
				continue;
			if (expected[i] != returned[i])
				return false;
		}
	}
	return true;
}

void addCopyingComputeGroup(tcu::TestCaseGroup* group)
{
	const tcu::StringTemplate shaderTemplate(
		"OpCapability Shader\n"

		"OpExtension \"SPV_KHR_storage_buffer_storage_class\"\n"

		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint GLCompute %main \"main\" %var_id\n"
		"OpExecutionMode %main LocalSize 1 1 1\n"

		"OpDecorate %var_id BuiltIn GlobalInvocationId\n"
		"OpDecorate %var_input Binding 0\n"
		"OpDecorate %var_input DescriptorSet 0\n"

		"OpDecorate %var_outdata Binding 1\n"
		"OpDecorate %var_outdata DescriptorSet 0\n"

		"OpMemberDecorate %type_container_struct 0 Offset 0\n"
		"OpMemberDecorate %type_container_struct 1 Offset ${OFFSET_1}\n"
		"OpMemberDecorate %type_container_struct 2 Offset ${OFFSET_2}\n"
		"OpMemberDecorate %type_container_struct 3 Offset ${OFFSET_3}\n"
		"OpDecorate %type_container_struct Block\n"

		+ std::string(getComputeAsmCommonTypes()) +

		//struct EmptyStruct {};
		//struct ContainerStruct {
		//  int i;
		//  A a1;
		//  A a2;
		//  int j;
		//};
		//layout(set=, binding = ) buffer block B b;

		// types
		"%type_empty_struct					= OpTypeStruct\n"
		"%type_container_struct				= OpTypeStruct %i32 %type_empty_struct %type_empty_struct %i32\n"

		"%type_container_struct_ubo_ptr		= OpTypePointer Uniform %type_container_struct\n"
		"%type_container_struct_ssbo_ptr	= OpTypePointer StorageBuffer %type_container_struct\n"

		// variables
		"%var_id							= OpVariable %uvec3ptr Input\n"
		"${VARIABLES}\n"

		// void main function
		"%main								= OpFunction %void None %voidf\n"
		"%label								= OpLabel\n"

		"${COPYING_METHOD}"

		"OpReturn\n"
		"OpFunctionEnd\n");

	struct BufferType
	{
		std::string				name;
		VkDescriptorType		descriptorType;
		std::vector<deUint32>	offsets;
		std::vector<int>		input;
		std::vector<int>		expectedOutput;
		std::string				spirvVariables;
		std::string				spirvCopyObject;

		BufferType (const std::string&				name_,
					VkDescriptorType				descriptorType_,
					const std::vector<uint32_t>&	offsets_,
					const std::vector<int>&			input_,
					const std::vector<int>&			expectedOutput_,
					const std::string&				spirvVariables_,
					const std::string&				spirvCopyObject_)
			: name				(name_)
			, descriptorType	(descriptorType_)
			, offsets			(offsets_)
			, input				(input_)
			, expectedOutput	(expectedOutput_)
			, spirvVariables	(spirvVariables_)
			, spirvCopyObject	(spirvCopyObject_)
			{}
	};
	const std::vector<BufferType> bufferTypes
	{
		{
			"ubo",
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,

			// structure decorated as Block for variable in Uniform storage class
			// must follow relaxed uniform buffer layout rules and be aligned to 16
			{0, 16, 32, 48},
			{2, 0, 0, 0, 3, 0, 0, 0, 5, 0, 0, 0, 7, 0, 0, 0},
			{2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0},

			"%var_input						= OpVariable %type_container_struct_ubo_ptr Uniform\n"
			"%var_outdata					= OpVariable %type_container_struct_ssbo_ptr StorageBuffer\n",

			"%input_copy					= OpCopyObject %type_container_struct_ubo_ptr %var_input\n"
		},
		{
			"ssbo",
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,

			{0, 4, 8, 12},
			{2, 3, 5, 7 },
			{2, 0, 0, 7 },

			"%var_input						= OpVariable %type_container_struct_ssbo_ptr StorageBuffer\n"
			"%var_outdata					= OpVariable %type_container_struct_ssbo_ptr StorageBuffer\n",

			"%input_copy					= OpCopyObject %type_container_struct_ssbo_ptr %var_input\n"
		}
	};

	struct CopyingMethod
	{
		std::string name;
		std::string spirvCopyCode;

		CopyingMethod (const std::string& name_, const std::string& spirvCopyCode_)
			: name			(name_)
			, spirvCopyCode	(spirvCopyCode_)
			{}
	};
	const std::vector<CopyingMethod> copyingMethods
	{
		{
			"copy_object",

			"%result						= OpLoad %type_container_struct %input_copy\n"
			"OpStore %var_outdata %result\n"
		},
		{
			"copy_memory",

			"OpCopyMemory %var_outdata %var_input\n"
		}
	};

	for (const auto& bufferType : bufferTypes)
	{
		for (const auto& copyingMethod : copyingMethods)
		{
			std::string name = copyingMethod.name + "_" + bufferType.name;

			std::map<std::string, std::string> specializationMap
			{
				{ "OFFSET_1",		de::toString(bufferType.offsets[1]) },
				{ "OFFSET_2",		de::toString(bufferType.offsets[2]) },
				{ "OFFSET_3",		de::toString(bufferType.offsets[3]) },
				{ "VARIABLES",		bufferType.spirvVariables },

				// NOTE: to simlify code spirvCopyObject is added also when OpCopyMemory is used
				{ "COPYING_METHOD", bufferType.spirvCopyObject + copyingMethod.spirvCopyCode },
			};

			ComputeShaderSpec spec;
			spec.assembly		= shaderTemplate.specialize(specializationMap);
			spec.numWorkGroups	= tcu::IVec3(1, 1, 1);
			spec.verifyIO		= verifyResult;
			spec.inputs.push_back (Resource(BufferSp(new Int32Buffer(bufferType.input)), bufferType.descriptorType));
			spec.outputs.push_back(Resource(BufferSp(new Int32Buffer(bufferType.expectedOutput))));
			group->addChild(new SpvAsmComputeShaderCase(group->getTestContext(), name.c_str(), "", spec));
		}
	}
}

void addPointerComparisionComputeGroup(tcu::TestCaseGroup* group)
{
	// NOTE: pointer comparison is possible only for StorageBuffer storage class

	std::string computeSource =
		"OpCapability Shader\n"
		"OpCapability VariablePointersStorageBuffer\n"

		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint GLCompute %main \"main\" %var_id %var_input %var_outdata\n"
		"OpExecutionMode %main LocalSize 1 1 1\n"

		"OpDecorate %var_id BuiltIn GlobalInvocationId\n"
		"OpDecorate %var_input Binding 0\n"
		"OpDecorate %var_input DescriptorSet 0\n"

		"OpDecorate %var_outdata Binding 1\n"
		"OpDecorate %var_outdata DescriptorSet 0\n"

		"OpMemberDecorate %type_container_struct 0 Offset 0\n"
		"OpMemberDecorate %type_container_struct 1 Offset 4\n"
		"OpMemberDecorate %type_container_struct 2 Offset 8\n"
		"OpMemberDecorate %type_container_struct 3 Offset 12\n"
		"OpDecorate %type_container_struct Block\n"

		"OpMemberDecorate %type_i32_struct 0 Offset 0\n"
		"OpDecorate %type_i32_struct Block\n"

		+ std::string(getComputeAsmCommonTypes("StorageBuffer")) +

		//struct EmptyStruct {};
		//struct ContainerStruct {
		//  int i;
		//  A a1;
		//  A a2;
		//  int j;
		//};
		//layout(set=, binding = ) buffer block B b;

		// types
		"%type_empty_struct					= OpTypeStruct\n"
		"%type_container_struct				= OpTypeStruct %i32 %type_empty_struct %type_empty_struct %i32\n"
		"%type_i32_struct					= OpTypeStruct %i32\n"

		// constants
		"%c_i32_0							= OpConstant %i32 0\n"
		"%c_i32_1							= OpConstant %i32 1\n"
		"%c_i32_2							= OpConstant %i32 2\n"

		"%type_container_struct_in_ptr		= OpTypePointer StorageBuffer %type_container_struct\n"
		"%type_i32_struct_out_ptr			= OpTypePointer StorageBuffer %type_i32_struct\n"

		"%type_func_struct_ptr_ptr			= OpTypePointer StorageBuffer %type_empty_struct\n"

		// variables
		"%var_id							= OpVariable %uvec3ptr Input\n"
		"%var_input							= OpVariable %type_container_struct_in_ptr StorageBuffer\n"
		"%var_outdata						= OpVariable %type_i32_struct_out_ptr StorageBuffer\n"

		// void main function
		"%main								= OpFunction %void None %voidf\n"
		"%label								= OpLabel\n"

		// compare pointers to empty structures
		"%ptr_to_first						= OpAccessChain %type_func_struct_ptr_ptr %var_input %c_i32_1\n"
		"%ptr_to_second						= OpAccessChain %type_func_struct_ptr_ptr %var_input %c_i32_2\n"
		"%pointers_not_equal				= OpPtrNotEqual %bool %ptr_to_first %ptr_to_second\n"
		"%result							= OpSelect %i32 %pointers_not_equal %c_i32_1 %c_i32_0\n"
		"%outloc							= OpAccessChain %i32ptr %var_outdata %c_i32_0\n"
		"OpStore %outloc %result\n"

		"OpReturn\n"
		"OpFunctionEnd\n";

	tcu::TestContext&	testCtx			= group->getTestContext();
	std::vector<int>	input			= { 2, 3, 5, 7 };
	std::vector<int>	expectedOutput	= { 1 };

	ComputeShaderSpec spec;
	spec.assembly		= computeSource;
	spec.numWorkGroups	= tcu::IVec3(1, 1, 1);
	spec.spirvVersion	= SPIRV_VERSION_1_4;
	spec.requestedVulkanFeatures.extVariablePointers = EXTVARIABLEPOINTERSFEATURES_VARIABLE_POINTERS_STORAGEBUFFER;
	spec.inputs.push_back (Resource(BufferSp(new Int32Buffer(input))));
	spec.outputs.push_back(Resource(BufferSp(new Int32Buffer(expectedOutput))));
	spec.extensions.push_back("VK_KHR_spirv_1_4");
	group->addChild(new SpvAsmComputeShaderCase(testCtx, "ssbo", "", spec));
}

} // anonymous

tcu::TestCaseGroup* createEmptyStructComputeGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group	(new tcu::TestCaseGroup(testCtx, "empty_struct", "Tests empty structs in UBOs and SSBOs"));

	addTestGroup(group.get(), "copying",			"Test copying struct which contains an empty struct",	addCopyingComputeGroup);
	addTestGroup(group.get(), "pointer_comparison",	"Test pointer comparisons of empty struct members",		addPointerComparisionComputeGroup);

	return group.release();
}

} // SpirVAssembly
} // vkt
