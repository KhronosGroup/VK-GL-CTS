/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 Mobica Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice(s) and this permission notice shall be included
 * in all copies or substantial portions of the Materials.
 *
 * The Materials are Confidential Information as defined by the
 * Khronos Membership Agreement until designated non-confidential by Khronos,
 * at which point this condition clause shall be removed.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
 *
 *//*!
 * \file
 * \brief Compute Shader Built-in variable tests.
 *//*--------------------------------------------------------------------*/

#include "vktComputeShaderBuiltinVarTests.hpp"

#include "vktTestCaseUtil.hpp"
#include "vkDefs.hpp"
#include "vkPlatform.hpp"
#include "vkStrUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkPrograms.hpp"
#include "vkTypeUtil.hpp"
#include "tcuTestLog.hpp"
#include "tcuFormatUtil.hpp"
#include "deUniquePtr.hpp"
#include "deSharedPtr.hpp"
#include "gluShaderUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "tcuVectorUtil.hpp"
#include <map>

namespace vkt
{
namespace compute
{
namespace
{

using namespace vk;
using std::string;
using std::vector;
using std::map;
using tcu::TestLog;
using tcu::UVec3;
using tcu::IVec3;

class ComputeBuiltinVarInstance;
class ComputeBuiltinVarCase;

const string PrefixProgramName ="compute_";

static inline bool compareNumComponents (const UVec3& a, const UVec3& b,const int numComps)
{
	DE_ASSERT(numComps == 1 || numComps == 3);
	return numComps == 3 ? tcu::allEqual(a, b) : a.x() == b.x();
}

static inline UVec3 readResultVec (const deUint32* ptr, const int numComps)
{
	UVec3 res;
	for (int ndx = 0; ndx < numComps; ndx++)
		res[ndx] = ptr[ndx];
	return res;
}

struct LogComps
{
	const UVec3&	v;
	int				numComps;

					LogComps	(const UVec3 &v_, int numComps_) : v(v_), numComps(numComps_) {}
};

static inline std::ostream& operator<< (std::ostream& str, const LogComps& c)
{
	DE_ASSERT(c.numComps == 1 || c.numComps == 3);
	return c.numComps == 3 ? str << c.v : str << c.v.x();
}

struct SubCase
{
	const UVec3		localSize;
	const UVec3		numWorkGroups;

					SubCase			(void) {}
					SubCase			(const UVec3& localSize_, const UVec3& numWorkGroups_)
						: localSize		(localSize_)
						, numWorkGroups	(numWorkGroups_) {}
};

vk::Move<vk::VkPipelineLayout> makePipelineLayout (const vk::DeviceInterface&		vki,
												   const vk::VkDevice				device,
												   const deUint32					numDescriptorSets,
												   const vk::VkDescriptorSetLayout*	descriptorSetLayouts)
{
	const vk::VkPipelineLayoutCreateInfo createInfo =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		DE_NULL,
		numDescriptorSets,		// descriptorSetCount
		descriptorSetLayouts,	// pSetLayouts
		0u,						// pushConstantRangeCount
		DE_NULL,				// pPushConstantRanges
	};
	return vk::createPipelineLayout(vki, device, &createInfo);
}

vk::Move<vk::VkPipeline> makeComputePipeline (const vk::DeviceInterface&	vki,
											  const vk::VkDevice			device,
											  const vk::BinaryCollection&	programCollection,
											  const string					programName,
											  const vk::VkPipelineLayout	layout)
{
	const vk::Move<vk::VkShaderModule>		computeModule(vk::createShaderModule(vki, device, programCollection.get(programName.c_str()), (vk::VkShaderModuleCreateFlags)0u));
	const vk::VkShaderCreateInfo			shaderCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_SHADER_CREATE_INFO,
		DE_NULL,
		*computeModule,		// module
		"main",				// pName
		0u,					// flags
		vk::VK_SHADER_STAGE_COMPUTE
	};
	const vk::Move<vk::VkShader>				computeShader(vk::createShader(vki, device, &shaderCreateInfo));

	const vk::VkPipelineShaderStageCreateInfo	cs =
	{
		vk::VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		DE_NULL,
		vk::VK_SHADER_STAGE_COMPUTE,	// stage
		*computeShader,					// shader
		DE_NULL,						// pSpecializationInfo
	};

