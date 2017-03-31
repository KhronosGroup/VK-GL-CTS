/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017 Google Inc.
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
 * \brief SPIR-V Assembly Tests for the SPV_KHR_variable_pointers extension
 *//*--------------------------------------------------------------------*/

#include "tcuFloat.hpp"
#include "tcuRGBA.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuTestLog.hpp"
#include "tcuVectorUtil.hpp"

#include "vkDefs.hpp"
#include "vkDeviceUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPlatform.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkStrUtil.hpp"
#include "vkTypeUtil.hpp"

#include "deRandom.hpp"
#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"
#include "deMath.h"

#include "vktSpvAsmComputeShaderCase.hpp"
#include "vktSpvAsmComputeShaderTestUtil.hpp"
#include "vktSpvAsmGraphicsShaderTestUtil.hpp"
#include "vktSpvAsmVariablePointersTests.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"

#include <limits>
#include <map>
#include <string>
#include <sstream>
#include <utility>

namespace vkt
{
namespace SpirVAssembly
{

using namespace vk;
using std::map;
using std::string;
using std::vector;
using tcu::IVec3;
using tcu::IVec4;
using tcu::RGBA;
using tcu::TestLog;
using tcu::TestStatus;
using tcu::Vec4;
using de::UniquePtr;
using tcu::StringTemplate;
using tcu::Vec4;

namespace
{

template<typename T>
void fillRandomScalars (de::Random& rnd, T minValue, T maxValue, void* dst, int numValues, int offset = 0)
{
	T* const typedPtr = (T*)dst;
	for (int ndx = 0; ndx < numValues; ndx++)
		typedPtr[offset + ndx] = randomScalar<T>(rnd, minValue, maxValue);
}

void addComputeVariablePointersGroup (tcu::TestCaseGroup* group)
{
	tcu::TestContext&				testCtx					= group->getTestContext();
	de::Random						rnd						(deStringHash(group->getName()));
	const int						seed					= testCtx.getCommandLine().getBaseSeed();
	const int						numMuxes				= 100;
	std::string						inputArraySize			= "200";
	vector<float>					inputAFloats			(2*numMuxes, 0);
	vector<float>					inputBFloats			(2*numMuxes, 0);
	vector<float>					inputSFloats			(numMuxes, 0);
	vector<float>					AmuxAOutputFloats		(numMuxes, 0);
	vector<float>					AmuxBOutputFloats		(numMuxes, 0);
	vector<float>					incrAmuxAOutputFloats	(numMuxes, 0);
	vector<float>					incrAmuxBOutputFloats	(numMuxes, 0);
	VulkanFeatures					requiredFeatures;

	// Each output entry is chosen as follows: ( 0 <= i < numMuxes)
	// 1) For tests with one input buffer:  output[i] = (s[i] < 0) ? A[2*i] : A[2*i+1];
	// 2) For tests with two input buffers: output[i] = (s[i] < 0) ? A[i]   : B[i];

	fillRandomScalars(rnd, -100.f, 100.f, &inputAFloats[0], 2*numMuxes);
	fillRandomScalars(rnd, -100.f, 100.f, &inputBFloats[0], 2*numMuxes);

	// We want to guarantee that the S input has some positive and some negative values.
	// We choose random negative numbers for the first half, random positive numbers for the second half, and then shuffle.
	fillRandomScalars(rnd, -100.f, -1.f , &inputSFloats[0], numMuxes / 2);
	fillRandomScalars(rnd, 1.f   , 100.f, &inputSFloats[numMuxes / 2], numMuxes / 2);
	de::Random(seed).shuffle(inputSFloats.begin(), inputSFloats.end());

	for (size_t i = 0; i < numMuxes; ++i)
	{
		AmuxAOutputFloats[i]     = (inputSFloats[i] < 0) ? inputAFloats[2*i]     : inputAFloats[2*i+1];
		AmuxBOutputFloats[i]	 = (inputSFloats[i] < 0) ? inputAFloats[i]		 : inputBFloats[i];
		incrAmuxAOutputFloats[i] = (inputSFloats[i] < 0) ? 1 + inputAFloats[2*i] : 1+ inputAFloats[2*i+1];
		incrAmuxBOutputFloats[i] = (inputSFloats[i] < 0) ? 1 + inputAFloats[i]	 : 1 + inputBFloats[i];
	}

	const StringTemplate shaderTemplate (
		"OpCapability Shader\n"

		"${ExtraCapability}\n"

		"OpExtension \"SPV_KHR_variable_pointers\"\n"
		"OpExtension \"SPV_KHR_storage_buffer_storage_class\"\n"
		"OpMemoryModel Logical GLSL450\n"
		"OpEntryPoint GLCompute %main \"main\" %id\n"
		"OpExecutionMode %main LocalSize 1 1 1\n"

		"OpSource GLSL 430\n"
		"OpName %main           \"main\"\n"
		"OpName %id             \"gl_GlobalInvocationID\"\n"

		// Decorations
		"OpDecorate %id BuiltIn GlobalInvocationId\n"
		"OpDecorate %indata_a DescriptorSet 0\n"
		"OpDecorate %indata_a Binding 0\n"
		"OpDecorate %indata_b DescriptorSet 0\n"
		"OpDecorate %indata_b Binding 1\n"
		"OpDecorate %indata_s DescriptorSet 0\n"
		"OpDecorate %indata_s Binding 2\n"
		"OpDecorate %outdata DescriptorSet 0\n"
		"OpDecorate %outdata Binding 3\n"
		"OpDecorate %f32arr ArrayStride 4\n"
		"OpDecorate %sb_f32ptr ArrayStride 4\n"
		"OpDecorate %buf Block\n"
		"OpMemberDecorate %buf 0 Offset 0\n"

		+ string(getComputeAsmCommonTypes()) +

		"%sb_f32ptr				= OpTypePointer StorageBuffer %f32\n"
		"%buf					= OpTypeStruct %f32arr\n"
		"%bufptr				= OpTypePointer StorageBuffer %buf\n"
		"%indata_a				= OpVariable %bufptr StorageBuffer\n"
		"%indata_b				= OpVariable %bufptr StorageBuffer\n"
		"%indata_s				= OpVariable %bufptr StorageBuffer\n"
		"%outdata				= OpVariable %bufptr StorageBuffer\n"
		"%id					= OpVariable %uvec3ptr Input\n"
		"%zero				    = OpConstant %i32 0\n"
		"%one					= OpConstant %i32 1\n"
		"%fzero					= OpConstant %f32 0\n"
		"%fone					= OpConstant %f32 1\n"

		"${ExtraTypes}"

		"${ExtraGlobalScopeVars}"

		// We're going to put the "selector" function here.
		// This function type is needed tests that use OpFunctionCall.
		"%selector_func_type	= OpTypeFunction %sb_f32ptr %bool %sb_f32ptr %sb_f32ptr\n"
		"%choose_input_func		= OpFunction %sb_f32ptr None %selector_func_type\n"
		"%is_neg_param			= OpFunctionParameter %bool\n"
		"%first_ptr_param		= OpFunctionParameter %sb_f32ptr\n"
		"%second_ptr_param		= OpFunctionParameter %sb_f32ptr\n"
		"%selector_func_begin	= OpLabel\n"
		"%result_ptr			= OpSelect %sb_f32ptr %is_neg_param %first_ptr_param %second_ptr_param\n"
		"OpReturnValue %result_ptr\n"
		"OpFunctionEnd\n"

		// main function is the entry_point
		"%main					= OpFunction %void None %voidf\n"
		"%label					= OpLabel\n"

		"${ExtraFunctionScopeVars}"

		"%idval					= OpLoad %uvec3 %id\n"
		"%i						= OpCompositeExtract %u32 %idval 0\n"
		"%two_i					= OpIAdd %u32 %i %i\n"
		"%two_i_plus_1			= OpIAdd %u32 %two_i %one\n"
		"%inloc_a_i				= OpAccessChain %sb_f32ptr %indata_a %zero %i\n"
		"%inloc_b_i				= OpAccessChain %sb_f32ptr %indata_b %zero %i\n"
		"%inloc_s_i             = OpAccessChain %sb_f32ptr %indata_s %zero %i\n"
		"%outloc_i              = OpAccessChain %sb_f32ptr %outdata  %zero %i\n"
		"%inloc_a_2i			= OpAccessChain %sb_f32ptr %indata_a %zero %two_i\n"
		"%inloc_a_2i_plus_1		= OpAccessChain %sb_f32ptr %indata_a %zero %two_i_plus_1\n"
		"%inval_s_i				= OpLoad %f32 %inloc_s_i\n"
		"%is_neg				= OpFOrdLessThan %bool %inval_s_i %fzero\n"

		"${ExtraSetupComputations}"

		"${ResultStrategy}"

		"%mux_output			= OpLoad %f32 ${VarPtrName}\n"
		"						  OpStore %outloc_i %mux_output\n"
		"						  OpReturn\n"
		"						  OpFunctionEnd\n");

	const bool singleInputBuffer[]  = { true, false };
	for (int inputBufferTypeIndex = 0 ; inputBufferTypeIndex < 2; ++inputBufferTypeIndex)
	{
		const bool isSingleInputBuffer			= singleInputBuffer[inputBufferTypeIndex];
		const string extraCap					= isSingleInputBuffer	? "OpCapability VariablePointersStorageBuffer\n" : "OpCapability VariablePointers\n";
		const vector<float>& expectedOutput		= isSingleInputBuffer	? AmuxAOutputFloats		 : AmuxBOutputFloats;
		const vector<float>& expectedIncrOutput	= isSingleInputBuffer	? incrAmuxAOutputFloats	 : incrAmuxBOutputFloats;
		const string bufferType					= isSingleInputBuffer	? "single_buffer"	 : "two_buffers";
		const string muxInput1					= isSingleInputBuffer	? " %inloc_a_2i "		 : " %inloc_a_i ";
		const string muxInput2					= isSingleInputBuffer	? " %inloc_a_2i_plus_1 " : " %inloc_b_i ";

		// Set the proper extension features required for the test
		if (isSingleInputBuffer)
			requiredFeatures.extVariablePointers	= EXTVARIABLEPOINTERSFEATURES_VARIABLE_POINTERS_STORAGEBUFFER;
		else
			requiredFeatures.extVariablePointers	= EXTVARIABLEPOINTERSFEATURES_VARIABLE_POINTERS;

		{ // Variable Pointer Reads (using OpSelect)
			ComputeShaderSpec				spec;
			map<string, string>				specs;
			string name						= "reads_opselect_" + bufferType;
			specs["ExtraCapability"]		= extraCap;
			specs["ExtraTypes"]				= "";
			specs["ExtraGlobalScopeVars"]	= "";
			specs["ExtraFunctionScopeVars"]	= "";
			specs["ExtraSetupComputations"]	= "";
			specs["VarPtrName"]				= "%mux_output_var_ptr";
			specs["ResultStrategy"]			= "%mux_output_var_ptr	= OpSelect %sb_f32ptr %is_neg" + muxInput1 + muxInput2 + "\n";
			spec.inputTypes[0]				= VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			spec.inputTypes[1]				= VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			spec.inputTypes[2]				= VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			spec.assembly					= shaderTemplate.specialize(specs);
			spec.numWorkGroups				= IVec3(numMuxes, 1, 1);
			spec.requestedVulkanFeatures	= requiredFeatures;
			spec.inputs.push_back(BufferSp(new Float32Buffer(inputAFloats)));
			spec.inputs.push_back(BufferSp(new Float32Buffer(inputBFloats)));
			spec.inputs.push_back(BufferSp(new Float32Buffer(inputSFloats)));
			spec.outputs.push_back(BufferSp(new Float32Buffer(expectedOutput)));
			spec.extensions.push_back("VK_KHR_variable_pointers");
			group->addChild(new SpvAsmComputeShaderCase(testCtx, name.c_str(), name.c_str(), spec));
		}
		{ // Variable Pointer Reads (using OpFunctionCall)
			ComputeShaderSpec				spec;
			map<string, string>				specs;
			string name						= "reads_opfunctioncall_" + bufferType;
			specs["ExtraCapability"]		= extraCap;
			specs["ExtraTypes"]				= "";
			specs["ExtraGlobalScopeVars"]	= "";
			specs["ExtraFunctionScopeVars"]	= "";
			specs["ExtraSetupComputations"]	= "";
			specs["VarPtrName"]				= "%mux_output_var_ptr";
			specs["ResultStrategy"]			= "%mux_output_var_ptr = OpFunctionCall %sb_f32ptr %choose_input_func %is_neg" + muxInput1 + muxInput2 + "\n";
			spec.inputTypes[0]				= VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			spec.inputTypes[1]				= VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			spec.inputTypes[2]				= VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			spec.assembly					= shaderTemplate.specialize(specs);
			spec.numWorkGroups				= IVec3(numMuxes, 1, 1);
			spec.requestedVulkanFeatures	= requiredFeatures;
			spec.inputs.push_back(BufferSp(new Float32Buffer(inputAFloats)));
			spec.inputs.push_back(BufferSp(new Float32Buffer(inputBFloats)));
			spec.inputs.push_back(BufferSp(new Float32Buffer(inputSFloats)));
			spec.outputs.push_back(BufferSp(new Float32Buffer(expectedOutput)));
			spec.extensions.push_back("VK_KHR_variable_pointers");
			group->addChild(new SpvAsmComputeShaderCase(testCtx, name.c_str(), name.c_str(), spec));
		}
		{ // Variable Pointer Reads (using OpPhi)
			ComputeShaderSpec				spec;
			map<string, string>				specs;
			string name						= "reads_opphi_" + bufferType;
			specs["ExtraCapability"]		= extraCap;
			specs["ExtraTypes"]				= "";
			specs["ExtraGlobalScopeVars"]	= "";
			specs["ExtraFunctionScopeVars"]	= "";
			specs["ExtraSetupComputations"]	= "";
			specs["VarPtrName"]				= "%mux_output_var_ptr";
			specs["ResultStrategy"]			=
				"							  OpSelectionMerge %end_label None\n"
				"							  OpBranchConditional %is_neg %take_mux_input_1 %take_mux_input_2\n"
				"%take_mux_input_1			= OpLabel\n"
				"							  OpBranch %end_label\n"
				"%take_mux_input_2			= OpLabel\n"
				"						      OpBranch %end_label\n"
				"%end_label					= OpLabel\n"
				"%mux_output_var_ptr		= OpPhi %sb_f32ptr" + muxInput1 + "%take_mux_input_1" + muxInput2 + "%take_mux_input_2\n";
			spec.inputTypes[0]				= VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			spec.inputTypes[1]				= VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			spec.inputTypes[2]				= VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			spec.assembly					= shaderTemplate.specialize(specs);
			spec.numWorkGroups				= IVec3(numMuxes, 1, 1);
			spec.requestedVulkanFeatures	= requiredFeatures;
			spec.inputs.push_back(BufferSp(new Float32Buffer(inputAFloats)));
			spec.inputs.push_back(BufferSp(new Float32Buffer(inputBFloats)));
			spec.inputs.push_back(BufferSp(new Float32Buffer(inputSFloats)));
			spec.outputs.push_back(BufferSp(new Float32Buffer(expectedOutput)));
			spec.extensions.push_back("VK_KHR_variable_pointers");
			group->addChild(new SpvAsmComputeShaderCase(testCtx, name.c_str(), name.c_str(), spec));
		}
		{ // Variable Pointer Reads (using OpCopyObject)
			ComputeShaderSpec				spec;
			map<string, string>				specs;
			string name						= "reads_opcopyobject_" + bufferType;
			specs["ExtraCapability"]		= extraCap;
			specs["ExtraTypes"]				= "";
			specs["ExtraGlobalScopeVars"]	= "";
			specs["ExtraFunctionScopeVars"]	= "";
			specs["ExtraSetupComputations"]	= "";
			specs["VarPtrName"]				= "%mux_output_var_ptr";
			specs["ResultStrategy"]			=
				"%mux_input_1_copy			= OpCopyObject %sb_f32ptr" + muxInput1 + "\n"
				"%mux_input_2_copy			= OpCopyObject %sb_f32ptr" + muxInput2 + "\n"
				"%mux_output_var_ptr		= OpSelect %sb_f32ptr %is_neg %mux_input_1_copy %mux_input_2_copy\n";
			spec.inputTypes[0]				= VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			spec.inputTypes[1]				= VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			spec.inputTypes[2]				= VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			spec.assembly					= shaderTemplate.specialize(specs);
			spec.numWorkGroups				= IVec3(numMuxes, 1, 1);
			spec.requestedVulkanFeatures	= requiredFeatures;
			spec.inputs.push_back(BufferSp(new Float32Buffer(inputAFloats)));
			spec.inputs.push_back(BufferSp(new Float32Buffer(inputBFloats)));
			spec.inputs.push_back(BufferSp(new Float32Buffer(inputSFloats)));
			spec.outputs.push_back(BufferSp(new Float32Buffer(expectedOutput)));
			spec.extensions.push_back("VK_KHR_variable_pointers");
			group->addChild(new SpvAsmComputeShaderCase(testCtx, name.c_str(), name.c_str(), spec));
		}
		{ // Test storing into Private variables.
			const char* storageClasses[]		= {"Private", "Function"};
			for (int classId = 0; classId < 2; ++classId)
			{
				ComputeShaderSpec				spec;
				map<string, string>				specs;
				std::string storageClass		= storageClasses[classId];
				std::string name				= "stores_" + string(de::toLower(storageClass)) + "_" + bufferType;
				std::string description			= "Test storing variable pointer into " + storageClass + " variable.";
				std::string extraVariable		= "%mux_output_copy	= OpVariable %sb_f32ptrptr " + storageClass + "\n";
				specs["ExtraTypes"]				= "%sb_f32ptrptr = OpTypePointer " + storageClass + " %sb_f32ptr\n";
				specs["ExtraCapability"]		= extraCap;
				specs["ExtraGlobalScopeVars"]	= (classId == 0) ? extraVariable : "";
				specs["ExtraFunctionScopeVars"]	= (classId == 1) ? extraVariable : "";
				specs["ExtraSetupComputations"]	= "";
				specs["VarPtrName"]				= "%mux_output_var_ptr";
				specs["ResultStrategy"]			=
					"%opselect_result			= OpSelect %sb_f32ptr %is_neg" + muxInput1 + muxInput2 + "\n"
					"							  OpStore %mux_output_copy %opselect_result\n"
					"%mux_output_var_ptr		= OpLoad %sb_f32ptr %mux_output_copy\n";
				spec.inputTypes[0]				= VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				spec.inputTypes[1]				= VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				spec.inputTypes[2]				= VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				spec.assembly					= shaderTemplate.specialize(specs);
				spec.numWorkGroups				= IVec3(numMuxes, 1, 1);
				spec.requestedVulkanFeatures	= requiredFeatures;
				spec.inputs.push_back(BufferSp(new Float32Buffer(inputAFloats)));
				spec.inputs.push_back(BufferSp(new Float32Buffer(inputBFloats)));
				spec.inputs.push_back(BufferSp(new Float32Buffer(inputSFloats)));
				spec.outputs.push_back(BufferSp(new Float32Buffer(expectedOutput)));
				spec.extensions.push_back("VK_KHR_variable_pointers");
				group->addChild(new SpvAsmComputeShaderCase(testCtx, name.c_str(), description.c_str(), spec));
			}
		}
		{ // Variable Pointer Reads (Using OpPtrAccessChain)
			ComputeShaderSpec				spec;
			map<string, string>				specs;
			std::string name				= "reads_opptraccesschain_" + bufferType;
			std::string in_1				= isSingleInputBuffer ? " %a_2i_ptr "		 : " %a_i_ptr ";
			std::string in_2				= isSingleInputBuffer ? " %a_2i_plus_1_ptr " : " %b_i_ptr ";
			specs["ExtraTypes"]				= "";
			specs["ExtraCapability"]		= extraCap;
			specs["ExtraGlobalScopeVars"]	= "";
			specs["ExtraFunctionScopeVars"]	= "";
			specs["ExtraSetupComputations"]	= "";
			specs["VarPtrName"]				= "%mux_output_var_ptr";
			specs["ResultStrategy"]			=
					"%a_ptr					= OpAccessChain %sb_f32ptr %indata_a %zero %zero\n"
					"%b_ptr					= OpAccessChain %sb_f32ptr %indata_b %zero %zero\n"
					"%s_ptr					= OpAccessChain %sb_f32ptr %indata_s %zero %zero\n"
					"%out_ptr               = OpAccessChain %sb_f32ptr %outdata  %zero %zero\n"
					"%a_i_ptr               = OpPtrAccessChain %sb_f32ptr %a_ptr %i\n"
					"%b_i_ptr               = OpPtrAccessChain %sb_f32ptr %b_ptr %i\n"
					"%s_i_ptr               = OpPtrAccessChain %sb_f32ptr %s_ptr %i\n"
					"%a_2i_ptr              = OpPtrAccessChain %sb_f32ptr %a_ptr %two_i\n"
					"%a_2i_plus_1_ptr       = OpPtrAccessChain %sb_f32ptr %a_ptr %two_i_plus_1\n"
					"%mux_output_var_ptr    = OpSelect %sb_f32ptr %is_neg " + in_1 + in_2 + "\n";
			spec.inputTypes[0]				= VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			spec.inputTypes[1]				= VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			spec.inputTypes[2]				= VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			spec.assembly					= shaderTemplate.specialize(specs);
			spec.numWorkGroups				= IVec3(numMuxes, 1, 1);
			spec.requestedVulkanFeatures	= requiredFeatures;
			spec.inputs.push_back(BufferSp(new Float32Buffer(inputAFloats)));
			spec.inputs.push_back(BufferSp(new Float32Buffer(inputBFloats)));
			spec.inputs.push_back(BufferSp(new Float32Buffer(inputSFloats)));
			spec.outputs.push_back(BufferSp(new Float32Buffer(expectedOutput)));
			spec.extensions.push_back("VK_KHR_variable_pointers");
			group->addChild(new SpvAsmComputeShaderCase(testCtx, name.c_str(), name.c_str(), spec));
		}
		{   // Variable Pointer Writes
			ComputeShaderSpec				spec;
			map<string, string>				specs;
			std::string	name				= "writes_" + bufferType;
			specs["ExtraCapability"]		= extraCap;
			specs["ExtraTypes"]				= "";
			specs["ExtraGlobalScopeVars"]	= "";
			specs["ExtraFunctionScopeVars"]	= "";
			specs["ExtraSetupComputations"]	= "";
			specs["VarPtrName"]				= "%mux_output_var_ptr";
			specs["ResultStrategy"]			= "%mux_output_var_ptr = OpSelect %sb_f32ptr %is_neg" + muxInput1 + muxInput2 + "\n" +
											  "               %val = OpLoad %f32 %mux_output_var_ptr\n"
											  "        %val_plus_1 = OpFAdd %f32 %val %fone\n"
											  "						 OpStore %mux_output_var_ptr %val_plus_1\n";
			spec.inputTypes[0]				= VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			spec.inputTypes[1]				= VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			spec.inputTypes[2]				= VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			spec.assembly					= shaderTemplate.specialize(specs);
			spec.numWorkGroups				= IVec3(numMuxes, 1, 1);
			spec.requestedVulkanFeatures	= requiredFeatures;
			spec.inputs.push_back(BufferSp(new Float32Buffer(inputAFloats)));
			spec.inputs.push_back(BufferSp(new Float32Buffer(inputBFloats)));
			spec.inputs.push_back(BufferSp(new Float32Buffer(inputSFloats)));
			spec.outputs.push_back(BufferSp(new Float32Buffer(expectedIncrOutput)));
			spec.extensions.push_back("VK_KHR_variable_pointers");
			group->addChild(new SpvAsmComputeShaderCase(testCtx, name.c_str(), name.c_str(), spec));
		}

		// If we only have VariablePointersStorageBuffer, then the extension does not apply to Workgroup storage class.
		// Therefore the Workgroup tests apply to cases where the VariablePointers capability is used (when 2 input buffers are used).
		if (!isSingleInputBuffer)
		{
			// VariablePointers on Workgroup
			ComputeShaderSpec				spec;
			map<string, string>				specs;
			std::string name				= "workgroup_" + bufferType;
			specs["ExtraCapability"]		= extraCap;
			specs["ExtraTypes"]				=
					"%c_i32_N				= OpConstant %i32 " + inputArraySize + " \n"
					"%f32arr_N				= OpTypeArray %f32 %c_i32_N\n"
					"%f32arr_wrkgrp_ptr		= OpTypePointer Workgroup %f32arr_N\n"
					"%f32_wrkgrp_ptr		= OpTypePointer Workgroup %f32\n";
			specs["ExtraGlobalScopeVars"]	=
					"%AW					= OpVariable %f32arr_wrkgrp_ptr Workgroup\n"
					"%BW					= OpVariable %f32arr_wrkgrp_ptr Workgroup\n";
			specs["ExtraFunctionScopeVars"]	= "";
			specs["ExtraSetupComputations"]	=
					"%loc_AW_i				= OpAccessChain %f32_wrkgrp_ptr %AW %i\n"
					"%loc_BW_i				= OpAccessChain %f32_wrkgrp_ptr %BW %i\n"
					"%inval_a_i				= OpLoad %f32 %inloc_a_i\n"
					"%inval_b_i				= OpLoad %f32 %inloc_b_i\n"
					"%inval_a_2i			= OpLoad %f32 %inloc_a_2i\n"
					"%inval_a_2i_plus_1		= OpLoad %f32 %inloc_a_2i_plus_1\n";
			specs["VarPtrName"]				= "%output_var_ptr";
			specs["ResultStrategy"]			=
					"						  OpStore %loc_AW_i %inval_a_i\n"
					"						  OpStore %loc_BW_i %inval_b_i\n"
					"%output_var_ptr		= OpSelect %f32_wrkgrp_ptr %is_neg %loc_AW_i %loc_BW_i\n";
			spec.inputTypes[0]				= VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			spec.inputTypes[1]				= VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			spec.inputTypes[2]				= VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			spec.assembly					= shaderTemplate.specialize(specs);
			spec.numWorkGroups				= IVec3(numMuxes, 1, 1);
			spec.requestedVulkanFeatures	= requiredFeatures;
			spec.inputs.push_back(BufferSp(new Float32Buffer(inputAFloats)));
			spec.inputs.push_back(BufferSp(new Float32Buffer(inputBFloats)));
			spec.inputs.push_back(BufferSp(new Float32Buffer(inputSFloats)));
			spec.outputs.push_back(BufferSp(new Float32Buffer(expectedOutput)));
			spec.extensions.push_back("VK_KHR_variable_pointers");
			group->addChild(new SpvAsmComputeShaderCase(testCtx, name.c_str(), name.c_str(), spec));
		}
	}
}

void addGraphicsVariablePointersGroup (tcu::TestCaseGroup* testGroup)
{
	tcu::TestContext&				testCtx					= testGroup->getTestContext();
	de::Random						rnd						(deStringHash(testGroup->getName()));
	map<string, string>				fragments;
	RGBA							defaultColors[4];
	vector<string>					extensions;
	const int						seed					= testCtx.getCommandLine().getBaseSeed();
	const int						numMuxes				= 100;
	const std::string				numMuxesStr				= "100";
	vector<float>					inputAFloats			(2*numMuxes, 0);
	vector<float>					inputBFloats			(2*numMuxes, 0);
	vector<float>					inputSFloats			(numMuxes, 0);
	vector<float>					AmuxAOutputFloats		(numMuxes, 0);
	vector<float>					AmuxBOutputFloats		(numMuxes, 0);
	vector<float>					incrAmuxAOutputFloats	(numMuxes, 0);
	vector<float>					incrAmuxBOutputFloats	(numMuxes, 0);
	VulkanFeatures					requiredFeatures;

	extensions.push_back("VK_KHR_variable_pointers");
	getDefaultColors(defaultColors);

	// Each output entry is chosen as follows: ( 0 <= i < numMuxes)
	// 1) For tests with one input buffer:  output[i] = (s[i] < 0) ? A[2*i] : A[2*i+1];
	// 2) For tests with two input buffers: output[i] = (s[i] < 0) ? A[i]   : B[i];

	fillRandomScalars(rnd, -100.f, 100.f, &inputAFloats[0], 2*numMuxes);
	fillRandomScalars(rnd, -100.f, 100.f, &inputBFloats[0], 2*numMuxes);

	// We want to guarantee that the S input has some positive and some negative values.
	// We choose random negative numbers for the first half, random positive numbers for the second half, and then shuffle.
	fillRandomScalars(rnd, -100.f, -1.f , &inputSFloats[0], numMuxes / 2);
	fillRandomScalars(rnd, 1.f   , 100.f, &inputSFloats[numMuxes / 2], numMuxes / 2);
	de::Random(seed).shuffle(inputSFloats.begin(), inputSFloats.end());

	for (size_t i = 0; i < numMuxes; ++i)
	{
		AmuxAOutputFloats[i]	 = (inputSFloats[i] < 0) ? inputAFloats[2*i]	 : inputAFloats[2*i+1];
		AmuxBOutputFloats[i]	 = (inputSFloats[i] < 0) ? inputAFloats[i]		 : inputBFloats[i];
		incrAmuxAOutputFloats[i] = (inputSFloats[i] < 0) ? 1 + inputAFloats[2*i] : 1 + inputAFloats[2*i+1];
		incrAmuxBOutputFloats[i] = (inputSFloats[i] < 0) ? 1 + inputAFloats[i]	 : 1 + inputBFloats[i];
	}

	fragments["extension"]		= "OpExtension \"SPV_KHR_variable_pointers\"\n"
								  "OpExtension \"SPV_KHR_storage_buffer_storage_class\"\n";

	const StringTemplate preMain		(
		"%c_i32_limit = OpConstant %i32 " + numMuxesStr + "\n"
		"     %sb_f32 = OpTypePointer StorageBuffer %f32\n"
		"     %ra_f32 = OpTypeRuntimeArray %f32\n"
		"        %buf = OpTypeStruct %ra_f32\n"
		"     %sb_buf = OpTypePointer StorageBuffer %buf\n"

		" ${ExtraTypes}"

		" ${ExtraGlobalScopeVars}"

		"   %indata_a = OpVariable %sb_buf StorageBuffer\n"
		"   %indata_b = OpVariable %sb_buf StorageBuffer\n"
		"   %indata_s = OpVariable %sb_buf StorageBuffer\n"
		"    %outdata = OpVariable %sb_buf StorageBuffer\n"

		" ${ExtraFunctions} ");

	const std::string selectorFunction	(
		// We're going to put the "selector" function here.
		// This function type is needed for tests that use OpFunctionCall.
		"%selector_func_type	= OpTypeFunction %sb_f32 %bool %sb_f32 %sb_f32\n"
		"%choose_input_func		= OpFunction %sb_f32 None %selector_func_type\n"
		"%is_neg_param			= OpFunctionParameter %bool\n"
		"%first_ptr_param		= OpFunctionParameter %sb_f32\n"
		"%second_ptr_param		= OpFunctionParameter %sb_f32\n"
		"%selector_func_begin	= OpLabel\n"
		"%result_ptr			= OpSelect %sb_f32 %is_neg_param %first_ptr_param %second_ptr_param\n"
		"OpReturnValue %result_ptr\n"
		"OpFunctionEnd\n");

	const StringTemplate decoration		(
		"OpMemberDecorate %buf 0 Offset 0\n"
		"OpDecorate %buf Block\n"
		"OpDecorate %ra_f32 ArrayStride 4\n"
		"OpDecorate %sb_f32 ArrayStride 4\n"
		"OpDecorate %indata_a DescriptorSet 0\n"
		"OpDecorate %indata_b DescriptorSet 0\n"
		"OpDecorate %indata_s DescriptorSet 0\n"
		"OpDecorate %outdata  DescriptorSet 0\n"
		"OpDecorate %indata_a Binding 0\n"
		"OpDecorate %indata_b Binding 1\n"
		"OpDecorate %indata_s Binding 2\n"
		"OpDecorate %outdata  Binding 3\n");

	const StringTemplate testFunction	(
		"%test_code		= OpFunction %v4f32 None %v4f32_function\n"
		"%param			= OpFunctionParameter %v4f32\n"
		"%entry			= OpLabel\n"

		"${ExtraFunctionScopeVars}"

		"%i				= OpVariable %fp_i32 Function\n"

		"%should_run    = OpFunctionCall %bool %isUniqueIdZero\n"
		"                 OpSelectionMerge %end_if None\n"
		"                 OpBranchConditional %should_run %run_test %end_if\n"

		"%run_test      = OpLabel\n"
		"				OpStore %i %c_i32_0\n"
		"				OpBranch %loop\n"
		// loop header
		"%loop			= OpLabel\n"
		"%15			= OpLoad %i32 %i\n"
		"%lt			= OpSLessThan %bool %15 %c_i32_limit\n"
		"				OpLoopMerge %merge %inc None\n"
		"				OpBranchConditional %lt %write %merge\n"
		// loop body
		"%write				= OpLabel\n"
		"%30				= OpLoad %i32 %i\n"
		"%two_i				= OpIAdd %i32 %30 %30\n"
		"%two_i_plus_1		= OpIAdd %i32 %two_i %c_i32_1\n"
		"%loc_s_i			= OpAccessChain %sb_f32 %indata_s %c_i32_0 %30\n"
		"%loc_a_i			= OpAccessChain %sb_f32 %indata_a %c_i32_0 %30\n"
		"%loc_b_i			= OpAccessChain %sb_f32 %indata_b %c_i32_0 %30\n"
		"%loc_a_2i			= OpAccessChain %sb_f32 %indata_a %c_i32_0 %two_i\n"
		"%loc_a_2i_plus_1	= OpAccessChain %sb_f32 %indata_a %c_i32_0 %two_i_plus_1\n"
		"%loc_outdata_i		= OpAccessChain %sb_f32 %outdata  %c_i32_0 %30\n"
		"%val_s_i			= OpLoad %f32 %loc_s_i\n"
		"%is_neg			= OpFOrdLessThan %bool %val_s_i %c_f32_0\n"

		// select using a strategy.
		"${ResultStrategy}"

		// load through the variable pointer
		"%mux_output	= OpLoad %f32 ${VarPtrName}\n"

		// store to the output vector.
		"				OpStore %loc_outdata_i %mux_output\n"
		"				OpBranch %inc\n"
		// ++i
		"  %inc			= OpLabel\n"
		"   %37			= OpLoad %i32 %i\n"
		"   %39			= OpIAdd %i32 %37 %c_i32_1\n"
		"         OpStore %i %39\n"
		"         OpBranch %loop\n"

		// Return and FunctionEnd
		"%merge			= OpLabel\n"
		"                 OpBranch %end_if\n"
		"%end_if		= OpLabel\n"
		"OpReturnValue %param\n"
		"OpFunctionEnd\n");

	const bool singleInputBuffer[] = { true, false };
	for (int inputBufferTypeIndex = 0 ; inputBufferTypeIndex < 2; ++inputBufferTypeIndex)
	{
		const bool isSingleInputBuffer			= singleInputBuffer[inputBufferTypeIndex];
		const string cap						= isSingleInputBuffer	? "OpCapability VariablePointersStorageBuffer\n" : "OpCapability VariablePointers\n";
		const vector<float>& expectedOutput		= isSingleInputBuffer	? AmuxAOutputFloats		 : AmuxBOutputFloats;
		const vector<float>& expectedIncrOutput = isSingleInputBuffer	? incrAmuxAOutputFloats	 : incrAmuxBOutputFloats;
		const string bufferType					= isSingleInputBuffer	? "single_buffer"		 : "two_buffers";
		const string muxInput1					= isSingleInputBuffer	? " %loc_a_2i "			 : " %loc_a_i ";
		const string muxInput2					= isSingleInputBuffer	? " %loc_a_2i_plus_1 "	 : " %loc_b_i ";

		// Set the proper extension features required for the test
		if (isSingleInputBuffer)
			requiredFeatures.extVariablePointers	= EXTVARIABLEPOINTERSFEATURES_VARIABLE_POINTERS_STORAGEBUFFER;
		else
			requiredFeatures.extVariablePointers	= EXTVARIABLEPOINTERSFEATURES_VARIABLE_POINTERS;

		// All of the following tests write their results into an output SSBO, therefore they require the following features.
		requiredFeatures.coreFeatures.vertexPipelineStoresAndAtomics = DE_TRUE;
		requiredFeatures.coreFeatures.fragmentStoresAndAtomics		 = DE_TRUE;

		{ // Variable Pointer Reads (using OpSelect)
			GraphicsResources				resources;
			map<string, string>				specs;
			string name						= "reads_opselect_" + bufferType;
			specs["ExtraTypes"]				= "";
			specs["ExtraGlobalScopeVars"]	= "";
			specs["ExtraFunctionScopeVars"]	= "";
			specs["ExtraFunctions"]			= "";
			specs["VarPtrName"]				= "%mux_output_var_ptr";
			specs["ResultStrategy"]			= "%mux_output_var_ptr	= OpSelect %sb_f32 %is_neg" + muxInput1 + muxInput2 + "\n";

			fragments["capability"]			= cap;
			fragments["decoration"]			= decoration.specialize(specs);
			fragments["pre_main"]			= preMain.specialize(specs);
			fragments["testfun"]			= testFunction.specialize(specs);

			resources.inputs.push_back(std::make_pair(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, BufferSp(new Float32Buffer(inputAFloats))));
			resources.inputs.push_back(std::make_pair(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, BufferSp(new Float32Buffer(inputBFloats))));
			resources.inputs.push_back(std::make_pair(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, BufferSp(new Float32Buffer(inputSFloats))));
			resources.outputs.push_back(std::make_pair(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, BufferSp(new Float32Buffer(expectedOutput))));
			createTestsForAllStages(name.c_str(), defaultColors, defaultColors, fragments, resources, extensions, testGroup, requiredFeatures);
		}
		{ // Variable Pointer Reads (using OpFunctionCall)
			GraphicsResources				resources;
			map<string, string>				specs;
			string name						= "reads_opfunctioncall_" + bufferType;
			specs["ExtraTypes"]				= "";
			specs["ExtraGlobalScopeVars"]	= "";
			specs["ExtraFunctionScopeVars"]	= "";
			specs["ExtraFunctions"]			= selectorFunction;
			specs["VarPtrName"]				= "%mux_output_var_ptr";
			specs["ResultStrategy"]			= "%mux_output_var_ptr = OpFunctionCall %sb_f32 %choose_input_func %is_neg" + muxInput1 + muxInput2 + "\n";

			fragments["capability"]			= cap;
			fragments["decoration"]			= decoration.specialize(specs);
			fragments["pre_main"]			= preMain.specialize(specs);
			fragments["testfun"]			= testFunction.specialize(specs);

			resources.inputs.push_back(std::make_pair(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, BufferSp(new Float32Buffer(inputAFloats))));
			resources.inputs.push_back(std::make_pair(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, BufferSp(new Float32Buffer(inputBFloats))));
			resources.inputs.push_back(std::make_pair(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, BufferSp(new Float32Buffer(inputSFloats))));
			resources.outputs.push_back(std::make_pair(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, BufferSp(new Float32Buffer(expectedOutput))));
			createTestsForAllStages(name.c_str(), defaultColors, defaultColors, fragments, resources, extensions, testGroup, requiredFeatures);
		}
		{ // Variable Pointer Reads (using OpPhi)
			GraphicsResources				resources;
			map<string, string>				specs;
			string name						= "reads_opphi_" + bufferType;
			specs["ExtraTypes"]				= "";
			specs["ExtraGlobalScopeVars"]	= "";
			specs["ExtraFunctionScopeVars"]	= "";
			specs["ExtraFunctions"]			= "";
			specs["VarPtrName"]				= "%mux_output_var_ptr";
			specs["ResultStrategy"]			=
				"							  OpSelectionMerge %end_label None\n"
				"							  OpBranchConditional %is_neg %take_mux_input_1 %take_mux_input_2\n"
				"%take_mux_input_1			= OpLabel\n"
				"							  OpBranch %end_label\n"
				"%take_mux_input_2			= OpLabel\n"
				"						      OpBranch %end_label\n"
				"%end_label					= OpLabel\n"
				"%mux_output_var_ptr		= OpPhi %sb_f32" + muxInput1 + "%take_mux_input_1" + muxInput2 + "%take_mux_input_2\n";

			fragments["capability"]			= cap;
			fragments["decoration"]			= decoration.specialize(specs);
			fragments["pre_main"]			= preMain.specialize(specs);
			fragments["testfun"]			= testFunction.specialize(specs);

			resources.inputs.push_back(std::make_pair(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, BufferSp(new Float32Buffer(inputAFloats))));
			resources.inputs.push_back(std::make_pair(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, BufferSp(new Float32Buffer(inputBFloats))));
			resources.inputs.push_back(std::make_pair(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, BufferSp(new Float32Buffer(inputSFloats))));
			resources.outputs.push_back(std::make_pair(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, BufferSp(new Float32Buffer(expectedOutput))));
			createTestsForAllStages(name.c_str(), defaultColors, defaultColors, fragments, resources, extensions, testGroup, requiredFeatures);
		}
		{ // Variable Pointer Reads (using OpCopyObject)
			GraphicsResources				resources;
			map<string, string>				specs;
			string name						= "reads_opcopyobject_" + bufferType;
			specs["ExtraTypes"]				= "";
			specs["ExtraGlobalScopeVars"]	= "";
			specs["ExtraFunctionScopeVars"]	= "";
			specs["ExtraFunctions"]			= "";
			specs["VarPtrName"]				= "%mux_output_var_ptr";
			specs["ResultStrategy"]			=
				"%mux_input_1_copy			= OpCopyObject %sb_f32" + muxInput1 + "\n"
				"%mux_input_2_copy			= OpCopyObject %sb_f32" + muxInput2 + "\n"
				"%mux_output_var_ptr		= OpSelect %sb_f32 %is_neg %mux_input_1_copy %mux_input_2_copy\n";

			fragments["capability"]			= cap;
			fragments["decoration"]			= decoration.specialize(specs);
			fragments["pre_main"]			= preMain.specialize(specs);
			fragments["testfun"]			= testFunction.specialize(specs);

			resources.inputs.push_back(std::make_pair(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, BufferSp(new Float32Buffer(inputAFloats))));
			resources.inputs.push_back(std::make_pair(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, BufferSp(new Float32Buffer(inputBFloats))));
			resources.inputs.push_back(std::make_pair(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, BufferSp(new Float32Buffer(inputSFloats))));
			resources.outputs.push_back(std::make_pair(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, BufferSp(new Float32Buffer(expectedOutput))));
			createTestsForAllStages(name.c_str(), defaultColors, defaultColors, fragments, resources, extensions, testGroup, requiredFeatures);
		}
		{ // Test storing into Private variables.
			const char* storageClasses[]		= {"Private", "Function"};
			for (int classId = 0; classId < 2; ++classId)
			{
				GraphicsResources				resources;
				map<string, string>				specs;
				std::string storageClass		= storageClasses[classId];
				std::string name				= "stores_" + string(de::toLower(storageClass)) + "_" + bufferType;
				std::string extraVariable		= "%mux_output_copy	= OpVariable %sb_f32ptrptr " + storageClass + "\n";
				specs["ExtraTypes"]				= "%sb_f32ptrptr = OpTypePointer " + storageClass + " %sb_f32\n";
				specs["ExtraGlobalScopeVars"]	= (classId == 0) ? extraVariable : "";
				specs["ExtraFunctionScopeVars"]	= (classId == 1) ? extraVariable : "";
				specs["ExtraFunctions"]			= "";
				specs["VarPtrName"]				= "%mux_output_var_ptr";
				specs["ResultStrategy"]			=
					"%opselect_result			= OpSelect %sb_f32 %is_neg" + muxInput1 + muxInput2 + "\n"
					"							  OpStore %mux_output_copy %opselect_result\n"
					"%mux_output_var_ptr		= OpLoad %sb_f32 %mux_output_copy\n";

				fragments["capability"]			= cap;
				fragments["decoration"]			= decoration.specialize(specs);
				fragments["pre_main"]			= preMain.specialize(specs);
				fragments["testfun"]			= testFunction.specialize(specs);

				resources.inputs.push_back(std::make_pair(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, BufferSp(new Float32Buffer(inputAFloats))));
				resources.inputs.push_back(std::make_pair(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, BufferSp(new Float32Buffer(inputBFloats))));
				resources.inputs.push_back(std::make_pair(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, BufferSp(new Float32Buffer(inputSFloats))));
				resources.outputs.push_back(std::make_pair(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, BufferSp(new Float32Buffer(expectedOutput))));
				createTestsForAllStages(name.c_str(), defaultColors, defaultColors, fragments, resources, extensions, testGroup, requiredFeatures);
			}
		}
		{ // Variable Pointer Reads (using OpPtrAccessChain)
			GraphicsResources				resources;
			map<string, string>				specs;
			std::string name				= "reads_opptraccesschain_" + bufferType;
			std::string in_1				= isSingleInputBuffer ? " %a_2i_ptr "		 : " %a_i_ptr ";
			std::string in_2				= isSingleInputBuffer ? " %a_2i_plus_1_ptr " : " %b_i_ptr ";
			specs["ExtraTypes"]				= "";
			specs["ExtraGlobalScopeVars"]	= "";
			specs["ExtraFunctionScopeVars"]	= "";
			specs["ExtraFunctions"]			= "";
			specs["VarPtrName"]				= "%mux_output_var_ptr";
			specs["ResultStrategy"]			=
					"%a_ptr					= OpAccessChain %sb_f32 %indata_a %c_i32_0 %c_i32_0\n"
					"%b_ptr					= OpAccessChain %sb_f32 %indata_b %c_i32_0 %c_i32_0\n"
					"%s_ptr					= OpAccessChain %sb_f32 %indata_s %c_i32_0 %c_i32_0\n"
					"%out_ptr               = OpAccessChain %sb_f32 %outdata  %c_i32_0 %c_i32_0\n"
					"%a_i_ptr               = OpPtrAccessChain %sb_f32 %a_ptr %30\n"
					"%b_i_ptr               = OpPtrAccessChain %sb_f32 %b_ptr %30\n"
					"%s_i_ptr               = OpPtrAccessChain %sb_f32 %s_ptr %30\n"
					"%a_2i_ptr              = OpPtrAccessChain %sb_f32 %a_ptr %two_i\n"
					"%a_2i_plus_1_ptr       = OpPtrAccessChain %sb_f32 %a_ptr %two_i_plus_1\n"
					"%mux_output_var_ptr    = OpSelect %sb_f32 %is_neg " + in_1 + in_2 + "\n";

			fragments["decoration"]			= decoration.specialize(specs);
			fragments["pre_main"]			= preMain.specialize(specs);
			fragments["testfun"]			= testFunction.specialize(specs);
			fragments["capability"]			= cap;

			resources.inputs.push_back(std::make_pair(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, BufferSp(new Float32Buffer(inputAFloats))));
			resources.inputs.push_back(std::make_pair(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, BufferSp(new Float32Buffer(inputBFloats))));
			resources.inputs.push_back(std::make_pair(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, BufferSp(new Float32Buffer(inputSFloats))));
			resources.outputs.push_back(std::make_pair(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, BufferSp(new Float32Buffer(expectedOutput))));
			createTestsForAllStages(name.c_str(), defaultColors, defaultColors, fragments, resources, extensions, testGroup, requiredFeatures);
		}
		{   // Variable Pointer Writes
			GraphicsResources				resources;
			map<string, string>				specs;
			std::string	name				= "writes_" + bufferType;
			specs["ExtraTypes"]				= "";
			specs["ExtraGlobalScopeVars"]	= "";
			specs["ExtraFunctionScopeVars"]	= "";
			specs["ExtraFunctions"]			= "";
			specs["VarPtrName"]				= "%mux_output_var_ptr";
			specs["ResultStrategy"]			=
					   "%mux_output_var_ptr = OpSelect %sb_f32 %is_neg" + muxInput1 + muxInput2 + "\n" +
					   "               %val = OpLoad %f32 %mux_output_var_ptr\n"
					   "        %val_plus_1 = OpFAdd %f32 %val %c_f32_1\n"
					   "					  OpStore %mux_output_var_ptr %val_plus_1\n";
			fragments["capability"]			= cap;
			fragments["decoration"]			= decoration.specialize(specs);
			fragments["pre_main"]			= preMain.specialize(specs);
			fragments["testfun"]			= testFunction.specialize(specs);

			resources.inputs.push_back(std::make_pair(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, BufferSp(new Float32Buffer(inputAFloats))));
			resources.inputs.push_back(std::make_pair(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, BufferSp(new Float32Buffer(inputBFloats))));
			resources.inputs.push_back(std::make_pair(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, BufferSp(new Float32Buffer(inputSFloats))));
			resources.outputs.push_back(std::make_pair(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, BufferSp(new Float32Buffer(expectedIncrOutput))));
			createTestsForAllStages(name.c_str(), defaultColors, defaultColors, fragments, resources, extensions, testGroup, requiredFeatures);
		}
	}
}

} // anonymous

tcu::TestCaseGroup* createVariablePointersComputeGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group	(new tcu::TestCaseGroup(testCtx, "variable_pointers", "Compute tests for SPV_KHR_variable_pointers extension"));
	addTestGroup(group.get(), "compute", "Test the variable pointer extension using a compute shader", addComputeVariablePointersGroup);

	// \todo [2017-03-17 ehsann] A couple of things to do:
	// * Add more tests (similar to existing ones) using data types other than Float.
	return group.release();
}

tcu::TestCaseGroup* createVariablePointersGraphicsGroup (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group	(new tcu::TestCaseGroup(testCtx, "variable_pointers", "Graphics tests for SPV_KHR_variable_pointers extension"));
	addTestGroup(group.get(), "graphics", "Testing Variable Pointers in graphics pipeline", addGraphicsVariablePointersGroup);

	// \todo [2017-03-17 ehsann] A couple of things to do:
	// * Add more tests (similar to existing ones) using data types other than Float.
	return group.release();
}

} // SpirVAssembly
} // vkt
