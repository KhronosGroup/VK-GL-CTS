/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2020 The Khronos Group Inc.
 * Copyright (c) 2020 Intel Corporation
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
 * \brief VK_KHR_workgroup_memory_explicit_layout tests
 *//*--------------------------------------------------------------------*/

#include "vktComputeWorkgroupMemoryExplicitLayoutTests.hpp"
#include "vktAmberTestCase.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktTestGroupUtil.hpp"

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

struct CheckSupportParams
{
	bool needsScalar;
	bool needsInt8;
	bool needsInt16;
	bool needsInt64;
	bool needsFloat16;
	bool needsFloat64;

	void useType(glu::DataType dt)
	{
		using namespace glu;

		needsInt8		|= isDataTypeIntOrIVec8Bit(dt) || isDataTypeUintOrUVec8Bit(dt);
		needsInt16		|= isDataTypeIntOrIVec16Bit(dt) || isDataTypeUintOrUVec16Bit(dt);
		needsFloat16	|= isDataTypeFloat16OrVec(dt);
		needsFloat64	|= isDataTypeDoubleOrDVec(dt);
	}
};

void checkSupportWithParams(Context& context, const CheckSupportParams& params)
{
	context.requireDeviceFunctionality("VK_KHR_workgroup_memory_explicit_layout");
	context.requireDeviceFunctionality("VK_KHR_spirv_1_4");

	VkPhysicalDeviceWorkgroupMemoryExplicitLayoutFeaturesKHR layout_features;
	deMemset(&layout_features, 0, sizeof(layout_features));
	layout_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_WORKGROUP_MEMORY_EXPLICIT_LAYOUT_FEATURES_KHR;
	layout_features.pNext = DE_NULL;

	VkPhysicalDeviceShaderFloat16Int8Features f16_i8_features;
	deMemset(&f16_i8_features, 0, sizeof(f16_i8_features));
	f16_i8_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES;
	f16_i8_features.pNext = &layout_features;

	VkPhysicalDeviceFeatures2 features2;
	deMemset(&features2, 0, sizeof(features2));
	features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	features2.pNext = &f16_i8_features;
	context.getInstanceInterface().getPhysicalDeviceFeatures2(context.getPhysicalDevice(), &features2);

	if (params.needsScalar)
	{
		if (layout_features.workgroupMemoryExplicitLayoutScalarBlockLayout != VK_TRUE)
			TCU_THROW(NotSupportedError, "workgroupMemoryExplicitLayoutScalarBlockLayout not supported");
	}

	if (params.needsInt8)
	{
		if (f16_i8_features.shaderInt8 != VK_TRUE)
			TCU_THROW(NotSupportedError, "shaderInt8 not supported");
		if (layout_features.workgroupMemoryExplicitLayout8BitAccess != VK_TRUE)
			TCU_THROW(NotSupportedError, "workgroupMemoryExplicitLayout8BitAccess not supported");
	}

	if (params.needsInt16)
	{
		if (features2.features.shaderInt16 != VK_TRUE)
			TCU_THROW(NotSupportedError, "shaderInt16 not supported");
		if (layout_features.workgroupMemoryExplicitLayout16BitAccess != VK_TRUE)
			TCU_THROW(NotSupportedError, "workgroupMemoryExplicitLayout16BitAccess not supported");
	}

	if (params.needsInt64)
	{
		if (features2.features.shaderInt64 != VK_TRUE)
			TCU_THROW(NotSupportedError, "shaderInt64 not supported");
	}

	if (params.needsFloat16)
	{
		if (f16_i8_features.shaderFloat16 != VK_TRUE)
			TCU_THROW(NotSupportedError, "shaderFloat16 not supported");
		if (layout_features.workgroupMemoryExplicitLayout16BitAccess != VK_TRUE)
			TCU_THROW(NotSupportedError, "workgroupMemoryExplicitLayout16BitAccess not supported");
	}

	if (params.needsFloat64)
	{
		if (features2.features.shaderFloat64 != VK_TRUE)
			TCU_THROW(NotSupportedError, "shaderFloat64 not supported");
	}
}

tcu::TestStatus runCompute(Context& context, deUint32 workgroupSize)
{
	const DeviceInterface&	vk			= context.getDeviceInterface();
	const VkDevice			device		= context.getDevice();
	Allocator&				allocator	= context.getDefaultAllocator();
	tcu::TestLog&			log			= context.getTestContext().getLog();

	de::MovePtr<BufferWithMemory> buffer;
	VkDescriptorBufferInfo bufferDescriptor;

	VkDeviceSize size = sizeof(deUint32) * workgroupSize;

	buffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
		vk, device, allocator, makeBufferCreateInfo(size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
		MemoryRequirement::HostVisible | MemoryRequirement::Cached));
	bufferDescriptor = makeDescriptorBufferInfo(**buffer, 0, size);

	deUint32* ptr = (deUint32*)buffer->getAllocation().getHostPtr();

	deMemset(ptr, 0xFF, static_cast<std::size_t>(size));

	DescriptorSetLayoutBuilder layoutBuilder;
	layoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);

	Unique<VkDescriptorSetLayout> descriptorSetLayout(layoutBuilder.build(vk, device));
	Unique<VkDescriptorPool> descriptorPool(DescriptorPoolBuilder()
		.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1u)
		.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u));
	Unique<VkDescriptorSet> descriptorSet(makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout));

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
	flushAlloc(vk, device, buffer->getAllocation());

	const Unique<VkShaderModule> shader(createShaderModule(vk, device, context.getBinaryCollection().get("comp"), 0));
	const VkPipelineShaderStageCreateInfo shaderInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		DE_NULL,
		0,
		VK_SHADER_STAGE_COMPUTE_BIT,
		*shader,
		"main",
		DE_NULL,
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
	setUpdateBuilder.writeSingle(*descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0),
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferDescriptor);
	setUpdateBuilder.update(vk, device);

	beginCommandBuffer(vk, *cmdBuffer, 0);

	vk.cmdBindDescriptorSets(*cmdBuffer, bindPoint, *pipelineLayout, 0u, 1, &*descriptorSet, 0u, DE_NULL);
	vk.cmdBindPipeline(*cmdBuffer, bindPoint, *pipeline);

	vk.cmdDispatch(*cmdBuffer, 1, 1, 1);

	endCommandBuffer(vk, *cmdBuffer);

	submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

	invalidateAlloc(vk, device, buffer->getAllocation());
	for (deUint32 i = 0; i < workgroupSize; ++i)
	{
		deUint32 expected = i;
		if (ptr[i] != expected)
		{
			log << tcu::TestLog::Message << "failure at index " << i << ": expected " << expected << ", got: " << ptr[i] << tcu::TestLog::EndMessage;
			return tcu::TestStatus::fail("compute failed");
		}
	}

	return tcu::TestStatus::pass("compute succeeded");
}

class AliasTest : public vkt::TestCase
{
public:
	enum Requirements
	{
		RequirementNone    = 0,
		RequirementFloat16 = 1 << 0,
		RequirementFloat64 = 1 << 1,
		RequirementInt8    = 1 << 2,
		RequirementInt16   = 1 << 3,
		RequirementInt64   = 1 << 4,
	};

