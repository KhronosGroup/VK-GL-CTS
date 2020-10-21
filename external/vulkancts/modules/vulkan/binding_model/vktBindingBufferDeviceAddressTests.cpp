/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017-2019 The Khronos Group Inc.
 * Copyright (c) 2018-2019 NVIDIA Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Tests for VK_EXT_buffer_device_address.
 *//*--------------------------------------------------------------------*/

#include "vktBindingBufferDeviceAddressTests.hpp"

#include "vkBufferWithMemory.hpp"
#include "vkImageWithMemory.hpp"
#include "vkQueryUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkObjUtil.hpp"

#include "vktTestGroupUtil.hpp"
#include "vktTestCase.hpp"

#include "deDefs.h"
#include "deMath.h"
#include "deRandom.h"
#include "deRandom.hpp"
#include "deSharedPtr.hpp"
#include "deString.h"

#include "tcuTestCase.hpp"
#include "tcuTestLog.hpp"

#include <string>
#include <sstream>

namespace vkt
{
namespace BindingModel
{
namespace
{
using namespace vk;
using namespace std;

typedef de::MovePtr<Unique<VkBuffer> >	VkBufferSp;
typedef de::MovePtr<Allocation>			AllocationSp;

static const deUint32 DIM = 8;

typedef enum
{
	BASE_UBO = 0,
	BASE_SSBO,
} Base;

#define ENABLE_RAYTRACING 0

typedef enum
{
	STAGE_COMPUTE = 0,
	STAGE_VERTEX,
	STAGE_FRAGMENT,
	STAGE_RAYGEN,
} Stage;

typedef enum
{
	BT_SINGLE = 0,
	BT_MULTI,
	BT_REPLAY,
} BufType;

typedef enum
{
	LAYOUT_STD140 = 0,
	LAYOUT_SCALAR,
} Layout;

typedef enum
{
	CONVERT_NONE = 0,
	CONVERT_UINT64,
	CONVERT_UVEC2,
	CONVERT_U64CMP,
	CONVERT_UVEC2CMP,
	CONVERT_UVEC2TOU64,
	CONVERT_U64TOUVEC2,
} Convert;

struct CaseDef
{
	deUint32 set;
	deUint32 depth;
	Base base;
	Stage stage;
	Convert convertUToPtr;
	bool storeInLocal;
	BufType bufType;
	Layout layout;
};

class BufferAddressTestInstance : public TestInstance
{
public:
						BufferAddressTestInstance	(Context& context, const CaseDef& data);
						~BufferAddressTestInstance	(void);
	tcu::TestStatus		iterate						(void);
	virtual	void		fillBuffer					(const std::vector<deUint8 *>& cpuAddrs,
													 const std::vector<deUint64>& gpuAddrs,
													 deUint32 bufNum, deUint32 curDepth) const;
private:
	CaseDef				m_data;

