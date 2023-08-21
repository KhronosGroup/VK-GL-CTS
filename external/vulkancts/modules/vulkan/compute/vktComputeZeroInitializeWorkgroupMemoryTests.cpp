/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2020 The Khronos Group Inc.
 * Copyright (c) 2020 Google LLC.
 * Copyright (c) 2023 LunarG, Inc.
 * Copyright (c) 2023 Nintendo
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
 * \brief VK_KHR_zero_initialize_workgroup_memory tests
 *//*--------------------------------------------------------------------*/

#include "vktComputeZeroInitializeWorkgroupMemoryTests.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"
#include "vktAmberTestCase.hpp"

#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkQueryUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkDefs.hpp"
#include "vkRef.hpp"

#include "tcuCommandLine.hpp"
#include "tcuTestLog.hpp"

#include "deRandom.hpp"
#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"

#include <algorithm>
#include <vector>

using namespace vk;

namespace vkt
{
namespace compute
{
namespace
{

tcu::TestStatus runCompute(Context& context, deUint32 bufferSize,
							deUint32 numWGX, deUint32 numWGY, deUint32 numWGZ,
							vk::ComputePipelineConstructionType m_computePipelineConstructionType,
							const std::vector<deUint32> specValues = {},
							deUint32 increment = 0)
{
	const DeviceInterface&	vk			= context.getDeviceInterface();
	const VkDevice			device		= context.getDevice();
	Allocator&				allocator	= context.getDefaultAllocator();
	tcu::TestLog&			log			= context.getTestContext().getLog();

	de::MovePtr<BufferWithMemory> buffer;
	VkDescriptorBufferInfo bufferDescriptor;

	VkDeviceSize size = bufferSize;
	buffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
		vk, device, allocator, makeBufferCreateInfo(size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
		MemoryRequirement::HostVisible));
	bufferDescriptor = makeDescriptorBufferInfo(**buffer, 0, size);

	deUint32* ptr = (deUint32*)buffer->getAllocation().getHostPtr();
	deMemset(ptr, increment ? 0 : 0xff, (size_t)size);

	DescriptorSetLayoutBuilder layoutBuilder;
	layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);

	Unique<VkDescriptorSetLayout> descriptorSetLayout(layoutBuilder.build(vk, device));
	Unique<VkDescriptorPool> descriptorPool(DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u)
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));
	Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

	std::vector<VkSpecializationMapEntry> entries(specValues.size());
	if (!specValues.empty())
	{
		for (deUint32 i = 0; i < specValues.size(); ++i)
		{
			entries[i] = {i, (deUint32)(sizeof(deUint32) * i), sizeof(deUint32)};
		}
	}
	const VkSpecializationInfo specInfo =
	{
		(deUint32)specValues.size(),
		entries.data(),
		specValues.size() * sizeof(deUint32),
		specValues.data(),
	};
	VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
	flushAlloc(vk, device, buffer->getAllocation());

	ComputePipelineWrapper			pipeline(vk, device, m_computePipelineConstructionType, context.getBinaryCollection().get("comp"));
	pipeline.setDescriptorSetLayout(descriptorSetLayout.get());
	pipeline.setSpecializationInfo(specInfo);
	pipeline.buildPipeline();

	const VkQueue queue = context.getUniversalQueue();
	Move<VkCommandPool> cmdPool = createCommandPool(vk, device,
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		context.getUniversalQueueFamilyIndex());
	Move<VkCommandBuffer> cmdBuffer = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	DescriptorSetUpdateBuilder setUpdateBuilder;
	setUpdateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0),
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptor);
	setUpdateBuilder.update(vk, device);

	beginCommandBuffer(vk, *cmdBuffer, 0);

	vk.cmdBindDescriptorSets(*cmdBuffer, bindPoint, pipeline.getPipelineLayout(), 0u, 1, &*descriptorSet, 0u, DE_NULL);
	pipeline.bind(*cmdBuffer);

	vk.cmdDispatch(*cmdBuffer, numWGX, numWGY, numWGZ);

	endCommandBuffer(vk, *cmdBuffer);

	submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

	invalidateAlloc(vk, device, buffer->getAllocation());

	for (deUint32 i = 0; i < (deUint32)size / sizeof(deUint32); ++i)
	{
		deUint32 expected = increment ? numWGX * numWGY * numWGZ : 0u;
		if (ptr[i] != expected)
		{
			log << tcu::TestLog::Message << "failure at index " << i << ": expected " << expected << ", got: " << ptr[i] << tcu::TestLog::EndMessage;
			return tcu::TestStatus::fail("compute failed");
		}
	}

	return tcu::TestStatus::pass("compute succeeded");
}

class MaxWorkgroupMemoryInstance : public vkt::TestInstance
{
public:
	MaxWorkgroupMemoryInstance	(Context& context, deUint32 numWorkgroups, const vk::ComputePipelineConstructionType computePipelineConstructionType)
		: TestInstance						(context)
		, m_numWorkgroups					(numWorkgroups)
		, m_computePipelineConstructionType	(computePipelineConstructionType)
	{
	}
	tcu::TestStatus iterate		(void);

private:
	deUint32							m_numWorkgroups;
	vk::ComputePipelineConstructionType m_computePipelineConstructionType;
};

class MaxWorkgroupMemoryTest : public vkt::TestCase
{
public:
	MaxWorkgroupMemoryTest(tcu::TestContext& testCtx,
							const std::string& name,
							const std::string& description,
							deUint32 numWorkgroups,
							const vk::ComputePipelineConstructionType computePipelineConstructionType)
		: TestCase							(testCtx, name, description),
		m_numWorkgroups						(numWorkgroups)
		, m_computePipelineConstructionType	(computePipelineConstructionType)
	{
	}

	void initPrograms(SourceCollections& sourceCollections) const;
	TestInstance* createInstance(Context& context) const
	{
		return new MaxWorkgroupMemoryInstance(context, m_numWorkgroups, m_computePipelineConstructionType);
	}
	virtual void checkSupport(Context& context) const;

private:
	deUint32							m_numWorkgroups;
	vk::ComputePipelineConstructionType m_computePipelineConstructionType;
};

void MaxWorkgroupMemoryTest::checkSupport(Context& context) const
{
	context.requireDeviceFunctionality("VK_KHR_zero_initialize_workgroup_memory");
	checkShaderObjectRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), m_computePipelineConstructionType);
}

void MaxWorkgroupMemoryTest::initPrograms(SourceCollections& sourceCollections) const
{
	std::ostringstream src;
	src << "#version 450\n";
	src << "#extension GL_EXT_null_initializer : enable\n";
	src << "layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;\n";
	src << "layout(set = 0, binding = 0) buffer A { uint a[]; } a;\n";
	src << "layout(constant_id = 3) const uint num_elems = " << 16384 / 16 << ";\n";
	src << "layout(constant_id = 4) const uint num_wgs = 0;\n";
	src << "shared uvec4 wg_mem[num_elems] = {};\n";
	src << "void main() {\n";
	src << "  uint idx_z = gl_LocalInvocationID.z * gl_WorkGroupSize.x * gl_WorkGroupSize.y;\n";
	src << "  uint idx_y = gl_LocalInvocationID.y * gl_WorkGroupSize.x;\n";
	src << "  uint idx_x = gl_LocalInvocationID.x;\n";
	src << "  uint idx = idx_x + idx_y + idx_z;\n";
	src << "  uint wg_size = gl_WorkGroupSize.x * gl_WorkGroupSize.y * gl_WorkGroupSize.z;\n";
	src << "  for (uint i = 0; i < num_elems; ++i) {\n";
	src << "    for (uint j = 0; j < 4; ++j) {\n";
	src << "      uint shared_idx = 4*i + j;\n";
	src << "      uint wg_val = wg_mem[i][j];\n";
	src << "      if (idx == shared_idx) {\n";
	src << "        atomicAdd(a.a[idx], wg_val == 0 ? 1 : 0);\n";
	src << "      } else if (idx == 0 && shared_idx >= wg_size) {\n";
	src << "        atomicAdd(a.a[shared_idx], wg_val == 0 ? 1 : 0);\n";
	src << "      }\n";
	src << "    }\n";
	src << "  }\n";
	src << "}\n";

	sourceCollections.glslSources.add("comp") << glu::ComputeSource(src.str());
}

