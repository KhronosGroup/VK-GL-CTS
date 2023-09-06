/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
 * Copyright (c) 2021 Valve Corporation.
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
 * \brief Mesh Shader Synchronization Tests
 *//*--------------------------------------------------------------------*/

#include "vktMeshShaderSyncTests.hpp"
#include "vktMeshShaderUtil.hpp"
#include "vktTestCase.hpp"

#include "vkDefs.hpp"
#include "vkTypeUtil.hpp"
#include "vkImageWithMemory.hpp"
#include "vkBufferWithMemory.hpp"
#include "vkObjUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkImageUtil.hpp"

#include "deUniquePtr.hpp"

#include <iostream>
#include <sstream>
#include <vector>

namespace vkt
{
namespace MeshShader
{

namespace
{

using GroupPtr = de::MovePtr<tcu::TestCaseGroup>;

using namespace vk;

// Stages that will be used in these tests.
enum class Stage
{
	HOST = 0,
	TRANSFER,
	TASK,
	MESH,
	FRAG,
};

std::ostream& operator<< (std::ostream& stream, Stage stage)
{
	switch (stage)
	{
	case Stage::HOST:		stream << "host";		break;
	case Stage::TRANSFER:	stream << "transfer";	break;
	case Stage::TASK:		stream << "task";		break;
	case Stage::MESH:		stream << "mesh";		break;
	case Stage::FRAG:		stream << "frag";		break;
	default: DE_ASSERT(false); break;
	}

	return stream;
}

bool isShaderStage (Stage stage)
{
	return (stage == Stage::TASK || stage == Stage::MESH || stage == Stage::FRAG);
}

VkPipelineStageFlags stageToFlags (Stage stage)
{
	switch (stage)
	{
	case Stage::HOST:		return VK_PIPELINE_STAGE_HOST_BIT;
	case Stage::TRANSFER:	return VK_PIPELINE_STAGE_TRANSFER_BIT;
	case Stage::TASK:		return VK_PIPELINE_STAGE_TASK_SHADER_BIT_NV;
	case Stage::MESH:		return VK_PIPELINE_STAGE_MESH_SHADER_BIT_NV;
	case Stage::FRAG:		return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	default:				DE_ASSERT(false); break;
	}

	// Unreachable.
	DE_ASSERT(false);
	return 0u;
}

VkFormat getImageFormat ()
{
	return VK_FORMAT_R32_UINT;
}

VkExtent3D getImageExtent ()
{
	return makeExtent3D(1u, 1u, 1u);
}

// Types of resources we will use.
enum class ResourceType
{
	UNIFORM_BUFFER = 0,
	STORAGE_BUFFER,
	STORAGE_IMAGE,
	SAMPLED_IMAGE,
};

VkDescriptorType resourceTypeToDescriptor (ResourceType resType)
{
	switch (resType)
	{
	case ResourceType::UNIFORM_BUFFER:	return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	case ResourceType::STORAGE_BUFFER:	return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	case ResourceType::STORAGE_IMAGE:	return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	case ResourceType::SAMPLED_IMAGE:	return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	default:							DE_ASSERT(false); break;
	}

	// Unreachable.
	DE_ASSERT(false);
	return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
}

// Will the test use a specific barrier or a general memory barrier?
enum class BarrierType
{
	GENERAL = 0,
	SPECIFIC,
};

// Types of writes we will use.
enum class WriteAccess
{
	HOST_WRITE = 0,
	TRANSFER_WRITE,
	SHADER_WRITE,
};

VkAccessFlags writeAccessToFlags (WriteAccess access)
{
	switch (access)
	{
	case WriteAccess::HOST_WRITE:		return VK_ACCESS_HOST_WRITE_BIT;
	case WriteAccess::TRANSFER_WRITE:	return VK_ACCESS_TRANSFER_WRITE_BIT;
	case WriteAccess::SHADER_WRITE:		return VK_ACCESS_SHADER_WRITE_BIT;
	default:							DE_ASSERT(false); break;
	}

	// Unreachable.
	DE_ASSERT(false);
	return 0u;
}

// Types of reads we will use.
enum class ReadAccess
{
	HOST_READ = 0,
	TRANSFER_READ,
	SHADER_READ,
	UNIFORM_READ,
};

VkAccessFlags readAccessToFlags (ReadAccess access)
{
	switch (access)
	{
	case ReadAccess::HOST_READ:			return VK_ACCESS_HOST_READ_BIT;
	case ReadAccess::TRANSFER_READ:		return VK_ACCESS_TRANSFER_READ_BIT;
	case ReadAccess::SHADER_READ:		return VK_ACCESS_SHADER_READ_BIT;
	case ReadAccess::UNIFORM_READ:		return VK_ACCESS_UNIFORM_READ_BIT;
	default:							DE_ASSERT(false); break;
	}

	// Unreachable.
	DE_ASSERT(false);
	return 0u;
}

// Auxiliary functions to verify certain combinations are possible.

// Check if the writing stage can use the specified write access.
bool canWriteFromStageAsAccess (Stage writeStage, WriteAccess access)
{
	switch (writeStage)
	{
	case Stage::HOST:		return (access == WriteAccess::HOST_WRITE);
	case Stage::TRANSFER:	return (access == WriteAccess::TRANSFER_WRITE);
	case Stage::TASK:		// fallthrough
	case Stage::MESH:		// fallthrough
	case Stage::FRAG:		return (access == WriteAccess::SHADER_WRITE);
	default:				DE_ASSERT(false); break;
	}

	return false;
}

// Check if the reading stage can use the specified read access.
bool canReadFromStageAsAccess (Stage readStage, ReadAccess access)
{
	switch (readStage)
	{
	case Stage::HOST:		return (access == ReadAccess::HOST_READ);
	case Stage::TRANSFER:	return (access == ReadAccess::TRANSFER_READ);
	case Stage::TASK:		// fallthrough
	case Stage::MESH:		// fallthrough
	case Stage::FRAG:		return (access == ReadAccess::SHADER_READ || access == ReadAccess::UNIFORM_READ);
	default:				DE_ASSERT(false); break;
	}

	return false;
}

// Check if reading the given resource type is possible with the given type of read access.
bool canReadResourceAsAccess (ResourceType resType, ReadAccess access)
{
	if (access == ReadAccess::UNIFORM_READ)
		return (resType == ResourceType::UNIFORM_BUFFER);
	return true;
}

// Check if writing to the given resource type is possible with the given type of write access.
bool canWriteResourceAsAccess (ResourceType resType, WriteAccess access)
{
	if (resType == ResourceType::UNIFORM_BUFFER)
		return (access != WriteAccess::SHADER_WRITE);
	return true;
}

// Check if the given stage can write to the given resource type.
bool canWriteTo (Stage stage, ResourceType resType)
{
	switch (stage)
	{
	case Stage::HOST:		return (resType == ResourceType::UNIFORM_BUFFER || resType == ResourceType::STORAGE_BUFFER);
	case Stage::TRANSFER:	return true;
	case Stage::TASK:		// fallthrough
	case Stage::MESH:		return (resType == ResourceType::STORAGE_BUFFER || resType == ResourceType::STORAGE_IMAGE);
	default:				DE_ASSERT(false); break;
	}

	return false;
}

// Check if the given stage can read from the given resource type.
bool canReadFrom (Stage stage, ResourceType resType)
{
	switch (stage)
	{
	case Stage::HOST:		return (resType == ResourceType::UNIFORM_BUFFER || resType == ResourceType::STORAGE_BUFFER);
	case Stage::TRANSFER:	// fallthrough
	case Stage::TASK:		// fallthrough
	case Stage::MESH:
	case Stage::FRAG:		return true;
	default:				DE_ASSERT(false); break;
	}

	return false;
}

// Will we need to store the test value in an auxiliar buffer to be read?
bool needsAuxiliarSourceBuffer (Stage fromStage, Stage toStage)
{
	DE_UNREF(toStage);
	return (fromStage == Stage::TRANSFER);
}

// Will we need to store the read operation result into an auxiliar buffer to be checked?
bool needsAuxiliarDestBuffer (Stage fromStage, Stage toStage)
{
	DE_UNREF(fromStage);
	return (toStage == Stage::TRANSFER);
}

// Needs any auxiliar buffer for any case?
bool needsAuxiliarBuffer (Stage fromStage, Stage toStage)
{
	return (needsAuxiliarSourceBuffer(fromStage, toStage) || needsAuxiliarDestBuffer(fromStage, toStage));
}

// Will the final value be stored in the auxiliar destination buffer?
bool valueInAuxiliarDestBuffer (Stage toStage)
{
	return (toStage == Stage::TRANSFER);
}

// Will the final value be stored in the resource buffer itself?
bool valueInResourceBuffer (Stage toStage)
{
	return (toStage == Stage::HOST);
}

// Will the final value be stored in the color buffer?
bool valueInColorBuffer (Stage toStage)
{
	return (!valueInAuxiliarDestBuffer(toStage) && !valueInResourceBuffer(toStage));
}

// Image usage flags for the image resource.
VkImageUsageFlags resourceImageUsageFlags (ResourceType resourceType)
{
	VkImageUsageFlags flags = (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

	switch (resourceType)
	{
	case ResourceType::STORAGE_IMAGE:	flags |= VK_IMAGE_USAGE_STORAGE_BIT;	break;
	case ResourceType::SAMPLED_IMAGE:	flags |= VK_IMAGE_USAGE_SAMPLED_BIT;	break;
	default: DE_ASSERT(false); break;
	}

	return flags;
}

// Buffer usage flags for the buffer resource.
VkBufferUsageFlags resourceBufferUsageFlags (ResourceType resourceType)
{
	VkBufferUsageFlags flags = (VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

	switch (resourceType)
	{
	case ResourceType::UNIFORM_BUFFER:	flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;	break;
	case ResourceType::STORAGE_BUFFER:	flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;	break;
	default: DE_ASSERT(false); break;
	}

	return flags;
}

// Is the resource written to and read from a shader stage?
bool readAndWriteFromShaders (Stage fromStage, Stage toStage)
{
	return (isShaderStage(fromStage) && isShaderStage(toStage));
}

struct TestParams
{
	Stage			fromStage;
	Stage			toStage;
	ResourceType	resourceType;
	BarrierType		barrierType;
	WriteAccess		writeAccess;
	ReadAccess		readAccess;
	uint32_t		testValue;

protected:
	bool readsOrWritesIn (Stage stage) const
	{
		DE_ASSERT(fromStage != toStage);
		return (fromStage == stage || toStage == stage);
	}

public:
	bool needsTask () const
	{
		return readsOrWritesIn(Stage::TASK);
	}

	bool readsOrWritesInMesh () const
	{
		return readsOrWritesIn(Stage::MESH);
	}

	std::string getResourceDecl () const
	{
		const auto			imgFormat		= ((resourceType == ResourceType::STORAGE_IMAGE) ? ", r32ui" : "");
		const auto			storagePrefix	= ((writeAccess == WriteAccess::SHADER_WRITE) ? "" : "readonly ");
		std::ostringstream	decl;

		decl << "layout (set=0, binding=0" << imgFormat << ") ";
		switch (resourceType)
		{
		case ResourceType::UNIFORM_BUFFER:	decl << "uniform UniformBuffer { uint value; } ub;";					break;
		case ResourceType::STORAGE_BUFFER:	decl << storagePrefix << "buffer StorageBuffer { uint value; } sb;";	break;
		case ResourceType::STORAGE_IMAGE:	decl << storagePrefix << "uniform uimage2D si;";						break;
		case ResourceType::SAMPLED_IMAGE:	decl << "uniform usampler2D sampled;";									break;
		default:							DE_ASSERT(false);														break;
		}

		decl << "\n";
		return decl.str();
	}

	struct PushConstantStruct
	{
		uint32_t writeVal;
		uint32_t readVal;
	};

	// Get declaration for the "pc" push constant block. Must match the structure above.
	std::string getPushConstantDecl () const
	{
		std::ostringstream pc;
		pc
			<< "layout (push_constant, std430) uniform PushConstantBlock {\n"
			<< "    uint writeVal;\n"
			<< "    uint readVal;\n"
			<< "} pc;\n"
			;
		return pc.str();
	}

	std::string getReadStatement (const std::string& outName) const
	{
		std::ostringstream statement;
		statement << "    if (pc.readVal > 0u) { " << outName << " = ";

		switch (resourceType)
		{
		case ResourceType::UNIFORM_BUFFER:	statement << "ub.value";							break;
		case ResourceType::STORAGE_BUFFER:	statement << "sb.value";							break;
		case ResourceType::STORAGE_IMAGE:	statement << "imageLoad(si, ivec2(0, 0)).x";		break;
		case ResourceType::SAMPLED_IMAGE:	statement << "texture(sampled, vec2(0.5, 0.5)).x";	break;
		default:							DE_ASSERT(false); break;
		}

		statement << "; }\n";
		return statement.str();
	}

	std::string getWriteStatement (const std::string& valueName) const
	{
		std::ostringstream statement;
		statement << "    if (pc.writeVal > 0u) { ";

		switch (resourceType)
		{
		case ResourceType::STORAGE_BUFFER:	statement << "sb.value = " << valueName;											break;
		case ResourceType::STORAGE_IMAGE:	statement << "imageStore(si, ivec2(0, 0), uvec4(" << valueName << ", 0, 0, 0))";	break;
		case ResourceType::UNIFORM_BUFFER:	// fallthrough
		case ResourceType::SAMPLED_IMAGE:	// fallthrough
		default:							DE_ASSERT(false); break;
		}

		statement << "; }\n";
		return statement.str();
	}

	VkShaderStageFlags getResourceShaderStages () const
	{
		VkShaderStageFlags flags = 0u;

		if (fromStage == Stage::TASK || toStage == Stage::TASK)	flags |= VK_SHADER_STAGE_TASK_BIT_NV;
		if (fromStage == Stage::MESH || toStage == Stage::MESH)	flags |= VK_SHADER_STAGE_MESH_BIT_NV;
		if (fromStage == Stage::FRAG || toStage == Stage::FRAG)	flags |= VK_SHADER_STAGE_FRAGMENT_BIT;

		// We assume at least something must be done either on the task or mesh shaders for the tests to be interesting.
		DE_ASSERT((flags & (VK_SHADER_STAGE_TASK_BIT_NV | VK_SHADER_STAGE_MESH_BIT_NV)) != 0u);
		return flags;
	}

	// We'll prefer to keep the image in the general layout if it will be written to from a shader stage or if the barrier is going to be a generic memory barrier.
	bool preferGeneralLayout () const
	{
		return (isShaderStage(fromStage) || (barrierType == BarrierType::GENERAL) || (resourceType == ResourceType::STORAGE_IMAGE));
	}

	// A subpass dependency is needed if both the source and destination stages are shader stages.
	bool needsSubpassDependency () const
	{
		return readAndWriteFromShaders(fromStage, toStage);
	}
};

class MeshShaderSyncCase : public vkt::TestCase
{
public:
					MeshShaderSyncCase		(tcu::TestContext& testCtx, const std::string& name, const std::string& description, const TestParams& params)
						: vkt::TestCase (testCtx, name, description), m_params (params)
						{}

	virtual			~MeshShaderSyncCase		(void) {}

	void			checkSupport			(Context& context) const override;
	void			initPrograms			(vk::SourceCollections& programCollection) const override;
	TestInstance*	createInstance			(Context& context) const override;

protected:
	TestParams		m_params;
};

class MeshShaderSyncInstance : public vkt::TestInstance
{
public:
						MeshShaderSyncInstance	(Context& context, const TestParams& params) : vkt::TestInstance(context), m_params(params) {}
	virtual				~MeshShaderSyncInstance	(void) {}

	tcu::TestStatus		iterate					(void) override;

protected:
	TestParams			m_params;
};

void MeshShaderSyncCase::checkSupport (Context& context) const
{
	checkTaskMeshShaderSupportNV(context, m_params.needsTask(), true);

	if (m_params.writeAccess == WriteAccess::SHADER_WRITE)
	{
		context.requireDeviceCoreFeature(DEVICE_CORE_FEATURE_VERTEX_PIPELINE_STORES_AND_ATOMICS);
	}
}

void MeshShaderSyncCase::initPrograms (vk::SourceCollections& programCollection) const
{
	const bool	needsTaskShader	= m_params.needsTask();
	const auto	valueStr		= de::toString(m_params.testValue);
	const auto	resourceDecl	= m_params.getResourceDecl();
	const auto	pcDecl			= m_params.getPushConstantDecl();

	if (needsTaskShader)
	{

		std::ostringstream task;
		task
			<< "#version 450\n"
			<< "#extension GL_NV_mesh_shader : enable\n"
			<< "\n"
			<< "layout(local_size_x=1) in;\n"
			<< "\n"
			<< "out taskNV TaskData { uint value; } td;\n"
			<< "\n"
			<< resourceDecl
			<< pcDecl
			<< "\n"
			<< "void main ()\n"
			<< "{\n"
			<< "    gl_TaskCountNV = 1u;\n"
			<< "    td.value = 0u;\n"
			<< ((m_params.fromStage == Stage::TASK)	? m_params.getWriteStatement(valueStr)	: "")
			<< ((m_params.toStage == Stage::TASK)	? m_params.getReadStatement("td.value")	: "")
			<< "}\n"
			;
		programCollection.glslSources.add("task") << glu::TaskSource(task.str());
	}

	{
		const bool rwInMesh = m_params.readsOrWritesInMesh();

		std::ostringstream mesh;
		mesh
			<< "#version 450\n"
			<< "#extension GL_NV_mesh_shader : enable\n"
			<< "\n"
			<< "layout(local_size_x=1) in;\n"
			<< "layout(triangles) out;\n"
			<< "layout(max_vertices=3, max_primitives=1) out;\n"
			<< "\n"
			<< (needsTaskShader ? "in taskNV TaskData { uint value; } td;\n" : "")
			<< "layout (location=0) out perprimitiveNV uint primitiveValue[];\n"
			<< "\n"
			<< (rwInMesh ? resourceDecl : "")
			<< (rwInMesh ? pcDecl : "")
			<< "\n"
			<< "void main ()\n"
			<< "{\n"
			<< "    gl_PrimitiveCountNV = 1u;\n"
			<< (needsTaskShader ? "    primitiveValue[0] = td.value;\n" : "")
			<< ((m_params.fromStage == Stage::MESH)	? m_params.getWriteStatement(valueStr)				: "")
			<< ((m_params.toStage == Stage::MESH)	? m_params.getReadStatement("primitiveValue[0]")	: "")
			<< "\n"
			<< "    gl_MeshVerticesNV[0].gl_Position = vec4(-1.0, -1.0, 0.0, 1.0);\n"
			<< "    gl_MeshVerticesNV[1].gl_Position = vec4(-1.0,  3.0, 0.0, 1.0);\n"
			<< "    gl_MeshVerticesNV[2].gl_Position = vec4( 3.0, -1.0, 0.0, 1.0);\n"
			<< "    gl_PrimitiveIndicesNV[0] = 0;\n"
			<< "    gl_PrimitiveIndicesNV[1] = 1;\n"
			<< "    gl_PrimitiveIndicesNV[2] = 2;\n"
			<< "}\n"
			;
		programCollection.glslSources.add("mesh") << glu::MeshSource(mesh.str());
	}

	{
		const bool			readFromFrag = (m_params.toStage == Stage::FRAG);
		std::ostringstream	frag;

		frag
			<< "#version 450\n"
			<< "#extension GL_NV_mesh_shader : enable\n"
			<< "\n"
			<< "layout (location=0) in perprimitiveNV flat uint primitiveValue;\n"
			<< "layout (location=0) out uvec4 outColor;\n"
			<< "\n"
			<< (readFromFrag ? resourceDecl : "")
			<< (readFromFrag ? pcDecl : "")
			<< "\n"
			<< "void main ()\n"
			<< "{\n"
			<< "    outColor = uvec4(primitiveValue, 0, 0, 0);\n"
			<< "    uint readVal = 0u;\n"
			<< (readFromFrag ? m_params.getReadStatement("readVal")	: "")
			<< (readFromFrag ? "    outColor = uvec4(readVal, 0, 0, 0);\n"		: "")
			<< "}\n"
			;
		programCollection.glslSources.add("frag") << glu::FragmentSource(frag.str());
	}
}

TestInstance* MeshShaderSyncCase::createInstance (Context& context) const
{
	return new MeshShaderSyncInstance(context, m_params);
}

// General description behind these tests.
//
//	From				To
//	==============================
//	HOST				TASK			Prepare buffer from host. Only valid for uniform and storage buffers. Read value from task into td.value. Verify color buffer.
//	HOST				MESH			Same situation. Read value from mesh into primitiveValue[0]. Verify color buffer.
//	TRANSFER			TASK			Prepare auxiliary host-coherent source buffer from host. Copy buffer to buffer or buffer to image. Read from task into td.value. Verify color buffer.
//	TRANSFER			MESH			Same initial steps. Read from mesh into primitiveValue[0]. Verify color buffer.
//	TASK				MESH			Write value to buffer or image from task shader. Only valid for storage buffers and images. Read from mesh into primitiveValue[0]. Verify color buffer.
//	TASK				FRAG			Same write procedure and restrictions. Read from frag into outColor. Verify color buffer.
//	TASK				TRANSFER		Same write procedure and restrictions. Prepare auxiliary host-coherent read buffer and copy buffer to buffer or image to buffer. Verify auxiliary buffer.
//	TASK				HOST			Due to From/To restrictions, only valid for storage buffers. Same write procedure. Read and verify buffer directly.
//	MESH				FRAG			Same as task to frag but the write instructions need to be in the mesh shader.
//	MESH				TRANSFER		Same as task to transfer but the write instructions need to be in the mesh shader.
//	MESH				HOST			Same as task to host but the write instructions need to be in the mesh shader.
//

Move<VkRenderPass> createCustomRenderPass (const DeviceInterface& vkd, VkDevice device, VkFormat colorFormat, const TestParams& params)
{
	const std::vector<VkAttachmentDescription> attachmentDescs =
	{
		{
			0u,											//	VkAttachmentDescriptionFlags	flags;
			colorFormat,								//	VkFormat						format;
			VK_SAMPLE_COUNT_1_BIT,						//	VkSampleCountFlagBits			samples;
			VK_ATTACHMENT_LOAD_OP_CLEAR,				//	VkAttachmentLoadOp				loadOp;
			VK_ATTACHMENT_STORE_OP_STORE,				//	VkAttachmentStoreOp				storeOp;
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,			//	VkAttachmentLoadOp				stencilLoadOp;
			VK_ATTACHMENT_STORE_OP_DONT_CARE,			//	VkAttachmentStoreOp				stencilStoreOp;
			VK_IMAGE_LAYOUT_UNDEFINED,					//	VkImageLayout					initialLayout;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,	//	VkImageLayout					finalLayout;
		}
	};

	const std::vector<VkAttachmentReference> attachmentRefs = { { 0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL } };

	const std::vector<VkSubpassDescription> subpassDescs =
	{
		{
			0u,												//	VkSubpassDescriptionFlags		flags;
			VK_PIPELINE_BIND_POINT_GRAPHICS,				//	VkPipelineBindPoint				pipelineBindPoint;
			0u,												//	uint32_t						inputAttachmentCount;
			nullptr,										//	const VkAttachmentReference*	pInputAttachments;
			static_cast<uint32_t>(attachmentRefs.size()),	//	uint32_t						colorAttachmentCount;
			de::dataOrNull(attachmentRefs),					//	const VkAttachmentReference*	pColorAttachments;
			nullptr,										//	const VkAttachmentReference*	pResolveAttachments;
			nullptr,										//	const VkAttachmentReference*	pDepthStencilAttachment;
			0u,												//	uint32_t						preserveAttachmentCount;
			nullptr,										//	const uint32_t*					pPreserveAttachments;
		}
	};

	// When both stages are shader stages, the dependency will be expressed as a subpass dependency.
	std::vector<VkSubpassDependency> dependencies;
	if (params.needsSubpassDependency())
	{
		const VkSubpassDependency dependency =
		{
			0u,											//	uint32_t				srcSubpass;
			0u,											//	uint32_t				dstSubpass;
			stageToFlags(params.fromStage),				//	VkPipelineStageFlags	srcStageMask;
			stageToFlags(params.toStage),				//	VkPipelineStageFlags	dstStageMask;
			writeAccessToFlags(params.writeAccess),		//	VkAccessFlags			srcAccessMask;
			readAccessToFlags(params.readAccess),		//	VkAccessFlags			dstAccessMask;
			0u,											//	VkDependencyFlags		dependencyFlags;
		};
		dependencies.push_back(dependency);
	}

	const VkRenderPassCreateInfo createInfo =
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,		//	VkStructureType					sType;
		nullptr,										//	const void*						pNext;
		0u,												//	VkRenderPassCreateFlags			flags;
		static_cast<uint32_t>(attachmentDescs.size()),	//	uint32_t						attachmentCount;
		de::dataOrNull(attachmentDescs),				//	const VkAttachmentDescription*	pAttachments;
		static_cast<uint32_t>(subpassDescs.size()),		//	uint32_t						subpassCount;
		de::dataOrNull(subpassDescs),					//	const VkSubpassDescription*		pSubpasses;
		static_cast<uint32_t>(dependencies.size()),		//	uint32_t						dependencyCount;
		de::dataOrNull(dependencies),					//	const VkSubpassDependency*		pDependencies;
	};

	return createRenderPass(vkd, device, &createInfo);
}

void hostToTransferMemoryBarrier (const DeviceInterface& vkd, VkCommandBuffer cmdBuffer)
{
	const auto barrier = makeMemoryBarrier(VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
	vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 1u, &barrier, 0u, nullptr, 0u, nullptr);
}

void transferToHostMemoryBarrier (const DeviceInterface& vkd, VkCommandBuffer cmdBuffer)
{
	const auto barrier = makeMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
	vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0u, 1u, &barrier, 0u, nullptr, 0u, nullptr);
}

tcu::TestStatus MeshShaderSyncInstance::iterate (void)
{
	const auto&	vkd						= m_context.getDeviceInterface();
	const auto	device					= m_context.getDevice();
	auto&		alloc					= m_context.getDefaultAllocator();
	const auto	queueIndex				= m_context.getUniversalQueueFamilyIndex();
	const auto	queue					= m_context.getUniversalQueue();

	const auto	imageFormat				= getImageFormat();
	const auto	imageExtent				= getImageExtent();
	const auto	colorBufferUsage		= (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	const auto	colorSRR				= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
	const auto	colorSRL				= makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u);
	const auto	bufferSize				= static_cast<VkDeviceSize>(sizeof(m_params.testValue));
	const auto	descriptorType			= resourceTypeToDescriptor(m_params.resourceType);
	const auto	resourceStages			= m_params.getResourceShaderStages();
	const auto	auxiliarBufferUsage		= (VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	const auto	useGeneralLayout		= m_params.preferGeneralLayout();
	const bool	needsTwoDrawCalls		= m_params.needsSubpassDependency();

	const auto	writeAccessFlags		= writeAccessToFlags(m_params.writeAccess);
	const auto	readAccessFlags			= readAccessToFlags(m_params.readAccess);
	const auto	fromStageFlags			= stageToFlags(m_params.fromStage);
	const auto	toStageFlags			= stageToFlags(m_params.toStage);

	// Prepare color buffer.
	const VkImageCreateInfo colorBufferCreateInfo =
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
		nullptr,								//	const void*				pNext;
		0u,										//	VkImageCreateFlags		flags;
		VK_IMAGE_TYPE_2D,						//	VkImageType				imageType;
		imageFormat,							//	VkFormat				format;
		imageExtent,							//	VkExtent3D				extent;
		1u,										//	uint32_t				mipLevels;
		1u,										//	uint32_t				arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,					//	VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling			tiling;
		colorBufferUsage,						//	VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode			sharingMode;
		0u,										//	uint32_t				queueFamilyIndexCount;
		nullptr,								//	const uint32_t*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout			initialLayout;
	};
	ImageWithMemory	colorBuffer		(vkd, device, alloc, colorBufferCreateInfo, MemoryRequirement::Any);
	const auto		colorBufferView	= makeImageView(vkd, device, colorBuffer.get(), VK_IMAGE_VIEW_TYPE_2D, imageFormat, colorSRR);

	// Main resource.
	using ImageWithMemoryPtr	= de::MovePtr<ImageWithMemory>;
	using BufferWithMemoryPtr	= de::MovePtr<BufferWithMemory>;

	ImageWithMemoryPtr	imageResource;
	Move<VkImageView>	imageResourceView;
	VkImageLayout		imageDescriptorLayout	= (useGeneralLayout ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	VkImageLayout		currentLayout			= VK_IMAGE_LAYOUT_UNDEFINED;
	BufferWithMemoryPtr	bufferResource;

	bool useImageResource	= false;
	bool useBufferResource	= false;

	switch (m_params.resourceType)
	{
	case ResourceType::UNIFORM_BUFFER:
	case ResourceType::STORAGE_BUFFER:
		useBufferResource = true;
		break;
	case ResourceType::STORAGE_IMAGE:
	case ResourceType::SAMPLED_IMAGE:
		useImageResource = true;
		break;
	default:
		DE_ASSERT(false);
		break;
	}

	// One resource needed.
	DE_ASSERT(useImageResource != useBufferResource);

	if (useImageResource)
	{
		const auto resourceImageUsage = resourceImageUsageFlags(m_params.resourceType);

		const VkImageCreateInfo resourceCreateInfo =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,	//	VkStructureType			sType;
			nullptr,								//	const void*				pNext;
			0u,										//	VkImageCreateFlags		flags;
			VK_IMAGE_TYPE_2D,						//	VkImageType				imageType;
			imageFormat,							//	VkFormat				format;
			imageExtent,							//	VkExtent3D				extent;
			1u,										//	uint32_t				mipLevels;
			1u,										//	uint32_t				arrayLayers;
			VK_SAMPLE_COUNT_1_BIT,					//	VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,				//	VkImageTiling			tiling;
			resourceImageUsage,						//	VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,				//	VkSharingMode			sharingMode;
			0u,										//	uint32_t				queueFamilyIndexCount;
			nullptr,								//	const uint32_t*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED,				//	VkImageLayout			initialLayout;
		};
		imageResource		= ImageWithMemoryPtr(new ImageWithMemory(vkd, device, alloc, resourceCreateInfo, MemoryRequirement::Any));
		imageResourceView	= makeImageView(vkd, device, imageResource->get(), VK_IMAGE_VIEW_TYPE_2D, imageFormat, colorSRR);
	}
	else
	{
		const auto resourceBufferUsage		= resourceBufferUsageFlags(m_params.resourceType);
		const auto resourceBufferCreateInfo	= makeBufferCreateInfo(bufferSize, resourceBufferUsage);
		bufferResource = BufferWithMemoryPtr(new BufferWithMemory(vkd, device, alloc, resourceBufferCreateInfo, MemoryRequirement::HostVisible));
	}

	Move<VkSampler> sampler;
	if (descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
	{
		const VkSamplerCreateInfo samplerCreateInfo =
		{
			VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,	//	VkStructureType			sType;
			nullptr,								//	const void*				pNext;
			0u,										//	VkSamplerCreateFlags	flags;
			VK_FILTER_NEAREST,						//	VkFilter				magFilter;
			VK_FILTER_NEAREST,						//	VkFilter				minFilter;
			VK_SAMPLER_MIPMAP_MODE_NEAREST,			//	VkSamplerMipmapMode		mipmapMode;
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,	//	VkSamplerAddressMode	addressModeU;
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,	//	VkSamplerAddressMode	addressModeV;
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,	//	VkSamplerAddressMode	addressModeW;
			0.0f,									//	float					mipLodBias;
			VK_FALSE,								//	VkBool32				anisotropyEnable;
			1.0f,									//	float					maxAnisotropy;
			VK_FALSE,								//	VkBool32				compareEnable;
			VK_COMPARE_OP_NEVER,					//	VkCompareOp				compareOp;
			0.0f,									//	float					minLod;
			0.0f,									//	float					maxLod;
			VK_BORDER_COLOR_INT_TRANSPARENT_BLACK,	//	VkBorderColor			borderColor;
			VK_FALSE,								//	VkBool32				unnormalizedCoordinates;
		};
		sampler = createSampler(vkd, device, &samplerCreateInfo);
	}

	// Auxiliary host-coherent buffer for some cases. Being host-coherent lets us avoid extra barriers that would "pollute" synchronization tests.
	BufferWithMemoryPtr hostCoherentBuffer;
	void*				hostCoherentDataPtr = nullptr;
	if (needsAuxiliarBuffer(m_params.fromStage, m_params.toStage))
	{
		const auto auxiliarBufferCreateInfo = makeBufferCreateInfo(bufferSize, auxiliarBufferUsage);
		hostCoherentBuffer	= BufferWithMemoryPtr(new BufferWithMemory(vkd, device, alloc, auxiliarBufferCreateInfo, (MemoryRequirement::HostVisible | MemoryRequirement::Coherent)));
		hostCoherentDataPtr	= hostCoherentBuffer->getAllocation().getHostPtr();
	}

	// Descriptor pool.
	Move<VkDescriptorPool> descriptorPool;
	{
		DescriptorPoolBuilder poolBuilder;
		poolBuilder.addType(descriptorType);
		descriptorPool = poolBuilder.build(vkd, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);
	}

	// Descriptor set layout.
	Move<VkDescriptorSetLayout> setLayout;
	{
		DescriptorSetLayoutBuilder layoutBuilder;
		layoutBuilder.addSingleBinding(descriptorType, resourceStages);
		setLayout = layoutBuilder.build(vkd, device);
	}

	// Descriptor set.
	const auto descriptorSet = makeDescriptorSet(vkd, device, descriptorPool.get(), setLayout.get());

	// Update descriptor set.
	{
		DescriptorSetUpdateBuilder	updateBuilder;
		const auto					location = DescriptorSetUpdateBuilder::Location::binding(0u);

		switch (descriptorType)
		{
		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
		case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
			{
				const auto bufferInfo = makeDescriptorBufferInfo(bufferResource->get(), 0ull, bufferSize);
				updateBuilder.writeSingle(descriptorSet.get(), location, descriptorType, &bufferInfo);
			}
			break;
		case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
		case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
			{
				auto descriptorImageInfo = makeDescriptorImageInfo(sampler.get(), imageResourceView.get(), imageDescriptorLayout);
				updateBuilder.writeSingle(descriptorSet.get(), location, descriptorType, &descriptorImageInfo);
			}
			break;
		default:
			DE_ASSERT(false); break;
		}

		updateBuilder.update(vkd, device);
	}

	// Shader modules.
	Move<VkShaderModule> taskShader;
	Move<VkShaderModule> meshShader;
	Move<VkShaderModule> fragShader;

	const auto& binaries = m_context.getBinaryCollection();

	if (m_params.needsTask())
		taskShader = createShaderModule(vkd, device, binaries.get("task"), 0u);
	meshShader = createShaderModule(vkd, device, binaries.get("mesh"), 0u);
	fragShader = createShaderModule(vkd, device, binaries.get("frag"), 0u);

	using PushConstantStruct = TestParams::PushConstantStruct;

	// Pipeline layout, render pass, framebuffer.
	const auto pcSize			= static_cast<uint32_t>(sizeof(PushConstantStruct));
	const auto pcRange			= makePushConstantRange(resourceStages, 0u, pcSize);
	const auto pipelineLayout	= makePipelineLayout(vkd, device, setLayout.get(), &pcRange);
	const auto renderPass		= createCustomRenderPass(vkd, device, imageFormat, m_params);
	const auto framebuffer		= makeFramebuffer(vkd, device, renderPass.get(), colorBufferView.get(), imageExtent.width, imageExtent.height);

	// Pipeline.
	std::vector<VkViewport>	viewports	(1u, makeViewport(imageExtent));
	std::vector<VkRect2D>	scissors	(1u, makeRect2D(imageExtent));
	const auto				pipeline	= makeGraphicsPipeline(vkd, device, pipelineLayout.get(), taskShader.get(), meshShader.get(), fragShader.get(), renderPass.get(), viewports, scissors);

	// Command pool and buffer.
	const auto cmdPool		= makeCommandPool(vkd, device, queueIndex);
	const auto cmdBufferPtr	= allocateCommandBuffer(vkd, device, cmdPool.get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	const auto cmdBuffer	= cmdBufferPtr.get();

	beginCommandBuffer(vkd, cmdBuffer);

	if (m_params.fromStage == Stage::HOST)
	{
		// Prepare buffer from host when the source stage is the host.
		DE_ASSERT(useBufferResource);

		auto& resourceBufferAlloc	= bufferResource->getAllocation();
		void* resourceBufferDataPtr	= resourceBufferAlloc.getHostPtr();

		deMemcpy(resourceBufferDataPtr, &m_params.testValue, sizeof(m_params.testValue));
		flushAlloc(vkd, device, resourceBufferAlloc);
	}
	else if (m_params.fromStage == Stage::TRANSFER)
	{
		// Put value in host-coherent buffer and transfer it to the resource buffer or image.
		deMemcpy(hostCoherentDataPtr, &m_params.testValue, sizeof(m_params.testValue));
		hostToTransferMemoryBarrier(vkd, cmdBuffer);

		if (useBufferResource)
		{
			const auto copyRegion = makeBufferCopy(0ull, 0ull, bufferSize);
			vkd.cmdCopyBuffer(cmdBuffer, hostCoherentBuffer->get(), bufferResource->get(), 1u, &copyRegion);
		}
		else
		{
			// Move image to the right layout for transfer.
			const auto newLayout = (useGeneralLayout ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			if (newLayout != currentLayout)
			{
				const auto preCopyBarrier = makeImageMemoryBarrier(0u, VK_ACCESS_TRANSFER_WRITE_BIT, currentLayout, newLayout, imageResource->get(), colorSRR);
				vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u, &preCopyBarrier);
				currentLayout = newLayout;
			}
			const auto copyRegion = makeBufferImageCopy(imageExtent, colorSRL);
			vkd.cmdCopyBufferToImage(cmdBuffer, hostCoherentBuffer->get(), imageResource->get(), currentLayout, 1u, &copyRegion);
		}
	}
	else if (m_params.fromStage == Stage::TASK || m_params.fromStage == Stage::MESH)
	{
		// The image or buffer will be written to from shaders. Images need to be in the right layout.
		if (useImageResource)
		{
			const auto newLayout = VK_IMAGE_LAYOUT_GENERAL;
			if (newLayout != currentLayout)
			{
				const auto preWriteBarrier = makeImageMemoryBarrier(0u, (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT), currentLayout, newLayout, imageResource->get(), colorSRR);
				vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, fromStageFlags, 0u, 0u, nullptr, 0u, nullptr, 1u, &preWriteBarrier);
				currentLayout = newLayout;
			}
		}
	}
	else
	{
		DE_ASSERT(false);
	}

	// If the resource is going to be read from shaders, we'll insert the main barrier before running the pipeline.
	if (isShaderStage(m_params.toStage) && !needsTwoDrawCalls)
	{
		if (m_params.barrierType == BarrierType::GENERAL)
		{
			const auto memoryBarrier = makeMemoryBarrier(writeAccessFlags, readAccessFlags);
			vkd.cmdPipelineBarrier(cmdBuffer, fromStageFlags, toStageFlags, 0u, 1u, &memoryBarrier, 0u, nullptr, 0u, nullptr);
		}
		else if (m_params.barrierType == BarrierType::SPECIFIC)
		{
			if (useBufferResource)
			{
				const auto bufferBarrier = makeBufferMemoryBarrier(writeAccessFlags, readAccessFlags, bufferResource->get(), 0ull, bufferSize);
				vkd.cmdPipelineBarrier(cmdBuffer, fromStageFlags, toStageFlags, 0u, 0u, nullptr, 1u, &bufferBarrier, 0u, nullptr);
			}
			else
			{
				const auto newLayout	= (useGeneralLayout ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
				const auto imageBarrier	= makeImageMemoryBarrier(writeAccessFlags, readAccessFlags, currentLayout, newLayout, imageResource->get(), colorSRR);

				vkd.cmdPipelineBarrier(cmdBuffer, fromStageFlags, toStageFlags, 0u, 0u, nullptr, 0u, nullptr, 1u, &imageBarrier);
				currentLayout = newLayout;
			}
		}
		else
		{
			DE_ASSERT(false);
		}
	}

	if (needsTwoDrawCalls)
	{
		// Transition image to the general layout before writing to it. When we need two draw calls (because the image will be
		// written to and read from a shader stage), the layout will always be general.
		if (useImageResource)
		{
			const auto newLayout	= VK_IMAGE_LAYOUT_GENERAL;
			const auto imageBarrier	= makeImageMemoryBarrier(0u, writeAccessFlags, currentLayout, newLayout, imageResource->get(), colorSRR);

			vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, fromStageFlags, 0u, 0u, nullptr, 0u, nullptr, 1u, &imageBarrier);
			currentLayout = newLayout;
		}
	}

	// Run the pipeline.
	beginRenderPass(vkd, cmdBuffer, renderPass.get(), framebuffer.get(), scissors.at(0), tcu::UVec4(0u));
	vkd.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), 0u, 1u, &descriptorSet.get(), 0u, nullptr);
	vkd.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());
	if (needsTwoDrawCalls)
	{
		// The first draw call will write to the resource and the second one will read from the resource.
		PushConstantStruct pcData;

		pcData.writeVal	= 1u;
		pcData.readVal	= 0u;

		vkd.cmdPushConstants(cmdBuffer, pipelineLayout.get(), resourceStages, 0u, pcSize, &pcData);
		vkd.cmdDrawMeshTasksNV(cmdBuffer, 1u, 0u);

		// Use a barrier between both draw calls. The barrier must be generic because:
		//    * VUID-vkCmdPipelineBarrier-bufferMemoryBarrierCount-01178 forbids using buffer barriers inside render passes.
		//    * VUID-vkCmdPipelineBarrier-image-04073 forbids using image memory barriers inside render passes with resources that are not attachments.
		if (m_params.barrierType == BarrierType::GENERAL)
		{
			const auto memoryBarrier = makeMemoryBarrier(writeAccessFlags, readAccessFlags);
			vkd.cmdPipelineBarrier(cmdBuffer, fromStageFlags, toStageFlags, 0u, 1u, &memoryBarrier, 0u, nullptr, 0u, nullptr);
		}
		else
		{
			DE_ASSERT(false);
		}

		pcData.writeVal	= 0u;
		pcData.readVal	= 1u;

		vkd.cmdPushConstants(cmdBuffer, pipelineLayout.get(), resourceStages, 0u, pcSize, &pcData);
		vkd.cmdDrawMeshTasksNV(cmdBuffer, 1u, 0u);
	}
	else
	{
		PushConstantStruct pcData;
		pcData.writeVal	= 1u;
		pcData.readVal	= 1u;

		vkd.cmdPushConstants(cmdBuffer, pipelineLayout.get(), resourceStages, 0u, pcSize, &pcData);
		vkd.cmdDrawMeshTasksNV(cmdBuffer, 1u, 0u);
	}
	endRenderPass(vkd, cmdBuffer);