	const vk::VkComputePipelineCreateInfo		createInfo =
	{
		vk::VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		DE_NULL,
		cs,								// cs
		0u,								// flags
		layout,							// layout
		(vk::VkPipeline)0,				// basePipelineHandle
		0u,								// basePipelineIndex
	};
	return createComputePipeline(vki, device, (vk::VkPipelineCache)0u, &createInfo);
}

class BufferObject
{
public:
								BufferObject	(const vk::DeviceInterface&			vki,
												 const vk::VkDevice					device,
												 vk::Allocator&						allocator,
												 const vk::VkDeviceSize				bufferSize,
												 const vk::VkBufferUsageFlagBits	usage);

	vk::VkBuffer				getVKBuffer		(void) const { return *m_buffer; }
	deUint8*					mapBuffer		(void);
	void						unmapBuffer		(void);

protected:
	const vk::DeviceInterface&	m_device_interface;
	const vk::VkDevice			m_device;
	de::MovePtr<vk::Allocation> m_allocation;
	const vk::VkDeviceSize		m_bufferSize;
	vk::Move<vk::VkBuffer>		m_buffer;
};

BufferObject::BufferObject (const vk::DeviceInterface&		device_interface,
							const vk::VkDevice				device,
							vk::Allocator&					allocator,
							const vk::VkDeviceSize			bufferSize,
							const vk::VkBufferUsageFlagBits	usage)
	: m_device_interface(device_interface)
	, m_device			(device)
	, m_bufferSize		(bufferSize)
{
	const vk::VkBufferCreateInfo	bufferCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		DE_NULL,
		bufferSize,									// size
		usage,										// usage
		0u,											// flags
		vk::VK_SHARING_MODE_EXCLUSIVE,				// sharingMode
		0u,											// queueFamilyCount
		DE_NULL,									// pQueueFamilyIndices
	};

	m_buffer = vk::createBuffer(device_interface, device, &bufferCreateInfo);

	const vk::VkMemoryRequirements requirements = vk::getBufferMemoryRequirements(device_interface, device, *m_buffer);

	m_allocation = allocator.allocate(requirements, vk::MemoryRequirement::HostVisible);

	VK_CHECK(device_interface.bindBufferMemory(device, *m_buffer, m_allocation->getMemory(), m_allocation->getOffset()));
}

deUint8* BufferObject::mapBuffer (void)
{
	invalidateMappedMemoryRange(m_device_interface, m_device, m_allocation->getMemory(), m_allocation->getOffset(), m_bufferSize);

	return (deUint8*)m_allocation->getHostPtr();
}

void BufferObject::unmapBuffer (void)
{
	flushMappedMemoryRange(m_device_interface, m_device, m_allocation->getMemory(), m_allocation->getOffset(), m_bufferSize);
}

class CommandBuffer
{
public:
								CommandBuffer			(const vk::DeviceInterface&	device_interface,
														 const vk::VkDevice			device,
														 const deUint32				queueFamilyIndex);

	vk::VkCmdBuffer				beginRecordingCommands	(void);
	void						endRecordingCommands	(void);

protected:

	const vk::DeviceInterface&	m_device_interface;
	const vk::VkDevice			m_device;
	vk::Move<vk::VkCmdPool>		m_cmdPool;
	vk::Move<vk::VkCmdBuffer>	m_cmdBuffer;
};

CommandBuffer::CommandBuffer (const vk::DeviceInterface&	device_interface,
							  const vk::VkDevice			device,
							  const deUint32				queueFamilyIndex)
	: m_device_interface(device_interface)
	, m_device			(device)
{
	const vk::VkCmdPoolCreateInfo cmdPoolCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_CMD_POOL_CREATE_INFO,
		DE_NULL,
		queueFamilyIndex,						// queueFamilyIndex
		vk::VK_CMD_POOL_CREATE_TRANSIENT_BIT,	// flags
	};

	m_cmdPool = vk::createCommandPool(device_interface, device, &cmdPoolCreateInfo);

	const vk::VkCmdBufferCreateInfo	cmdBufCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_CMD_BUFFER_CREATE_INFO,
		DE_NULL,
		*m_cmdPool,							// cmdPool
		vk::VK_CMD_BUFFER_LEVEL_PRIMARY,	// level
		0u,									// flags
	};

	m_cmdBuffer = vk::createCommandBuffer(device_interface, device, &cmdBufCreateInfo);
}