	enum
	{
		WIDTH = 256,
		HEIGHT = 256
	};
};

BufferAddressTestInstance::BufferAddressTestInstance (Context& context, const CaseDef& data)
	: vkt::TestInstance		(context)
	, m_data				(data)
{
}

BufferAddressTestInstance::~BufferAddressTestInstance (void)
{
}

class BufferAddressTestCase : public TestCase
{
	public:
							BufferAddressTestCase	(tcu::TestContext& context, const char* name, const char* desc, const CaseDef data);
							~BufferAddressTestCase	(void);
	virtual	void			initPrograms			(SourceCollections& programCollection) const;
	virtual TestInstance*	createInstance			(Context& context) const;
	virtual void			checkSupport			(Context& context) const;
	virtual	void			checkBuffer				(std::stringstream& checks, deUint32 bufNum, deUint32 curDepth, const std::string &prefix) const;

private:
	CaseDef					m_data;
};

BufferAddressTestCase::BufferAddressTestCase (tcu::TestContext& context, const char* name, const char* desc, const CaseDef data)
	: vkt::TestCase	(context, name, desc)
	, m_data		(data)
{
}

BufferAddressTestCase::~BufferAddressTestCase	(void)
{
}

void BufferAddressTestCase::checkSupport (Context& context) const
{
	if (!context.isBufferDeviceAddressSupported())
		TCU_THROW(NotSupportedError, "Physical storage buffer pointers not supported");

	if (m_data.stage == STAGE_VERTEX && !context.getDeviceFeatures().vertexPipelineStoresAndAtomics)
		TCU_THROW(NotSupportedError, "Vertex pipeline stores and atomics not supported");

	if (m_data.set >= context.getDeviceProperties().limits.maxBoundDescriptorSets)
		TCU_THROW(NotSupportedError, "descriptor set number not supported");

	bool isBufferDeviceAddressWithCaptureReplaySupported =
			(context.isDeviceFunctionalitySupported("VK_KHR_buffer_device_address") && context.getBufferDeviceAddressFeatures().bufferDeviceAddressCaptureReplay) ||
			(context.isDeviceFunctionalitySupported("VK_EXT_buffer_device_address") && context.getBufferDeviceAddressFeaturesEXT().bufferDeviceAddressCaptureReplay);
	if (m_data.bufType == BT_REPLAY && !isBufferDeviceAddressWithCaptureReplaySupported)
		TCU_THROW(NotSupportedError, "Capture/replay of physical storage buffer pointers not supported");

	if (m_data.layout == LAYOUT_SCALAR && !context.getScalarBlockLayoutFeatures().scalarBlockLayout)
		TCU_THROW(NotSupportedError, "Scalar block layout not supported");

#if ENABLE_RAYTRACING
	if (m_data.stage == STAGE_RAYGEN &&
		!context.isDeviceFunctionalitySupported("VK_NV_ray_tracing"))
	{
		TCU_THROW(NotSupportedError, "Ray tracing not supported");
	}
#endif

	const bool needsInt64	= (	m_data.convertUToPtr == CONVERT_UINT64		||
								m_data.convertUToPtr == CONVERT_U64CMP		||
								m_data.convertUToPtr == CONVERT_U64TOUVEC2	||
								m_data.convertUToPtr == CONVERT_UVEC2TOU64	);

	const bool needsKHR		= (	m_data.convertUToPtr == CONVERT_UVEC2		||
								m_data.convertUToPtr == CONVERT_UVEC2CMP	||
								m_data.convertUToPtr == CONVERT_U64TOUVEC2	||
								m_data.convertUToPtr == CONVERT_UVEC2TOU64	);

	if (needsInt64 && !context.getDeviceFeatures().shaderInt64)
		TCU_THROW(NotSupportedError, "Int64 not supported");
	if (needsKHR && !context.isDeviceFunctionalitySupported("VK_KHR_buffer_device_address"))
		TCU_THROW(NotSupportedError, "VK_KHR_buffer_device_address not supported");
}

void BufferAddressTestCase::checkBuffer (std::stringstream& checks, deUint32 bufNum, deUint32 curDepth, const std::string &prefix) const
{
	string newPrefix = prefix;
	if (curDepth > 0)
	{
		if (m_data.convertUToPtr == CONVERT_UINT64 || m_data.convertUToPtr == CONVERT_UVEC2TOU64)
			newPrefix = "T1(uint64_t(T1(" + newPrefix + ")))";
		else if (m_data.convertUToPtr == CONVERT_UVEC2 || m_data.convertUToPtr == CONVERT_U64TOUVEC2)
			newPrefix = "T1(uvec2(T1(" + newPrefix + ")))";
	}

	if (m_data.storeInLocal && curDepth != 0)
	{
		std::string localName = "l" + de::toString(bufNum);
		checks << "   " << ((bufNum & 1) ? "restrict " : "") << "T1 " << localName << " = " << newPrefix << ";\n";
		newPrefix = localName;
	}

	checks << "   accum |= " << newPrefix << ".a[0] - " << bufNum*3+0 << ";\n";
	checks << "   accum |= " << newPrefix << ".a[pc.identity[1]] - " << bufNum*3+1 << ";\n";
	checks << "   accum |= " << newPrefix << ".b - " << bufNum*3+2 << ";\n";
	checks << "   accum |= int(" << newPrefix << ".e[0][0] - " << bufNum*3+3 << ");\n";
	checks << "   accum |= int(" << newPrefix << ".e[0][1] - " << bufNum*3+5 << ");\n";
	checks << "   accum |= int(" << newPrefix << ".e[1][0] - " << bufNum*3+4 << ");\n";
	checks << "   accum |= int(" << newPrefix << ".e[1][1] - " << bufNum*3+6 << ");\n";

	if (m_data.layout == LAYOUT_SCALAR)
	{
		checks << "   f = " << newPrefix << ".f;\n";
		checks << "   accum |= f.x - " << bufNum*3+7 << ";\n";
		checks << "   accum |= f.y - " << bufNum*3+8 << ";\n";
		checks << "   accum |= f.z - " << bufNum*3+9 << ";\n";
	}

	const std::string localPrefix = "l" + de::toString(bufNum);

	if (m_data.convertUToPtr == CONVERT_U64CMP || m_data.convertUToPtr == CONVERT_UVEC2CMP)
	{
		const std::string type = ((m_data.convertUToPtr == CONVERT_U64CMP) ? "uint64_t" : "uvec2");

		checks << "   " << type << " " << localPrefix << "c0 = " << type << "(" << newPrefix << ".c[0]);\n";
		checks << "   " << type << " " << localPrefix << "c1 = " << type << "(" << newPrefix << ".c[pc.identity[1]]);\n";
		checks << "   " << type << " " << localPrefix << "d  = " << type << "(" << newPrefix << ".d);\n";
	}

	if (curDepth != m_data.depth)
	{
		// Check non-null pointers and inequality among them.
		if (m_data.convertUToPtr == CONVERT_U64CMP)
		{
			checks << "   if (" << localPrefix << "c0 == zero ||\n"
				   << "       " << localPrefix << "c1 == zero ||\n"
				   << "       " << localPrefix << "d  == zero ||\n"
				   << "       " << localPrefix << "c0 == " << localPrefix << "c1 ||\n"
				   << "       " << localPrefix << "c1 == " << localPrefix << "d  ||\n"
				   << "       " << localPrefix << "c0 == " << localPrefix << "d  ) {\n"
				   << "     accum |= 1;\n"
				   << "   }\n";
		}
		else if (m_data.convertUToPtr == CONVERT_UVEC2CMP)
		{
			checks << "   if (all(equal(" << localPrefix << "c0, zero)) ||\n"
				   << "       all(equal(" << localPrefix << "c1, zero)) ||\n"
				   << "       all(equal(" << localPrefix << "d , zero)) ||\n"
				   << "       all(equal(" << localPrefix << "c0, " << localPrefix << "c1)) ||\n"
				   << "       all(equal(" << localPrefix << "c1, " << localPrefix << "d )) ||\n"
				   << "       all(equal(" << localPrefix << "c0, " << localPrefix << "d )) ) {\n"
				   << "     accum |= 1;\n"
				   << "   }\n";
		}

		checkBuffer(checks, bufNum*3+1, curDepth+1, newPrefix + ".c[0]");
		checkBuffer(checks, bufNum*3+2, curDepth+1, newPrefix + ".c[pc.identity[1]]");
		checkBuffer(checks, bufNum*3+3, curDepth+1, newPrefix + ".d");
	}
	else
	{
		// Check null pointers nonexplicitly.
		if (m_data.convertUToPtr == CONVERT_U64CMP)
		{
			checks << "   if (!(" << localPrefix << "c0 == " << localPrefix << "c1 &&\n"
				   << "         " << localPrefix << "c1 == " << localPrefix << "d  &&\n"
				   << "         " << localPrefix << "c0 == " << localPrefix << "d  )) {\n"
				   << "     accum |= 1;\n"
				   << "   }\n";
		}
		else if (m_data.convertUToPtr == CONVERT_UVEC2CMP)
		{
			checks << "   if (!(all(equal(" << localPrefix << "c0, " << localPrefix << "c1)) &&\n"
				   << "         all(equal(" << localPrefix << "c1, " << localPrefix << "d )) &&\n"
				   << "         all(equal(" << localPrefix << "c0, " << localPrefix << "d )) )) {\n"
				   << "     accum |= 1;\n"
				   << "   }\n";
		}
	}
}

void BufferAddressTestInstance::fillBuffer (const std::vector<deUint8 *>& cpuAddrs,
											const std::vector<deUint64>& gpuAddrs,
											deUint32 bufNum, deUint32 curDepth) const
{
	deUint8 *buf = cpuAddrs[bufNum];

	deUint32 aStride = m_data.layout == LAYOUT_SCALAR ? 1 : 4; // (in deUint32s)
	deUint32 cStride = m_data.layout == LAYOUT_SCALAR ? 1 : 2; // (in deUint64s)
	deUint32 matStride = m_data.layout == LAYOUT_SCALAR ? 2 : 4; // (in floats)

	// a
	((deUint32 *)(buf+0))[0] = bufNum*3+0;
	((deUint32 *)(buf+0))[aStride] = bufNum*3+1;
	// b
	((deUint32 *)(buf+32))[0] = bufNum*3+2;
	if (m_data.layout == LAYOUT_SCALAR)
	{
		// f
		((deUint32 *)(buf+36))[0] = bufNum*3+7;
		((deUint32 *)(buf+36))[1] = bufNum*3+8;
		((deUint32 *)(buf+36))[2] = bufNum*3+9;
	}
	// e
	((float *)(buf+96))[0] = (float)(bufNum*3+3);
	((float *)(buf+96))[1] = (float)(bufNum*3+4);
	((float *)(buf+96))[matStride] = (float)(bufNum*3+5);
	((float *)(buf+96))[matStride+1] = (float)(bufNum*3+6);

	if (curDepth != m_data.depth)
	{
		// c
		((deUint64 *)(buf+48))[0] = gpuAddrs[bufNum*3+1];
		((deUint64 *)(buf+48))[cStride] = gpuAddrs[bufNum*3+2];
		// d
		((deUint64 *)(buf+80))[0] = gpuAddrs[bufNum*3+3];

		fillBuffer(cpuAddrs, gpuAddrs, bufNum*3+1, curDepth+1);
		fillBuffer(cpuAddrs, gpuAddrs, bufNum*3+2, curDepth+1);
		fillBuffer(cpuAddrs, gpuAddrs, bufNum*3+3, curDepth+1);
	}
	else
	{
		// c
		((deUint64 *)(buf+48))[0] = 0ull;
		((deUint64 *)(buf+48))[cStride] = 0ull;
		// d
		((deUint64 *)(buf+80))[0] = 0ull;
	}
}


void BufferAddressTestCase::initPrograms (SourceCollections& programCollection) const
{
	std::stringstream decls, checks, localDecls;

	std::string baseStorage = m_data.base == BASE_UBO ? "uniform" : "buffer";
	std::string memberStorage = "buffer";

	decls << "layout(r32ui, set = " << m_data.set << ", binding = 0) uniform uimage2D image0_0;\n";
	decls << "layout(buffer_reference) " << memberStorage << " T1;\n";

	std::string refType;
	switch (m_data.convertUToPtr)
	{
	case CONVERT_UINT64:
	case CONVERT_U64TOUVEC2:
		refType = "uint64_t";
		break;

	case CONVERT_UVEC2:
	case CONVERT_UVEC2TOU64:
		refType = "uvec2";
		break;

	default:
		refType = "T1";
		break;
	}

	std::string layout = m_data.layout == LAYOUT_SCALAR ? "scalar" : "std140";
	decls <<
			"layout(set = " << m_data.set << ", binding = 1, " << layout << ") " << baseStorage << " T2 {\n"
			"   layout(offset = 0) int a[2]; // stride = 4 for scalar, 16 for std140\n"
			"   layout(offset = 32) int b;\n"
			<< ((m_data.layout == LAYOUT_SCALAR) ? "   layout(offset = 36) ivec3 f;\n" : "") <<
			"   layout(offset = 48) " << refType << " c[2]; // stride = 8 for scalar, 16 for std140\n"
			"   layout(offset = 80) " << refType << " d;\n"
			"   layout(offset = 96, row_major) mat2 e; // tightly packed for scalar, 16 byte matrix stride for std140\n"
			"} x;\n";
	decls <<
			"layout(buffer_reference, " << layout << ") " << memberStorage << " T1 {\n"
			"   layout(offset = 0) int a[2]; // stride = 4 for scalar, 16 for std140\n"
			"   layout(offset = 32) int b;\n"
			<< ((m_data.layout == LAYOUT_SCALAR) ? "   layout(offset = 36) ivec3 f;\n" : "") <<
			"   layout(offset = 48) " << refType << " c[2]; // stride = 8 for scalar, 16 for std140\n"
			"   layout(offset = 80) " << refType << " d;\n"
			"   layout(offset = 96, row_major) mat2 e; // tightly packed for scalar, 16 byte matrix stride for std140\n"
			"};\n";

	if (m_data.convertUToPtr == CONVERT_U64CMP)
		localDecls << "  uint64_t zero = uint64_t(0);\n";
	else if (m_data.convertUToPtr == CONVERT_UVEC2CMP)
		localDecls << "  uvec2 zero = uvec2(0, 0);\n";

	checkBuffer(checks, 0, 0, "x");

	std::stringstream pushdecl;
	pushdecl << "layout (push_constant, std430) uniform Block { int identity[32]; } pc;\n";

	vk::ShaderBuildOptions::Flags flags = vk::ShaderBuildOptions::Flags(0);
	if (m_data.layout == LAYOUT_SCALAR)
		flags = vk::ShaderBuildOptions::FLAG_ALLOW_SCALAR_OFFSETS;

	// The conversion and comparison in uvec2 form test needs SPIR-V 1.5 for OpBitcast.
	const vk::SpirvVersion spirvVersion = ((m_data.convertUToPtr == CONVERT_UVEC2CMP) ? vk::SPIRV_VERSION_1_5 : vk::SPIRV_VERSION_1_0);

	switch (m_data.stage)
	{
	default: DE_ASSERT(0); // Fallthrough
	case STAGE_COMPUTE:
		{
			std::stringstream css;
			css <<
				"#version 450 core\n"
				"#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable\n"
				"#extension GL_EXT_buffer_reference : enable\n"
				"#extension GL_EXT_scalar_block_layout : enable\n"
				"#extension GL_EXT_buffer_reference_uvec2 : enable\n"
				<< pushdecl.str()
				<< decls.str() <<
				"layout(local_size_x = 1, local_size_y = 1) in;\n"
				"void main()\n"
				"{\n"
				"  int accum = 0, temp;\n"
				"  ivec3 f;\n"
				<< localDecls.str()
				<< checks.str() <<
				"  uvec4 color = (accum != 0) ? uvec4(0,0,0,0) : uvec4(1,0,0,1);\n"
				"  imageStore(image0_0, ivec2(gl_GlobalInvocationID.xy), color);\n"
				"}\n";

			programCollection.glslSources.add("test") << glu::ComputeSource(css.str())
				<< vk::ShaderBuildOptions(programCollection.usedVulkanVersion, spirvVersion, flags);
			break;
		}
#if ENABLE_RAYTRACING
	case STAGE_RAYGEN:
		{
			std::stringstream css;
			css <<
				"#version 460 core\n"
				"#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable\n"
				"#extension GL_EXT_buffer_reference : enable\n"
				"#extension GL_EXT_scalar_block_layout : enable\n"
				"#extension GL_EXT_buffer_reference_uvec2 : enable\n"
				"#extension GL_NV_ray_tracing : require\n"
				<< pushdecl.str()
				<< decls.str() <<
				"void main()\n"
				"{\n"
				"  int accum = 0, temp;\n"
				"  ivec3 f;\n"
				<< localDecls.str()
				<< checks.str() <<
				"  uvec4 color = (accum != 0) ? uvec4(0,0,0,0) : uvec4(1,0,0,1);\n"
				"  imageStore(image0_0, ivec2(gl_LaunchIDNV.xy), color);\n"
				"}\n";

			programCollection.glslSources.add("test") << glu::RaygenSource(css.str())
				<< vk::ShaderBuildOptions(programCollection.usedVulkanVersion, spirvVersion, flags);
			break;
		}
#endif
	case STAGE_VERTEX:
		{
			std::stringstream vss;
			vss <<
				"#version 450 core\n"
				"#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable\n"
				"#extension GL_EXT_buffer_reference : enable\n"
				"#extension GL_EXT_scalar_block_layout : enable\n"
				"#extension GL_EXT_buffer_reference_uvec2 : enable\n"
				<< pushdecl.str()
				<< decls.str()  <<
				"void main()\n"
				"{\n"
				"  int accum = 0, temp;\n"
				"  ivec3 f;\n"
				<< localDecls.str()
				<< checks.str() <<
				"  uvec4 color = (accum != 0) ? uvec4(0,0,0,0) : uvec4(1,0,0,1);\n"
				"  imageStore(image0_0, ivec2(gl_VertexIndex % " << DIM << ", gl_VertexIndex / " << DIM << "), color);\n"
				"  gl_PointSize = 1.0f;\n"
				"}\n";

			programCollection.glslSources.add("test") << glu::VertexSource(vss.str())
				<< vk::ShaderBuildOptions(programCollection.usedVulkanVersion, spirvVersion, flags);
			break;
		}
	case STAGE_FRAGMENT:
		{
			std::stringstream vss;
			vss <<
				"#version 450 core\n"
				"void main()\n"
				"{\n"
				// full-viewport quad
				"  gl_Position = vec4( 2.0*float(gl_VertexIndex&2) - 1.0, 4.0*(gl_VertexIndex&1)-1.0, 1.0 - 2.0 * float(gl_VertexIndex&1), 1);\n"
				"}\n";

			programCollection.glslSources.add("vert") << glu::VertexSource(vss.str());

			std::stringstream fss;
			fss <<
				"#version 450 core\n"
				"#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable\n"
				"#extension GL_EXT_buffer_reference : enable\n"
				"#extension GL_EXT_scalar_block_layout : enable\n"
				"#extension GL_EXT_buffer_reference_uvec2 : enable\n"
				<< pushdecl.str()
				<< decls.str() <<
				"void main()\n"
				"{\n"
				"  int accum = 0, temp;\n"
				"  ivec3 f;\n"
				<< localDecls.str()
				<< checks.str() <<
				"  uvec4 color = (accum != 0) ? uvec4(0,0,0,0) : uvec4(1,0,0,1);\n"
				"  imageStore(image0_0, ivec2(gl_FragCoord.x, gl_FragCoord.y), color);\n"
				"}\n";

			programCollection.glslSources.add("test") << glu::FragmentSource(fss.str())
				<< vk::ShaderBuildOptions(programCollection.usedVulkanVersion, spirvVersion, flags);
			break;
		}
	}

}

TestInstance* BufferAddressTestCase::createInstance (Context& context) const
{
	return new BufferAddressTestInstance(context, m_data);
}

VkBufferCreateInfo makeBufferCreateInfo (const void*				pNext,
										 const VkDeviceSize			bufferSize,
										 const VkBufferUsageFlags	usage,
										 const VkBufferCreateFlags  flags)
{
	const VkBufferCreateInfo bufferCreateInfo =
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// VkStructureType		sType;
		pNext,									// const void*			pNext;
		flags,									// VkBufferCreateFlags	flags;
		bufferSize,								// VkDeviceSize			size;
		usage,									// VkBufferUsageFlags	usage;
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode		sharingMode;
		0u,										// deUint32				queueFamilyIndexCount;
		DE_NULL,								// const deUint32*		pQueueFamilyIndices;
	};
	return bufferCreateInfo;
}

tcu::TestStatus BufferAddressTestInstance::iterate (void)
{
	const InstanceInterface&vki						= m_context.getInstanceInterface();
	const DeviceInterface&	vk						= m_context.getDeviceInterface();
	const VkPhysicalDevice&	physDevice				= m_context.getPhysicalDevice();
	const VkDevice			device					= m_context.getDevice();
	Allocator&				allocator				= m_context.getDefaultAllocator();
	const bool				useKHR					= m_context.isDeviceFunctionalitySupported("VK_KHR_buffer_device_address");


	VkFlags allShaderStages = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	VkFlags allPipelineStages = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

#if ENABLE_RAYTRACING
	if (m_data.stage == STAGE_RAYGEN)
	{
		allShaderStages = VK_SHADER_STAGE_RAYGEN_BIT_NV;
		allPipelineStages = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV;
	}
#endif

	VkPhysicalDeviceProperties2 properties;
	deMemset(&properties, 0, sizeof(properties));
	properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;

#if ENABLE_RAYTRACING
	VkPhysicalDeviceRayTracingPropertiesNV rayTracingProperties;
	deMemset(&rayTracingProperties, 0, sizeof(rayTracingProperties));
	rayTracingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PROPERTIES_NV;

	if (m_context.isDeviceFunctionalitySupported("VK_NV_ray_tracing"))
	{
		properties.pNext = &rayTracingProperties;
	}
#endif

	m_context.getInstanceInterface().getPhysicalDeviceProperties2(m_context.getPhysicalDevice(), &properties);

	VkPipelineBindPoint bindPoint;

	switch (m_data.stage)
	{
	case STAGE_COMPUTE:
		bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
		break;
#if ENABLE_RAYTRACING
	case STAGE_RAYGEN:
		bindPoint = VK_PIPELINE_BIND_POINT_RAY_TRACING_NV;
		break;
#endif
	default:
		bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		break;
	}

	Move<vk::VkDescriptorPool>	descriptorPool;
	Move<vk::VkDescriptorSet>	descriptorSet;

	VkDescriptorPoolCreateFlags poolCreateFlags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

	VkDescriptorSetLayoutBinding bindings[2];
	bindings[0].binding = 0;
	bindings[0].stageFlags = allShaderStages;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[0].descriptorCount = 1;
	bindings[1].binding = 1;
	bindings[1].stageFlags = allShaderStages;
	bindings[1].descriptorType = m_data.base == BASE_UBO ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[1].descriptorCount = 1;

	// Create a layout and allocate a descriptor set for it.
	VkDescriptorSetLayoutCreateInfo setLayoutCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		DE_NULL,

		0,
		(deUint32)2,
		&bindings[0]
	};