	enum Flags
	{
		FlagNone         = 0,
		FlagLayoutStd430 = 1 << 0,
		FlagLayoutStd140 = 1 << 1,
		FlagLayoutScalar = 1 << 2,
		FlagFunction     = 1 << 3,
		FlagBarrier      = 1 << 4,
	};

	enum LayoutFlags
	{
		LayoutNone    = 0,

		LayoutDefault = 1 << 0,
		LayoutStd140  = 1 << 1,
		LayoutStd430  = 1 << 2,
		LayoutScalar  = 1 << 3,
		LayoutAll     = LayoutDefault | LayoutStd140 | LayoutStd430 | LayoutScalar,

		LayoutCount   = 4,
	};

	enum Function
	{
		FunctionNone = 0,
		FunctionRead,
		FunctionWrite,
		FunctionReadWrite,
		FunctionCount,
	};

	enum Synchronization
	{
		SynchronizationNone = 0,
		SynchronizationBarrier,
		SynchronizationCount,
	};

	struct CaseDef
	{
		std::string extraTypes;

		std::string writeDesc;
		std::string	writeType;
		std::string writeValue;

		std::string readDesc;
		std::string readType;
		std::string readValue;

		LayoutFlags layout;
		Function func;
		Synchronization sync;
		Requirements requirements;

		std::string testName() const
		{
			std::string name = writeDesc + "_to_" + readDesc;

			// In a valid test case, only one flag will be set.
			switch (layout)
			{
			case LayoutDefault:
				name += "_default";
				break;
			case LayoutStd140:
				name += "_std140";
				break;
			case LayoutStd430:
				name += "_std430";
				break;
			case LayoutScalar:
				name += "_scalar";
				break;
			default:
				DE_ASSERT(0);
				break;
			}

			switch (func)
			{
			case FunctionNone:
				break;
			case FunctionRead:
				name += "_func_read";
				break;
			case FunctionWrite:
				name += "_func_write";
				break;
			case FunctionReadWrite:
				name += "_func_read_write";
				break;
			default:
				DE_ASSERT(0);
				break;
			}

			switch (sync)
			{
			case SynchronizationNone:
				break;
			case SynchronizationBarrier:
				name += "_barrier";
				break;
			default:
				DE_ASSERT(0);
				break;
			}

			return name;
		}
	};

	AliasTest(tcu::TestContext& testCtx, const CaseDef& caseDef)
		: TestCase(testCtx, caseDef.testName(), caseDef.testName()),
		m_caseDef(caseDef)
	{
	}

	virtual void checkSupport(Context& context) const;
	void initPrograms(SourceCollections& sourceCollections) const;

	class Instance : public vkt::TestInstance
	{
	public:
		Instance(Context& context, const CaseDef& caseDef)
			: TestInstance(context),
			  m_caseDef(caseDef)
		{
		}

		tcu::TestStatus iterate(void)
		{
			return runCompute(m_context, 1u);
		}

	private:
		CaseDef m_caseDef;
	};

	TestInstance* createInstance(Context& context) const
	{
		return new Instance(context, m_caseDef);
	}

private:
	CaseDef m_caseDef;
};

void AliasTest::checkSupport(Context& context) const
{
	CheckSupportParams p;
	deMemset(&p, 0, sizeof(p));

	p.needsScalar	= m_caseDef.layout == LayoutScalar;
	p.needsInt8		= m_caseDef.requirements & RequirementInt8;
	p.needsInt16	= m_caseDef.requirements & RequirementInt16;
	p.needsInt64	= m_caseDef.requirements & RequirementInt64;
	p.needsFloat16	= m_caseDef.requirements & RequirementFloat16;
	p.needsFloat64	= m_caseDef.requirements & RequirementFloat64;

	checkSupportWithParams(context, p);
}

void AliasTest::initPrograms(SourceCollections& sourceCollections) const
{
	std::string layout;
	switch (m_caseDef.layout)
	{
	case LayoutStd140:
		layout = "layout(std140)";
		break;
	case LayoutStd430:
		layout = "layout(std430)";
		break;
	case LayoutScalar:
		layout = "layout(scalar)";
		break;
	default:
		// No layout specified.
		break;
	}

	std::ostringstream src;

	src << "#version 450\n";
	src << "#extension GL_EXT_shared_memory_block : enable\n";
	src << "#extension GL_EXT_shader_explicit_arithmetic_types : enable\n";

	if (m_caseDef.layout == LayoutScalar)
		src << "#extension GL_EXT_scalar_block_layout : enable\n";

	src << "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n";

	if (!m_caseDef.extraTypes.empty())
		src << m_caseDef.extraTypes << ";\n";

	src << layout << "shared A { " << m_caseDef.writeType << "; } a;\n";
	src << layout << "shared B { " << m_caseDef.readType << "; } b;\n";
	src << "layout(set = 0, binding = 0) buffer Result { uint result; };\n";

	if (m_caseDef.func == FunctionRead ||
		m_caseDef.func == FunctionReadWrite)
	{
		src << "void read(int index) {\n";
		src << "  if (b.v == " << m_caseDef.readValue << ")\n";
		src << "    result = index;\n";
		src << "}\n";
	}

	if (m_caseDef.func == FunctionWrite ||
		m_caseDef.func == FunctionReadWrite)
	{
		src << "void write(int index) {\n";
		src << "  if (index == 0)\n";
		src << "    a.v = " << m_caseDef.writeValue << ";\n";
		src << "}\n";
	}

	src << "void main() {\n";
	src << "  int index = int(gl_LocalInvocationIndex);\n";

	if (m_caseDef.func == FunctionWrite)
		src << "  write(index);\n";
	else
		src << "  a.v = " << m_caseDef.writeValue << ";\n";

	if (m_caseDef.sync == SynchronizationBarrier)
		src << "  barrier();\n";

	if (m_caseDef.func == FunctionRead ||
		m_caseDef.func == FunctionReadWrite)
	{
		src << "  read(index);\n";
	}
	else
	{
		src << "  if (b.v == " << m_caseDef.readValue << ")\n";
		src << "    result = index;\n";
	}
	src << "}\n";

	deUint32 buildFlags =
		m_caseDef.layout == LayoutScalar ? ShaderBuildOptions::FLAG_ALLOW_WORKGROUP_SCALAR_OFFSETS :
		                                   ShaderBuildOptions::Flags(0u);

	sourceCollections.glslSources.add("comp")
		<< glu::ComputeSource(src.str())
		<< vk::ShaderBuildOptions(sourceCollections.usedVulkanVersion, vk::SPIRV_VERSION_1_4, buildFlags);
}

std::string makeArray(const std::string& type, const std::vector<deUint64>& values)
{
	std::ostringstream s;
	s << type << "[](";
	for (std::size_t i = 0; i < values.size(); i++)
	{
		s << type << "(" << std::to_string(values[i]) << ")";
		if (i != values.size() - 1)
			s << ", ";
	};
	s << ")";
	return s.str();
}