tcu::TestStatus MaxWorkgroupMemoryInstance::iterate (void)
{
	VkPhysicalDeviceProperties properties;
	m_context.getInstanceInterface().getPhysicalDeviceProperties(m_context.getPhysicalDevice(), &properties);
	const deUint32 maxMemSize = properties.limits.maxComputeSharedMemorySize;

	const deUint32 maxWG = std::min(247u, (properties.limits.maxComputeWorkGroupInvocations / 13) * 13);
	deUint32 wgx = (properties.limits.maxComputeWorkGroupSize[0] / 13) * 13;
	deUint32 wgy = 1;
	deUint32 wgz = 1;
	if (wgx < maxWG)
	{
		wgy = std::min(maxWG / wgx, (properties.limits.maxComputeWorkGroupSize[1] / 13) * 13);
	}
	if ((wgx * wgy) < maxWG)
	{
		wgz = std::min(maxWG / wgx / wgy, (properties.limits.maxComputeWorkGroupSize[2] / 13) * 13);
	}
	const deUint32 size = maxMemSize;
	const deUint32 numElems = maxMemSize / 16;

	return runCompute(m_context, size, m_numWorkgroups, 1, 1, m_computePipelineConstructionType, {wgx, wgy, wgz, numElems}, /*increment*/ 1);
}

void AddMaxWorkgroupMemoryTests(tcu::TestCaseGroup* group, vk::ComputePipelineConstructionType computePipelineConstructionType)
{
	std::vector<deUint32> workgroups = {1, 2, 4, 16, 64, 128};
	for (deUint32 i = 0; i < workgroups.size(); ++i) {
		deUint32 numWG = workgroups[i];
		group->addChild(new MaxWorkgroupMemoryTest(group->getTestContext(),
			de::toString(numWG), de::toString(numWG) + " workgroups", numWG, computePipelineConstructionType));
	}
}

struct TypeCaseDef
{
	std::string	typeName;
	deUint32	typeSize;
	deUint32	numElements;
	deUint32	numRows;
	deUint32	numVariables;
};

class TypeTestInstance : public vkt::TestInstance
{
public:
	TypeTestInstance(Context& context, const TypeCaseDef& caseDef, const vk::ComputePipelineConstructionType computePipelineConstructionType)
		: TestInstance						(context)
		, m_caseDef							(caseDef)
		, m_computePipelineConstructionType	(computePipelineConstructionType)
	{
	}
	tcu::TestStatus iterate (void);

private:
	TypeCaseDef m_caseDef;
	vk::ComputePipelineConstructionType m_computePipelineConstructionType;
};

class TypeTest : public vkt::TestCase
{
public:
	TypeTest(tcu::TestContext& testCtx,
			const std::string& name,
			const std::string& description,
			const TypeCaseDef& caseDef,
			const vk::ComputePipelineConstructionType computePipelineConstructionType)
		: TestCase							(testCtx, name, description)
		, m_caseDef							(caseDef)
		, m_computePipelineConstructionType	(computePipelineConstructionType)
	{
	}

	void initPrograms(SourceCollections& sourceCollections) const;
	TestInstance* createInstance(Context& context) const
	{
		return new TypeTestInstance(context, m_caseDef, m_computePipelineConstructionType);
	}
	virtual void checkSupport(Context& context) const;

private:
	TypeCaseDef							m_caseDef;
	vk::ComputePipelineConstructionType m_computePipelineConstructionType;
};

void TypeTest::checkSupport(Context& context) const
{
	context.requireDeviceFunctionality("VK_KHR_zero_initialize_workgroup_memory");
	checkShaderObjectRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), m_computePipelineConstructionType);

	VkPhysicalDeviceShaderFloat16Int8Features f16_i8_features;
	deMemset(&f16_i8_features, 0, sizeof(f16_i8_features));
	f16_i8_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES;
	f16_i8_features.pNext = DE_NULL;

	VkPhysicalDeviceFeatures2 features2;
	deMemset(&features2, 0, sizeof(features2));
	features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	features2.pNext = &f16_i8_features;
	context.getInstanceInterface().getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &features2);

	if (m_caseDef.typeName == "float16_t" ||
		m_caseDef.typeName == "f16vec2" ||
		m_caseDef.typeName == "f16vec3" ||
		m_caseDef.typeName == "f16vec4" ||
		m_caseDef.typeName == "f16mat2x2" ||
		m_caseDef.typeName == "f16mat2x3" ||
		m_caseDef.typeName == "f16mat2x4" ||
		m_caseDef.typeName == "f16mat3x2" ||
		m_caseDef.typeName == "f16mat3x3" ||
		m_caseDef.typeName == "f16mat3x4" ||
		m_caseDef.typeName == "f16mat4x2" ||
		m_caseDef.typeName == "f16mat4x3" ||
		m_caseDef.typeName == "f16mat4x4")
	{
		if (f16_i8_features.shaderFloat16 != VK_TRUE)
			TCU_THROW(NotSupportedError, "shaderFloat16 not supported");
	}

	if (m_caseDef.typeName == "float64_t" ||
		m_caseDef.typeName == "f64vec2" ||
		m_caseDef.typeName == "f64vec3" ||
		m_caseDef.typeName == "f64vec4"||
		m_caseDef.typeName == "f64mat2x2" ||
		m_caseDef.typeName == "f64mat2x3" ||
		m_caseDef.typeName == "f64mat2x4" ||
		m_caseDef.typeName == "f64mat3x2" ||
		m_caseDef.typeName == "f64mat3x3" ||
		m_caseDef.typeName == "f64mat3x4" ||
		m_caseDef.typeName == "f64mat4x2" ||
		m_caseDef.typeName == "f64mat4x3" ||
		m_caseDef.typeName == "f64mat4x4")
	{
		if (features2.features.shaderFloat64 != VK_TRUE)
			TCU_THROW(NotSupportedError, "shaderFloat64 not supported");
	}

	if (m_caseDef.typeName == "int8_t" ||
		m_caseDef.typeName == "i8vec2" ||
		m_caseDef.typeName == "i8vec3" ||
		m_caseDef.typeName == "i8vec4" ||
		m_caseDef.typeName == "uint8_t" ||
		m_caseDef.typeName == "u8vec2" ||
		m_caseDef.typeName == "u8vec3" ||
		m_caseDef.typeName == "u8vec4")
	{
		if (f16_i8_features.shaderInt8 != VK_TRUE)
			TCU_THROW(NotSupportedError, "shaderInt8 not supported");
	}

	if (m_caseDef.typeName == "int16_t" ||
		m_caseDef.typeName == "i16vec2" ||
		m_caseDef.typeName == "i16vec3" ||
		m_caseDef.typeName == "i16vec4" ||
		m_caseDef.typeName == "uint16_t" ||
		m_caseDef.typeName == "u16vec2" ||
		m_caseDef.typeName == "u16vec3" ||
		m_caseDef.typeName == "u16vec4")
	{
		if (features2.features.shaderInt16 != VK_TRUE)
			TCU_THROW(NotSupportedError, "shaderInt16 not supported");
	}

	if (m_caseDef.typeName == "int64_t" ||
		m_caseDef.typeName == "i64vec2" ||
		m_caseDef.typeName == "i64vec3" ||
		m_caseDef.typeName == "i64vec4" ||
		m_caseDef.typeName == "uint64_t" ||
		m_caseDef.typeName == "u64vec2" ||
		m_caseDef.typeName == "u64vec3" ||
		m_caseDef.typeName == "u64vec4")
	{
		if (features2.features.shaderInt64 != VK_TRUE)
			TCU_THROW(NotSupportedError, "shaderInt64 not supported");
	}
}