	Move<vk::VkDescriptorSetLayout>	descriptorSetLayout = vk::createDescriptorSetLayout(vk, device, &setLayoutCreateInfo);

	setLayoutCreateInfo.bindingCount = 0;
	Move<vk::VkDescriptorSetLayout>	emptyDescriptorSetLayout = vk::createDescriptorSetLayout(vk, device, &setLayoutCreateInfo);

	vk::DescriptorPoolBuilder poolBuilder;
	poolBuilder.addType(bindings[1].descriptorType, 1);
	poolBuilder.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1);

	descriptorPool = poolBuilder.build(vk, device, poolCreateFlags, 1u);
	descriptorSet = makeDescriptorSet(vk, device, *descriptorPool, *descriptorSetLayout);

	VkDeviceSize	align = de::max(de::max(properties.properties.limits.minUniformBufferOffsetAlignment,
											properties.properties.limits.minStorageBufferOffsetAlignment),
											(VkDeviceSize)128 /*sizeof(T1)*/);

	deUint32 numBindings = 1;
	for (deUint32 d = 0; d < m_data.depth; ++d)
	{
		numBindings = numBindings*3+1;
	}

	VkBufferDeviceAddressCreateInfoEXT addressCreateInfoEXT =
	{
		VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_CREATE_INFO_EXT,	// VkStructureType	 sType;
		DE_NULL,													// const void*		 pNext;
		0x000000000ULL,												// VkDeviceSize		 deviceAddress
	};