std::string makeU8Array(const std::vector<deUint64>& values)
{
	return makeArray("uint8_t", values);
}

std::string makeU16Array(const std::vector<deUint64>& values)
{
	return makeArray("uint16_t", values);
}

std::string makeU32Array(const std::vector<deUint64>& values)
{
	return makeArray("uint32_t", values);
}

void AddAliasTests(tcu::TestCaseGroup* group)
{
	const int DEFAULT = AliasTest::LayoutDefault;
	const int STD140 = AliasTest::LayoutStd140;
	const int STD430 = AliasTest::LayoutStd430;
	const int SCALAR = AliasTest::LayoutScalar;
	const int ALL = DEFAULT | STD140 | STD430 | SCALAR;

	const int FLOAT16 = AliasTest::RequirementFloat16;
	const int FLOAT64 = AliasTest::RequirementFloat64;
	const int INT8 = AliasTest::RequirementInt8;
	const int INT16 = AliasTest::RequirementInt16;
	const int INT64 = AliasTest::RequirementInt64;

#define CASE_EXTRA(L, R, E, D1, T1, V1, D2, T2, V2)						\
	{ E, D1, T1, V1, D2, T2, V2, AliasTest::LayoutFlags(L), AliasTest::FunctionNone, AliasTest::SynchronizationNone, AliasTest::Requirements(R) }

#define CASE_EXTRA_WITH_REVERSE(L, R, E, D1, T1, V1, D2, T2, V2)	\
	CASE_EXTRA(L, R, E, D1, T1, V1, D2, T2, V2),					\
	CASE_EXTRA(L, R, E, D2, T2, V2, D1, T1, V1)

#define CASE_WITH_REVERSE(L, R, D1, T1, V1, D2, T2, V2)	CASE_EXTRA_WITH_REVERSE(L, R, "", D1, T1, V1, D2, T2, V2)
#define CASE_SAME_TYPE(R, D, T, V)						CASE_EXTRA(ALL, R, "", D, T, V, D, T, V)
#define CASE(L, R, D1, T1, V1, D2, T2, V2)				CASE_EXTRA(L, R, "", D1, T1, V1, D2, T2, V2)


	std::vector<AliasTest::CaseDef> cases =
	{
		CASE_SAME_TYPE(0,		"bool_true",	"bool v",		"true"),
		CASE_SAME_TYPE(0,		"bool_false",	"bool v",		"false"),
		CASE_SAME_TYPE(0,		"bvec2",		"bvec2 v",		"bvec2(false, true)"),
		CASE_SAME_TYPE(0,		"bvec3",		"bvec3 v",		"bvec3(false, true, true)"),
		CASE_SAME_TYPE(0,		"bvec4",		"bvec4 v",		"bvec4(false, true, true, false)"),
		CASE_SAME_TYPE(INT8,	"u8",			"uint8_t v",	"uint8_t(10)"),
		CASE_SAME_TYPE(INT8,	"u8vec2",		"u8vec2 v",		"u8vec2(10, 20)"),
		CASE_SAME_TYPE(INT8,	"u8vec3",		"u8vec3 v",		"u8vec3(10, 20, 30)"),
		CASE_SAME_TYPE(INT8,	"u8vec4",		"u8vec4 v",		"u8vec4(10, 20, 30, 40)"),
		CASE_SAME_TYPE(INT8,	"i8",			"int8_t v",		"int8_t(-10)"),
		CASE_SAME_TYPE(INT8,	"i8vec2",		"i8vec2 v",		"i8vec2(-10, 20)"),
		CASE_SAME_TYPE(INT8,	"i8vec3",		"i8vec3 v",		"i8vec3(-10, 20, -30)"),
		CASE_SAME_TYPE(INT8,	"i8vec4",		"i8vec4 v",		"i8vec4(-10, 20, -30, 40)"),
		CASE_SAME_TYPE(INT16,	"u16",			"uint16_t v",	"uint16_t(1000)"),
		CASE_SAME_TYPE(INT16,	"u16vec2",		"u16vec2 v",	"u16vec2(1000, 2000)"),
		CASE_SAME_TYPE(INT16,	"u16vec3",		"u16vec3 v",	"u16vec3(1000, 2000, 3000)"),
		CASE_SAME_TYPE(INT16,	"u16vec4",		"u16vec4 v",	"u16vec4(1000, 2000, 3000, 4000)"),
		CASE_SAME_TYPE(INT16,	"i16",			"int16_t v",	"int16_t(-1000)"),
		CASE_SAME_TYPE(INT16,	"i16vec2",		"i16vec2 v",	"i16vec2(-1000, 2000)"),
		CASE_SAME_TYPE(INT16,	"i16vec3",		"i16vec3 v",	"i16vec3(-1000, 2000, -3000)"),
		CASE_SAME_TYPE(INT16,	"i16vec4",		"i16vec4 v",	"i16vec4(-1000, 2000, -3000, 4000)"),
		CASE_SAME_TYPE(0,		"u32",			"uint32_t v",	"uint32_t(100)"),
		CASE_SAME_TYPE(0,		"uvec2",		"uvec2 v",		"uvec2(100, 200)"),
		CASE_SAME_TYPE(0,		"uvec3",		"uvec3 v",		"uvec3(100, 200, 300)"),
		CASE_SAME_TYPE(0,		"uvec4",		"uvec4 v",		"uvec4(100, 200, 300, 400)"),
		CASE_SAME_TYPE(0,		"i32",			"int32_t v",	"int32_t(-100)"),
		CASE_SAME_TYPE(0,		"ivec2",		"ivec2 v",		"ivec2(-100, 200)"),
		CASE_SAME_TYPE(0,		"ivec3",		"ivec3 v",		"ivec3(-100, 200, -300)"),
		CASE_SAME_TYPE(0,		"ivec4",		"ivec4 v",		"ivec4(-100, 200, -300, 400)"),
		CASE_SAME_TYPE(INT64,	"u64",			"uint64_t v",	"uint64_t(1000)"),
		CASE_SAME_TYPE(INT64,	"u64vec2",		"u64vec2 v",	"u64vec2(1000, 2000)"),
		CASE_SAME_TYPE(INT64,	"u64vec3",		"u64vec3 v",	"u64vec3(1000, 2000, 3000)"),
		CASE_SAME_TYPE(INT64,	"u64vec4",		"u64vec4 v",	"u64vec4(1000, 2000, 3000, 4000)"),
		CASE_SAME_TYPE(INT64,	"i64",			"int64_t v",	"int64_t(-1000)"),
		CASE_SAME_TYPE(INT64,	"i64vec2",		"i64vec2 v",	"i64vec2(-1000, 2000)"),
		CASE_SAME_TYPE(INT64,	"i64vec3",		"i64vec3 v",	"i64vec3(-1000, 2000, -3000)"),
		CASE_SAME_TYPE(INT64,	"i64vec4",		"i64vec4 v",	"i64vec4(-1000, 2000, -3000, 4000)"),
		CASE_SAME_TYPE(FLOAT16,	"f16",			"float16_t v",	"float16_t(-100.0)"),
		CASE_SAME_TYPE(FLOAT16,	"f16vec2",		"f16vec2 v",	"f16vec2(100.0, -200.0)"),
		CASE_SAME_TYPE(FLOAT16,	"f16vec3",		"f16vec3 v",	"f16vec3(100.0, -200.0, 300.0)"),
		CASE_SAME_TYPE(FLOAT16,	"f16vec4",		"f16vec4 v",	"f16vec4(100.0, -200.0, 300.0, -400.0)"),
		CASE_SAME_TYPE(0,		"f32",			"float32_t v",	"float32_t(-100.0)"),
		CASE_SAME_TYPE(0,		"f32vec2",		"f32vec2 v",	"f32vec2(100.0, -200.0)"),
		CASE_SAME_TYPE(0,		"f32vec3",		"f32vec3 v",	"f32vec3(100.0, -200.0, 300.0)"),
		CASE_SAME_TYPE(0,		"f32vec4",		"f32vec4 v",	"f32vec4(100.0, -200.0, 300.0, -400.0)"),
		CASE_SAME_TYPE(FLOAT64,	"f64",			"float64_t v",	"float32_t(-100.0)"),
		CASE_SAME_TYPE(FLOAT64,	"f64vec2",		"f64vec2 v",	"f64vec2(100.0, -200.0)"),
		CASE_SAME_TYPE(FLOAT64,	"f64vec3",		"f64vec3 v",	"f64vec3(100.0, -200.0, 300.0)"),
		CASE_SAME_TYPE(FLOAT64,	"f64vec4",		"f64vec4 v",	"f64vec4(100.0, -200.0, 300.0, -400.0)"),
		CASE_SAME_TYPE(FLOAT16,	"f16mat2x2",	"f16mat2x2 v",	"f16mat2x2(1, 2, 3, 4)"),
		CASE_SAME_TYPE(FLOAT16,	"f16mat2x3",	"f16mat2x3 v",	"f16mat2x3(1, 2, 3, 4, 5, 6)"),
		CASE_SAME_TYPE(FLOAT16,	"f16mat2x4",	"f16mat2x4 v",	"f16mat2x4(1, 2, 3, 4, 5, 6, 7, 8)"),
		CASE_SAME_TYPE(FLOAT16,	"f16mat3x2",	"f16mat3x2 v",	"f16mat3x2(1, 2, 3, 4, 5, 6)"),
		CASE_SAME_TYPE(FLOAT16,	"f16mat3x3",	"f16mat3x3 v",	"f16mat3x3(1, 2, 3, 4, 5, 6, 7, 8, 9)"),
		CASE_SAME_TYPE(FLOAT16,	"f16mat3x4",	"f16mat3x4 v",	"f16mat3x4(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12)"),
		CASE_SAME_TYPE(FLOAT16,	"f16mat4x2",	"f16mat4x2 v",	"f16mat4x2(1, 2, 3, 4, 5, 6, 7, 8)"),
		CASE_SAME_TYPE(FLOAT16,	"f16mat4x3",	"f16mat4x3 v",	"f16mat4x3(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12)"),
		CASE_SAME_TYPE(FLOAT16,	"f16mat4x4",	"f16mat4x4 v",	"f16mat4x4(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16)"),
		CASE_SAME_TYPE(0,		"f32mat2x2",	"f32mat2x2 v",	"f32mat2x2(1, 2, 3, 4)"),
		CASE_SAME_TYPE(0,		"f32mat2x3",	"f32mat2x3 v",	"f32mat2x3(1, 2, 3, 4, 5, 6)"),
		CASE_SAME_TYPE(0,		"f32mat2x4",	"f32mat2x4 v",	"f32mat2x4(1, 2, 3, 4, 5, 6, 7, 8)"),
		CASE_SAME_TYPE(0,		"f32mat3x2",	"f32mat3x2 v",	"f32mat3x2(1, 2, 3, 4, 5, 6)"),
		CASE_SAME_TYPE(0,		"f32mat3x3",	"f32mat3x3 v",	"f32mat3x3(1, 2, 3, 4, 5, 6, 7, 8, 9)"),
		CASE_SAME_TYPE(0,		"f32mat3x4",	"f32mat3x4 v",	"f32mat3x4(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12)"),
		CASE_SAME_TYPE(0,		"f32mat4x2",	"f32mat4x2 v",	"f32mat4x2(1, 2, 3, 4, 5, 6, 7, 8)"),
		CASE_SAME_TYPE(0,		"f32mat4x3",	"f32mat4x3 v",	"f32mat4x3(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12)"),
		CASE_SAME_TYPE(0,		"f32mat4x4",	"f32mat4x4 v",	"f32mat4x4(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16)"),
		CASE_SAME_TYPE(FLOAT64,	"f64mat2x2",	"f64mat2x2 v",	"f64mat2x2(1, 2, 3, 4)"),
		CASE_SAME_TYPE(FLOAT64,	"f64mat2x3",	"f64mat2x3 v",	"f64mat2x3(1, 2, 3, 4, 5, 6)"),
		CASE_SAME_TYPE(FLOAT64,	"f64mat2x4",	"f64mat2x4 v",	"f64mat2x4(1, 2, 3, 4, 5, 6, 7, 8)"),
		CASE_SAME_TYPE(FLOAT64,	"f64mat3x2",	"f64mat3x2 v",	"f64mat3x2(1, 2, 3, 4, 5, 6)"),
		CASE_SAME_TYPE(FLOAT64,	"f64mat3x3",	"f64mat3x3 v",	"f64mat3x3(1, 2, 3, 4, 5, 6, 7, 8, 9)"),
		CASE_SAME_TYPE(FLOAT64,	"f64mat3x4",	"f64mat3x4 v",	"f64mat3x4(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12)"),
		CASE_SAME_TYPE(FLOAT64,	"f64mat4x2",	"f64mat4x2 v",	"f64mat4x2(1, 2, 3, 4, 5, 6, 7, 8)"),
		CASE_SAME_TYPE(FLOAT64,	"f64mat4x3",	"f64mat4x3 v",	"f64mat4x3(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12)"),
		CASE_SAME_TYPE(FLOAT64,	"f64mat4x4",	"f64mat4x4 v",	"f64mat4x4(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16)"),

		CASE_WITH_REVERSE(ALL, INT8,
			"i8",			"int8_t v",			"int8_t(-2)",
			"u8",			"uint8_t v",		"uint8_t(0xFE)"),
		CASE_WITH_REVERSE(ALL, INT16,
			"i16",			"int16_t v",		"int16_t(-2)",
			"u16",			"uint16_t v",		"uint16_t(0xFFFE)"),
		CASE_WITH_REVERSE(ALL, 0,
			"i32",			"int32_t v",		"int32_t(-2)",
			"u32",			"uint32_t v",		"uint32_t(0xFFFFFFFE)"),
		CASE_WITH_REVERSE(ALL, INT64,
			"i64",			"int64_t v",		"int64_t(-2UL)",
			"u64",			"uint64_t v",		"uint64_t(0xFFFFFFFFFFFFFFFEUL)"),
		CASE_WITH_REVERSE(ALL, FLOAT16 | INT16,
			"f16",			"float16_t v",		"float16_t(1.0)",
			"u16",			"uint16_t v",		"uint16_t(0x3C00)"),
		CASE_WITH_REVERSE(ALL, 0,
			"f32",			"float32_t v",		"float32_t(1.0)",
			"u32",			"uint32_t v",		"uint32_t(0x3F800000)"),
		CASE_WITH_REVERSE(ALL, FLOAT64 | INT64,
			"f64",			"float64_t v",		"float64_t(1.0)",
			"u64",			"uint64_t v",		"uint64_t(0x3FF0000000000000UL)"),

		CASE_WITH_REVERSE(DEFAULT | STD430 | SCALAR, INT16 | INT8,
			"u16",			"uint16_t v",		"uint16_t(0x1234)",
			"u8_array",		"uint8_t v[2]",		makeU8Array({0x34, 0x12})),
		CASE_WITH_REVERSE(DEFAULT | STD430 | SCALAR, INT8,
			"u32",			"uint32_t v",		"uint32_t(0x12345678)",
			"u8_array",		"uint8_t v[4]",		makeU8Array({0x78, 0x56, 0x34, 0x12})),
		CASE_WITH_REVERSE(DEFAULT | STD430 | SCALAR, INT16,
			"u32",			"uint32_t v",		"uint32_t(0x12345678)",
			"u16_array",	"uint16_t v[2]",	makeU16Array({0x5678, 0x1234})),
		CASE_WITH_REVERSE(DEFAULT | STD430 | SCALAR, INT64 | INT8,
			"u64",			"uint64_t v",		"uint64_t(0x1234567890ABCDEFUL)",
			"u8_array",		"uint8_t v[8]",		makeU8Array({0xEF, 0xCD, 0xAB, 0x90, 0x78, 0x56, 0x34, 0x12})),
		CASE_WITH_REVERSE(DEFAULT | STD430 | SCALAR, INT64 | INT16,
			"u64",			"uint64_t v",		"uint64_t(0x1234567890ABCDEFUL)",
			"u16_array",	"uint16_t v[4]",	makeU16Array({0xCDEF, 0x90AB, 0x5678, 0x1234})),
		CASE_WITH_REVERSE(DEFAULT | STD430 | SCALAR, INT64,
			"u64",			"uint64_t v",		"uint64_t(0x1234567890ABCDEFUL)",
			"u32_array",	"uint32_t v[2]",	makeU32Array({0x90ABCDEF, 0x12345678})),
		CASE_WITH_REVERSE(DEFAULT | STD430 | SCALAR, INT16 | INT8,
			"i16",			"int16_t v",		"int16_t(-2)",
			"u8_array",		"uint8_t v[2]",		makeU8Array({0xFE, 0xFF})),
		CASE_WITH_REVERSE(DEFAULT | STD430 | SCALAR, INT8,
			 "i32",			"int32_t v",		"int32_t(-2)",
			 "u8_array",	"uint8_t v[4]",		makeU8Array({0xFE, 0xFF, 0xFF, 0xFF})),
		CASE_WITH_REVERSE(DEFAULT | STD430 | SCALAR, INT16,
			 "i32",			"int32_t v",		"int32_t(-2)",
			 "u16_array",	"uint16_t v[2]",	makeU16Array({0xFFFE, 0xFFFF})),
		CASE_WITH_REVERSE(DEFAULT | STD430 | SCALAR, INT64 | INT8,
			 "i64",			"int64_t v",		"int64_t(-2UL)",
			 "u8_array",	"uint8_t v[8]",		makeU8Array({0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF})),
		CASE_WITH_REVERSE(DEFAULT | STD430 | SCALAR, INT64 | INT16,
			 "i64",			"int64_t v",		"int64_t(-2UL)",
			 "u16_array",	"uint16_t v[4]",	makeU16Array({0xFFFE, 0xFFFF, 0xFFFF, 0xFFFF})),
		CASE_WITH_REVERSE(DEFAULT | STD430 | SCALAR, INT64,
			 "i64",			"int64_t v",		"int64_t(-2UL)",
			 "u32_array",	"uint32_t v[2]",	makeU32Array({0xFFFFFFFE, 0xFFFFFFFF})),
		CASE_WITH_REVERSE(DEFAULT | STD430 | SCALAR, FLOAT16 | INT8,
			 "f16",			"float16_t v",		"float16_t(1.0)",
			 "u8_array",	"uint8_t v[2]",		makeU8Array({0x00, 0x3C})),
		CASE_WITH_REVERSE(DEFAULT | STD430 | SCALAR, INT8,
			 "f32",			"float32_t v",		"float32_t(1.0)",
			 "u8_array",	"uint8_t v[4]",		makeU8Array({0x00, 0x00, 0x80, 0x3F})),
		CASE_WITH_REVERSE(DEFAULT | STD430 | SCALAR, INT16,
			 "f32",			"float32_t v",		"float32_t(1.0)",
			 "u16_array",	"uint16_t v[2]",	makeU16Array({0x0000, 0x3F80})),
		CASE_WITH_REVERSE(DEFAULT | STD430 | SCALAR, FLOAT64 | INT8,
			 "f64",			"float64_t v",		"float64_t(1.0)",
			 "u8_array",	"uint8_t v[8]",		makeU8Array({0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x3F})),
		CASE_WITH_REVERSE(DEFAULT | STD430 | SCALAR, FLOAT64 | INT16,
			"f64",			"float64_t v",		"float64_t(1.0)",
			"u16_array",	"uint16_t v[4]",	makeU16Array({0x0000, 0x0000, 0x0000, 0x3FF0})),
		CASE_WITH_REVERSE(DEFAULT | STD430 | SCALAR, FLOAT64,
			 "f64",			"float64_t v",		"float64_t(1.0)",
			 "u32_array",	"uint32_t v[2]",	makeU32Array({0x00000000, 0x3FF00000})),

		CASE(DEFAULT | STD430, 0,
			 "vec4_array",	"vec4 v[3]",		"vec4[](vec4(1, 1, 2, 2), vec4(3, 3, 4, 4), vec4(5, 5, 6, 6))",
			 "vec2_array",	"vec2 v[6]",		"vec2[](vec2(1), vec2(2), vec2(3), vec2(4), vec2(5), vec2(6))"),
		CASE(STD140, 0,
			 "vec4_array",   "vec4 v[3]",		"vec4[](vec4(1, 1, 999, 999), vec4(2, 2, 999, 999), vec4(3, 3, 999, 999))",
			 "vec2_array",	"vec2 v[3]",		"vec2[](vec2(1), vec2(2), vec2(3))"),
		CASE(SCALAR, 0,
			 "vec4_array",	"vec4 v[3]",		"vec4[](vec4(1, 1, 2, 2), vec4(3, 3, 4, 4), vec4(5, 5, 6, 6))",
			 "vec2_array",	"vec2 v[6]",		"vec2[](vec2(1), vec2(2), vec2(3), vec2(4), vec2(5), vec2(6))"),

		CASE(DEFAULT | STD430, 0,
			 "vec4_array",	"vec4 v[3]",		"vec4[](vec4(1, 1, 1, 999), vec4(2, 2, 2, 999), vec4(3, 3, 3, 999))",
			 "vec3_array",	"vec3 v[3]",		"vec3[](vec3(1), vec3(2), vec3(3))"),
		CASE(STD140, 0,
			 "vec4_array",	"vec4 v[3]",		"vec4[](vec4(1, 1, 1, 999), vec4(2, 2, 2, 999), vec4(3, 3, 3, 999))",
			 "vec3_array",	"vec3 v[3]",		"vec3[](vec3(1), vec3(2), vec3(3))"),
		CASE(SCALAR, 0,
			 "vec4_array",	"vec4 v[3]",		"vec4[](vec4(1, 1, 1, 2), vec4(2, 2, 3, 3), vec4(3, 4, 4, 4))",
			 "vec3_array",	"vec3 v[4]",		"vec3[](vec3(1), vec3(2), vec3(3), vec3(4))"),

		CASE_EXTRA(DEFAULT | STD430 | SCALAR, INT8,
			"struct s { int a; int b; }",
			"u8_array",			"uint8_t v[8]",	makeU8Array({2, 0, 0, 0, 0xFE, 0xFF, 0xFF, 0xFF}),
			"struct_int_int",	"s v",			"s(2, -2)"),
		CASE_EXTRA(ALL, 0,
			"struct s { int a; int b; }",
			"uvec2",				"uvec2 v",		"uvec2(2, 0xFFFFFFFE)",
			"struct_int_int",	"s v",			"s(2, -2)"),
	};

#undef CASE_EXTRA
#undef CASE_EXTRA_WITH_REVERSE
#undef CASE_WITH_REVERSE
#undef CASE_SAME_TYPE
#undef CASE

	for (deUint32 i = 0; i < cases.size(); i++)
	{
		for (int syncIndex = 0; syncIndex < AliasTest::SynchronizationCount; syncIndex++)
		{
			const AliasTest::Synchronization sync = AliasTest::Synchronization(syncIndex);

			for (int funcIndex = 0; funcIndex < AliasTest::FunctionCount; funcIndex++)
			{
				const AliasTest::Function func = AliasTest::Function(funcIndex);

				for (int layoutIndex = 0; layoutIndex < AliasTest::LayoutCount; layoutIndex++)
				{
					const AliasTest::LayoutFlags layout = AliasTest::LayoutFlags(1 << layoutIndex);

					AliasTest::CaseDef c = cases[i];

					if (c.writeDesc == c.readDesc)
						continue;

					if ((c.layout & layout) == 0)
						continue;

					c.layout = layout;
					c.func = func;
					c.sync = sync;

					group->addChild(new AliasTest(group->getTestContext(), c));
				}
			}
		}
	}
}