void TypeTest::initPrograms(SourceCollections& sourceCollections) const
{
	std::ostringstream src;
	src << "#version 450\n";
	src << "#extension GL_EXT_null_initializer : enable\n";
	src << "#extension GL_EXT_shader_explicit_arithmetic_types : enable\n";
	src << "layout(local_size_x = " << m_caseDef.numElements * m_caseDef.numRows << ", local_size_y = 1, local_size_z = 1) in;\n";
	src << "layout(set = 0, binding = 0) buffer A  { uint a[]; } a;\n";
	for (deUint32 i = 0; i < m_caseDef.numVariables; ++i) {
		src << "shared " << m_caseDef.typeName << " wg_mem" << i << " = {};\n";
	}
	src << "void main() {\n";
	if (m_caseDef.numRows > 1)
	{
		src << "  uint row = gl_LocalInvocationID.x % " << m_caseDef.numRows << ";\n";
		src << "  uint col = gl_LocalInvocationID.x / " << m_caseDef.numRows << ";\n";
	}
	std::string conv = m_caseDef.typeSize > 4 ? "int64_t" : "int";
	for (deUint32 v = 0; v < m_caseDef.numVariables; ++v)
	{
		if (m_caseDef.numElements == 1)
		{
			// Scalars.
			src << "  a.a[" << v << "] = (" << conv << "(wg_mem" << v << ") ==  0) ? 0 : 1;\n";
		}
		else if (m_caseDef.numRows == 1)
		{
			// Vectors.
			src << "  a.a[" << v * m_caseDef.numRows * m_caseDef.numElements << " + gl_LocalInvocationID.x] = (" << conv << "(wg_mem" << v << "[gl_LocalInvocationID.x]) ==  0) ? 0 : 1;\n";
		}
		else
		{
			// Matrices.
			src << "  a.a[" << v * m_caseDef.numRows * m_caseDef.numElements << " + gl_LocalInvocationID.x] = (" << conv << "(wg_mem" << v << "[row][col]) ==  0) ? 0 : 1;\n";
		}
	}
	src << "}\n";

	sourceCollections.glslSources.add("comp") << glu::ComputeSource(src.str());
}

tcu::TestStatus TypeTestInstance::iterate (void)
{
	const deUint32 varBytes = m_caseDef.numElements * m_caseDef.numRows * (deUint32)sizeof(deUint32);
	return runCompute(m_context, varBytes * m_caseDef.numVariables, 1, 1, 1, m_computePipelineConstructionType);
}

void AddTypeTests(tcu::TestCaseGroup* group, vk::ComputePipelineConstructionType computePipelineConstructionType)
{
	deRandom rnd;
	deRandom_init(&rnd, 0);
	std::vector<TypeCaseDef> cases =
	{
		{"bool",		1,	1,	1,	0},
		{"bvec2",		1,	2,	1,	0},
		{"bvec3",		1,	3,	1,	0},
		{"bvec4",		1,	4,	1,	0},
		{"uint32_t",	4,	1,	1,	0},
		{"uvec2",		4,	2,	1,	0},
		{"uvec3",		4,	3,	1,	0},
		{"uvec4",		4,	4,	1,	0},
		{"int32_t",		4,	1,	1,	0},
		{"ivec2",		4,	2,	1,	0},
		{"ivec3",		4,	3,	1,	0},
		{"ivec4",		4,	4,	1,	0},
		{"uint8_t",		1,	1,	1,	0},
		{"u8vec2",		1,	2,	1,	0},
		{"u8vec3",		1,	3,	1,	0},
		{"u8vec4",		1,	4,	1,	0},
		{"int8_t",		1,	1,	1,	0},
		{"i8vec2",		1,	2,	1,	0},
		{"i8vec3",		1,	3,	1,	0},
		{"i8vec4",		1,	4,	1,	0},
		{"uint16_t",	2,	1,	1,	0},
		{"u16vec2",		2,	2,	1,	0},
		{"u16vec3",		2,	3,	1,	0},
		{"u16vec4",		2,	4,	1,	0},
		{"int16_t",		2,	1,	1,	0},
		{"i16vec2",		2,	2,	1,	0},
		{"i16vec3",		2,	3,	1,	0},
		{"i16vec4",		2,	4,	1,	0},
		{"uint64_t",	8,	1,	1,	0},
		{"u64vec2",		8,	2,	1,	0},
		{"u64vec3",		8,	3,	1,	0},
		{"u64vec4",		8,	4,	1,	0},
		{"int64_t",		8,	1,	1,	0},
		{"i64vec2",		8,	2,	1,	0},
		{"i64vec3",		8,	3,	1,	0},
		{"i64vec4",		8,	4,	1,	0},
		{"float32_t",	4,	1,	1,	0},
		{"f32vec2",		4,	2,	1,	0},
		{"f32vec3",		4,	3,	1,	0},
		{"f32vec4",		4,	4,	1,	0},
		{"f32mat2x2",	4,	2,	2,	0},
		{"f32mat2x3",	4,	3,	2,	0},
		{"f32mat2x4",	4,	4,	2,	0},
		{"f32mat3x2",	4,	2,	3,	0},
		{"f32mat3x3",	4,	3,	3,	0},
		{"f32mat3x4",	4,	4,	3,	0},
		{"f32mat4x2",	4,	2,	4,	0},
		{"f32mat4x3",	4,	3,	4,	0},
		{"f32mat4x4",	4,	4,	4,	0},
		{"float16_t",	2,	1,	1,	0},
		{"f16vec2",		2,	2,	1,	0},
		{"f16vec3",		2,	3,	1,	0},
		{"f16vec4",		2,	4,	1,	0},
		{"f16mat2x2",	2,	2,	2,	0},
		{"f16mat2x3",	2,	3,	2,	0},
		{"f16mat2x4",	2,	4,	2,	0},
		{"f16mat3x2",	2,	2,	3,	0},
		{"f16mat3x3",	2,	3,	3,	0},
		{"f16mat3x4",	2,	4,	3,	0},
		{"f16mat4x2",	2,	2,	4,	0},
		{"f16mat4x3",	2,	3,	4,	0},
		{"f16mat4x4",	2,	4,	4,	0},
		{"float64_t",	8,	1,	1,	0},
		{"f64vec2",		8,	2,	1,	0},
		{"f64vec3",		8,	3,	1,	0},
		{"f64vec4",		8,	4,	1,	0},
		{"f64mat2x2",	8,	2,	2,	0},
		{"f64mat2x3",	8,	3,	2,	0},
		{"f64mat2x4",	8,	4,	2,	0},
		{"f64mat3x2",	8,	2,	3,	0},
		{"f64mat3x3",	8,	3,	3,	0},
		{"f64mat3x4",	8,	4,	3,	0},
		{"f64mat4x2",	8,	2,	4,	0},
		{"f64mat4x3",	8,	3,	4,	0},
		{"f64mat4x4",	8,	4,	4,	0},
	};

	for (deUint32 i = 0; i < cases.size(); ++i)
	{
		cases[i].numVariables = (deRandom_getUint32(&rnd) % 16) + 1;
		group->addChild(
			new TypeTest(group->getTestContext(), cases[i].typeName.c_str(), cases[i].typeName.c_str(), cases[i], computePipelineConstructionType));
	}
}