	VkBufferOpaqueCaptureAddressCreateInfo bufferOpaqueCaptureAddressCreateInfo =
	{
		VK_STRUCTURE_TYPE_BUFFER_OPAQUE_CAPTURE_ADDRESS_CREATE_INFO,	// VkStructureType	 sType;
		DE_NULL,														// const void*		 pNext;
		0x000000000ULL,													// VkDeviceSize		 opaqueCaptureAddress
	};

	std::vector<deUint8 *> cpuAddrs(numBindings);
	std::vector<VkDeviceAddress> gpuAddrs(numBindings);
	std::vector<deUint64> opaqueBufferAddrs(numBindings);
	std::vector<deUint64> opaqueMemoryAddrs(numBindings);

	VkBufferDeviceAddressInfo bufferDeviceAddressInfo =
	{
		VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,	// VkStructureType	 sType;
		DE_NULL,										// const void*		 pNext;
		0,												// VkBuffer			 buffer
	};

	VkDeviceMemoryOpaqueCaptureAddressInfo deviceMemoryOpaqueCaptureAddressInfo =
	{
		VK_STRUCTURE_TYPE_DEVICE_MEMORY_OPAQUE_CAPTURE_ADDRESS_INFO,	// VkStructureType	 sType;
		DE_NULL,														// const void*		 pNext;
		0,																// VkDeviceMemory	 memory;
	};

	bool multiBuffer = m_data.bufType != BT_SINGLE;
	deUint32 numBuffers = multiBuffer ? numBindings : 1;
	VkDeviceSize bufferSize = multiBuffer ? align : (align*numBindings);

	vector<VkBufferSp>			buffers(numBuffers);
	vector<AllocationSp>		allocations(numBuffers);

	VkBufferCreateInfo			bufferCreateInfo = makeBufferCreateInfo(DE_NULL, bufferSize,
														VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
														VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
														VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
														m_data.bufType == BT_REPLAY ? VK_BUFFER_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT : 0);

	// VkMemoryAllocateFlags to be filled out later
	VkMemoryAllocateFlagsInfo	allocFlagsInfo =
	{
		VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,	//	VkStructureType	sType
		DE_NULL,										//	const void*		pNext
		0,												//	VkMemoryAllocateFlags    flags
		0,												//	uint32_t                 deviceMask
	};

	VkMemoryOpaqueCaptureAddressAllocateInfo memoryOpaqueCaptureAddressAllocateInfo =
	{
		VK_STRUCTURE_TYPE_MEMORY_OPAQUE_CAPTURE_ADDRESS_ALLOCATE_INFO,	// VkStructureType    sType;
		DE_NULL,														// const void*        pNext;
		0,																// uint64_t           opaqueCaptureAddress;
	};

	if (useKHR)
		allocFlagsInfo.flags |= VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

	if (useKHR && m_data.bufType == BT_REPLAY)
	{
		allocFlagsInfo.flags |= VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT;
		allocFlagsInfo.pNext = &memoryOpaqueCaptureAddressAllocateInfo;
	}

	for (deUint32 i = 0; i < numBuffers; ++i)
	{
		buffers[i] = VkBufferSp(new Unique<VkBuffer>(createBuffer(vk, device, &bufferCreateInfo)));

		// query opaque capture address before binding memory
		if (useKHR)
		{
			bufferDeviceAddressInfo.buffer = **buffers[i];
			opaqueBufferAddrs[i] = vk.getBufferOpaqueCaptureAddress(device, &bufferDeviceAddressInfo);
		}

		allocations[i] = AllocationSp(allocateExtended(vki, vk, physDevice, device, getBufferMemoryRequirements(vk, device, **buffers[i]), MemoryRequirement::HostVisible, &allocFlagsInfo));

		if (useKHR)
		{
			deviceMemoryOpaqueCaptureAddressInfo.memory = allocations[i]->getMemory();
			opaqueMemoryAddrs[i] = vk.getDeviceMemoryOpaqueCaptureAddress(device, &deviceMemoryOpaqueCaptureAddressInfo);
		}

		VK_CHECK(vk.bindBufferMemory(device, **buffers[i], allocations[i]->getMemory(), 0));
	}

	if (m_data.bufType == BT_REPLAY)
	{
		for (deUint32 i = 0; i < numBuffers; ++i)
		{
			bufferDeviceAddressInfo.buffer = **buffers[i];
			if (useKHR)
				gpuAddrs[i] = vk.getBufferDeviceAddress(device, &bufferDeviceAddressInfo);
			else
				gpuAddrs[i] = vk.getBufferDeviceAddressEXT(device, &bufferDeviceAddressInfo);
		}
		buffers.clear();
		buffers.resize(numBuffers);
		allocations.clear();
		allocations.resize(numBuffers);

		bufferCreateInfo.pNext = useKHR ? (void *)&bufferOpaqueCaptureAddressCreateInfo : (void *)&addressCreateInfoEXT;

		for (deInt32 i = numBuffers-1; i >= 0; --i)
		{
			addressCreateInfoEXT.deviceAddress = gpuAddrs[i];
			bufferOpaqueCaptureAddressCreateInfo.opaqueCaptureAddress = opaqueBufferAddrs[i];
			memoryOpaqueCaptureAddressAllocateInfo.opaqueCaptureAddress = opaqueMemoryAddrs[i];

			buffers[i] = VkBufferSp(new Unique<VkBuffer>(createBuffer(vk, device, &bufferCreateInfo)));
			allocations[i] = AllocationSp(allocateExtended(vki, vk, physDevice, device, getBufferMemoryRequirements(vk, device, **buffers[i]), MemoryRequirement::HostVisible, &allocFlagsInfo));
			VK_CHECK(vk.bindBufferMemory(device, **buffers[i], allocations[i]->getMemory(), 0));

			bufferDeviceAddressInfo.buffer = **buffers[i];
			VkDeviceSize newAddr;
			if (useKHR)
				newAddr = vk.getBufferDeviceAddress(device, &bufferDeviceAddressInfo);
			else
				newAddr = vk.getBufferDeviceAddressEXT(device, &bufferDeviceAddressInfo);
			if (newAddr != gpuAddrs[i])
				return tcu::TestStatus(QP_TEST_RESULT_FAIL, "address mismatch");
		}
	}

	// Create a buffer and compute the address for each "align" bytes.
	for (deUint32 i = 0; i < numBindings; ++i)
	{
		bufferDeviceAddressInfo.buffer = **buffers[multiBuffer ? i : 0];

		if (useKHR)
			gpuAddrs[i] = vk.getBufferDeviceAddress(device, &bufferDeviceAddressInfo);
		else
			gpuAddrs[i] = vk.getBufferDeviceAddressEXT(device, &bufferDeviceAddressInfo);
		cpuAddrs[i] = (deUint8 *)allocations[multiBuffer ? i : 0]->getHostPtr();
		if (!multiBuffer)
		{
			cpuAddrs[i] = cpuAddrs[i] + align*i;
			gpuAddrs[i] = gpuAddrs[i] + align*i;
		}
		//printf("addr 0x%08x`%08x\n", (unsigned)(gpuAddrs[i]>>32), (unsigned)(gpuAddrs[i]));
	}

	fillBuffer(cpuAddrs, gpuAddrs, 0, 0);

	for (deUint32 i = 0; i < numBuffers; ++i)
		flushAlloc(vk, device, *allocations[i]);

	const VkQueue					queue					= m_context.getUniversalQueue();
	Move<VkCommandPool>				cmdPool					= createCommandPool(vk, device, 0, m_context.getUniversalQueueFamilyIndex());
	Move<VkCommandBuffer>			cmdBuffer				= allocateCommandBuffer(vk, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	beginCommandBuffer(vk, *cmdBuffer, 0u);

	// Push constants are used for dynamic indexing. PushConstant[i] = i.

	const VkPushConstantRange pushConstRange =
	{
		allShaderStages,		// VkShaderStageFlags	stageFlags
		0,						// deUint32				offset
		128						// deUint32				size
	};

	deUint32 nonEmptySetLimit = m_data.base == BASE_UBO ? properties.properties.limits.maxPerStageDescriptorUniformBuffers :
														  properties.properties.limits.maxPerStageDescriptorStorageBuffers;
	nonEmptySetLimit = de::min(nonEmptySetLimit, properties.properties.limits.maxPerStageDescriptorStorageImages);

	vector<vk::VkDescriptorSetLayout>	descriptorSetLayoutsRaw(m_data.set+1);
	for (size_t i = 0; i < m_data.set+1; ++i)
	{
		// use nonempty descriptor sets to consume resources until we run out of descriptors
		if (i < nonEmptySetLimit - 1 || i == m_data.set)
			descriptorSetLayoutsRaw[i] = descriptorSetLayout.get();
		else
			descriptorSetLayoutsRaw[i] = emptyDescriptorSetLayout.get();
	}

	const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo =
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,				// sType
		DE_NULL,													// pNext
		(VkPipelineLayoutCreateFlags)0,
		m_data.set+1,												// setLayoutCount
		&descriptorSetLayoutsRaw[0],								// pSetLayouts
		1u,															// pushConstantRangeCount
		&pushConstRange,											// pPushConstantRanges
	};