vk::VkCmdBuffer CommandBuffer::beginRecordingCommands (void)
{
	const vk::VkCmdBufferBeginInfo	cmdBufBeginInfo =
	{
		vk::VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO,
		DE_NULL,
		vk::VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | vk::VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT,	// flags
		(vk::VkRenderPass)0u,																			// renderPass
		0u,																								// subpass
		(vk::VkFramebuffer)0u,																			// framebuffer
	};

	VK_CHECK(m_device_interface.beginCommandBuffer(*m_cmdBuffer, &cmdBufBeginInfo));

	return *m_cmdBuffer;
}

void CommandBuffer::endRecordingCommands (void)
{
	VK_CHECK(m_device_interface.endCommandBuffer(*m_cmdBuffer));
}

class Fence
{
public:
							Fence		(const vk::DeviceInterface&	device_interface,
										 const vk::VkDevice			device);

	vk::VkFence				getVKFence	(void) const { return *m_fence; }

private:
	vk::Move<vk::VkFence>	m_fence;
};

Fence::Fence (const vk::DeviceInterface& device_interface, const vk::VkDevice device)
{
	const vk::VkFenceCreateInfo fenceCreateInfo =
	{
		vk::VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		DE_NULL,
		0u,			// flags
	};

	m_fence = vk::createFence(device_interface, device, &fenceCreateInfo);
}

class ComputeBuiltinVarInstance : public vkt::TestInstance
{
public:
									ComputeBuiltinVarInstance	(Context&						context,
																 const vector<SubCase>&			subCases,
																 const glu::DataType			varType,
																 const ComputeBuiltinVarCase*	builtinVarCase);

	virtual tcu::TestStatus			iterate						(void);

private:
	const VkDevice					m_device;
	const DeviceInterface&			m_vki;
	const vk::VkQueue				m_queue;
	const deUint32					m_queueFamilyIndex;
	vector<SubCase>					m_subCases;
	const ComputeBuiltinVarCase*	m_builtin_var_case;
	int								m_subCaseNdx;
	const glu::DataType				m_varType;

	vk::VkDescriptorInfo			createDescriptorInfo		(vk::VkBuffer buffer, vk::VkDeviceSize offset, vk::VkDeviceSize range);
	vk::VkBufferMemoryBarrier		createResultBufferBarrier	(vk::VkBuffer buffer);
};

class ComputeBuiltinVarCase : public vkt::TestCase
{
public:
							ComputeBuiltinVarCase	(tcu::TestContext& context, const char* name, const char* varName, glu::DataType varType);
							~ComputeBuiltinVarCase	(void);

	TestInstance*			createInstance			(Context& context) const
	{
		return new ComputeBuiltinVarInstance(context, m_subCases, m_varType, this);
	};

	virtual void			initPrograms			(vk::SourceCollections& programCollection) const;
	virtual UVec3			computeReference		(const UVec3& numWorkGroups, const UVec3& workGroupSize, const UVec3& workGroupID, const UVec3& localInvocationID) const = 0;

protected:
	string					genBuiltinVarSource		(const string& varName, glu::DataType varType, const UVec3& localSize) const;
	vector<SubCase>			m_subCases;

private:
	deUint32				getProgram				(const tcu::UVec3& localSize);

	const string			m_varName;
	const glu::DataType		m_varType;
	int						m_subCaseNdx;

	ComputeBuiltinVarCase (const ComputeBuiltinVarCase& other);
	ComputeBuiltinVarCase& operator= (const ComputeBuiltinVarCase& other);
};

ComputeBuiltinVarCase::ComputeBuiltinVarCase (tcu::TestContext& context, const char* name, const char* varName, glu::DataType varType)
	: TestCase		(context, name, varName)
	, m_varName		(varName)
	, m_varType		(varType)
	, m_subCaseNdx	(0)
{
}

ComputeBuiltinVarCase::~ComputeBuiltinVarCase (void)
{
	ComputeBuiltinVarCase::deinit();
}

void ComputeBuiltinVarCase::initPrograms (vk::SourceCollections& programCollection) const
{
	for (int i = 0; i < m_subCases.size(); i++)
	{
		const SubCase&	subCase = m_subCases[i];
		std::ostringstream name;
		name << PrefixProgramName << i;
		programCollection.glslSources.add(name.str()) << glu::ComputeSource(genBuiltinVarSource(m_varName, m_varType, subCase.localSize).c_str());
	}
}