	// If the resource was written to from the shaders, insert the main barrier after running the pipeline.
	if (isShaderStage(m_params.fromStage) && !needsTwoDrawCalls)
	{
		if (m_params.barrierType == BarrierType::GENERAL)
		{
			const auto memoryBarrier = makeMemoryBarrier(writeAccessFlags, readAccessFlags);
			vkd.cmdPipelineBarrier(cmdBuffer, fromStageFlags, toStageFlags, 0u, 1u, &memoryBarrier, 0u, nullptr, 0u, nullptr);
		}
		else if (m_params.barrierType == BarrierType::SPECIFIC)
		{
			if (useBufferResource)
			{
				const auto bufferBarrier = makeBufferMemoryBarrier(writeAccessFlags, readAccessFlags, bufferResource->get(), 0ull, bufferSize);
				vkd.cmdPipelineBarrier(cmdBuffer, fromStageFlags, toStageFlags, 0u, 0u, nullptr, 1u, &bufferBarrier, 0u, nullptr);
			}
			else
			{
				// Note: the image will only be read from shader stages (which is covered in BarrierType::DEPENDENCY) or from the transfer stage.
				const auto newLayout	= (useGeneralLayout ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
				const auto imageBarrier	= makeImageMemoryBarrier(writeAccessFlags, readAccessFlags, currentLayout, newLayout, imageResource->get(), colorSRR);

				vkd.cmdPipelineBarrier(cmdBuffer, fromStageFlags, toStageFlags, 0u, 0u, nullptr, 0u, nullptr, 1u, &imageBarrier);
				currentLayout = newLayout;
			}
		}
		// For subpass dependencies, they have already been included in the render pass.
	}

	// Read resource from the destination stage if needed.
	if (m_params.toStage == Stage::HOST)
	{
		// Nothing to do. The test value should be in the resource buffer already, which is host-visible.
	}
	else if (m_params.toStage == Stage::TRANSFER)
	{
		// Copy value from resource to host-coherent buffer to be verified later.
		if (useBufferResource)
		{
			const auto copyRegion = makeBufferCopy(0ull, 0ull, bufferSize);
			vkd.cmdCopyBuffer(cmdBuffer, bufferResource->get(), hostCoherentBuffer->get(), 1u, &copyRegion);
		}
		else
		{
			const auto copyRegion = makeBufferImageCopy(imageExtent, colorSRL);
			vkd.cmdCopyImageToBuffer(cmdBuffer, imageResource->get(), currentLayout, hostCoherentBuffer->get(), 1u, &copyRegion);
		}

		transferToHostMemoryBarrier(vkd, cmdBuffer);
	}

	// If the output value will be available in the color buffer, take the chance to transfer its contents to a host-coherent buffer.
	BufferWithMemoryPtr colorVerificationBuffer;
	void*				colorVerificationDataPtr = nullptr;

	if (valueInColorBuffer(m_params.toStage))
	{
		const auto auxiliarBufferCreateInfo = makeBufferCreateInfo(bufferSize, auxiliarBufferUsage);
		colorVerificationBuffer		= BufferWithMemoryPtr(new BufferWithMemory(vkd, device, alloc, auxiliarBufferCreateInfo, (MemoryRequirement::HostVisible | MemoryRequirement::Coherent)));
		colorVerificationDataPtr	= colorVerificationBuffer->getAllocation().getHostPtr();

		const auto srcAccess	= (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
		const auto dstAccess	= VK_ACCESS_TRANSFER_READ_BIT;
		const auto colorBarrier	= makeImageMemoryBarrier(srcAccess, dstAccess, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, colorBuffer.get(), colorSRR);
		vkd.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u, &colorBarrier);

		const auto copyRegion = makeBufferImageCopy(imageExtent, colorSRL);
		vkd.cmdCopyImageToBuffer(cmdBuffer, colorBuffer.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, colorVerificationBuffer->get(), 1u, &copyRegion);

		transferToHostMemoryBarrier(vkd, cmdBuffer);
	}


	endCommandBuffer(vkd, cmdBuffer);
	submitCommandsAndWait(vkd, device, queue, cmdBuffer);

	// Verify output resources as needed.

	if (valueInAuxiliarDestBuffer(m_params.toStage))
	{
		uint32_t bufferValue;
		deMemcpy(&bufferValue, hostCoherentDataPtr, sizeof(bufferValue));

		if (bufferValue != m_params.testValue)
		{
			std::ostringstream msg;
			msg << "Unexpected value in auxiliar host-coherent buffer: found " << bufferValue << " and expected " << m_params.testValue;
			TCU_FAIL(msg.str());
		}
	}

	if (valueInResourceBuffer(m_params.toStage))
	{
		auto&		resourceBufferAlloc		= bufferResource->getAllocation();
		void*		resourceBufferDataPtr	= resourceBufferAlloc.getHostPtr();
		uint32_t	bufferValue;

		invalidateAlloc(vkd, device, resourceBufferAlloc);
		deMemcpy(&bufferValue, resourceBufferDataPtr, sizeof(bufferValue));

		if (bufferValue != m_params.testValue)
		{
			std::ostringstream msg;
			msg << "Unexpected value in resource buffer: found " << bufferValue << " and expected " << m_params.testValue;
			TCU_FAIL(msg.str());
		}
	}

	if (valueInColorBuffer(m_params.toStage))
	{
		uint32_t bufferValue;
		deMemcpy(&bufferValue, colorVerificationDataPtr, sizeof(bufferValue));

		if (bufferValue != m_params.testValue)
		{
			std::ostringstream msg;
			msg << "Unexpected value in color verification buffer: found " << bufferValue << " and expected " << m_params.testValue;
			TCU_FAIL(msg.str());
		}
	}

	return tcu::TestStatus::pass("Pass");
}

} // anonymous

tcu::TestCaseGroup* createMeshShaderSyncTests (tcu::TestContext& testCtx)
{
	const struct
	{
		Stage		fromStage;
		Stage		toStage;
	} stageCombinations[] =
	{
		// Combinations where the source and destination stages involve mesh shaders.
		// Note: this could be tested procedurally.
		{	Stage::HOST,		Stage::TASK			},
		{	Stage::HOST,		Stage::MESH			},
		{	Stage::TRANSFER,	Stage::TASK			},
		{	Stage::TRANSFER,	Stage::MESH			},
		{	Stage::TASK,		Stage::MESH			},
		{	Stage::TASK,		Stage::FRAG			},
		{	Stage::TASK,		Stage::TRANSFER		},
		{	Stage::TASK,		Stage::HOST			},
		{	Stage::MESH,		Stage::FRAG			},
		{	Stage::MESH,		Stage::TRANSFER		},
		{	Stage::MESH,		Stage::HOST			},
	};

	const struct
	{
		ResourceType	resourceType;
		const char*		name;
	} resourceTypes[] =
	{
		{ ResourceType::UNIFORM_BUFFER,	"uniform_buffer"	},
		{ ResourceType::STORAGE_BUFFER,	"storage_buffer"	},
		{ ResourceType::STORAGE_IMAGE,	"storage_image"		},
		{ ResourceType::SAMPLED_IMAGE,	"sampled_image"		},
	};

	const struct
	{
		BarrierType		barrierType;
		const char*		name;
	} barrierTypes[] =
	{
		{	BarrierType::GENERAL,		"memory_barrier"		},
		{	BarrierType::SPECIFIC,		"specific_barrier"		},
	};

	const struct
	{
		WriteAccess		writeAccess;
		const char*		name;
	} writeAccesses[] =
	{
		{	WriteAccess::HOST_WRITE,		"host_write"		},
		{	WriteAccess::TRANSFER_WRITE,	"transfer_write"	},
		{	WriteAccess::SHADER_WRITE,		"shader_write"		},
	};

	const struct
	{
		ReadAccess		readAccess;
		const char*		name;
	} readAccesses[] =
	{
		{	ReadAccess::HOST_READ,		"host_read"		},
		{	ReadAccess::TRANSFER_READ,	"transfer_read"	},
		{	ReadAccess::SHADER_READ,	"shader_read"	},
		{	ReadAccess::UNIFORM_READ,	"uniform_read"	},
	};

	uint32_t testValue = 1628510124u;

	GroupPtr mainGroup (new tcu::TestCaseGroup(testCtx, "synchronization", "Mesh Shader synchronization tests"));

	for (const auto& stageCombination : stageCombinations)
	{
		const std::string	combinationName		= de::toString(stageCombination.fromStage) + "_to_" + de::toString(stageCombination.toStage);
		GroupPtr			combinationGroup	(new tcu::TestCaseGroup(testCtx, combinationName.c_str(), ""));

		for (const auto& resourceCase : resourceTypes)
		{
			if (!canWriteTo(stageCombination.fromStage, resourceCase.resourceType))
				continue;

			if (!canReadFrom(stageCombination.toStage, resourceCase.resourceType))
				continue;

			GroupPtr resourceGroup (new tcu::TestCaseGroup(testCtx, resourceCase.name, ""));

			for (const auto& barrierCase : barrierTypes)
			{
				// See note above about VUID-vkCmdPipelineBarrier-bufferMemoryBarrierCount-01178 and VUID-vkCmdPipelineBarrier-image-04073.
				if (readAndWriteFromShaders(stageCombination.fromStage, stageCombination.toStage) && barrierCase.barrierType == BarrierType::SPECIFIC)
					continue;

				GroupPtr barrierGroup (new tcu::TestCaseGroup(testCtx, barrierCase.name, ""));

				for (const auto& writeCase	: writeAccesses)
				for (const auto& readCase	: readAccesses)
				{
					if (!canReadResourceAsAccess(resourceCase.resourceType, readCase.readAccess))
						continue;
					if (!canWriteResourceAsAccess(resourceCase.resourceType, writeCase.writeAccess))
						continue;
					if (!canReadFromStageAsAccess(stageCombination.toStage, readCase.readAccess))
						continue;
					if (!canWriteFromStageAsAccess(stageCombination.fromStage, writeCase.writeAccess))
						continue;

					const std::string accessCaseName = writeCase.name + std::string("_") + readCase.name;

					const TestParams testParams =
					{
						stageCombination.fromStage,	//	Stage			fromStage;
						stageCombination.toStage,	//	Stage			toStage;
						resourceCase.resourceType,	//	ResourceType	resourceType;
						barrierCase.barrierType,	//	BarrierType		barrierType;
						writeCase.writeAccess,		//	WriteAccess		writeAccess;
						readCase.readAccess,		//	ReadAccess		readAccess;
						testValue++,				//	uint32_t		testValue;
					};

					barrierGroup->addChild(new MeshShaderSyncCase(testCtx, accessCaseName, "", testParams));
				}

				resourceGroup->addChild(barrierGroup.release());
			}

			combinationGroup->addChild(resourceGroup.release());
		}

		mainGroup->addChild(combinationGroup.release());
	}

	return mainGroup.release();
}

} // MeshShader
} // vkt