class ZeroTest : public vkt::TestCase
{
public:
	struct CaseDef
	{
		glu::DataType zeroElementType;
		glu::DataType fieldType[2];
		deUint32 elements;

		std::string testName() const
		{
			std::string name = glu::getDataTypeName(zeroElementType);
			name += "_array_to";

			for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(fieldType); ++i)
			{
				if (fieldType[i] == glu::TYPE_INVALID)
					break;
				name += "_";
				name += glu::getDataTypeName(fieldType[i]);
			}
			name += "_array_" + de::toString(elements);
			return name;
		}
	};

	ZeroTest(tcu::TestContext& testCtx, const CaseDef& caseDef)
		: TestCase(testCtx, caseDef.testName(), caseDef.testName()),
		m_caseDef(caseDef)
	{
	}

	virtual void checkSupport(Context& context) const;
	void initPrograms(SourceCollections& sourceCollections) const;

	class Instance : public vkt::TestInstance
	{
	public:
		Instance(Context& context)
			: TestInstance(context)
		{
		}

		tcu::TestStatus iterate(void)
		{
			return runCompute(m_context, 1u);
		}
	};

	TestInstance* createInstance(Context& context) const
	{
		return new Instance(context);
	}

private:
	CaseDef m_caseDef;
};

void ZeroTest::checkSupport(Context& context) const
{
	CheckSupportParams p;
	deMemset(&p, 0, sizeof(p));

	DE_ASSERT(!glu::isDataTypeFloat16OrVec(m_caseDef.zeroElementType));

	p.useType(m_caseDef.zeroElementType);
	p.useType(m_caseDef.fieldType[0]);
	p.useType(m_caseDef.fieldType[1]);

	checkSupportWithParams(context, p);
}