string ComputeBuiltinVarCase::genBuiltinVarSource (const string& varName, glu::DataType varType, const UVec3& localSize) const
{
	std::ostringstream src;

	src << "#version 310 es\n"
		<< "layout (local_size_x = " << localSize.x() << ", local_size_y = " << localSize.y() << ", local_size_z = " << localSize.z() << ") in;\n"
		<< "layout(set = 0, binding = 0) uniform Stride\n"
		<< "{\n"
		<< "	uvec2 u_stride;\n"
		<< "}stride;\n"
		<< "layout(set = 0, binding = 1, std430) buffer Output\n"
		<< "{\n"
		<< "	" << glu::getDataTypeName(varType) << " result[];\n"
		<< "} sb_out;\n"
		<< "\n"
		<< "void main (void)\n"
		<< "{\n"
		<< "	highp uint offset = stride.u_stride.x*gl_GlobalInvocationID.z + stride.u_stride.y*gl_GlobalInvocationID.y + gl_GlobalInvocationID.x;\n"
		<< "	sb_out.result[offset] = " << varName << ";\n"
		<< "}\n";

	return src.str();
}

class NumWorkGroupsCase : public ComputeBuiltinVarCase
{
public:
	NumWorkGroupsCase (tcu::TestContext& context)
		: ComputeBuiltinVarCase(context, "num_work_groups", "gl_NumWorkGroups", glu::TYPE_UINT_VEC3)
	{
		m_subCases.push_back(SubCase(UVec3(1, 1, 1), UVec3(1, 1, 1)));
		m_subCases.push_back(SubCase(UVec3(1, 1, 1), UVec3(52, 1, 1)));
		m_subCases.push_back(SubCase(UVec3(1, 1, 1), UVec3(1, 39, 1)));
		m_subCases.push_back(SubCase(UVec3(1, 1, 1), UVec3(1, 1, 78)));
		m_subCases.push_back(SubCase(UVec3(1, 1, 1), UVec3(4, 7, 11)));
		m_subCases.push_back(SubCase(UVec3(2, 3, 4), UVec3(4, 7, 11)));
	}

	UVec3 computeReference (const UVec3& numWorkGroups, const UVec3& workGroupSize, const UVec3& workGroupID, const UVec3& localInvocationID) const
	{
		DE_UNREF(numWorkGroups);
		DE_UNREF(workGroupSize);
		DE_UNREF(workGroupID);
		DE_UNREF(localInvocationID);
		return numWorkGroups;
	}
};

class WorkGroupSizeCase : public ComputeBuiltinVarCase
{
public:
	WorkGroupSizeCase (tcu::TestContext& context)
		: ComputeBuiltinVarCase(context, "work_group_size", "gl_WorkGroupSize", glu::TYPE_UINT_VEC3)
	{
		m_subCases.push_back(SubCase(UVec3(1, 1, 1), UVec3(1, 1, 1)));
		m_subCases.push_back(SubCase(UVec3(1, 1, 1), UVec3(2, 7, 3)));
		m_subCases.push_back(SubCase(UVec3(2, 1, 1), UVec3(1, 1, 1)));
		m_subCases.push_back(SubCase(UVec3(2, 1, 1), UVec3(1, 3, 5)));
		m_subCases.push_back(SubCase(UVec3(1, 3, 1), UVec3(1, 1, 1)));
		m_subCases.push_back(SubCase(UVec3(1, 1, 7), UVec3(1, 1, 1)));
		m_subCases.push_back(SubCase(UVec3(1, 1, 7), UVec3(3, 3, 1)));
		m_subCases.push_back(SubCase(UVec3(10, 3, 4), UVec3(1, 1, 1)));
		m_subCases.push_back(SubCase(UVec3(10, 3, 4), UVec3(3, 1, 2)));
	}

	UVec3 computeReference (const UVec3& numWorkGroups, const UVec3& workGroupSize, const UVec3& workGroupID, const UVec3& localInvocationID) const
	{
		DE_UNREF(numWorkGroups);
		DE_UNREF(workGroupID);
		DE_UNREF(localInvocationID);
		return workGroupSize;
	}
};