struct CompositeCaseDef
{
	deUint32				index;
	std::string				typeDefinition;
	std::string				assignment;
	deUint32				elements;
	std::vector<deUint32>	specValues;

	CompositeCaseDef (uint32_t index_, const std::string& typeDefinition_, const std::string& assignment_, uint32_t elements_, const std::vector<uint32_t>& specValues_)
		: index				(index_)
		, typeDefinition	(typeDefinition_)
		, assignment		(assignment_)
		, elements			(elements_)
		, specValues		(specValues_)
		{}
};

class CompositeTestInstance : public vkt::TestInstance
{
public:
	CompositeTestInstance (Context& context, const CompositeCaseDef& caseDef, const vk::ComputePipelineConstructionType computePipelineConstructionType)
		: TestInstance						(context)
		, m_caseDef							(caseDef)
		, m_computePipelineConstructionType	(computePipelineConstructionType)
	{
	}
	tcu::TestStatus iterate (void);
private:
	CompositeCaseDef					m_caseDef;
	vk::ComputePipelineConstructionType m_computePipelineConstructionType;
};

class CompositeTest : public vkt::TestCase
{
public:
	CompositeTest(tcu::TestContext& testCtx,
				  const std::string& name,
				  const std::string& description,
				  const CompositeCaseDef& caseDef,
				  const vk::ComputePipelineConstructionType computePipelineConstructionType)
		: TestCase							(testCtx, name, description)
		, m_caseDef							(caseDef)
		, m_computePipelineConstructionType	(computePipelineConstructionType)
	{
	}

	void initPrograms(SourceCollections& sourceCollections) const;
	TestInstance* createInstance(Context& context) const
	{
		return new CompositeTestInstance(context, m_caseDef, m_computePipelineConstructionType);
	}
	virtual void checkSupport(Context& context) const;
private:
	CompositeCaseDef					m_caseDef;
	vk::ComputePipelineConstructionType m_computePipelineConstructionType;
};

void CompositeTest::checkSupport(Context& context) const
{
	context.requireDeviceFunctionality("VK_KHR_zero_initialize_workgroup_memory");
	checkShaderObjectRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), m_computePipelineConstructionType);

	VkPhysicalDeviceShaderFloat16Int8Features f16_i8_features;
	deMemset(&f16_i8_features, 0, sizeof(f16_i8_features));
	f16_i8_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES;
	f16_i8_features.pNext = DE_NULL;

	VkPhysicalDeviceFeatures2 features2;
	deMemset(&features2, 0, sizeof(features2));
	features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	features2.pNext = &f16_i8_features;
	context.getInstanceInterface().getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &features2);

	bool needsFloat16	= (m_caseDef.index & 0x1) != 0;
	bool needsFloat64	= (m_caseDef.index & 0x2) != 0;
	bool needsInt8		= (m_caseDef.index & 0x4) != 0;
	bool needsInt16		= (m_caseDef.index & 0x8) != 0;
	bool needsInt64		= (m_caseDef.index & 0x10) != 0;

	if (needsFloat16 && f16_i8_features.shaderFloat16 != VK_TRUE)
		TCU_THROW(NotSupportedError, "shaderFloat16 not supported");
	if (needsFloat64 && features2.features.shaderFloat64 != VK_TRUE)
		TCU_THROW(NotSupportedError, "shaderFloat64 not supported");
	if (needsInt8 && f16_i8_features.shaderInt8 != VK_TRUE)
		TCU_THROW(NotSupportedError, "shaderInt8 not supported");
	if (needsInt16 && features2.features.shaderInt16 != VK_TRUE)
		TCU_THROW(NotSupportedError, "shaderInt16 not supported");
	if (needsInt64 && features2.features.shaderInt64 != VK_TRUE)
		TCU_THROW(NotSupportedError, "shaderInt64 not supported");
}

void CompositeTest::initPrograms(SourceCollections& sourceCollections) const
{
	std::ostringstream src;
	src << "#version 450\n";
	src << "#extension GL_EXT_null_initializer : enable\n";
	src << "#extension GL_EXT_shader_explicit_arithmetic_types : enable\n";
	src << "\n";
	for (deUint32 i = 0; i < m_caseDef.specValues.size(); ++i) {
		src << "layout(constant_id = " << i << ") const uint specId" << i << " = 1;\n";
	}
	src << "\n";
	src << "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n";
	src << "layout(set = 0, binding = 0) buffer A { uint a[]; } a;\n";
	src << "\n";
	src << m_caseDef.typeDefinition;
	src << "\n";
	src << "void main() {\n";
	src << m_caseDef.assignment;
	src << "}\n";

	sourceCollections.glslSources.add("comp") << glu::ComputeSource(src.str());
}

tcu::TestStatus CompositeTestInstance::iterate (void)
{
	const deUint32 bufferSize = (deUint32)sizeof(deUint32) * m_caseDef.elements;
	return runCompute(m_context, bufferSize, 1, 1, 1, m_computePipelineConstructionType, m_caseDef.specValues);
}