std::string getDataTypeLiteral(glu::DataType dt, std::string baseValue)
{
	using namespace glu;

	if (isDataTypeVector(dt))
	{
		std::string elemValue = getDataTypeLiteral(getDataTypeScalarType(dt), baseValue);

		std::ostringstream result;
		result << getDataTypeName(dt) << "(";
		for (int i = 0; i < getDataTypeScalarSize(dt); ++i)
		{
			if (i > 0)
				result << ", ";
			result << elemValue;
		}
		result << ")";
		return result.str();
	}
	else if (isDataTypeScalar(dt))
	{
		return getDataTypeName(dt) + std::string("(") + baseValue + std::string(")");
	}
	else
	{
		DE_ASSERT(0);
		return std::string();
	}
}

void ZeroTest::initPrograms(SourceCollections& sourceCollections) const
{
	using namespace glu;

	std::ostringstream src;

	src << "#version 450\n"
		<< "#extension GL_EXT_shared_memory_block : enable\n"
		<< "#extension GL_EXT_shader_explicit_arithmetic_types : enable\n"
		<< "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n";

	// Large enough to cover the largest B block even if just 8-bit elements.
	// Small enough to fit in the minimum shared memory size limit even if with uvec4.
	src << "shared A { " << getDataTypeName(m_caseDef.zeroElementType) << " arr[256]; } zero;\n";

	src << "struct st {\n"
		<< "    " << getDataTypeName(m_caseDef.fieldType[0]) << " x;\n";
	if (m_caseDef.fieldType[1])
		src << "    " << getDataTypeName(m_caseDef.fieldType[1]) << " y;\n";
	src << "};\n";


	src << "shared B { st arr[4]; };\n"
		<< "layout(set = 0, binding = 0) buffer Result { uint result; };\n"
		<< "void main() {\n"
		<< "for (int i = 0; i < zero.arr.length(); i++) {\n"
		<< "    zero.arr[i] = " << getDataTypeLiteral(m_caseDef.zeroElementType, "1") << ";\n"
		<< "  }\n"
		<< "  for (int i = 0; i < zero.arr.length(); i++) {\n"
		<< "    zero.arr[i] = " << getDataTypeLiteral(m_caseDef.zeroElementType, "0") << ";\n"
		<< "  }\n"
		<< "  result = (\n";

	for (deUint32 i = 0; i < 4; i++)
	{
		src << "    ";
		if (i > 0)
			src << "&& ";
		src << "(arr[" << de::toString(i) << "].x == " << getDataTypeLiteral(m_caseDef.fieldType[0], "0") << ")\n";
		if (m_caseDef.fieldType[1])
			src << "    && (arr[" << de::toString(i) << "].y == " << getDataTypeLiteral(m_caseDef.fieldType[1], "0") << ")\n";
	}

	src << "  ) ? 0 : 0xFF;\n"
		<< "}\n";

	sourceCollections.glslSources.add("comp")
		<< ComputeSource(src.str())
		<< vk::ShaderBuildOptions(sourceCollections.usedVulkanVersion, vk::SPIRV_VERSION_1_4,
								  vk::ShaderBuildOptions::Flags(0u));
}