//-----------------------------------------------------------------------
class WorkGroupIDCase : public ComputeBuiltinVarCase
{
public:
	WorkGroupIDCase (tcu::TestContext& context)
		: ComputeBuiltinVarCase(context, "work_group_id", "gl_WorkGroupID", glu::TYPE_UINT_VEC3)
	{
		m_subCases.push_back(SubCase(UVec3(1, 1, 1), UVec3(1, 1, 1)));
		m_subCases.push_back(SubCase(UVec3(1, 1, 1), UVec3(52, 1, 1)));
		m_subCases.push_back(SubCase(UVec3(1, 1, 1), UVec3(1, 39, 1)));
		m_subCases.push_back(SubCase(UVec3(1, 1, 1), UVec3(1, 1, 78)));
		m_subCases.push_back(SubCase(UVec3(1, 1, 1), UVec3(4, 7, 11)));
		m_subCases.push_back(SubCase(UVec3(2, 3, 4), UVec3(4, 7, 11)));
	}

	UVec3 computeReference (const UVec3& numWorkGroups, const UVec3& workGroupSize, const UVec3& workGroupID, const UVec3& localInvocationID) const
	{
		DE_UNREF(numWorkGroups);
		DE_UNREF(workGroupSize);
		DE_UNREF(localInvocationID);
		return workGroupID;
	}
};

class LocalInvocationIDCase : public ComputeBuiltinVarCase
{
public:
	LocalInvocationIDCase (tcu::TestContext& context)
		: ComputeBuiltinVarCase(context, "local_invocation_id", "gl_LocalInvocationID", glu::TYPE_UINT_VEC3)
	{
		m_subCases.push_back(SubCase(UVec3(1, 1, 1), UVec3(1, 1, 1)));
		m_subCases.push_back(SubCase(UVec3(1, 1, 1), UVec3(2, 7, 3)));
		m_subCases.push_back(SubCase(UVec3(2, 1, 1), UVec3(1, 1, 1)));
		m_subCases.push_back(SubCase(UVec3(2, 1, 1), UVec3(1, 3, 5)));
		m_subCases.push_back(SubCase(UVec3(1, 3, 1), UVec3(1, 1, 1)));
		m_subCases.push_back(SubCase(UVec3(1, 1, 7), UVec3(1, 1, 1)));
		m_subCases.push_back(SubCase(UVec3(1, 1, 7), UVec3(3, 3, 1)));
		m_subCases.push_back(SubCase(UVec3(10, 3, 4), UVec3(1, 1, 1)));
		m_subCases.push_back(SubCase(UVec3(10, 3, 4), UVec3(3, 1, 2)));
	}

	UVec3 computeReference (const UVec3& numWorkGroups, const UVec3& workGroupSize, const UVec3& workGroupID, const UVec3& localInvocationID) const
	{
		DE_UNREF(numWorkGroups);
		DE_UNREF(workGroupSize);
		DE_UNREF(workGroupID);
		return localInvocationID;
	}
};

class GlobalInvocationIDCase : public ComputeBuiltinVarCase
{
public:
	GlobalInvocationIDCase (tcu::TestContext& context)
		: ComputeBuiltinVarCase(context, "global_invocation_id", "gl_GlobalInvocationID", glu::TYPE_UINT_VEC3)
	{
		m_subCases.push_back(SubCase(UVec3(1, 1, 1), UVec3(1, 1, 1)));
		m_subCases.push_back(SubCase(UVec3(1, 1, 1), UVec3(52, 1, 1)));
		m_subCases.push_back(SubCase(UVec3(1, 1, 1), UVec3(1, 39, 1)));
		m_subCases.push_back(SubCase(UVec3(1, 1, 1), UVec3(1, 1, 78)));
		m_subCases.push_back(SubCase(UVec3(1, 1, 1), UVec3(4, 7, 11)));
		m_subCases.push_back(SubCase(UVec3(2, 3, 4), UVec3(4, 7, 11)));
		m_subCases.push_back(SubCase(UVec3(10, 3, 4), UVec3(1, 1, 1)));
		m_subCases.push_back(SubCase(UVec3(10, 3, 4), UVec3(3, 1, 2)));
	}

	UVec3 computeReference (const UVec3& numWorkGroups, const UVec3& workGroupSize, const UVec3& workGroupID, const UVec3& localInvocationID) const
	{
		DE_UNREF(numWorkGroups);
		return workGroupID * workGroupSize + localInvocationID;
	}
};