void AddCompositeTests(tcu::TestCaseGroup* group, vk::ComputePipelineConstructionType computePipelineConstructionType)
{
	const std::vector<CompositeCaseDef> cases
	{
		{0,
		"shared uint wg_mem[specId0] = {};\n",

		"for (uint i = 0; i < specId0; ++i) {\n"
		"  a.a[i] = wg_mem[i];\n"
		"}\n",
		16,
		{16},
		},

		{0,
		"shared float wg_mem[specId0][specId1] = {};\n",

		"for (uint i = 0; i < specId0; ++i) {\n"
		"  for (uint j = 0; j < specId1; ++j) {\n"
		"    uint idx = i * specId1 + j;\n"
		"    a.a[idx] = wg_mem[i][j] == 0.0f ? 0 : 1;\n"
		"  }\n"
		"}\n",
		32,
		{4, 8},
		},

		{0,
		"struct Sa {\n"
		"  uint a;\n"
		"  uvec2 b;\n"
		"  uvec3 c;\n"
		"  uvec4 d;\n"
		"  float e;\n"
		"  vec2 f;\n"
		"  vec3 g;\n"
		"  vec4 h;\n"
		"  bool i;\n"
		"  bvec2 j;\n"
		"  bvec3 k;\n"
		"  bvec4 l;\n"
		"};\n"
		"shared Sa wg_mem = {};\n",

		"uint i = 0;\n"
		"a.a[i++] = wg_mem.a;\n"
		"a.a[i++] = wg_mem.b.x;\n"
		"a.a[i++] = wg_mem.b.y;\n"
		"a.a[i++] = wg_mem.c.x;\n"
		"a.a[i++] = wg_mem.c.y;\n"
		"a.a[i++] = wg_mem.c.z;\n"
		"a.a[i++] = wg_mem.d.x;\n"
		"a.a[i++] = wg_mem.d.y;\n"
		"a.a[i++] = wg_mem.d.z;\n"
		"a.a[i++] = wg_mem.d.w;\n"
		"a.a[i++] = wg_mem.e == 0.0f ? 0 : 1;\n"
		"a.a[i++] = wg_mem.f.x == 0.0f ? 0 : 1;\n"
		"a.a[i++] = wg_mem.f.y == 0.0f ? 0 : 1;\n"
		"a.a[i++] = wg_mem.g.x == 0.0f ? 0 : 1;\n"
		"a.a[i++] = wg_mem.g.y == 0.0f ? 0 : 1;\n"
		"a.a[i++] = wg_mem.g.z == 0.0f ? 0 : 1;\n"
		"a.a[i++] = wg_mem.h.x == 0.0f ? 0 : 1;\n"
		"a.a[i++] = wg_mem.h.y == 0.0f ? 0 : 1;\n"
		"a.a[i++] = wg_mem.h.z == 0.0f ? 0 : 1;\n"
		"a.a[i++] = wg_mem.h.w == 0.0f ? 0 : 1;\n"
		"a.a[i++] = wg_mem.i ? 1 : 0;\n"
		"a.a[i++] = wg_mem.j.x ? 1 : 0;\n"
		"a.a[i++] = wg_mem.j.y ? 1 : 0;\n"
		"a.a[i++] = wg_mem.k.x ? 1 : 0;\n"
		"a.a[i++] = wg_mem.k.y ? 1 : 0;\n"
		"a.a[i++] = wg_mem.k.z ? 1 : 0;\n"
		"a.a[i++] = wg_mem.l.x ? 1 : 0;\n"
		"a.a[i++] = wg_mem.l.y ? 1 : 0;\n"
		"a.a[i++] = wg_mem.l.z ? 1 : 0;\n"
		"a.a[i++] = wg_mem.l.w ? 1 : 0;\n",
		30,
		{},
		},

		{0,
		"struct Sa {\n"
		"  uint a;\n"
		"};\n"
		"struct Sb {\n"
		"  uvec2 a;\n"
		"};\n"
		"struct Sc {\n"
		"  Sa a[specId0];\n"
		"  Sb b[specId1];\n"
		"};\n"
		"shared Sc wg_mem[specId2] = {};\n",

		"uint idx = 0;\n"
		"for (uint i = 0; i < specId2; ++i) {\n"
		"  for (uint j = 0; j < specId0; ++j) {\n"
		"    a.a[idx++] = wg_mem[i].a[j].a;\n"
		"  }\n"
		"  for (uint j = 0; j < specId1; ++j) {\n"
		"    a.a[idx++] = wg_mem[i].b[j].a.x;\n"
		"    a.a[idx++] = wg_mem[i].b[j].a.y;\n"
		"  }\n"
		"}\n",
		32,
		{2,3,4},
		},

		{1,
		"struct Sa {\n"
		"  f16vec2 a;\n"
		"  float16_t b[specId0];\n"
		"};\n"
		"shared Sa wg_mem = {};\n",

		"uint idx = 0;\n"
		"a.a[idx++] = floatBitsToUint(wg_mem.a.x) == 0 ? 0 : 1;\n"
		"a.a[idx++] = floatBitsToUint(wg_mem.a.y) == 0 ? 0 : 1;\n"
		"for (uint i = 0; i < specId0; ++i) {\n"
		"  a.a[idx++] = floatBitsToUint(wg_mem.b[i]) == 0 ? 0 : 1;\n"
		"}\n",
		18,
		{16},
		},

		{2,
		"struct Sa {\n"
		"  f64vec2 a;\n"
		"  float64_t b[specId0];\n"
		"};\n"
		"shared Sa wg_mem = {};\n",

		"uint idx = 0;\n"
		"a.a[idx++] = wg_mem.a.x == 0.0 ? 0 : 1;\n"
		"a.a[idx++] = wg_mem.a.y == 0.0 ? 0 : 1;\n"
		"for (uint i = 0; i < specId0; ++i) {\n"
		"  a.a[idx++] = wg_mem.b[i] == 0.0 ? 0 : 1;\n"
		"}\n",
		7,
		{5},
		},

		{4,
		"struct Sa {\n"
		"  i8vec2 a;\n"
		"  int8_t b[specId0];\n"
		"};\n"
		"shared Sa wg_mem = {};\n",

		"uint idx = 0;\n"
		"a.a[idx++] = wg_mem.a.x == 0 ? 0 : 1;\n"
		"a.a[idx++] = wg_mem.a.y == 0 ? 0 : 1;\n"
		"for (uint i = 0; i < specId0; ++i) {\n"
		"  a.a[idx++] = wg_mem.b[i] == 0 ? 0 : 1;\n"
		"}\n",
		34,
		{32},
		},

		{8,
		"struct Sa {\n"
		"  i16vec2 a;\n"
		"  int16_t b[specId0];\n"
		"};\n"
		"shared Sa wg_mem = {};\n",

		"uint idx = 0;\n"
		"a.a[idx++] = wg_mem.a.x == 0 ? 0 : 1;\n"
		"a.a[idx++] = wg_mem.a.y == 0 ? 0 : 1;\n"
		"for (uint i = 0; i < specId0; ++i) {\n"
		"  a.a[idx++] = wg_mem.b[i] == 0 ? 0 : 1;\n"
		"}\n",
		122,
		{120},
		},

		{16,
		"struct Sa {\n"
		"  i64vec2 a;\n"
		"  int64_t b[specId0];\n"
		"};\n"
		"shared Sa wg_mem = {};\n",

		"uint idx = 0;\n"
		"a.a[idx++] = wg_mem.a.x == 0 ? 0 : 1;\n"
		"a.a[idx++] = wg_mem.a.y == 0 ? 0 : 1;\n"
		"for (uint i = 0; i < specId0; ++i) {\n"
		"  a.a[idx++] = wg_mem.b[i] == 0 ? 0 : 1;\n"
		"}\n",
		63,
		{61},
		},

		{0x1f,
		"struct Sa {\n"
		"  float16_t a;\n"
		"  float b;\n"
		"  int8_t c;\n"
		"  int16_t d;\n"
		"  int e;\n"
		"  int64_t f;\n"
		"  float64_t g;\n"
		"};\n"
		"shared Sa wg_mem = {};\n",

		"uint idx = 0;\n"
		"a.a[idx++] = floatBitsToUint(wg_mem.a) == 0 ? 0 : 1;\n"
		"a.a[idx++] = floatBitsToUint(wg_mem.b) == 0 ? 0 : 1;\n"
		"a.a[idx++] = uint(wg_mem.c);\n"
		"a.a[idx++] = uint(wg_mem.d);\n"
		"a.a[idx++] = uint(wg_mem.e);\n"
		"a.a[idx++] = uint(wg_mem.f);\n"
		"a.a[idx++] = wg_mem.g == 0.0 ? 0 : 1;\n",
		7,
		{},
		},

		{0,
		"struct Sa {\n"
		"  uint a;\n"
		"};\n"
		"struct Sb {\n"
		"  Sa a[specId0];\n"
		"  uint b;\n"
		"};\n"
		"struct Sc {\n"
		"  Sb b[specId1];\n"
		"  uint c;\n"
		"};\n"
		"struct Sd {\n"
		"  Sc c[specId2];\n"
		"  uint d;\n"
		"};\n"
		"struct Se {\n"
		"  Sd d[specId3];\n"
		"  uint e;\n"
		"};\n"
		"shared Se wg_mem[specId4] = {};\n",

		"uint idx = 0;\n"
		"for (uint i1 = 0; i1 < specId4; ++i1) {\n"
		"  a.a[idx++] = wg_mem[i1].e;\n"
		"  for (uint i2 = 0; i2 < specId3; ++i2) {\n"
		"    a.a[idx++] = wg_mem[i1].d[i2].d;\n"
		"    for (uint i3 = 0; i3 < specId2; ++i3) {\n"
		"      a.a[idx++] = wg_mem[i1].d[i2].c[i3].c;\n"
		"      for (uint i4 = 0; i4 < specId1; ++i4) {\n"
		"        a.a[idx++] = wg_mem[i1].d[i2].c[i3].b[i4].b;\n"
		"        for (uint i5 = 0; i5 < specId0; ++i5) {\n"
		"          a.a[idx++] = wg_mem[i1].d[i2].c[i3].b[i4].a[i5].a;\n"
		"        }\n"
		"      }\n"
		"    }\n"
		"  }\n"
		"}\n",
		872,
		{6,5,4,3,2},
		},
	};

	for (deUint32 i = 0; i < cases.size(); ++i)
	{
		group->addChild(
			new CompositeTest(group->getTestContext(), de::toString(i), de::toString(i), cases[i], computePipelineConstructionType));
	}
}