bool isTestedZeroElementType(glu::DataType dt)
{
	using namespace glu;

	// Select only a few interesting types.
	switch (dt)
	{
	case TYPE_UINT:
	case TYPE_UINT_VEC4:
	case TYPE_UINT8:
	case TYPE_UINT8_VEC4:
	case TYPE_UINT16:
		return true;
	default:
		return false;
	}
}

bool isTestedFieldType(glu::DataType dt)
{
	using namespace glu;

	// Select only a few interesting types.
	switch (dt)
	{
	case TYPE_UINT:
	case TYPE_UINT_VEC3:
	case TYPE_UINT8:
	case TYPE_UINT16:
	case TYPE_FLOAT:
	case TYPE_FLOAT_VEC4:
	case TYPE_FLOAT16:
	case TYPE_DOUBLE:
	case TYPE_DOUBLE_VEC4:
	case TYPE_BOOL:
		return true;

	default:
		return false;
	}
}

void AddZeroTests(tcu::TestCaseGroup* group)
{
	using namespace glu;

	ZeroTest::CaseDef c;

	for (deUint32 i = 0; i < TYPE_LAST; ++i)
	{
		c.zeroElementType = DataType(i);

		if (isTestedZeroElementType(c.zeroElementType))
		{
			deUint32 idx[2] = { 0, 0 };

			while (idx[1] < TYPE_LAST && idx[0] < TYPE_LAST)
			{
				c.fieldType[0] = DataType(idx[0]);
				c.fieldType[1] = DataType(idx[1]);

				if (isTestedFieldType(c.fieldType[0]) &&
					(c.fieldType[1] == TYPE_INVALID || isTestedFieldType(c.fieldType[1])))
				{
					for (deUint32 elements = 1; elements <= 4; ++elements)
					{
						c.elements = elements;
						group->addChild(new ZeroTest(group->getTestContext(), c));
					}
				}

				idx[0]++;
				if (idx[0] >= TYPE_LAST)
				{
					idx[1]++;
					idx[0] = 0;
				}
			}
		}
	}
}