class LocalInvocationIndexCase : public ComputeBuiltinVarCase
{
public:
	LocalInvocationIndexCase (tcu::TestContext& context)
		: ComputeBuiltinVarCase(context, "local_invocation_index", "gl_LocalInvocationIndex", glu::TYPE_UINT)
	{
		m_subCases.push_back(SubCase(UVec3(1, 1, 1), UVec3(1, 1, 1)));
		m_subCases.push_back(SubCase(UVec3(1, 1, 1), UVec3(1, 39, 1)));
		m_subCases.push_back(SubCase(UVec3(1, 1, 1), UVec3(4, 7, 11)));
		m_subCases.push_back(SubCase(UVec3(2, 3, 4), UVec3(4, 7, 11)));
		m_subCases.push_back(SubCase(UVec3(10, 3, 4), UVec3(1, 1, 1)));
		m_subCases.push_back(SubCase(UVec3(10, 3, 4), UVec3(3, 1, 2)));
	}

	UVec3 computeReference (const UVec3& numWorkGroups, const UVec3& workGroupSize, const UVec3& workGroupID, const UVec3& localInvocationID) const
	{
		DE_UNREF(workGroupID);
		DE_UNREF(numWorkGroups);
		return UVec3(localInvocationID.z()*workGroupSize.x()*workGroupSize.y() + localInvocationID.y()*workGroupSize.x() + localInvocationID.x(), 0, 0);
	}
};

ComputeBuiltinVarInstance::ComputeBuiltinVarInstance (Context&						context,
													  const vector<SubCase>&		subCases,
													  const glu::DataType			varType,
													  const ComputeBuiltinVarCase*	builtinVarCase)
	: vkt::TestInstance		(context)
	, m_device				(m_context.getDevice())
	, m_vki					(m_context.getDeviceInterface())
	, m_queue				(context.getUniversalQueue())
	, m_queueFamilyIndex	(context.getUniversalQueueFamilyIndex())
	, m_subCases			(subCases)
	, m_builtin_var_case	(builtinVarCase)
	, m_subCaseNdx			(0)
	, m_varType				(varType)
{
}

vk::VkDescriptorInfo ComputeBuiltinVarInstance::createDescriptorInfo (vk::VkBuffer buffer, vk::VkDeviceSize offset, vk::VkDeviceSize range)
{
	const vk::VkDescriptorInfo resultInfo =
	{
		0,							// bufferView
		0,							// sampler
		0,							// imageView
		(vk::VkImageLayout)0,		// imageLayout
		{ buffer, offset, range }	// bufferInfo
	};
	return resultInfo;
}