	Move<VkPipelineLayout> pipelineLayout = createPipelineLayout(vk, device, &pipelineLayoutCreateInfo, NULL);

	// PushConstant[i] = i
	for (deUint32 i = 0; i < (deUint32)(128 / sizeof(deUint32)); ++i)
	{
		vk.cmdPushConstants(*cmdBuffer, *pipelineLayout, allShaderStages,
							(deUint32)(i * sizeof(deUint32)), (deUint32)sizeof(deUint32), &i);
	}

	de::MovePtr<BufferWithMemory> copyBuffer;
	copyBuffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
		vk, device, allocator, makeBufferCreateInfo(DE_NULL, DIM*DIM*sizeof(deUint32), VK_BUFFER_USAGE_TRANSFER_DST_BIT, 0), MemoryRequirement::HostVisible));

	const VkImageCreateInfo			imageCreateInfo			=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	// VkStructureType			sType;
		DE_NULL,								// const void*				pNext;
		(VkImageCreateFlags)0u,					// VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,						// VkImageType				imageType;
		VK_FORMAT_R32_UINT,						// VkFormat					format;
		{
			DIM,								// deUint32	width;
			DIM,								// deUint32	height;
			1u									// deUint32	depth;
		},										// VkExtent3D				extent;
		1u,										// deUint32					mipLevels;
		1u,										// deUint32					arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,				// VkImageTiling			tiling;
		VK_IMAGE_USAGE_STORAGE_BIT
		| VK_IMAGE_USAGE_TRANSFER_SRC_BIT
		| VK_IMAGE_USAGE_TRANSFER_DST_BIT,		// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,				// VkSharingMode			sharingMode;
		0u,										// deUint32					queueFamilyIndexCount;
		DE_NULL,								// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED				// VkImageLayout			initialLayout;
	};

	VkImageViewCreateInfo		imageViewCreateInfo		=
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		(VkImageViewCreateFlags)0u,					// VkImageViewCreateFlags	flags;
		DE_NULL,									// VkImage					image;
		VK_IMAGE_VIEW_TYPE_2D,						// VkImageViewType			viewType;
		VK_FORMAT_R32_UINT,							// VkFormat					format;
		{
			VK_COMPONENT_SWIZZLE_R,					// VkComponentSwizzle	r;
			VK_COMPONENT_SWIZZLE_G,					// VkComponentSwizzle	g;
			VK_COMPONENT_SWIZZLE_B,					// VkComponentSwizzle	b;
			VK_COMPONENT_SWIZZLE_A					// VkComponentSwizzle	a;
		},											// VkComponentMapping		 components;
		{
			VK_IMAGE_ASPECT_COLOR_BIT,				// VkImageAspectFlags	aspectMask;
			0u,										// deUint32				baseMipLevel;
			1u,										// deUint32				levelCount;
			0u,										// deUint32				baseArrayLayer;
			1u										// deUint32				layerCount;
		}											// VkImageSubresourceRange	subresourceRange;
	};

	de::MovePtr<ImageWithMemory> image;
	Move<VkImageView> imageView;

	image = de::MovePtr<ImageWithMemory>(new ImageWithMemory(
		vk, device, allocator, imageCreateInfo, MemoryRequirement::Any));
	imageViewCreateInfo.image = **image;
	imageView = createImageView(vk, device, &imageViewCreateInfo, NULL);

	VkDescriptorImageInfo imageInfo = makeDescriptorImageInfo(DE_NULL, *imageView, VK_IMAGE_LAYOUT_GENERAL);
	VkDescriptorBufferInfo bufferInfo = makeDescriptorBufferInfo(**buffers[0], 0, align);

	VkWriteDescriptorSet w =
	{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,							// sType
		DE_NULL,														// pNext
		*descriptorSet,													// dstSet
		(deUint32)0,													// dstBinding
		0,																// dstArrayElement
		1u,																// descriptorCount
		bindings[0].descriptorType,										// descriptorType
		&imageInfo,														// pImageInfo
		&bufferInfo,													// pBufferInfo
		DE_NULL,														// pTexelBufferView
	};
	vk.updateDescriptorSets(device, 1, &w, 0, NULL);

	w.dstBinding = 1;
	w.descriptorType = bindings[1].descriptorType;
	vk.updateDescriptorSets(device, 1, &w, 0, NULL);

	vk.cmdBindDescriptorSets(*cmdBuffer, bindPoint, *pipelineLayout, m_data.set, 1, &descriptorSet.get(), 0, DE_NULL);

	Move<VkPipeline> pipeline;
	Move<VkRenderPass> renderPass;
	Move<VkFramebuffer> framebuffer;
	de::MovePtr<BufferWithMemory> sbtBuffer;

	m_context.getTestContext().touchWatchdogAndDisableIntervalTimeLimit();

	if (m_data.stage == STAGE_COMPUTE)
	{
		const Unique<VkShaderModule>	shader(createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0));

		const VkPipelineShaderStageCreateInfo	shaderCreateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			DE_NULL,
			(VkPipelineShaderStageCreateFlags)0,
			VK_SHADER_STAGE_COMPUTE_BIT,								// stage
			*shader,													// shader
			"main",
			DE_NULL,													// pSpecializationInfo
		};

		const VkComputePipelineCreateInfo		pipelineCreateInfo =
		{
			VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
			DE_NULL,
			0u,															// flags
			shaderCreateInfo,											// cs
			*pipelineLayout,											// layout
			(vk::VkPipeline)0,											// basePipelineHandle
			0u,															// basePipelineIndex
		};
		pipeline = createComputePipeline(vk, device, DE_NULL, &pipelineCreateInfo, NULL);
	}
#if ENABLE_RAYTRACING
	else if (m_data.stage == STAGE_RAYGEN)
	{
		const Unique<VkShaderModule>	shader(createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0));

		const VkPipelineShaderStageCreateInfo	shaderCreateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			DE_NULL,
			(VkPipelineShaderStageCreateFlags)0,
			VK_SHADER_STAGE_RAYGEN_BIT_NV,								// stage
			*shader,													// shader
			"main",
			DE_NULL,													// pSpecializationInfo
		};

		VkRayTracingShaderGroupCreateInfoNV group =
		{
			VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV,
			DE_NULL,
			VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV,			// type
			0,														// generalShader
			VK_SHADER_UNUSED_NV,									// closestHitShader
			VK_SHADER_UNUSED_NV,									// anyHitShader
			VK_SHADER_UNUSED_NV,									// intersectionShader
		};

		VkRayTracingPipelineCreateInfoNV pipelineCreateInfo = {
			VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_NV,	// sType
			DE_NULL,												// pNext
			0,														// flags
			1,														// stageCount
			&shaderCreateInfo,										// pStages
			1,														// groupCount
			&group,													// pGroups
			0,														// maxRecursionDepth
			*pipelineLayout,										// layout
			(vk::VkPipeline)0,										// basePipelineHandle
			0u,														// basePipelineIndex
		};

		pipeline = createRayTracingPipelineNV(vk, device, DE_NULL, &pipelineCreateInfo, NULL);

		sbtBuffer = de::MovePtr<BufferWithMemory>(new BufferWithMemory(
			vk, device, allocator, makeBufferCreateInfo(DE_NULL, rayTracingProperties.shaderGroupHandleSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, 0), MemoryRequirement::HostVisible));
		deUint32 *ptr = (deUint32 *)sbtBuffer->getAllocation().getHostPtr();
		invalidateAlloc(vk, device, sbtBuffer->getAllocation());

		vk.getRayTracingShaderGroupHandlesNV(device, *pipeline, 0, 1, rayTracingProperties.shaderGroupHandleSize, ptr);
	}