class PaddingTest : public vkt::TestCase
{
public:
	struct CaseDef
	{
		std::vector<glu::DataType> types;
		std::vector<deUint32> offsets;
		std::vector<std::string> values;
		deUint32 expected[32];

		std::string testName() const
		{
			DE_ASSERT(types.size() > 0);
			DE_ASSERT(types.size() == offsets.size());
			DE_ASSERT(types.size() == values.size());

			std::string name;
			for (deUint32 i = 0; i < types.size(); ++i)
			{
				if (i > 0)
					name += "_";
				name += glu::getDataTypeName(types[i]);
				name += "_" + de::toString(offsets[i]);
			}
			return name;
		}

		void add(glu::DataType dt, deUint32 offset, const std::string& v)
		{
			types.push_back(dt);
			offsets.push_back(offset);
			values.push_back(v);
		}

		bool needsScalar() const
		{
			for (deUint32 i = 0; i < offsets.size(); ++i)
			{
				if (offsets[i] % 4 != 0)
					return true;
			}
			return false;
		}
	};

	PaddingTest(tcu::TestContext& testCtx, const CaseDef& caseDef)
		: TestCase(testCtx, caseDef.testName(), caseDef.testName()),
		m_caseDef(caseDef)
	{
	}

	virtual void checkSupport(Context& context) const;
	void initPrograms(SourceCollections& sourceCollections) const;

	class Instance : public vkt::TestInstance
	{
	public:
		Instance(Context& context, const CaseDef& caseDef)
			: TestInstance(context),
			  m_caseDef(caseDef)
		{
		}

		tcu::TestStatus iterate(void)
		{
			return runCompute(m_context, 1u);
		}

	private:
		CaseDef m_caseDef;
	};

	TestInstance* createInstance(Context& context) const
	{
		return new Instance(context, m_caseDef);
	}

private:
	CaseDef m_caseDef;
};

void PaddingTest::checkSupport(Context& context) const
{
	CheckSupportParams p;
	deMemset(&p, 0, sizeof(p));

	for (deUint32 i = 0; i < m_caseDef.types.size(); ++i)
		p.useType(m_caseDef.types[i]);

	p.needsScalar = m_caseDef.needsScalar();

	checkSupportWithParams(context, p);
}

void PaddingTest::initPrograms(SourceCollections& sourceCollections) const
{
	using namespace glu;

	std::ostringstream src;

	src << "#version 450\n"
		<< "#extension GL_EXT_shared_memory_block : enable\n"
		<< "#extension GL_EXT_shader_explicit_arithmetic_types : enable\n"
		<< "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n";

	src	<< "shared A { uint32_t words[32]; };\n";

	if (m_caseDef.needsScalar())
	{
		src << "#extension GL_EXT_scalar_block_layout : enable\n"
			<< "layout (scalar) ";
	}

	src << "shared B {\n";

	for (deUint32 i = 0; i < m_caseDef.types.size(); ++i)
	{
		src << "  layout(offset = " << m_caseDef.offsets[i] << ") "
			<< glu::getDataTypeName(m_caseDef.types[i]) << " x" << i << ";\n";
	}

	src	<< "};\n"
		<< "layout(set = 0, binding = 0) buffer Result { uint result; };\n";

	src	<< "void main() {\n"
		<< "for (int i = 0; i < 32; i++) words[i] = 0;\n";

	for (deUint32 i = 0; i < m_caseDef.values.size(); ++i)
		src << "x" << i << " = " << m_caseDef.values[i] << ";\n";

	src << "result = 32;\n";
	for (deUint32 i = 0; i < 32; ++i)
	{
		src	<< "if (words[" << std::dec << i << "] == 0x"
			<< std::uppercase << std::hex << m_caseDef.expected[i]
			<< ") result--;\n";
	}

	src << "}\n";

	sourceCollections.glslSources.add("comp")
		<< ComputeSource(src.str())
		<< vk::ShaderBuildOptions(sourceCollections.usedVulkanVersion, vk::SPIRV_VERSION_1_4,
								  vk::ShaderBuildOptions::Flags(0u));
}