tcu::TestStatus	ComputeBuiltinVarInstance::iterate (void)
{
	std::ostringstream program_name;
	program_name << PrefixProgramName << m_subCaseNdx;

	const SubCase&				subCase				= m_subCases[m_subCaseNdx];
	const tcu::UVec3			globalSize			= subCase.localSize*subCase.numWorkGroups;
	const tcu::UVec2			stride				(globalSize[0] * globalSize[1], globalSize[0]);
	const deUint32				sizeOfUniformBuffer	= sizeof(stride);
	const int					numScalars			= glu::getDataTypeScalarSize(m_varType);
	const deUint32				numInvocations		= subCase.localSize[0] * subCase.localSize[1] * subCase.localSize[2] * subCase.numWorkGroups[0] * subCase.numWorkGroups[1] * subCase.numWorkGroups[2];

	deUint32					resultBufferStride = 0;
	switch (m_varType)
	{
		case glu::TYPE_UINT:
			resultBufferStride = sizeof(deUint32);
			break;
		case glu::TYPE_UINT_VEC2:
			resultBufferStride = sizeof(tcu::UVec2);
			break;
		case glu::TYPE_UINT_VEC3:
		case glu::TYPE_UINT_VEC4:
			resultBufferStride = sizeof(tcu::UVec4);
			break;
		default:
			DE_ASSERT("Illegal data type");
	}

	const deUint32				resultBufferSize	= numInvocations * resultBufferStride;

	/* Create result buffer */
	BufferObject uniformBuffer(m_vki, m_device, m_context.getDefaultAllocator(), (vk::VkDeviceSize)sizeOfUniformBuffer, vk::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	BufferObject resultBuffer(m_vki, m_device, m_context.getDefaultAllocator(), (vk::VkDeviceSize)resultBufferSize, vk::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

	memcpy(uniformBuffer.mapBuffer(),&stride,sizeOfUniformBuffer);
	uniformBuffer.unmapBuffer();

	/* Create descriptorSetLayout */
	vk::DescriptorSetLayoutBuilder layoutBuilder;
	layoutBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT);
	layoutBuilder.addSingleBinding(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vk::VK_SHADER_STAGE_COMPUTE_BIT);
	const vk::Unique<vk::VkDescriptorSetLayout>	descriptorSetLayout = layoutBuilder.build(m_vki, m_device);

	const Unique<VkPipelineLayout> pipelineLayout(makePipelineLayout(m_vki, m_device, 1, &descriptorSetLayout.get()));
	const Unique<VkPipeline> pipeline(makeComputePipeline(m_vki, m_device, m_context.getBinaryCollection(), program_name.str(), *pipelineLayout));

	//Create descriptionPool
	DescriptorPoolBuilder descriptorPool_build;
	descriptorPool_build.addType(vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	descriptorPool_build.addType(vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	const vk::Unique<vk::VkDescriptorPool>descriptorPool = descriptorPool_build.build(m_vki, m_device, vk::VK_DESCRIPTOR_POOL_USAGE_DYNAMIC, 1);

	const vk::VkBufferMemoryBarrier bufferBarrier =
	{
		vk::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
		DE_NULL,
		vk::VK_MEMORY_OUTPUT_SHADER_WRITE_BIT,		// outputMask
		vk::VK_MEMORY_INPUT_HOST_READ_BIT,			// inputMask
		vk::VK_QUEUE_FAMILY_IGNORED,				// srcQueueFamilyIndex
		vk::VK_QUEUE_FAMILY_IGNORED,				// destQueueFamilyIndex
		resultBuffer.getVKBuffer(),					// buffer
		(vk::VkDeviceSize)0u,						// offset
		(vk::VkDeviceSize)resultBufferSize,			// size
	};

	const void* const postBarrier[] = { &bufferBarrier };

	/* Create command buffer */
	CommandBuffer cmdBuffer(m_vki, m_device, m_queueFamilyIndex);

	/* Begin recording commands */
	vk::VkCmdBuffer vkCmdBuffer = cmdBuffer.beginRecordingCommands();

	/* Bind compute pipeline */
	m_vki.cmdBindPipeline(vkCmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline);

	/* Create descriptor set */
	vk::Move<vk::VkDescriptorSet> descriptorSet = allocDescriptorSet(m_vki, m_device, *descriptorPool, vk::VK_DESCRIPTOR_SET_USAGE_ONE_SHOT, *descriptorSetLayout);

	const vk::VkDescriptorInfo		resultDescriptorInfo = createDescriptorInfo(resultBuffer.getVKBuffer(), 0u, (vk::VkDeviceSize)resultBufferSize);
	const vk::VkDescriptorInfo		uniformDescriptorInfo = createDescriptorInfo(uniformBuffer.getVKBuffer(), 0u, (vk::VkDeviceSize)sizeOfUniformBuffer);

	vk::DescriptorSetUpdateBuilder	descriptorSetBuilder;
	descriptorSetBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(0u), vk::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &uniformDescriptorInfo);
	descriptorSetBuilder.writeSingle(*descriptorSet, vk::DescriptorSetUpdateBuilder::Location::binding(1u), vk::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &resultDescriptorInfo);
	descriptorSetBuilder.update(m_vki, m_device);

	/* Bind descriptor set */
	m_vki.cmdBindDescriptorSets(vkCmdBuffer, vk::VK_PIPELINE_BIND_POINT_COMPUTE, *pipelineLayout, 0, 1, &descriptorSet.get(), 0, DE_NULL);

	/* Dispatch indirect compute command */
	m_vki.cmdDispatch(vkCmdBuffer, subCase.numWorkGroups[0], subCase.numWorkGroups[1], subCase.numWorkGroups[2]);

	m_vki.cmdPipelineBarrier(vkCmdBuffer, vk::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, vk::VK_PIPELINE_STAGE_HOST_BIT, vk::VK_FALSE, 1, postBarrier);

	/* End recording commands */
	cmdBuffer.endRecordingCommands();

	/* Create fence object that will allow to wait for command buffer's execution completion */
	Fence cmdBufferFence(m_vki, m_device);

	/* Submit command buffer to queue */
	VK_CHECK(m_vki.queueSubmit(m_queue, 1, &vkCmdBuffer, cmdBufferFence.getVKFence()));

	/* Wait for command buffer execution finish */
	const deUint64		infiniteTimeout = ~(deUint64)0u;
	VK_CHECK(m_vki.waitForFences(m_device, 1, &cmdBufferFence.getVKFence(), 0u, infiniteTimeout));

	const deUint8*	 ptr = resultBuffer.mapBuffer();

	int			numFailed		= 0;
	const int	maxLogPrints	= 10;

	tcu::TestContext& testCtx	= m_context.getTestContext();

	for (deUint32 groupZ = 0; groupZ < subCase.numWorkGroups.z(); groupZ++)
	for (deUint32 groupY = 0; groupY < subCase.numWorkGroups.y(); groupY++)
	for (deUint32 groupX = 0; groupX < subCase.numWorkGroups.x(); groupX++)
	for (deUint32 localZ = 0; localZ < subCase.localSize.z(); localZ++)
	for (deUint32 localY = 0; localY < subCase.localSize.y(); localY++)
	for (deUint32 localX = 0; localX < subCase.localSize.x(); localX++)
	{
		const UVec3			refGroupID(groupX, groupY, groupZ);
		const UVec3			refLocalID(localX, localY, localZ);
		const UVec3			refGlobalID = refGroupID * subCase.localSize + refLocalID;

		const deUint32		refOffset = stride.x()*refGlobalID.z() + stride.y()*refGlobalID.y() + refGlobalID.x();

		const UVec3			refValue = m_builtin_var_case->computeReference(subCase.numWorkGroups, subCase.localSize, refGroupID, refLocalID);

		const deUint32*		resPtr = (const deUint32*)(ptr + refOffset * resultBufferStride);
		const UVec3			resValue = readResultVec(resPtr, numScalars);

		if (!compareNumComponents(refValue, resValue, numScalars))
		{
			if (numFailed < maxLogPrints)
				testCtx.getLog()
				<< TestLog::Message
				<< "ERROR: comparison failed at offset " << refOffset
				<< ": expected " << LogComps(refValue, numScalars)
				<< ", got " << LogComps(resValue, numScalars)
				<< TestLog::EndMessage;
			else if (numFailed == maxLogPrints)
				testCtx.getLog() << TestLog::Message << "..." << TestLog::EndMessage;

			numFailed += 1;
		}
	}

	testCtx.getLog() << TestLog::Message << (numInvocations - numFailed) << " / " << numInvocations << " values passed" << TestLog::EndMessage;

	if (numFailed > 0)
		return tcu::TestStatus::fail("Comparison failed");

	m_subCaseNdx += 1;
	return (m_subCaseNdx < (int)m_subCases.size()) ? tcu::TestStatus::incomplete() :tcu::TestStatus::pass("Comparison succeeded");
}

class ComputeShaderBuiltinVarTests : public tcu::TestCaseGroup
{
public:
			ComputeShaderBuiltinVarTests	(tcu::TestContext& context);

	void	init							(void);

private:
	ComputeShaderBuiltinVarTests (const ComputeShaderBuiltinVarTests& other);
	ComputeShaderBuiltinVarTests& operator= (const ComputeShaderBuiltinVarTests& other);
};

ComputeShaderBuiltinVarTests::ComputeShaderBuiltinVarTests (tcu::TestContext& context)
	: TestCaseGroup(context, "compute", "Compute Shader Builtin Variables")
{
}

void ComputeShaderBuiltinVarTests::init (void)
{
	addChild(new NumWorkGroupsCase(this->getTestContext()));
	addChild(new WorkGroupSizeCase(this->getTestContext()));
	addChild(new WorkGroupIDCase(this->getTestContext()));
	addChild(new LocalInvocationIDCase(this->getTestContext()));
	addChild(new GlobalInvocationIDCase(this->getTestContext()));
	addChild(new LocalInvocationIndexCase(this->getTestContext()));
}

} // anonymous

tcu::TestCaseGroup* createComputeShaderBuiltinVarTests (tcu::TestContext& testCtx)
{
	de::MovePtr<tcu::TestCaseGroup> computeShaderBuiltinVarTests(new tcu::TestCaseGroup(testCtx, "builtin_var", "Shader builtin var tests"));

	computeShaderBuiltinVarTests->addChild(new ComputeShaderBuiltinVarTests(testCtx));

	return computeShaderBuiltinVarTests.release();
}

} // compute
} // vkt