#endif
	else
	{

		const vk::VkSubpassDescription		subpassDesc			=
		{
			(vk::VkSubpassDescriptionFlags)0,
			vk::VK_PIPELINE_BIND_POINT_GRAPHICS,					// pipelineBindPoint
			0u,														// inputCount
			DE_NULL,												// pInputAttachments
			0u,														// colorCount
			DE_NULL,												// pColorAttachments
			DE_NULL,												// pResolveAttachments
			DE_NULL,												// depthStencilAttachment
			0u,														// preserveCount
			DE_NULL,												// pPreserveAttachments
		};
		const vk::VkRenderPassCreateInfo	renderPassParams	=
		{
			vk::VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,			// sType
			DE_NULL,												// pNext
			(vk::VkRenderPassCreateFlags)0,
			0u,														// attachmentCount
			DE_NULL,												// pAttachments
			1u,														// subpassCount
			&subpassDesc,											// pSubpasses
			0u,														// dependencyCount
			DE_NULL,												// pDependencies
		};

		renderPass = createRenderPass(vk, device, &renderPassParams);

		const vk::VkFramebufferCreateInfo	framebufferParams	=
		{
			vk::VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	// sType
			DE_NULL,										// pNext
			(vk::VkFramebufferCreateFlags)0,
			*renderPass,									// renderPass
			0u,												// attachmentCount
			DE_NULL,										// pAttachments
			DIM,											// width
			DIM,											// height
			1u,												// layers
		};

		framebuffer = createFramebuffer(vk, device, &framebufferParams);

		const VkPipelineVertexInputStateCreateInfo		vertexInputStateCreateInfo		=
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// VkStructureType							sType;
			DE_NULL,													// const void*								pNext;
			(VkPipelineVertexInputStateCreateFlags)0,					// VkPipelineVertexInputStateCreateFlags	flags;
			0u,															// deUint32									vertexBindingDescriptionCount;
			DE_NULL,													// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
			0u,															// deUint32									vertexAttributeDescriptionCount;
			DE_NULL														// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
		};

		const VkPipelineInputAssemblyStateCreateInfo	inputAssemblyStateCreateInfo	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType							sType;
			DE_NULL,														// const void*								pNext;
			(VkPipelineInputAssemblyStateCreateFlags)0,						// VkPipelineInputAssemblyStateCreateFlags	flags;
			(m_data.stage == STAGE_VERTEX) ? VK_PRIMITIVE_TOPOLOGY_POINT_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, // VkPrimitiveTopology						topology;
			VK_FALSE														// VkBool32									primitiveRestartEnable;
		};

		const VkPipelineRasterizationStateCreateInfo	rasterizationStateCreateInfo	=
		{
			VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,		// VkStructureType							sType;
			DE_NULL,														// const void*								pNext;
			(VkPipelineRasterizationStateCreateFlags)0,						// VkPipelineRasterizationStateCreateFlags	flags;
			VK_FALSE,														// VkBool32									depthClampEnable;
			(m_data.stage == STAGE_VERTEX) ? VK_TRUE : VK_FALSE,			// VkBool32									rasterizerDiscardEnable;
			VK_POLYGON_MODE_FILL,											// VkPolygonMode							polygonMode;
			VK_CULL_MODE_NONE,												// VkCullModeFlags							cullMode;
			VK_FRONT_FACE_CLOCKWISE,										// VkFrontFace								frontFace;
			VK_FALSE,														// VkBool32									depthBiasEnable;
			0.0f,															// float									depthBiasConstantFactor;
			0.0f,															// float									depthBiasClamp;
			0.0f,															// float									depthBiasSlopeFactor;
			1.0f															// float									lineWidth;
		};

		const VkPipelineMultisampleStateCreateInfo		multisampleStateCreateInfo =
		{
			VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType							sType
			DE_NULL,													// const void*								pNext
			0u,															// VkPipelineMultisampleStateCreateFlags	flags
			VK_SAMPLE_COUNT_1_BIT,										// VkSampleCountFlagBits					rasterizationSamples
			VK_FALSE,													// VkBool32									sampleShadingEnable
			1.0f,														// float									minSampleShading
			DE_NULL,													// const VkSampleMask*						pSampleMask
			VK_FALSE,													// VkBool32									alphaToCoverageEnable
			VK_FALSE													// VkBool32									alphaToOneEnable
		};

		VkViewport viewport = makeViewport(DIM, DIM);
		VkRect2D scissor = makeRect2D(DIM, DIM);

		const VkPipelineViewportStateCreateInfo			viewportStateCreateInfo				=
		{
			VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,	// VkStructureType							sType
			DE_NULL,												// const void*								pNext
			(VkPipelineViewportStateCreateFlags)0,					// VkPipelineViewportStateCreateFlags		flags
			1u,														// deUint32									viewportCount
			&viewport,												// const VkViewport*						pViewports
			1u,														// deUint32									scissorCount
			&scissor												// const VkRect2D*							pScissors
		};

		Move<VkShaderModule> fs;
		Move<VkShaderModule> vs;

		deUint32 numStages;
		if (m_data.stage == STAGE_VERTEX)
		{
			vs = createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0);
			fs = createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0); // bogus
			numStages = 1u;
		}
		else
		{
			vs = createShaderModule(vk, device, m_context.getBinaryCollection().get("vert"), 0);
			fs = createShaderModule(vk, device, m_context.getBinaryCollection().get("test"), 0);
			numStages = 2u;
		}

		const VkPipelineShaderStageCreateInfo	shaderCreateInfo[2] =
		{
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				DE_NULL,
				(VkPipelineShaderStageCreateFlags)0,
				VK_SHADER_STAGE_VERTEX_BIT,									// stage
				*vs,														// shader
				"main",
				DE_NULL,													// pSpecializationInfo
			},
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				DE_NULL,
				(VkPipelineShaderStageCreateFlags)0,
				VK_SHADER_STAGE_FRAGMENT_BIT,								// stage
				*fs,														// shader
				"main",
				DE_NULL,													// pSpecializationInfo
			}
		};

		const VkGraphicsPipelineCreateInfo				graphicsPipelineCreateInfo		=
		{
			VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,	// VkStructureType									sType;
			DE_NULL,											// const void*										pNext;
			(VkPipelineCreateFlags)0,							// VkPipelineCreateFlags							flags;
			numStages,											// deUint32											stageCount;
			&shaderCreateInfo[0],								// const VkPipelineShaderStageCreateInfo*			pStages;
			&vertexInputStateCreateInfo,						// const VkPipelineVertexInputStateCreateInfo*		pVertexInputState;
			&inputAssemblyStateCreateInfo,						// const VkPipelineInputAssemblyStateCreateInfo*	pInputAssemblyState;
			DE_NULL,											// const VkPipelineTessellationStateCreateInfo*		pTessellationState;
			&viewportStateCreateInfo,							// const VkPipelineViewportStateCreateInfo*			pViewportState;
			&rasterizationStateCreateInfo,						// const VkPipelineRasterizationStateCreateInfo*	pRasterizationState;
			&multisampleStateCreateInfo,						// const VkPipelineMultisampleStateCreateInfo*		pMultisampleState;
			DE_NULL,											// const VkPipelineDepthStencilStateCreateInfo*		pDepthStencilState;
			DE_NULL,											// const VkPipelineColorBlendStateCreateInfo*		pColorBlendState;
			DE_NULL,											// const VkPipelineDynamicStateCreateInfo*			pDynamicState;
			pipelineLayout.get(),								// VkPipelineLayout									layout;
			renderPass.get(),									// VkRenderPass										renderPass;
			0u,													// deUint32											subpass;
			DE_NULL,											// VkPipeline										basePipelineHandle;
			0													// int												basePipelineIndex;
		};

		pipeline = createGraphicsPipeline(vk, device, DE_NULL, &graphicsPipelineCreateInfo);
	}

	m_context.getTestContext().touchWatchdogAndEnableIntervalTimeLimit();

	const VkImageMemoryBarrier imageBarrier =
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,				// VkStructureType		sType
		DE_NULL,											// const void*			pNext
		0u,													// VkAccessFlags		srcAccessMask
		VK_ACCESS_TRANSFER_WRITE_BIT,						// VkAccessFlags		dstAccessMask
		VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout		oldLayout
		VK_IMAGE_LAYOUT_GENERAL,							// VkImageLayout		newLayout
		VK_QUEUE_FAMILY_IGNORED,							// uint32_t				srcQueueFamilyIndex
		VK_QUEUE_FAMILY_IGNORED,							// uint32_t				dstQueueFamilyIndex
		**image,											// VkImage				image
		{
			VK_IMAGE_ASPECT_COLOR_BIT,				// VkImageAspectFlags	aspectMask
			0u,										// uint32_t				baseMipLevel
			1u,										// uint32_t				mipLevels,
			0u,										// uint32_t				baseArray
			1u,										// uint32_t				arraySize
		}
	};

	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
							(VkDependencyFlags)0,
							0, (const VkMemoryBarrier*)DE_NULL,
							0, (const VkBufferMemoryBarrier*)DE_NULL,
							1, &imageBarrier);

	vk.cmdBindPipeline(*cmdBuffer, bindPoint, *pipeline);

	VkImageSubresourceRange range = makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	VkClearValue clearColor = makeClearValueColorU32(0,0,0,0);

	VkMemoryBarrier					memBarrier =
	{
		VK_STRUCTURE_TYPE_MEMORY_BARRIER,	// sType
		DE_NULL,							// pNext
		0u,									// srcAccessMask
		0u,									// dstAccessMask
	};

	vk.cmdClearColorImage(*cmdBuffer, **image, VK_IMAGE_LAYOUT_GENERAL, &clearColor.color, 1, &range);

	memBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	memBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	vk.cmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, allPipelineStages,
		0, 1, &memBarrier, 0, DE_NULL, 0, DE_NULL);

	if (m_data.stage == STAGE_COMPUTE)
	{
		vk.cmdDispatch(*cmdBuffer, DIM, DIM, 1);
	}
#if ENABLE_RAYTRACING
	else if (m_data.stage == STAGE_RAYGEN)
	{
		vk.cmdTraceRaysNV(*cmdBuffer,
			**sbtBuffer, 0,
			DE_NULL, 0, 0,
			DE_NULL, 0, 0,
			DE_NULL, 0, 0,
			DIM, DIM, 1);
	}