enum Dim {
	DimX,
	DimY,
	DimZ,
};

class MaxWorkgroupsInstance : public vkt::TestInstance
{
public:
	MaxWorkgroupsInstance(Context &context, Dim dim, const vk::ComputePipelineConstructionType computePipelineConstructionType)
		: TestInstance						(context)
		, m_dim								(dim)
		, m_computePipelineConstructionType	(computePipelineConstructionType)
	{
	}
	tcu::TestStatus iterate (void);
private:
	Dim									m_dim;
	vk::ComputePipelineConstructionType m_computePipelineConstructionType;
};

class MaxWorkgroupsTest : public vkt::TestCase
{
public:
	MaxWorkgroupsTest(tcu::TestContext& testCtx,
					  const std::string& name,
					  const std::string& description,
					  Dim dim,
					  const vk::ComputePipelineConstructionType computePipelineConstructionType)
		: TestCase							(testCtx, name, description)
		, m_dim								(dim)
		, m_computePipelineConstructionType	(computePipelineConstructionType)
	{
	}

	void initPrograms(SourceCollections& sourceCollections) const;
	TestInstance* createInstance(Context& context) const
	{
		return new MaxWorkgroupsInstance(context, m_dim, m_computePipelineConstructionType);
	}
	virtual void checkSupport(Context& context) const;
private:
	Dim									m_dim;
	vk::ComputePipelineConstructionType m_computePipelineConstructionType;
};

void MaxWorkgroupsTest::checkSupport(Context& context) const
{
	context.requireDeviceFunctionality("VK_KHR_zero_initialize_workgroup_memory");
	checkShaderObjectRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), m_computePipelineConstructionType);
}

void MaxWorkgroupsTest::initPrograms(SourceCollections& sourceCollections) const
{
	std::ostringstream src;
	src << "#version 450\n";
	src << "#extension GL_EXT_null_initializer : enable\n";
	src << "\n";
	src << "layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;\n";
	src << "layout(set = 0, binding = 0) buffer A { uint a[]; } a;\n";
	src << "shared uint wg_mem[2] = {};\n";
	std::string dim;
	switch (m_dim) {
		case DimX:
			dim = "x";
			break;
		case DimY:
			dim = "y";
			break;
		case DimZ:
			dim = "z";
			break;
	}
	src << "\n";
	src << "void main() {\n";
	src << "  uint idx_z = gl_LocalInvocationID.z * gl_WorkGroupSize.x * gl_WorkGroupSize.y;\n";
	src << "  uint idx_y = gl_LocalInvocationID.y * gl_WorkGroupSize.x;\n";
	src << "  uint idx_x = gl_LocalInvocationID.x;\n";
	src << "  uint idx = idx_x + idx_y + idx_z;\n";
	src << "  if (gl_LocalInvocationID.x == 0) {\n";
	src << "    wg_mem[0] = atomicExchange(wg_mem[1], wg_mem[0]);\n";
	src << "  }\n";
	src << "  barrier();\n";
	src << "  atomicAdd(a.a[idx], wg_mem[idx_x % 2] == 0 ? 1 : 0);\n";
	src << "}\n";

	sourceCollections.glslSources.add("comp") << glu::ComputeSource(src.str());
}

tcu::TestStatus MaxWorkgroupsInstance::iterate (void)
{
	VkPhysicalDeviceProperties properties;
	deMemset(&properties, 0, sizeof(properties));
	m_context.getInstanceInterface().getPhysicalDeviceProperties(m_context.getPhysicalDevice(), &properties);

	const deUint32 maxWG = std::min(2048u, properties.limits.maxComputeWorkGroupInvocations);
	deUint32 wgx = properties.limits.maxComputeWorkGroupSize[0];
	deUint32 wgy = 1;
	deUint32 wgz = 1;
	if (wgx < maxWG)
	{
		wgy = std::min(maxWG / wgx, properties.limits.maxComputeWorkGroupSize[1]);
	}
	if ((wgx * wgy) < maxWG)
	{
		wgz = std::min(maxWG / wgx / wgy, properties.limits.maxComputeWorkGroupSize[2]);
	}
	deUint32 size = (deUint32)sizeof(deUint32) * wgx * wgy * wgz;

	deUint32 num_wgx = m_dim == DimX ? 65535 : 1;
	deUint32 num_wgy = m_dim == DimY ? 65535 : 1;
	deUint32 num_wgz = m_dim == DimZ ? 65535 : 1;

	return runCompute(m_context, size, num_wgx, num_wgy, num_wgz, m_computePipelineConstructionType, {wgx, wgy, wgz}, /*increment*/ 1);
}

void AddMaxWorkgroupsTests(tcu::TestCaseGroup* group, vk::ComputePipelineConstructionType computePipelineConstructionType)
{
	group->addChild(new MaxWorkgroupsTest(group->getTestContext(), "x", "max x dim workgroups", DimX, computePipelineConstructionType));
	group->addChild(new MaxWorkgroupsTest(group->getTestContext(), "y", "max y dim workgroups", DimY, computePipelineConstructionType));
	group->addChild(new MaxWorkgroupsTest(group->getTestContext(), "z", "max z dim workgroups", DimZ, computePipelineConstructionType));
}

class SpecializeWorkgroupInstance : public vkt::TestInstance
{
public:
	SpecializeWorkgroupInstance (Context &context, deUint32 x, deUint32 y, deUint32 z, const vk::ComputePipelineConstructionType computePipelineConstructionType)
		: TestInstance						(context)
		, m_x								(x)
		, m_y								(y)
		, m_z								(z)
		, m_computePipelineConstructionType	(computePipelineConstructionType)
	{
	}
	tcu::TestStatus iterate (void);
private:
	deUint32							m_x;
	deUint32							m_y;
	deUint32							m_z;
	vk::ComputePipelineConstructionType m_computePipelineConstructionType;
};

class SpecializeWorkgroupTest : public vkt::TestCase
{
public:
	SpecializeWorkgroupTest(tcu::TestContext& testCtx,
					  const std::string& name,
					  const std::string& description,
					  deUint32 x, deUint32 y, deUint32 z,
					  const vk::ComputePipelineConstructionType computePipelineConstructionType)
		: TestCase							(testCtx, name, description)
		, m_x								(x)
		, m_y								(y)
		, m_z								(z)
		, m_computePipelineConstructionType	(computePipelineConstructionType)
	{
	}

	void initPrograms(SourceCollections& sourceCollections) const;
	TestInstance* createInstance(Context& context) const
	{
		return new SpecializeWorkgroupInstance(context, m_x, m_y, m_z, m_computePipelineConstructionType);
	}
	virtual void checkSupport(Context& context) const;
private:
	deUint32							m_x;
	deUint32							m_y;
	deUint32							m_z;
	vk::ComputePipelineConstructionType m_computePipelineConstructionType;
};

void SpecializeWorkgroupTest::checkSupport(Context& context) const
{
	context.requireDeviceFunctionality("VK_KHR_zero_initialize_workgroup_memory");
	checkShaderObjectRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), m_computePipelineConstructionType);

	VkPhysicalDeviceProperties properties;
	deMemset(&properties, 0, sizeof(properties));
	context.getInstanceInterface().getPhysicalDeviceProperties(context.getPhysicalDevice(), &properties);
	if (m_x * m_y * m_z > properties.limits.maxComputeWorkGroupInvocations)
		TCU_THROW(NotSupportedError, "Workgroup size exceeds limits");
}