void AddPaddingTests(tcu::TestCaseGroup* group)
{
	using namespace glu;

	for (deUint32 i = 0; i < 31; ++i)
	{
		for (deUint32 j = i + 1; j < 32; j += 4)
		{
			PaddingTest::CaseDef c;
			deMemset(&c, 0, sizeof(c));

			c.add(TYPE_UINT, 4 * i, "0x1234");
			c.expected[i] = 0x1234;

			c.add(TYPE_UINT, 4 * j, "0x5678");
			c.expected[j] = 0x5678;

			group->addChild(new PaddingTest(group->getTestContext(), c));
		}
	}

	for (deUint32 i = 0; i < 127; ++i)
	{
		for (deUint32 j = i + 1; j < 32; j += 16)
		{
			PaddingTest::CaseDef c;
			deMemset(&c, 0, sizeof(c));

			deUint8* expected = reinterpret_cast<deUint8*>(c.expected);

			c.add(TYPE_UINT8, i, "uint8_t(0xAA)");
			expected[i] = 0xAA;

			c.add(TYPE_UINT8, j, "uint8_t(0xBB)");
			expected[j] = 0xBB;

			group->addChild(new PaddingTest(group->getTestContext(), c));
		}
	}
}

class SizeTest : public vkt::TestCase
{
public:
	SizeTest(tcu::TestContext& testCtx, deUint32 size)
		: TestCase(testCtx, de::toString(size), de::toString(size))
		, m_size(size)
	{
		DE_ASSERT(size % 8 == 0);
	}

	virtual void checkSupport(Context& context) const;
	void initPrograms(SourceCollections& sourceCollections) const;

	class Instance : public vkt::TestInstance
	{
	public:
		Instance(Context& context)
			: TestInstance(context)
		{
		}

		tcu::TestStatus iterate(void)
		{
			return runCompute(m_context, 1u);
		}
	};

	TestInstance* createInstance(Context& context) const
	{
		return new Instance(context);
	}

private:
	deUint32 m_size;
};

void SizeTest::checkSupport(Context& context) const
{
	context.requireDeviceFunctionality("VK_KHR_workgroup_memory_explicit_layout");
	context.requireDeviceFunctionality("VK_KHR_spirv_1_4");

	if (context.getDeviceProperties().limits.maxComputeSharedMemorySize < m_size)
		TCU_THROW(NotSupportedError, "Not enough shared memory supported.");
}

void SizeTest::initPrograms(SourceCollections& sourceCollections) const
{
	using namespace glu;

	std::ostringstream src;

	src << "#version 450\n";
	src << "#extension GL_EXT_shared_memory_block : enable\n";
	src << "#extension GL_EXT_shader_explicit_arithmetic_types : enable\n";
	src << "layout(local_size_x = 8, local_size_y = 1, local_size_z = 1) in;\n";

	for (deUint32 i = 0; i < 8; ++i)
		src << "shared B" << i << " { uint32_t words[" << (m_size / 4) << "]; } b" << i << ";\n";

	src << "layout(set = 0, binding = 0) buffer Result { uint result; };\n";

	src	<< "void main() {\n";
	src << "  int index = int(gl_LocalInvocationIndex);\n";
	src << "  int size = " << (m_size / 4) << ";\n";

	src << "  if (index == 0) for (int x = 0; x < size; x++) b0.words[x] = 0xFFFF;\n";
	src << "  barrier();\n";

	src << "  for (int x = 0; x < size; x++) {\n";
	src << "    if (x % 8 != index) continue;\n";
	for (deUint32 i = 0; i < 8; ++i)
		src << "    if (index == " << i << ") b" << i << ".words[x] = (x << 3) | " << i << ";\n";
	src << "  }\n";

	src << "  barrier();\n";
	src << "  if (index != 0) return;\n";

	src << "  int r = size;\n";
	src << "  for (int x = 0; x < size; x++) {\n";
	src << "    int expected = (x << 3) | (x % 8);\n";
	src << "    if (b0.words[x] == expected) r--;\n";
	src << "  }\n";
	src << "  result = r;\n";
	src << "}\n";

	sourceCollections.glslSources.add("comp")
		<< ComputeSource(src.str())
		<< vk::ShaderBuildOptions(sourceCollections.usedVulkanVersion, vk::SPIRV_VERSION_1_4,
								  vk::ShaderBuildOptions::Flags(0u));
}

void AddSizeTests(tcu::TestCaseGroup* group)
{
	deUint32 sizes[] =
	{
		8u,
		64u,
		4096u,

		// Dynamic generation of shaders based on properties reported
		// by devices is not allowed in the CTS, so let's create a few
		// variants based on common known maximum sizes.
		16384u,
		32768u,
		49152u,
		65536u,
	};

	for (deUint32 i = 0; i < DE_LENGTH_OF_ARRAY(sizes); ++i)
		group->addChild(new SizeTest(group->getTestContext(), sizes[i]));
}

cts_amber::AmberTestCase* CreateAmberTestCase(tcu::TestContext& testCtx,
											  const char* name,
											  const char* description,
											  const std::string& filename,
											  const std::vector<std::string>& requirements = std::vector<std::string>())
{
	vk::SpirVAsmBuildOptions asm_options(VK_MAKE_VERSION(1, 1, 0), vk::SPIRV_VERSION_1_4);
	asm_options.supports_VK_KHR_spirv_1_4 = true;

	cts_amber::AmberTestCase *t = cts_amber::createAmberTestCase(testCtx, name, description, "compute/workgroup_memory_explicit_layout", filename, requirements);
	t->setSpirVAsmBuildOptions(asm_options);
	t->addRequirement("VK_KHR_workgroup_memory_explicit_layout");
	return t;
}

void AddCopyMemoryTests(tcu::TestCaseGroup* group)
{
	tcu::TestContext& testCtx = group->getTestContext();

	group->addChild(CreateAmberTestCase(testCtx, "basic", "", "copy_memory_basic.amber"));
	group->addChild(CreateAmberTestCase(testCtx, "two_invocations", "", "copy_memory_two_invocations.amber"));
	group->addChild(CreateAmberTestCase(testCtx, "variable_pointers", "", "copy_memory_variable_pointers.amber",
										{ "VariablePointerFeatures.variablePointers" }));
}

} // anonymous

tcu::TestCaseGroup* createWorkgroupMemoryExplicitLayoutTests(tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> tests(new tcu::TestCaseGroup(testCtx, "workgroup_memory_explicit_layout", "VK_KHR_workgroup_memory_explicit_layout tests"));

	tcu::TestCaseGroup* alias = new tcu::TestCaseGroup(testCtx, "alias", "Aliasing between different blocks and types");
	AddAliasTests(alias);
	tests->addChild(alias);

	tcu::TestCaseGroup* zero = new tcu::TestCaseGroup(testCtx, "zero", "Manually zero initialize a block and read from another");
	AddZeroTests(zero);
	tests->addChild(zero);

	tcu::TestCaseGroup* padding = new tcu::TestCaseGroup(testCtx, "padding", "Padding as part of the explicit layout");
	AddPaddingTests(padding);
	tests->addChild(padding);

	tcu::TestCaseGroup* size = new tcu::TestCaseGroup(testCtx, "size", "Test blocks of various sizes");
	AddSizeTests(size);
	tests->addChild(size);

	tcu::TestCaseGroup* copy_memory = new tcu::TestCaseGroup(testCtx, "copy_memory", "Test OpCopyMemory with Workgroup memory");
	AddCopyMemoryTests(copy_memory);
	tests->addChild(copy_memory);

	return tests.release();
}

} // compute
} // vkt