#endif
	else
	{
		beginRenderPass(vk, *cmdBuffer, *renderPass, *framebuffer,
						makeRect2D(DIM, DIM),
						0, DE_NULL, VK_SUBPASS_CONTENTS_INLINE);
		// Draw a point cloud for vertex shader testing, and a single quad for fragment shader testing
		if (m_data.stage == STAGE_VERTEX)
		{
			vk.cmdDraw(*cmdBuffer, DIM*DIM, 1u, 0u, 0u);
		}
		else
		{
			vk.cmdDraw(*cmdBuffer, 4u, 1u, 0u, 0u);
		}
		endRenderPass(vk, *cmdBuffer);
	}

	memBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	memBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
	vk.cmdPipelineBarrier(*cmdBuffer, allPipelineStages, VK_PIPELINE_STAGE_TRANSFER_BIT,
		0, 1, &memBarrier, 0, DE_NULL, 0, DE_NULL);

	const VkBufferImageCopy copyRegion = makeBufferImageCopy(makeExtent3D(DIM, DIM, 1u),
															 makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u));
	vk.cmdCopyImageToBuffer(*cmdBuffer, **image, VK_IMAGE_LAYOUT_GENERAL, **copyBuffer, 1u, &copyRegion);

	endCommandBuffer(vk, *cmdBuffer);

	submitCommandsAndWait(vk, device, queue, cmdBuffer.get());

	deUint32 *ptr = (deUint32 *)copyBuffer->getAllocation().getHostPtr();
	invalidateAlloc(vk, device, copyBuffer->getAllocation());

	qpTestResult res = QP_TEST_RESULT_PASS;

	for (deUint32 i = 0; i < DIM*DIM; ++i)
	{
		if (ptr[i] != 1)
		{
			res = QP_TEST_RESULT_FAIL;
		}
	}

	return tcu::TestStatus(res, qpGetTestResultName(res));
}

class CaptureReplayTestCase : public TestCase
{
public:
							CaptureReplayTestCase	(tcu::TestContext& context, const char* name, const char* desc, deUint32 seed);
							~CaptureReplayTestCase	(void);
	virtual	void			initPrograms			(SourceCollections& programCollection) const { DE_UNREF(programCollection); }
	virtual TestInstance*	createInstance			(Context& context) const;
	virtual void			checkSupport			(Context& context) const;
private:
	deUint32				m_seed;
};

CaptureReplayTestCase::CaptureReplayTestCase (tcu::TestContext& context, const char* name, const char* desc, deUint32 seed)
	: vkt::TestCase	(context, name, desc)
	, m_seed(seed)
{
}

CaptureReplayTestCase::~CaptureReplayTestCase	(void)
{
}

void CaptureReplayTestCase::checkSupport (Context& context) const
{
	if (!context.isBufferDeviceAddressSupported())
		TCU_THROW(NotSupportedError, "Physical storage buffer pointers not supported");

	bool isBufferDeviceAddressWithCaptureReplaySupported =
			(context.isDeviceFunctionalitySupported("VK_KHR_buffer_device_address") && context.getBufferDeviceAddressFeatures().bufferDeviceAddressCaptureReplay) ||
			(context.isDeviceFunctionalitySupported("VK_EXT_buffer_device_address") && context.getBufferDeviceAddressFeaturesEXT().bufferDeviceAddressCaptureReplay);

	if (!isBufferDeviceAddressWithCaptureReplaySupported)
		TCU_THROW(NotSupportedError, "Capture/replay of physical storage buffer pointers not supported");
}

class CaptureReplayTestInstance : public TestInstance
{
public:
						CaptureReplayTestInstance	(Context& context, deUint32 seed);
						~CaptureReplayTestInstance	(void);
	tcu::TestStatus		iterate						(void);
private:
	deUint32			m_seed;
};

CaptureReplayTestInstance::CaptureReplayTestInstance (Context& context, deUint32 seed)
	: vkt::TestInstance		(context)
	, m_seed(seed)
{
}

CaptureReplayTestInstance::~CaptureReplayTestInstance (void)
{
}

TestInstance* CaptureReplayTestCase::createInstance (Context& context) const
{
	return new CaptureReplayTestInstance(context, m_seed);
}

tcu::TestStatus CaptureReplayTestInstance::iterate (void)
{
	const InstanceInterface&vki						= m_context.getInstanceInterface();
	const DeviceInterface&	vk						= m_context.getDeviceInterface();
	const VkPhysicalDevice&	physDevice				= m_context.getPhysicalDevice();
	const VkDevice			device					= m_context.getDevice();
	const bool				useKHR					= m_context.isDeviceFunctionalitySupported("VK_KHR_buffer_device_address");
	de::Random				rng(m_seed);

	VkBufferDeviceAddressCreateInfoEXT addressCreateInfoEXT =
	{
		VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_CREATE_INFO_EXT,	// VkStructureType	 sType;
		DE_NULL,													// const void*		 pNext;
		0x000000000ULL,												// VkDeviceSize		 deviceAddress
	};

	VkBufferOpaqueCaptureAddressCreateInfo bufferOpaqueCaptureAddressCreateInfo =
	{
		VK_STRUCTURE_TYPE_BUFFER_OPAQUE_CAPTURE_ADDRESS_CREATE_INFO,	// VkStructureType	 sType;
		DE_NULL,														// const void*		 pNext;
		0x000000000ULL,													// VkDeviceSize		 opaqueCaptureAddress
	};

	const deUint32 numBuffers = 100;
	std::vector<VkDeviceSize> bufferSizes(numBuffers);
	// random sizes, powers of two [4K, 4MB]
	for (deUint32 i = 0; i < numBuffers; ++i)
		bufferSizes[i] = 4096 << (rng.getUint32() % 11);

	std::vector<VkDeviceAddress> gpuAddrs(numBuffers);
	std::vector<deUint64> opaqueBufferAddrs(numBuffers);
	std::vector<deUint64> opaqueMemoryAddrs(numBuffers);

	VkBufferDeviceAddressInfo bufferDeviceAddressInfo =
	{
		VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,	// VkStructureType	 sType;
		DE_NULL,										// const void*		 pNext;
		0,												// VkBuffer			 buffer
	};

	VkDeviceMemoryOpaqueCaptureAddressInfo deviceMemoryOpaqueCaptureAddressInfo =
	{
		VK_STRUCTURE_TYPE_DEVICE_MEMORY_OPAQUE_CAPTURE_ADDRESS_INFO,	// VkStructureType	 sType;
		DE_NULL,														// const void*		 pNext;
		0,																// VkDeviceMemory	 memory;
	};

	vector<VkBufferSp>			buffers(numBuffers);
	vector<AllocationSp>		allocations(numBuffers);

	VkBufferCreateInfo			bufferCreateInfo = makeBufferCreateInfo(DE_NULL, 0,
														VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
														VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
														VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
														VK_BUFFER_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT);

	// VkMemoryAllocateFlags to be filled out later
	VkMemoryAllocateFlagsInfo	allocFlagsInfo =
	{
		VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,	//	VkStructureType	sType
		DE_NULL,										//	const void*		pNext
		0,												//	VkMemoryAllocateFlags    flags
		0,												//	uint32_t                 deviceMask
	};

	VkMemoryOpaqueCaptureAddressAllocateInfo memoryOpaqueCaptureAddressAllocateInfo =
	{
		VK_STRUCTURE_TYPE_MEMORY_OPAQUE_CAPTURE_ADDRESS_ALLOCATE_INFO,	// VkStructureType    sType;
		DE_NULL,														// const void*        pNext;
		0,																// uint64_t           opaqueCaptureAddress;
	};

	if (useKHR)
		allocFlagsInfo.flags |= VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

	if (useKHR)
	{
		allocFlagsInfo.flags |= VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT;
		allocFlagsInfo.pNext = &memoryOpaqueCaptureAddressAllocateInfo;
	}

	for (deUint32 i = 0; i < numBuffers; ++i)
	{
		bufferCreateInfo.size = bufferSizes[i];
		buffers[i] = VkBufferSp(new Unique<VkBuffer>(createBuffer(vk, device, &bufferCreateInfo)));

		// query opaque capture address before binding memory
		if (useKHR)
		{
			bufferDeviceAddressInfo.buffer = **buffers[i];
			opaqueBufferAddrs[i] = vk.getBufferOpaqueCaptureAddress(device, &bufferDeviceAddressInfo);
		}

		allocations[i] = AllocationSp(allocateExtended(vki, vk, physDevice, device, getBufferMemoryRequirements(vk, device, **buffers[i]), MemoryRequirement::HostVisible, &allocFlagsInfo));

		if (useKHR)
		{
			deviceMemoryOpaqueCaptureAddressInfo.memory = allocations[i]->getMemory();
			opaqueMemoryAddrs[i] = vk.getDeviceMemoryOpaqueCaptureAddress(device, &deviceMemoryOpaqueCaptureAddressInfo);
		}

		VK_CHECK(vk.bindBufferMemory(device, **buffers[i], allocations[i]->getMemory(), 0));
	}

	for (deUint32 i = 0; i < numBuffers; ++i)
	{
		bufferDeviceAddressInfo.buffer = **buffers[i];
		if (useKHR)
			gpuAddrs[i] = vk.getBufferDeviceAddress(device, &bufferDeviceAddressInfo);
		else
			gpuAddrs[i] = vk.getBufferDeviceAddressEXT(device, &bufferDeviceAddressInfo);
	}
	buffers.clear();
	buffers.resize(numBuffers);
	allocations.clear();
	allocations.resize(numBuffers);

	bufferCreateInfo.pNext = useKHR ? (void *)&bufferOpaqueCaptureAddressCreateInfo : (void *)&addressCreateInfoEXT;

	for (deInt32 i = numBuffers-1; i >= 0; --i)
	{
		addressCreateInfoEXT.deviceAddress = gpuAddrs[i];
		bufferOpaqueCaptureAddressCreateInfo.opaqueCaptureAddress = opaqueBufferAddrs[i];
		memoryOpaqueCaptureAddressAllocateInfo.opaqueCaptureAddress = opaqueMemoryAddrs[i];

		bufferCreateInfo.size = bufferSizes[i];
		buffers[i] = VkBufferSp(new Unique<VkBuffer>(createBuffer(vk, device, &bufferCreateInfo)));
		allocations[i] = AllocationSp(allocateExtended(vki, vk, physDevice, device, getBufferMemoryRequirements(vk, device, **buffers[i]), MemoryRequirement::HostVisible, &allocFlagsInfo));
		VK_CHECK(vk.bindBufferMemory(device, **buffers[i], allocations[i]->getMemory(), 0));

		bufferDeviceAddressInfo.buffer = **buffers[i];
		VkDeviceSize newAddr;
		if (useKHR)
			newAddr = vk.getBufferDeviceAddress(device, &bufferDeviceAddressInfo);
		else
			newAddr = vk.getBufferDeviceAddressEXT(device, &bufferDeviceAddressInfo);
		if (newAddr != gpuAddrs[i])
			return tcu::TestStatus(QP_TEST_RESULT_FAIL, "address mismatch");
	}

	return tcu::TestStatus(QP_TEST_RESULT_PASS, qpGetTestResultName(QP_TEST_RESULT_PASS));
}

}	// anonymous