void SpecializeWorkgroupTest::initPrograms(SourceCollections& sourceCollections) const
{
	std::ostringstream src;
	src << "#version 450\n";
	src << "#extension GL_EXT_null_initializer : enable\n";
	src << "\n";
	src << "layout(constant_id = 0) const uint WGX = 1;\n";
	src << "layout(constant_id = 1) const uint WGY = 1;\n";
	src << "layout(constant_id = 2) const uint WGZ = 1;\n";
	src << "layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;\n";
	src << "layout(set = 0, binding = 0) buffer A { uint a[]; } a;\n";
	src << "shared uint wg_mem[WGX][WGY][WGZ] = {};\n";
	src << "\n";
	src << "void main() {\n";
	src << "  a.a[gl_LocalInvocationID.z * gl_WorkGroupSize.x * gl_WorkGroupSize.y + gl_LocalInvocationID.y * gl_WorkGroupSize.x + gl_LocalInvocationID.x] = wg_mem[gl_LocalInvocationID.x][gl_LocalInvocationID.y][gl_LocalInvocationID.z];\n";
	src << "}\n";

	sourceCollections.glslSources.add("comp") << glu::ComputeSource(src.str());
}

tcu::TestStatus SpecializeWorkgroupInstance::iterate (void)
{
	const deUint32 size = (deUint32)sizeof(deUint32) * m_x * m_y * m_z;
	return runCompute(m_context, size, 1, 1, 1, m_computePipelineConstructionType, {m_x, m_y, m_z});
}

void AddSpecializeWorkgroupTests(tcu::TestCaseGroup* group, vk::ComputePipelineConstructionType computePipelineConstructionType)
{
	for (deUint32 z = 1; z <= 8; ++z)
	{
		for (deUint32 y = 1; y <= 8; ++y)
		{
			for (deUint32 x = 1; x <= 8; ++x)
			{
				group->addChild(new SpecializeWorkgroupTest(group->getTestContext(),
					de::toString(x) + "_" + de::toString(y) + "_" + de::toString(z),
					de::toString(x) + "_" + de::toString(y) + "_" + de::toString(z),
					x, y, z,
					computePipelineConstructionType));
			}
		}
	}
}

class RepeatedPipelineInstance : public vkt::TestInstance
{
public:
	RepeatedPipelineInstance (Context& context, deUint32 xSize, deUint32 repeat, deUint32 odd)
		: TestInstance						(context)
		, m_xSize							(xSize)
		, m_repeat							(repeat)
		, m_odd								(odd)
	{
	}
	tcu::TestStatus iterate (void);
private:
	deUint32							m_xSize;
	deUint32							m_repeat;
	deUint32							m_odd;
};

class RepeatedPipelineTest : public vkt::TestCase
{
public:
	RepeatedPipelineTest(tcu::TestContext& testCtx,
						const std::string& name,
						const std::string& description,
						deUint32 xSize, deUint32 repeat, deUint32 odd,
						const vk::ComputePipelineConstructionType computePipelineConstructionType)
		: TestCase							(testCtx, name, description)
		, m_xSize							(xSize)
		, m_repeat							(repeat)
		, m_odd								(odd)
		, m_computePipelineConstructionType	(computePipelineConstructionType)
	{
	}

	void initPrograms(SourceCollections& sourceCollections) const;
	TestInstance* createInstance(Context& context) const
	{
		return new RepeatedPipelineInstance(context, m_xSize, m_repeat, m_odd);
	}
	virtual void checkSupport(Context& context) const;
private:
	deUint32							m_xSize;
	deUint32							m_repeat;
	deUint32							m_odd;
	vk::ComputePipelineConstructionType m_computePipelineConstructionType;
};

void RepeatedPipelineTest::checkSupport(Context& context) const
{
	context.requireDeviceFunctionality("VK_KHR_zero_initialize_workgroup_memory");
	checkShaderObjectRequirements(context.getInstanceInterface(), context.getPhysicalDevice(), m_computePipelineConstructionType);
}

void RepeatedPipelineTest::initPrograms(SourceCollections& sourceCollections) const
{
	std::ostringstream src;
	src << "#version 450\n";
	src << "#extension GL_EXT_null_initializer : enable\n";
	src << "\n";
	src << "layout(constant_id = 0) const uint WGX = 1;\n";
	src << "layout(local_size_x_id = 0, local_size_y = 2, local_size_z = 1) in;\n";
	src << "\n";
	src << "layout(set = 0, binding = 0) buffer A { uint a[]; } a;\n";
	src << "layout(set = 0, binding = 1) buffer B { uint b[]; } b;\n";
	src << "\n";
	src << "shared uint wg_mem[WGX][2] = {};\n";
	src << "void main() {\n";
	src << "  if (gl_LocalInvocationID.y == " << m_odd << ") {\n";
	src << "    wg_mem[gl_LocalInvocationID.x][gl_LocalInvocationID.y] = b.b[gl_LocalInvocationID.y * WGX + gl_LocalInvocationID.x];\n";
	src << "  }\n";
	src << "  barrier();\n";
	src << "  a.a[gl_LocalInvocationID.y * WGX + gl_LocalInvocationID.x] = wg_mem[gl_LocalInvocationID.x][gl_LocalInvocationID.y];\n";
	src << "}\n";

	sourceCollections.glslSources.add("comp") << glu::ComputeSource(src.str());
}