tcu::TestCaseGroup*	createBufferDeviceAddressTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "buffer_device_address", "Test VK_EXT_buffer_device_address"));

	typedef struct
	{
		deUint32				count;
		const char*				name;
		const char*				description;
	} TestGroupCase;

	TestGroupCase setCases[] =
	{
		{ 0,	"set0",		"set 0"		},
		{ 3,	"set3",		"set 3"		},
		{ 7,	"set7",		"set 7"		},
		{ 15,	"set15",	"set 15"	},
		{ 31,	"set31",	"set 31"	},
	};

	TestGroupCase depthCases[] =
	{
		{ 1,	"depth1",	"1 nested struct"		},
		{ 2,	"depth2",	"2 nested structs"		},
		{ 3,	"depth3",	"3 nested structs"		},
	};

	TestGroupCase baseCases[] =
	{
		{ BASE_UBO,	"baseubo",	"base ubo"		},
		{ BASE_SSBO,"basessbo",	"base ssbo"		},
	};

	TestGroupCase cvtCases[] =
	{
		{ CONVERT_NONE,			"load",				"load reference"										},
		{ CONVERT_UINT64,		"convert",			"load and convert reference"							},
		{ CONVERT_UVEC2,		"convertuvec2",		"load and convert reference to uvec2"					},
		{ CONVERT_U64CMP,		"convertchecku64",	"load, convert and compare references as uint64_t"		},
		{ CONVERT_UVEC2CMP,		"convertcheckuv2",	"load, convert and compare references as uvec2"			},
		{ CONVERT_UVEC2TOU64,	"crossconvertu2p",	"load reference as uint64_t and convert it to uvec2"	},
		{ CONVERT_U64TOUVEC2,	"crossconvertp2u",	"load reference as uvec2 and convert it to uint64_t"	},
	};

	TestGroupCase storeCases[] =
	{
		{ 0,	"nostore",		"don't store intermediate reference"		},
		{ 1,	"store",		"store intermediate reference"				},
	};

	TestGroupCase btCases[] =
	{
		{ BT_SINGLE,	"single",		"single buffer"	},
		{ BT_MULTI,		"multi",		"multiple buffers"	},
		{ BT_REPLAY,	"replay",		"multiple buffers and VK_BUFFER_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT_EXT"	},
	};

	TestGroupCase layoutCases[] =
	{
		{ LAYOUT_STD140,	"std140",		"std140"	},
		{ LAYOUT_SCALAR,	"scalar",		"scalar"	},
	};

	TestGroupCase stageCases[] =
	{
		{ STAGE_COMPUTE,	"comp",		"compute"	},
		{ STAGE_FRAGMENT,	"frag",		"fragment"	},
		{ STAGE_VERTEX,		"vert",		"vertex"	},
#if ENABLE_RAYTRACING
		{ STAGE_RAYGEN,		"rgen",		"raygen"	},
#endif
	};

	for (int setNdx = 0; setNdx < DE_LENGTH_OF_ARRAY(setCases); setNdx++)
	{
		de::MovePtr<tcu::TestCaseGroup> setGroup(new tcu::TestCaseGroup(testCtx, setCases[setNdx].name, setCases[setNdx].description));
		for (int depthNdx = 0; depthNdx < DE_LENGTH_OF_ARRAY(depthCases); depthNdx++)
		{
			de::MovePtr<tcu::TestCaseGroup> depthGroup(new tcu::TestCaseGroup(testCtx, depthCases[depthNdx].name, depthCases[depthNdx].description));
			for (int baseNdx = 0; baseNdx < DE_LENGTH_OF_ARRAY(baseCases); baseNdx++)
			{
				de::MovePtr<tcu::TestCaseGroup> baseGroup(new tcu::TestCaseGroup(testCtx, baseCases[baseNdx].name, baseCases[baseNdx].description));
				for (int cvtNdx = 0; cvtNdx < DE_LENGTH_OF_ARRAY(cvtCases); cvtNdx++)
				{
					de::MovePtr<tcu::TestCaseGroup> cvtGroup(new tcu::TestCaseGroup(testCtx, cvtCases[cvtNdx].name, cvtCases[cvtNdx].description));
					for (int storeNdx = 0; storeNdx < DE_LENGTH_OF_ARRAY(storeCases); storeNdx++)
					{
						de::MovePtr<tcu::TestCaseGroup> storeGroup(new tcu::TestCaseGroup(testCtx, storeCases[storeNdx].name, storeCases[storeNdx].description));
						for (int btNdx = 0; btNdx < DE_LENGTH_OF_ARRAY(btCases); btNdx++)
						{
							de::MovePtr<tcu::TestCaseGroup> btGroup(new tcu::TestCaseGroup(testCtx, btCases[btNdx].name, btCases[btNdx].description));
							for (int layoutNdx = 0; layoutNdx < DE_LENGTH_OF_ARRAY(layoutCases); layoutNdx++)
							{
								de::MovePtr<tcu::TestCaseGroup> layoutGroup(new tcu::TestCaseGroup(testCtx, layoutCases[layoutNdx].name, layoutCases[layoutNdx].description));
								for (int stageNdx = 0; stageNdx < DE_LENGTH_OF_ARRAY(stageCases); stageNdx++)
								{
									CaseDef c =
									{
										setCases[setNdx].count,						// deUint32 set;
										depthCases[depthNdx].count,					// deUint32 depth;
										(Base)baseCases[baseNdx].count,				// Base base;
										(Stage)stageCases[stageNdx].count,			// Stage stage;
										(Convert)cvtCases[cvtNdx].count,			// Convert convertUToPtr;
										!!storeCases[storeNdx].count,				// bool storeInLocal;
										(BufType)btCases[btNdx].count,				// BufType bufType;
										(Layout)layoutCases[layoutNdx].count,		// Layout layout;
									};

									// Skip more complex test cases for most descriptor sets, to reduce runtime.
									if (c.set != 3 && (c.depth == 3 || c.layout != LAYOUT_STD140))
										continue;

									layoutGroup->addChild(new BufferAddressTestCase(testCtx, stageCases[stageNdx].name, stageCases[stageNdx].description, c));
								}
								btGroup->addChild(layoutGroup.release());
							}
							storeGroup->addChild(btGroup.release());
						}
						cvtGroup->addChild(storeGroup.release());
					}
					baseGroup->addChild(cvtGroup.release());
				}
				depthGroup->addChild(baseGroup.release());
			}
			setGroup->addChild(depthGroup.release());
		}
		group->addChild(setGroup.release());
	}

	de::MovePtr<tcu::TestCaseGroup> capGroup(new tcu::TestCaseGroup(testCtx, "capture_replay_stress", "Test VK_BUFFER_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT_EXT"));
	for (deUint32 i = 0; i < 10; ++i)
	{
		capGroup->addChild(new CaptureReplayTestCase(testCtx, (std::string("seed_") + de::toString(i)).c_str(), "", i));
	}
	group->addChild(capGroup.release());
	return group.release();
}

}	// BindingModel
}	// vkt