tcu::TestStatus RepeatedPipelineInstance::iterate (void)
{
	Context& context					= m_context;
	const deUint32 bufferSize			= m_xSize * 2 * (deUint32)sizeof(deUint32);
	const deUint32 numBuffers			= 2;

	const DeviceInterface&	vk			= context.getDeviceInterface();
	const VkDevice			device		= context.getDevice();
	Allocator&				allocator	= context.getDefaultAllocator();
	tcu::TestLog&			log			= context.getTestContext().getLog();

	de::MovePtr<BufferWithMemory> buffers[numBuffers];
	VkDescriptorBufferInfo bufferDescriptors[numBuffers];

	VkDeviceSize size = bufferSize;
	for (deUint32 i = 0; i < numBuffers; ++i)
	{
		buffers[i] = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
			vk, device, allocator, makeBufferCreateInfo(size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
			MemoryRequirement::HostVisible | MemoryRequirement::Cached));
		bufferDescriptors[i] = makeDescriptorBufferInfo(**buffers[i], 0, size);
	}

	deUint32* ptrs[numBuffers];
	for (deUint32 i = 0; i < numBuffers; ++i)
	{
		ptrs[i] = (deUint32*)buffers[i]->getAllocation().getHostPtr();
	}
	for (deUint32 i = 0; i < bufferSize / sizeof(deUint32); ++i)
	{
		ptrs[1][i] = i;
	}
	deMemset(ptrs[0], 0xff, (size_t)size);

	DescriptorSetLayoutBuilder layoutBuilder;
	for (deUint32 i = 0; i < numBuffers; ++i)
	{
		layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	}

	Unique<VkDescriptorSetLayout> descriptorSetLayout(layoutBuilder.build(vk, device));
	Unique<VkDescriptorPool> descriptorPool(DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, numBuffers)
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));
	Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

	const deUint32 specData[1] =
	{
		m_xSize,
	};
	const vk::VkSpecializationMapEntry entries[1] =
	{
		{0, (deUint32)(sizeof(deUint32) * 0), sizeof(deUint32)},
	};
	const vk::VkSpecializationInfo specInfo =
	{
		1,
		entries,
		sizeof(specData),
		specData
	};

	const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		DE_NULL,
		(VkPipelineLayoutCreateFlags)0,
		1,
		&descriptorSetLayout.get(),
		0u,
		DE_NULL,
	};
	Move<VkPipelineLayout> pipelineLayout = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo, NULL);
	VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;

	for (deUint32 i = 0; i < numBuffers; ++i)
	{
		flushAlloc(vk, device, buffers[i]->getAllocation());
	}

	const Unique<VkShaderModule> shader(createShaderModule(vk, device, context.getBinaryCollection().get("comp"), 0));
	const VkPipelineShaderStageCreateInfo shaderInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		DE_NULL,
		0,
		VK_SHADER_STAGE_COMPUTE_BIT,
		*shader,
		"main",
		&specInfo,
	};

	const VkComputePipelineCreateInfo pipelineInfo =
	{
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		DE_NULL,
		0u,
		shaderInfo,
		*pipelineLayout,
		(VkPipeline)0,
		0u,
	};
	Move<VkPipeline> pipeline = createComputePipeline(vk, device, DE_NULL, &pipelineInfo, NULL);

	const VkQueue queue = context.getUniversalQueue();
	Move<VkCommandPool> cmdPool = createCommandPool(vk, device,
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		context.getUniversalQueueFamilyIndex());
	Move<VkCommandBuffer> cmdBuffer = allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	DescriptorSetUpdateBuilder setUpdateBuilder;
	for (deUint32 i = 0; i < numBuffers; ++i)
	{
		setUpdateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(i),
									 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptors[i]);
	}
	setUpdateBuilder.update(vk, device);

	beginCommandBuffer(vk, *cmdBuffer, 0);

	vk.cmdBindDescriptorSets(*cmdBuffer, bindPoint, *pipelineLayout, 0u, 1, &*descriptorSet, 0u, DE_NULL);
	vk.cmdBindPipeline(*cmdBuffer, bindPoint, *pipeline);

	vk.cmdDispatch(*cmdBuffer, 1, 1, 1);

	endCommandBuffer(vk, *cmdBuffer);

	for (deUint32 r = 0; r < m_repeat; ++r)
	{
		submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

		invalidateAlloc(vk, device, buffers[0]->getAllocation());

		for (deUint32 i = 0; i < (deUint32)size / sizeof(deUint32); ++i)
		{
			deUint32 expected = (m_odd == (i / m_xSize)) ? i : 0u;
			if (ptrs[0][i] != expected)
			{
				log << tcu::TestLog::Message << "failure at index " << i << ": expected " << expected << ", got: " << ptrs[0][i] << tcu::TestLog::EndMessage;
				return tcu::TestStatus::fail("compute failed");
			}
		}

		deMemset(ptrs[0], 0xff, (size_t)size);
		flushAlloc(vk, device, buffers[0]->getAllocation());
		setUpdateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0),
									 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptors[0]);
		setUpdateBuilder.update(vk, device);
	}

	return tcu::TestStatus::pass("compute succeeded");
}

void AddRepeatedPipelineTests(tcu::TestCaseGroup* group, vk::ComputePipelineConstructionType computePipelineConstructionType)
{
	std::vector<deUint32> xSizes = {4, 16, 32, 64};
	std::vector<deUint32> odds = {0, 1};
	std::vector<deUint32> repeats = {2, 4, 8, 16};
	for (deUint32 i = 0; i < xSizes.size(); ++i)
	{
		deUint32 x = xSizes[i];
		for (deUint32 j = 0; j < odds.size(); ++j)
		{
			deUint32 odd = odds[j];
			for (deUint32 k = 0; k < repeats.size(); ++k)
			{
				deUint32 repeat = repeats[k];
				group->addChild(new RepeatedPipelineTest(group->getTestContext(),
					std::string("x_") + de::toString(x) + (odd == 1 ? "_odd" : "_even") + "_repeat_" + de::toString(repeat),
					std::string("x_") + de::toString(x) + (odd == 1 ? "_odd" : "_even") + "_repeat_" + de::toString(repeat),
					x, odd, repeat, computePipelineConstructionType));
			}
		}
	}
}
#ifndef CTS_USES_VULKANSC
void AddSharedMemoryTests (tcu::TestCaseGroup* group, vk::ComputePipelineConstructionType)
{
	tcu::TestContext&			testCtx		= group->getTestContext();
	std::string					filePath	= "compute/zero_initialize_workgroup_memory";
	std::vector<std::string>	requirements;

	std::string					testNames[]	=
	{
		"workgroup_size_128",
		"workgroup_size_8x8x2",
		"workgroup_size_8x2x8",
		"workgroup_size_2x8x8",
		"workgroup_size_8x4x4",
		"workgroup_size_4x8x4",
		"workgroup_size_4x4x8"
	};

	requirements.push_back("VK_KHR_zero_initialize_workgroup_memory");

	for (const auto& testName : testNames)
	{
		group->addChild(cts_amber::createAmberTestCase(testCtx, testName.c_str(), "", filePath.c_str(), testName + ".amber", requirements));
	}
}
#endif // CTS_USES_VULKANSC

} // anonymous

tcu::TestCaseGroup* createZeroInitializeWorkgroupMemoryTests (tcu::TestContext& testCtx, vk::ComputePipelineConstructionType computePipelineConstructionType)
{
	de::MovePtr<tcu::TestCaseGroup> tests(new tcu::TestCaseGroup(testCtx, "zero_initialize_workgroup_memory", "VK_KHR_zero_intialize_workgroup_memory tests"));

	tcu::TestCaseGroup* maxWorkgroupMemoryGroup =
		new tcu::TestCaseGroup(testCtx, "max_workgroup_memory", "Read initialization of max workgroup memory");
	AddMaxWorkgroupMemoryTests(maxWorkgroupMemoryGroup, computePipelineConstructionType);
	tests->addChild(maxWorkgroupMemoryGroup);

	tcu::TestCaseGroup* typeGroup = new tcu::TestCaseGroup(testCtx, "types", "basic type tests");
	AddTypeTests(typeGroup, computePipelineConstructionType);
	tests->addChild(typeGroup);

	tcu::TestCaseGroup* compositeGroup = new tcu::TestCaseGroup(testCtx, "composites", "composite type tests");
	AddCompositeTests(compositeGroup, computePipelineConstructionType);
	tests->addChild(compositeGroup);

	tcu::TestCaseGroup* maxWorkgroupsGroup = new tcu::TestCaseGroup(testCtx, "max_workgroups", "max workgroups");
	AddMaxWorkgroupsTests(maxWorkgroupsGroup, computePipelineConstructionType);
	tests->addChild(maxWorkgroupsGroup);

	tcu::TestCaseGroup* specializeWorkgroupGroup = new tcu::TestCaseGroup(testCtx, "specialize_workgroup", "specialize workgroup size");
	AddSpecializeWorkgroupTests(specializeWorkgroupGroup, computePipelineConstructionType);
	tests->addChild(specializeWorkgroupGroup);

	tcu::TestCaseGroup* repeatPipelineGroup = new tcu::TestCaseGroup(testCtx, "repeat_pipeline", "repeated pipeline run");
	AddRepeatedPipelineTests(repeatPipelineGroup, computePipelineConstructionType);
	tests->addChild(repeatPipelineGroup);

#ifndef CTS_USES_VULKANSC
	tcu::TestCaseGroup* subgroupInvocationGroup = new tcu::TestCaseGroup(testCtx, "shared_memory_blocks", "shared memory tests");
	AddSharedMemoryTests(subgroupInvocationGroup, computePipelineConstructionType);
	tests->addChild(subgroupInvocationGroup);
#endif // CTS_USES_VULKANSC

	return tests.release();
}

} // compute
} // vkt
