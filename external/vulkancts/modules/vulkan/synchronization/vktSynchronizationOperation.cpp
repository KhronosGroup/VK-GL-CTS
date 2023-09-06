/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
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
 * \brief Synchronization operation abstraction
 *//*--------------------------------------------------------------------*/

#include "vktSynchronizationOperation.hpp"
#include "vkDefs.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkBarrierUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkTypeUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkObjUtil.hpp"
#include "deUniquePtr.hpp"
#include "tcuTestLog.hpp"
#include "tcuTextureUtil.hpp"
#include <vector>
#include <sstream>

namespace vkt
{
namespace synchronization
{
namespace
{
using namespace vk;

enum Constants
{
	MAX_IMAGE_DIMENSION_2D	= 0x1000u,
	MAX_UBO_RANGE			= 0x4000u,
	MAX_UPDATE_BUFFER_SIZE	= 0x10000u,
};

enum BufferType
{
	BUFFER_TYPE_UNIFORM,
	BUFFER_TYPE_STORAGE,
	BUFFER_TYPE_UNIFORM_TEXEL,
};

enum AccessMode
{
	ACCESS_MODE_READ,
	ACCESS_MODE_WRITE,
};

enum PipelineType
{
	PIPELINE_TYPE_GRAPHICS,
	PIPELINE_TYPE_COMPUTE,
};

static const char* const s_perVertexBlock =	"gl_PerVertex {\n"
											"    vec4 gl_Position;\n"
											"}";

static const SyncInfo emptySyncInfo =
{
	0,							// VkPipelineStageFlags		stageMask;
	0,							// VkAccessFlags			accessMask;
	VK_IMAGE_LAYOUT_UNDEFINED,	// VkImageLayout			imageLayout;
};

std::string getShaderStageName(VkShaderStageFlagBits stage)
{
	switch (stage)
	{
		default:
			DE_FATAL("Unhandled stage!");
			return "";
		case VK_SHADER_STAGE_COMPUTE_BIT:
			return "compute";
		case VK_SHADER_STAGE_FRAGMENT_BIT:
			return "fragment";
		case VK_SHADER_STAGE_VERTEX_BIT:
			return "vertex";
		case VK_SHADER_STAGE_GEOMETRY_BIT:
			return "geometry";
		case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
			return "tess_control";
		case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
			return "tess_eval";
	}
}

//! A pipeline that can be embedded inside an operation.
class Pipeline
{
public:
	virtual			~Pipeline		(void) {}
	virtual void	recordCommands	(OperationContext& context, const VkCommandBuffer cmdBuffer, const VkDescriptorSet descriptorSet) = 0;
};

//! Vertex data that covers the whole viewport with two triangles.
class VertexGrid
{
public:
	VertexGrid (OperationContext& context)
		: m_vertexFormat (VK_FORMAT_R32G32B32A32_SFLOAT)
		, m_vertexStride (tcu::getPixelSize(mapVkFormat(m_vertexFormat)))
	{
		const DeviceInterface&	vk			= context.getDeviceInterface();
		const VkDevice			device		= context.getDevice();
		Allocator&				allocator	= context.getAllocator();

		// Vertex positions
		{
			m_vertexData.push_back(tcu::Vec4( 1.0f,  1.0f, 0.0f, 1.0f));
			m_vertexData.push_back(tcu::Vec4(-1.0f,  1.0f, 0.0f, 1.0f));
			m_vertexData.push_back(tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f));

			m_vertexData.push_back(tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f));
			m_vertexData.push_back(tcu::Vec4( 1.0f, -1.0f, 0.0f, 1.0f));
			m_vertexData.push_back(tcu::Vec4( 1.0f,  1.0f, 0.0f, 1.0f));
		}

		{
			const VkDeviceSize vertexDataSizeBytes = m_vertexData.size() * sizeof(m_vertexData[0]);

			m_vertexBuffer = de::MovePtr<Buffer>(new Buffer(vk, device, allocator, makeBufferCreateInfo(vertexDataSizeBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT), MemoryRequirement::HostVisible));
			DE_ASSERT(sizeof(m_vertexData[0]) == m_vertexStride);

			{
				const Allocation& alloc = m_vertexBuffer->getAllocation();

				deMemcpy(alloc.getHostPtr(), &m_vertexData[0], static_cast<std::size_t>(vertexDataSizeBytes));
				flushAlloc(vk, device, alloc);
			}
		}

		// Indices
		{
			const VkDeviceSize	indexBufferSizeBytes	= sizeof(deUint32) * m_vertexData.size();
			const deUint32		numIndices				= static_cast<deUint32>(m_vertexData.size());

			m_indexBuffer = de::MovePtr<Buffer>(new Buffer(vk, device, allocator, makeBufferCreateInfo(indexBufferSizeBytes, VK_BUFFER_USAGE_INDEX_BUFFER_BIT), MemoryRequirement::HostVisible));

			{
				const Allocation&	alloc	= m_indexBuffer->getAllocation();
				deUint32* const		pData	= static_cast<deUint32*>(alloc.getHostPtr());

				for (deUint32 i = 0; i < numIndices; ++i)
					pData[i] = i;

				flushAlloc(vk, device, alloc);
			}
		}
	}

	VkFormat	getVertexFormat		(void) const { return m_vertexFormat; }
	deUint32	getVertexStride		(void) const { return m_vertexStride; }
	VkIndexType getIndexType		(void) const { return VK_INDEX_TYPE_UINT32; }
	deUint32	getNumVertices		(void) const { return static_cast<deUint32>(m_vertexData.size()); }
	deUint32	getNumIndices		(void) const { return getNumVertices(); }
	VkBuffer	getVertexBuffer		(void) const { return **m_vertexBuffer; }
	VkBuffer	getIndexBuffer		(void) const { return **m_indexBuffer; }

private:
	const VkFormat				m_vertexFormat;
	const deUint32				m_vertexStride;
	std::vector<tcu::Vec4>		m_vertexData;
	de::MovePtr<Buffer>			m_vertexBuffer;
	de::MovePtr<Buffer>			m_indexBuffer;
};

//! Add flags for all shader stages required to support a particular stage (e.g. fragment requires vertex as well).
VkShaderStageFlags getRequiredStages (const VkShaderStageFlagBits stage)
{
	VkShaderStageFlags flags = 0;

	DE_ASSERT(stage == VK_SHADER_STAGE_COMPUTE_BIT || (stage & VK_SHADER_STAGE_COMPUTE_BIT) == 0);

	if (stage & VK_SHADER_STAGE_ALL_GRAPHICS)
		flags |= VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

	if (stage & (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT))
		flags |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;

	if (stage & VK_SHADER_STAGE_GEOMETRY_BIT)
		flags |= VK_SHADER_STAGE_GEOMETRY_BIT;

	if (stage & VK_SHADER_STAGE_COMPUTE_BIT)
		flags |= VK_SHADER_STAGE_COMPUTE_BIT;

	return flags;
}

//! Check that SSBO read/write is available and that all shader stages are supported.
void requireFeaturesForSSBOAccess (OperationContext& context, const VkShaderStageFlags usedStages)
{
	const InstanceInterface&	vki			= context.getInstanceInterface();
	const VkPhysicalDevice		physDevice	= context.getPhysicalDevice();
	FeatureFlags				flags		= (FeatureFlags)0;

	if (usedStages & VK_SHADER_STAGE_FRAGMENT_BIT)
		flags |= FEATURE_FRAGMENT_STORES_AND_ATOMICS;

	if (usedStages & (VK_SHADER_STAGE_ALL_GRAPHICS & (~VK_SHADER_STAGE_FRAGMENT_BIT)))
		flags |= FEATURE_VERTEX_PIPELINE_STORES_AND_ATOMICS;

	if (usedStages & VK_SHADER_STAGE_GEOMETRY_BIT)
		flags |= FEATURE_GEOMETRY_SHADER;

	if (usedStages & (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT))
		flags |= FEATURE_TESSELLATION_SHADER;

	requireFeatures(vki, physDevice, flags);
}

Data getHostBufferData (const OperationContext& context, const Buffer& hostBuffer, const VkDeviceSize size)
{
	const DeviceInterface&	vk		= context.getDeviceInterface();
	const VkDevice			device	= context.getDevice();
	const Allocation&		alloc	= hostBuffer.getAllocation();
	const Data				data	=
	{
		static_cast<std::size_t>(size),					// std::size_t		size;
		static_cast<deUint8*>(alloc.getHostPtr()),		// const deUint8*	data;
	};

	invalidateAlloc(vk, device, alloc);

	return data;
}

void setHostBufferData (const OperationContext& context, const Buffer& hostBuffer, const Data& data)
{
	const DeviceInterface&	vk		= context.getDeviceInterface();
	const VkDevice			device	= context.getDevice();
	const Allocation&		alloc	= hostBuffer.getAllocation();

	deMemcpy(alloc.getHostPtr(), data.data, data.size);
	flushAlloc(vk, device, alloc);
}

void assertValidShaderStage (const VkShaderStageFlagBits stage)
{
	switch (stage)
	{
		case VK_SHADER_STAGE_VERTEX_BIT:
		case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
		case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
		case VK_SHADER_STAGE_GEOMETRY_BIT:
		case VK_SHADER_STAGE_FRAGMENT_BIT:
		case VK_SHADER_STAGE_COMPUTE_BIT:
			// OK
			break;

		default:
			DE_FATAL("Invalid shader stage");
			break;
	}
}

VkPipelineStageFlags pipelineStageFlagsFromShaderStageFlagBits (const VkShaderStageFlagBits shaderStage)
{
	switch (shaderStage)
	{
		case VK_SHADER_STAGE_VERTEX_BIT:					return VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT_KHR;
		case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:		return VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT_KHR;
		case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:	return VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT_KHR;
		case VK_SHADER_STAGE_GEOMETRY_BIT:					return VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT_KHR;
		case VK_SHADER_STAGE_FRAGMENT_BIT:					return VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR;
		case VK_SHADER_STAGE_COMPUTE_BIT:					return VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR;

		// Other usages are probably an error, so flag that.
		default:
			DE_FATAL("Invalid shader stage");
			return (VkPipelineStageFlags)0;
	}
}

//! Fill destination buffer with a repeating pattern.
void fillPattern (void* const pData, const VkDeviceSize size, bool useIndexPattern = false)
{
	// There are two pattern options - most operations use primePattern,
	// indexPattern is only needed for testing vertex index bufffer.
	static const deUint8	primePattern[]	= { 2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31 };
	static const deUint32	indexPattern[]	= { 0, 1, 2, 3, 4 };

	const deUint8*			pattern			= (useIndexPattern ? reinterpret_cast<const deUint8*>(indexPattern)
															   : primePattern);
	const deUint32			patternSize		= static_cast<deUint32>(useIndexPattern
															   ? DE_LENGTH_OF_ARRAY(indexPattern)*sizeof(deUint32)
															   : DE_LENGTH_OF_ARRAY(primePattern));
	deUint8* const			pBytes			= static_cast<deUint8*>(pData);

	for (deUint32 i = 0; i < size; ++i)
		pBytes[i] = pattern[i % patternSize];
}

//! Get size in bytes of a pixel buffer with given extent.
VkDeviceSize getPixelBufferSize (const VkFormat format, const VkExtent3D& extent)
{
	const int pixelSize = tcu::getPixelSize(mapVkFormat(format));
	return (pixelSize * extent.width * extent.height * extent.depth);
}

//! Determine the size of a 2D image that can hold sizeBytes data.
VkExtent3D get2DImageExtentWithSize (const VkDeviceSize sizeBytes, const deUint32 pixelSize)
{
	const deUint32 size = static_cast<deUint32>(sizeBytes / pixelSize);

	DE_ASSERT(size <= MAX_IMAGE_DIMENSION_2D * MAX_IMAGE_DIMENSION_2D);

	return makeExtent3D(
		std::min(size, static_cast<deUint32>(MAX_IMAGE_DIMENSION_2D)),
		(size / MAX_IMAGE_DIMENSION_2D) + (size % MAX_IMAGE_DIMENSION_2D != 0 ? 1u : 0u),
		1u);
}

VkClearValue makeClearValue (const VkFormat format)
{
	if (isDepthStencilFormat(format))
		return makeClearValueDepthStencil(0.4f, 21u);
	else
	{
		if (isIntFormat(format) || isUintFormat(format))
			return makeClearValueColorU32(8u, 16u, 24u, 32u);
		else
			return makeClearValueColorF32(0.25f, 0.49f, 0.75f, 1.0f);
	}
}

void clearPixelBuffer (tcu::PixelBufferAccess& pixels, const VkClearValue& clearValue)
{
	const tcu::TextureFormat		format			= pixels.getFormat();
	const tcu::TextureChannelClass	channelClass	= tcu::getTextureChannelClass(format.type);

	if (format.order == tcu::TextureFormat::D)
	{
		for (int z = 0; z < pixels.getDepth(); z++)
		for (int y = 0; y < pixels.getHeight(); y++)
		for (int x = 0; x < pixels.getWidth(); x++)
			pixels.setPixDepth(clearValue.depthStencil.depth, x, y, z);
	}
	else if (format.order == tcu::TextureFormat::S)
	{
		for (int z = 0; z < pixels.getDepth(); z++)
		for (int y = 0; y < pixels.getHeight(); y++)
		for (int x = 0; x < pixels.getWidth(); x++)
			pixels.setPixStencil(clearValue.depthStencil.stencil, x, y, z);
	}
	else if (format.order == tcu::TextureFormat::DS)
	{
		for (int z = 0; z < pixels.getDepth(); z++)
		for (int y = 0; y < pixels.getHeight(); y++)
		for (int x = 0; x < pixels.getWidth(); x++)
		{
			pixels.setPixDepth(clearValue.depthStencil.depth, x, y, z);
			pixels.setPixStencil(clearValue.depthStencil.stencil, x, y, z);
		}
	}
	else if (channelClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER || channelClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER)
	{
		const tcu::UVec4 color (clearValue.color.uint32);

		for (int z = 0; z < pixels.getDepth(); z++)
		for (int y = 0; y < pixels.getHeight(); y++)
		for (int x = 0; x < pixels.getWidth(); x++)
			pixels.setPixel(color, x, y, z);
	}
	else
	{
		const tcu::Vec4 color (clearValue.color.float32);

		for (int z = 0; z < pixels.getDepth(); z++)
		for (int y = 0; y < pixels.getHeight(); y++)
		for (int x = 0; x < pixels.getWidth(); x++)
			pixels.setPixel(color, x, y, z);
	}
}

VkImageViewType getImageViewType (const VkImageType imageType)
{
	switch (imageType)
	{
		case VK_IMAGE_TYPE_1D:		return VK_IMAGE_VIEW_TYPE_1D;
		case VK_IMAGE_TYPE_2D:		return VK_IMAGE_VIEW_TYPE_2D;
		case VK_IMAGE_TYPE_3D:		return VK_IMAGE_VIEW_TYPE_3D;

		default:
			DE_FATAL("Unknown image type");
			return VK_IMAGE_VIEW_TYPE_LAST;
	}
}

std::string getShaderImageType (const VkFormat format, const VkImageType imageType)
{
	const tcu::TextureFormat	texFormat	= mapVkFormat(format);
	const std::string			formatPart	= tcu::getTextureChannelClass(texFormat.type) == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER ? "u" :
											  tcu::getTextureChannelClass(texFormat.type) == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER   ? "i" : "";
	switch (imageType)
	{
		case VK_IMAGE_TYPE_1D:	return formatPart + "image1D";
		case VK_IMAGE_TYPE_2D:	return formatPart + "image2D";
		case VK_IMAGE_TYPE_3D:	return formatPart + "image3D";

		default:
			DE_FATAL("Unknown image type");
			return "";
	}
}

std::string getShaderImageFormatQualifier (const VkFormat format)
{
	const tcu::TextureFormat	texFormat	= mapVkFormat(format);
	const char*					orderPart	= DE_NULL;
	const char*					typePart	= DE_NULL;

	switch (texFormat.order)
	{
		case tcu::TextureFormat::R:		orderPart = "r";	break;
		case tcu::TextureFormat::RG:	orderPart = "rg";	break;
		case tcu::TextureFormat::RGB:	orderPart = "rgb";	break;
		case tcu::TextureFormat::RGBA:	orderPart = "rgba";	break;

		default:
			DE_FATAL("Unksupported texture channel order");
			break;
	}

	switch (texFormat.type)
	{
		case tcu::TextureFormat::FLOAT:				typePart = "32f";		break;
		case tcu::TextureFormat::HALF_FLOAT:		typePart = "16f";		break;

		case tcu::TextureFormat::UNSIGNED_INT32:	typePart = "32ui";		break;
		case tcu::TextureFormat::UNSIGNED_INT16:	typePart = "16ui";		break;
		case tcu::TextureFormat::UNSIGNED_INT8:		typePart = "8ui";		break;

		case tcu::TextureFormat::SIGNED_INT32:		typePart = "32i";		break;
		case tcu::TextureFormat::SIGNED_INT16:		typePart = "16i";		break;
		case tcu::TextureFormat::SIGNED_INT8:		typePart = "8i";		break;

		case tcu::TextureFormat::UNORM_INT16:		typePart = "16";		break;
		case tcu::TextureFormat::UNORM_INT8:		typePart = "8";			break;

		case tcu::TextureFormat::SNORM_INT16:		typePart = "16_snorm";	break;
		case tcu::TextureFormat::SNORM_INT8:		typePart = "8_snorm";	break;

		default:
			DE_FATAL("Unksupported texture channel type");
			break;
	}

	return std::string(orderPart) + typePart;
}

namespace FillUpdateBuffer
{

enum BufferOp
{
	BUFFER_OP_FILL,
	BUFFER_OP_UPDATE,
	BUFFER_OP_UPDATE_WITH_INDEX_PATTERN,
};

class Implementation : public Operation
{
public:
	Implementation (OperationContext& context, Resource& resource, const BufferOp bufferOp)
		: m_context		(context)
		, m_resource	(resource)
		, m_fillValue	(0x13)
		, m_bufferOp	(bufferOp)
	{
		DE_ASSERT((m_resource.getBuffer().size % sizeof(deUint32)) == 0);
		DE_ASSERT(m_bufferOp == BUFFER_OP_FILL || m_resource.getBuffer().size <= MAX_UPDATE_BUFFER_SIZE);

		m_data.resize(static_cast<size_t>(m_resource.getBuffer().size));

		if (m_bufferOp == BUFFER_OP_FILL)
		{
			const std::size_t	size	= m_data.size() / sizeof(m_fillValue);
			deUint32* const		pData	= reinterpret_cast<deUint32*>(&m_data[0]);

			for (deUint32 i = 0; i < size; ++i)
				pData[i] = m_fillValue;
		}
		else if (m_bufferOp == BUFFER_OP_UPDATE)
		{
			fillPattern(&m_data[0], m_data.size());
		}
		else if(m_bufferOp == BUFFER_OP_UPDATE_WITH_INDEX_PATTERN)
		{
			fillPattern(&m_data[0], m_data.size(), true);
		}
	}

	void recordCommands (const VkCommandBuffer cmdBuffer)
	{
		const DeviceInterface&	vk	= m_context.getDeviceInterface();

		if (m_bufferOp == BUFFER_OP_FILL)
		{
			vk.cmdFillBuffer(cmdBuffer, m_resource.getBuffer().handle, m_resource.getBuffer().offset, m_resource.getBuffer().size, m_fillValue);

			SynchronizationWrapperPtr synchronizationWrapper = getSynchronizationWrapper(m_context.getSynchronizationType(), vk, DE_FALSE);
			const VkBufferMemoryBarrier2KHR bufferMemoryBarrier2 = makeBufferMemoryBarrier2(
				VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,					// VkPipelineStageFlags2KHR			srcStageMask
				VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,						// VkAccessFlags2KHR				srcAccessMask
				VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,					// VkPipelineStageFlags2KHR			dstStageMask
				VK_ACCESS_2_TRANSFER_READ_BIT_KHR,						// VkAccessFlags2KHR				dstAccessMask
				m_resource.getBuffer().handle,							// VkBuffer							buffer
				0u,														// VkDeviceSize						offset
				m_resource.getBuffer().size								// VkDeviceSize						size
			);
			VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, &bufferMemoryBarrier2);
			synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &dependencyInfo);
		}
		else
			vk.cmdUpdateBuffer(cmdBuffer, m_resource.getBuffer().handle, m_resource.getBuffer().offset, m_resource.getBuffer().size, reinterpret_cast<deUint32*>(&m_data[0]));
	}

	SyncInfo getInSyncInfo (void) const
	{
		return emptySyncInfo;
	}

	SyncInfo getOutSyncInfo (void) const
	{
		const SyncInfo syncInfo =
		{
			VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,	// VkPipelineStageFlags		stageMask;
			VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,		// VkAccessFlags			accessMask;
			VK_IMAGE_LAYOUT_UNDEFINED,				// VkImageLayout			imageLayout;
		};

		return syncInfo;
	}

	Data getData (void) const
	{
		const Data data =
		{
			m_data.size(),		// std::size_t		size;
			&m_data[0],			// const deUint8*	data;
		};
		return data;
	}

	void setData (const Data& data)
	{
		deMemcpy(&m_data[0], data.data, data.size);
	}

private:
	OperationContext&		m_context;
	Resource&				m_resource;
	std::vector<deUint8>	m_data;
	const deUint32			m_fillValue;
	const BufferOp			m_bufferOp;
};

class Support : public OperationSupport
{
public:
	Support (const ResourceDescription& resourceDesc, const BufferOp bufferOp)
		: m_resourceDesc	(resourceDesc)
		, m_bufferOp		(bufferOp)
	{
		DE_ASSERT(m_bufferOp == BUFFER_OP_FILL || m_bufferOp == BUFFER_OP_UPDATE || m_bufferOp == BUFFER_OP_UPDATE_WITH_INDEX_PATTERN);
		DE_ASSERT(m_resourceDesc.type == RESOURCE_TYPE_BUFFER || m_resourceDesc.type == RESOURCE_TYPE_INDEX_BUFFER);
	}

	deUint32 getInResourceUsageFlags (void) const
	{
		return 0;
	}

	deUint32 getOutResourceUsageFlags (void) const
	{
		return VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	}

	VkQueueFlags getQueueFlags (const OperationContext& context) const
	{
		if (m_bufferOp == BUFFER_OP_FILL && !context.isDeviceFunctionalitySupported("VK_KHR_maintenance1"))
		{
			return VK_QUEUE_COMPUTE_BIT | VK_QUEUE_GRAPHICS_BIT;
		}

		return VK_QUEUE_TRANSFER_BIT;
	}

	de::MovePtr<Operation> build (OperationContext& context, Resource& resource) const
	{
		return de::MovePtr<Operation>(new Implementation(context, resource, m_bufferOp));
	}

	de::MovePtr<Operation> build (OperationContext&, Resource&, Resource&) const
	{
		DE_ASSERT(0);
		return de::MovePtr<Operation>();
	}

private:
	const ResourceDescription	m_resourceDesc;
	const BufferOp				m_bufferOp;
};

} // FillUpdateBuffer ns

namespace CopyBuffer
{

class Implementation : public Operation
{
public:
	Implementation (OperationContext& context, Resource& resource, const AccessMode mode)
		: m_context		(context)
		, m_resource	(resource)
		, m_mode		(mode)
	{
		const DeviceInterface&		vk				= m_context.getDeviceInterface();
		const VkDevice				device			= m_context.getDevice();
		Allocator&					allocator		= m_context.getAllocator();
		const VkBufferUsageFlags	hostBufferUsage	= (m_mode == ACCESS_MODE_READ ? VK_BUFFER_USAGE_TRANSFER_DST_BIT : VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

		m_hostBuffer = de::MovePtr<Buffer>(new Buffer(vk, device, allocator, makeBufferCreateInfo(m_resource.getBuffer().size, hostBufferUsage), MemoryRequirement::HostVisible));

		const Allocation& alloc = m_hostBuffer->getAllocation();

		if (m_mode == ACCESS_MODE_READ)
			deMemset(alloc.getHostPtr(), 0, static_cast<size_t>(m_resource.getBuffer().size));
		else
			fillPattern(alloc.getHostPtr(), m_resource.getBuffer().size);

		flushAlloc(vk, device, alloc);
	}

	void recordCommands (const VkCommandBuffer cmdBuffer)
	{
		const DeviceInterface&		vk						= m_context.getDeviceInterface();
		const VkBufferCopy			copyRegion				= makeBufferCopy(0u, 0u, m_resource.getBuffer().size);
		SynchronizationWrapperPtr	synchronizationWrapper	= getSynchronizationWrapper(m_context.getSynchronizationType(), vk, DE_FALSE);

		if (m_mode == ACCESS_MODE_READ)
		{
			vk.cmdCopyBuffer(cmdBuffer, m_resource.getBuffer().handle, **m_hostBuffer, 1u, &copyRegion);

			// Insert a barrier so copied data is available to the host
			const VkBufferMemoryBarrier2KHR bufferMemoryBarrier2 = makeBufferMemoryBarrier2(
				VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,					// VkPipelineStageFlags2KHR			srcStageMask
				VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,						// VkAccessFlags2KHR				srcAccessMask
				VK_PIPELINE_STAGE_2_HOST_BIT_KHR,						// VkPipelineStageFlags2KHR			dstStageMask
				VK_ACCESS_2_HOST_READ_BIT_KHR,							// VkAccessFlags2KHR				dstAccessMask
				**m_hostBuffer,											// VkBuffer							buffer
				0u,														// VkDeviceSize						offset
				m_resource.getBuffer().size								// VkDeviceSize						size
			);
			VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, &bufferMemoryBarrier2);
			synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &dependencyInfo);
		}
		else
		{
			// Insert a barrier so buffer data is available to the device
			//const VkBufferMemoryBarrier2KHR bufferMemoryBarrier2 = makeBufferMemoryBarrier2(
			//	VK_PIPELINE_STAGE_2_HOST_BIT_KHR,						// VkPipelineStageFlags2KHR			srcStageMask
			//	VK_ACCESS_2_HOST_WRITE_BIT_KHR,							// VkAccessFlags2KHR				srcAccessMask
			//	VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,					// VkPipelineStageFlags2KHR			dstStageMask
			//	VK_ACCESS_2_TRANSFER_READ_BIT_KHR,						// VkAccessFlags2KHR				dstAccessMask
			//	**m_hostBuffer,											// VkBuffer							buffer
			//	0u,														// VkDeviceSize						offset
			//	m_resource.getBuffer().size								// VkDeviceSize						size
			//);
			//VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, &bufferMemoryBarrier2);
			//synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &dependencyInfo);

			vk.cmdCopyBuffer(cmdBuffer, **m_hostBuffer, m_resource.getBuffer().handle, 1u, &copyRegion);
		}
	}

	SyncInfo getInSyncInfo (void) const
	{
		const VkAccessFlags access		= (m_mode == ACCESS_MODE_READ ? VK_ACCESS_2_TRANSFER_READ_BIT_KHR : 0);
		const SyncInfo		syncInfo	=
		{
			VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,	// VkPipelineStageFlags		stageMask;
			access,									// VkAccessFlags			accessMask;
			VK_IMAGE_LAYOUT_UNDEFINED,				// VkImageLayout			imageLayout;
		};
		return syncInfo;
	}

	SyncInfo getOutSyncInfo (void) const
	{
		const VkAccessFlags access		= (m_mode == ACCESS_MODE_WRITE ? VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR : 0);
		const SyncInfo		syncInfo	=
		{
			VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,	// VkPipelineStageFlags		stageMask;
			access,									// VkAccessFlags			accessMask;
			VK_IMAGE_LAYOUT_UNDEFINED,				// VkImageLayout			imageLayout;
		};
		return syncInfo;
	}

	Data getData (void) const
	{
		return getHostBufferData(m_context, *m_hostBuffer, m_resource.getBuffer().size);
	}

	void setData (const Data& data)
	{
		DE_ASSERT(m_mode == ACCESS_MODE_WRITE);
		setHostBufferData(m_context, *m_hostBuffer, data);
	}

private:
	OperationContext&		m_context;
	Resource&				m_resource;
	const AccessMode		m_mode;
	de::MovePtr<Buffer>		m_hostBuffer;
};

class Support : public OperationSupport
{
public:
	Support (const ResourceDescription& resourceDesc, const AccessMode mode)
		: m_mode			(mode)
	{
		DE_ASSERT(resourceDesc.type == RESOURCE_TYPE_BUFFER);
		DE_UNREF(resourceDesc);
	}

	deUint32 getInResourceUsageFlags (void) const
	{
		return m_mode == ACCESS_MODE_READ ? VK_BUFFER_USAGE_TRANSFER_SRC_BIT : 0;
	}

	deUint32 getOutResourceUsageFlags (void) const
	{
		return m_mode == ACCESS_MODE_WRITE ? VK_BUFFER_USAGE_TRANSFER_DST_BIT : 0;
	}

	VkQueueFlags getQueueFlags (const OperationContext& context) const
	{
		DE_UNREF(context);
		return VK_QUEUE_TRANSFER_BIT;
	}

	de::MovePtr<Operation> build (OperationContext& context, Resource& resource) const
	{
		return de::MovePtr<Operation>(new Implementation(context, resource, m_mode));
	}

	de::MovePtr<Operation> build (OperationContext&, Resource&, Resource&) const
	{
		DE_ASSERT(0);
		return de::MovePtr<Operation>();
	}

private:
	const AccessMode	m_mode;
};

class CopyImplementation : public Operation
{
public:
	CopyImplementation (OperationContext& context, Resource& inResource, Resource& outResource)
		: m_context		(context)
		, m_inResource	(inResource)
		, m_outResource	(outResource)
	{
	}

	void recordCommands (const VkCommandBuffer cmdBuffer)
	{
		const DeviceInterface&	vk			= m_context.getDeviceInterface();
		const VkBufferCopy		copyRegion	= makeBufferCopy(0u, 0u, m_inResource.getBuffer().size);

		vk.cmdCopyBuffer(cmdBuffer, m_inResource.getBuffer().handle, m_outResource.getBuffer().handle, 1u, &copyRegion);
	}

	SyncInfo getInSyncInfo (void) const
	{
		const SyncInfo		syncInfo	=
		{
			VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,	// VkPipelineStageFlags		stageMask;
			VK_ACCESS_2_TRANSFER_READ_BIT_KHR,		// VkAccessFlags			accessMask;
			VK_IMAGE_LAYOUT_UNDEFINED,				// VkImageLayout			imageLayout;
		};
		return syncInfo;
	}

	SyncInfo getOutSyncInfo (void) const
	{
		const SyncInfo		syncInfo	=
		{
			VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,	// VkPipelineStageFlags		stageMask;
			VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,		// VkAccessFlags			accessMask;
			VK_IMAGE_LAYOUT_UNDEFINED,				// VkImageLayout			imageLayout;
		};
		return syncInfo;
	}

	Data getData (void) const
	{
		Data data = { 0, DE_NULL };
		return data;
	}

	void setData (const Data&)
	{
		DE_ASSERT(0);
	}

private:
	OperationContext&		m_context;
	Resource&				m_inResource;
	Resource&				m_outResource;
	de::MovePtr<Buffer>		m_hostBuffer;
};

class CopySupport : public OperationSupport
{
public:
	CopySupport (const ResourceDescription& resourceDesc)
	{
		DE_ASSERT(resourceDesc.type == RESOURCE_TYPE_BUFFER);
		DE_UNREF(resourceDesc);
	}

	deUint32 getInResourceUsageFlags (void) const
	{
		return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	}

	deUint32 getOutResourceUsageFlags (void) const
	{
		return VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	}

	VkQueueFlags getQueueFlags (const OperationContext& context) const
	{
		DE_UNREF(context);
		return VK_QUEUE_TRANSFER_BIT;
	}

	de::MovePtr<Operation> build (OperationContext&, Resource&) const
	{
		DE_ASSERT(0);
		return de::MovePtr<Operation>();
	}

	de::MovePtr<Operation> build (OperationContext& context, Resource& inResource, Resource& outResource) const
	{
		return de::MovePtr<Operation>(new CopyImplementation(context, inResource, outResource));
	}
};

} // CopyBuffer ns

namespace CopyBlitResolveImage
{

class ImplementationBase : public Operation
{
public:
	//! Copy/Blit/Resolve etc. operation
	virtual void recordCopyCommand (const VkCommandBuffer cmdBuffer) = 0;

	//! Get source stage mask that is used during read - added to test synchronization2 new stage masks
	virtual VkPipelineStageFlags2KHR getReadSrcStageMask() const = 0;

	ImplementationBase (OperationContext& context, Resource& resource, const AccessMode mode)
		: m_context		(context)
		, m_resource	(resource)
		, m_mode		(mode)
		, m_bufferSize	(getPixelBufferSize(m_resource.getImage().format, m_resource.getImage().extent))
	{
		const DeviceInterface&	vk			= m_context.getDeviceInterface();
		const VkDevice			device		= m_context.getDevice();
		Allocator&				allocator	= m_context.getAllocator();

		m_hostBuffer = de::MovePtr<Buffer>(new Buffer(
			vk, device, allocator, makeBufferCreateInfo(m_bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
			MemoryRequirement::HostVisible));

		const Allocation& alloc = m_hostBuffer->getAllocation();
		if (m_mode == ACCESS_MODE_READ)
			deMemset(alloc.getHostPtr(), 0, static_cast<size_t>(m_bufferSize));
		else
			fillPattern(alloc.getHostPtr(), m_bufferSize);
		flushAlloc(vk, device, alloc);

		// Staging image
		const auto& imgResource = m_resource.getImage();
		m_image = de::MovePtr<Image>(new Image(
			vk, device, allocator,
			makeImageCreateInfo(imgResource.imageType, imgResource.extent, imgResource.format,
								(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT), VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL),
			MemoryRequirement::Any));
	}

	void recordCommands (const VkCommandBuffer cmdBuffer)
	{
		const DeviceInterface&		vk						= m_context.getDeviceInterface();
		const VkBufferImageCopy		bufferCopyRegion		= makeBufferImageCopy(m_resource.getImage().extent, m_resource.getImage().subresourceLayers);
		SynchronizationWrapperPtr	synchronizationWrapper	= getSynchronizationWrapper(m_context.getSynchronizationType(), vk, DE_FALSE);

		// Staging image layout
		{
			const VkImageMemoryBarrier2KHR imageMemoryBarrier2 = makeImageMemoryBarrier2(
				VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR,		// VkPipelineStageFlags2KHR			srcStageMask
				(VkAccessFlags)0,								// VkAccessFlags2KHR				srcAccessMask
				VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,			// VkPipelineStageFlags2KHR			dstStageMask
				VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,				// VkAccessFlags2KHR				dstAccessMask
				VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout					oldLayout
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,			// VkImageLayout					newLayout
				**m_image,										// VkImage							image
				m_resource.getImage().subresourceRange			// VkImageSubresourceRange			subresourceRange
			);
			VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, DE_NULL, &imageMemoryBarrier2);
			synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &dependencyInfo);
		}

		if (m_mode == ACCESS_MODE_READ)
		{
			// Resource Image -> Staging image
			recordCopyCommand(cmdBuffer);

			// Staging image layout
			const VkImageMemoryBarrier2KHR imageMemoryBarrier2 = makeImageMemoryBarrier2(
				getReadSrcStageMask(),							// VkPipelineStageFlags2KHR			srcStageMask
				VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,				// VkAccessFlags2KHR				srcAccessMask
				VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,			// VkPipelineStageFlags2KHR			dstStageMask
				VK_ACCESS_2_TRANSFER_READ_BIT_KHR,				// VkAccessFlags2KHR				dstAccessMask
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,			// VkImageLayout					oldLayout
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,			// VkImageLayout					newLayout
				**m_image,										// VkImage							image
				m_resource.getImage().subresourceRange			// VkImageSubresourceRange			subresourceRange
			);
			VkDependencyInfoKHR imageDependencyInfo = makeCommonDependencyInfo(DE_NULL, DE_NULL, &imageMemoryBarrier2);
			synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &imageDependencyInfo);

			// Image -> Host buffer
			vk.cmdCopyImageToBuffer(cmdBuffer, **m_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, **m_hostBuffer, 1u, &bufferCopyRegion);

			// Insert a barrier so copied data is available to the host
			const VkBufferMemoryBarrier2KHR bufferMemoryBarrier2 = makeBufferMemoryBarrier2(
				VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,			// VkPipelineStageFlags2KHR			srcStageMask
				VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,				// VkAccessFlags2KHR				srcAccessMask
				VK_PIPELINE_STAGE_2_HOST_BIT_KHR,				// VkPipelineStageFlags2KHR			dstStageMask
				VK_ACCESS_2_HOST_READ_BIT_KHR,					// VkAccessFlags2KHR				dstAccessMask
				**m_hostBuffer,									// VkBuffer							buffer
				0u,												// VkDeviceSize						offset
				m_bufferSize									// VkDeviceSize						size
			);
			VkDependencyInfoKHR bufferDependencyInfo = makeCommonDependencyInfo(DE_NULL, &bufferMemoryBarrier2);
			synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &bufferDependencyInfo);
		}
		else
		{
			// Host buffer -> Staging image
			vk.cmdCopyBufferToImage(cmdBuffer, **m_hostBuffer, **m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &bufferCopyRegion);

			// Staging image layout
			{
				const VkImageMemoryBarrier2KHR imageMemoryBarrier2 = makeImageMemoryBarrier2(
					VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,			// VkPipelineStageFlags2KHR			srcStageMask
					VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,				// VkAccessFlags2KHR				srcAccessMask
					VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,			// VkPipelineStageFlags2KHR			dstStageMask
					VK_ACCESS_2_TRANSFER_READ_BIT_KHR,				// VkAccessFlags2KHR				dstAccessMask
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,			// VkImageLayout					oldLayout
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,			// VkImageLayout					newLayout
					**m_image,										// VkImage							image
					m_resource.getImage().subresourceRange			// VkImageSubresourceRange			subresourceRange
				);
				VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, DE_NULL, &imageMemoryBarrier2);
				synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &dependencyInfo);
			}

			// Resource image layout
			{
				const VkImageMemoryBarrier2KHR imageMemoryBarrier2 = makeImageMemoryBarrier2(
					VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR,		// VkPipelineStageFlags2KHR			srcStageMask
					(VkAccessFlags2KHR)0,							// VkAccessFlags2KHR				srcAccessMask
					VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,			// VkPipelineStageFlags2KHR			dstStageMask
					VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,				// VkAccessFlags2KHR				dstAccessMask
					VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout					oldLayout
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,			// VkImageLayout					newLayout
					m_resource.getImage().handle,					// VkImage							image
					m_resource.getImage().subresourceRange			// VkImageSubresourceRange			subresourceRange
				);
				VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, DE_NULL, &imageMemoryBarrier2);
				synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &dependencyInfo);
			}

			// Staging image -> Resource Image
			recordCopyCommand(cmdBuffer);
		}
	}

	SyncInfo getInSyncInfo (void) const
	{
		const VkAccessFlags2KHR	access		= (m_mode == ACCESS_MODE_READ ? VK_ACCESS_2_TRANSFER_READ_BIT_KHR : VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR);
		const VkImageLayout		layout		= (m_mode == ACCESS_MODE_READ ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		const SyncInfo			syncInfo	=
		{
			VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,	// VkPipelineStageFlags		stageMask;
			access,									// VkAccessFlags			accessMask;
			layout,									// VkImageLayout			imageLayout;
		};
		return syncInfo;
	}

	SyncInfo getOutSyncInfo (void) const
	{
		const VkAccessFlags2KHR	access		= (m_mode == ACCESS_MODE_READ ? VK_ACCESS_2_TRANSFER_READ_BIT_KHR : VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR);
		const VkImageLayout		layout		= (m_mode == ACCESS_MODE_READ ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		const SyncInfo			syncInfo	=
		{
			VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,	// VkPipelineStageFlags		stageMask;
			access,									// VkAccessFlags			accessMask;
			layout,									// VkImageLayout			imageLayout;
		};
		return syncInfo;
	}

	Data getData (void) const
	{
		return getHostBufferData(m_context, *m_hostBuffer, m_bufferSize);
	}

	void setData (const Data& data)
	{
		DE_ASSERT(m_mode == ACCESS_MODE_WRITE);
		setHostBufferData(m_context, *m_hostBuffer, data);
	}

protected:
	OperationContext&		m_context;
	Resource&				m_resource;
	const AccessMode		m_mode;
	const VkDeviceSize		m_bufferSize;
	de::MovePtr<Buffer>		m_hostBuffer;
	de::MovePtr<Image>		m_image;
};

VkOffset3D makeExtentOffset (const Resource& resource)
{
	DE_ASSERT(resource.getType() == RESOURCE_TYPE_IMAGE);
	const VkExtent3D extent = resource.getImage().extent;

	switch (resource.getImage().imageType)
	{
		case VK_IMAGE_TYPE_1D:	return makeOffset3D(extent.width, 1, 1);
		case VK_IMAGE_TYPE_2D:	return makeOffset3D(extent.width, extent.height, 1);
		case VK_IMAGE_TYPE_3D:	return makeOffset3D(extent.width, extent.height, extent.depth);
		default:
			DE_ASSERT(0);
			return VkOffset3D();
	}
}

VkImageBlit makeBlitRegion (const Resource& resource)
{
	const VkImageBlit blitRegion =
	{
		resource.getImage().subresourceLayers,					// VkImageSubresourceLayers    srcSubresource;
		{ makeOffset3D(0, 0, 0), makeExtentOffset(resource) },	// VkOffset3D                  srcOffsets[2];
		resource.getImage().subresourceLayers,					// VkImageSubresourceLayers    dstSubresource;
		{ makeOffset3D(0, 0, 0), makeExtentOffset(resource) },	// VkOffset3D                  dstOffsets[2];
	};
	return blitRegion;
}

class BlitImplementation : public ImplementationBase
{
public:
	BlitImplementation (OperationContext& context, Resource& resource, const AccessMode mode)
		: ImplementationBase	(context, resource, mode)
		, m_blitRegion			(makeBlitRegion(m_resource))
	{
		const InstanceInterface&	vki				= m_context.getInstanceInterface();
		const VkPhysicalDevice		physDevice		= m_context.getPhysicalDevice();
		const auto&					imgResource		= m_resource.getImage();
		const VkFormatProperties	formatProps		= getPhysicalDeviceFormatProperties(vki, physDevice, imgResource.format);
		const auto&					features		= ((imgResource.tiling == VK_IMAGE_TILING_LINEAR) ? formatProps.linearTilingFeatures : formatProps.optimalTilingFeatures);
		const VkFormatFeatureFlags	requiredFlags	= (VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT);

		// SRC and DST blit is required because both images are using the same format.
		if ((features & requiredFlags) != requiredFlags)
			TCU_THROW(NotSupportedError, "Format doesn't support blits");
	}

	void recordCopyCommand (const VkCommandBuffer cmdBuffer)
	{
		const DeviceInterface&	vk	= m_context.getDeviceInterface();

		if (m_mode == ACCESS_MODE_READ)
		{
			// Resource Image -> Staging image
			vk.cmdBlitImage(cmdBuffer, m_resource.getImage().handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, **m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1u, &m_blitRegion, VK_FILTER_NEAREST);
		}
		else
		{
			// Staging image -> Resource Image
			vk.cmdBlitImage(cmdBuffer, **m_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_resource.getImage().handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1u, &m_blitRegion, VK_FILTER_NEAREST);
		}
	}

	VkPipelineStageFlags2KHR getReadSrcStageMask() const
	{
		return (m_context.getSynchronizationType() == SynchronizationType::LEGACY) ? VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR : VK_PIPELINE_STAGE_2_BLIT_BIT_KHR;
	}


private:
	const VkImageBlit	m_blitRegion;
};

template <typename ImageCopyOrResolve>
ImageCopyOrResolve makeImageRegion (const Resource& resource)
{
	return
	{
		resource.getImage().subresourceLayers,		// VkImageSubresourceLayers    srcSubresource;
		makeOffset3D(0, 0, 0),						// VkOffset3D                  srcOffset;
		resource.getImage().subresourceLayers,		// VkImageSubresourceLayers    dstSubresource;
		makeOffset3D(0, 0, 0),						// VkOffset3D                  dstOffset;
		resource.getImage().extent,					// VkExtent3D                  extent;
	};
}

class CopyImplementation : public ImplementationBase
{
public:
	CopyImplementation (OperationContext& context, Resource& resource, const AccessMode mode)
		: ImplementationBase	(context, resource, mode)
		, m_imageCopyRegion		(makeImageRegion<VkImageCopy>(m_resource))
	{
	}

	void recordCopyCommand (const VkCommandBuffer cmdBuffer)
	{
		const DeviceInterface&	vk	= m_context.getDeviceInterface();

		if (m_mode == ACCESS_MODE_READ)
		{
			// Resource Image -> Staging image
			vk.cmdCopyImage(cmdBuffer, m_resource.getImage().handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, **m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &m_imageCopyRegion);
		}
		else
		{
			// Staging image -> Resource Image
			vk.cmdCopyImage(cmdBuffer, **m_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_resource.getImage().handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &m_imageCopyRegion);
		}
	}

	VkPipelineStageFlags2KHR getReadSrcStageMask() const
	{
		return (m_context.getSynchronizationType() == SynchronizationType::LEGACY) ? VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR : VK_PIPELINE_STAGE_2_COPY_BIT_KHR;
	}

private:
	const VkImageCopy	m_imageCopyRegion;
};

class ResolveImplementation : public ImplementationBase
{
public:
	ResolveImplementation(OperationContext& context, Resource& resource, const AccessMode mode)
		: ImplementationBase	(context, resource, mode)
		, m_imageResolveRegion	(makeImageRegion<VkImageResolve>(resource))
	{
		DE_ASSERT(m_mode == ACCESS_MODE_READ);
	}

	void recordCopyCommand(const VkCommandBuffer cmdBuffer)
	{
		const DeviceInterface&	vk = m_context.getDeviceInterface();

		// Resource Image -> Staging image
		vk.cmdResolveImage(cmdBuffer, m_resource.getImage().handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, **m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &m_imageResolveRegion);
	}

	VkPipelineStageFlags2KHR getReadSrcStageMask() const
	{
		return (m_context.getSynchronizationType() == SynchronizationType::LEGACY) ? VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR : VK_PIPELINE_STAGE_2_RESOLVE_BIT_KHR;
	}

private:
	VkImageResolve m_imageResolveRegion;
};

enum Type
{
	TYPE_COPY,
	TYPE_BLIT,
	TYPE_RESOLVE,
};

class Support : public OperationSupport
{
public:
	Support (const ResourceDescription& resourceDesc, const Type type, const AccessMode mode)
		: m_type				(type)
		, m_mode				(mode)
	{
		DE_ASSERT(resourceDesc.type == RESOURCE_TYPE_IMAGE);

		const bool isDepthStencil	= isDepthStencilFormat(resourceDesc.imageFormat);
		m_requiredQueueFlags		= (isDepthStencil || m_type != TYPE_COPY ? VK_QUEUE_GRAPHICS_BIT : VK_QUEUE_TRANSFER_BIT);

		// Don't blit depth/stencil images.
		DE_ASSERT(m_type != TYPE_BLIT || !isDepthStencil);
	}

	deUint32 getInResourceUsageFlags (void) const
	{
		return (m_mode == ACCESS_MODE_READ ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0);
	}

	deUint32 getOutResourceUsageFlags (void) const
	{
		return (m_mode == ACCESS_MODE_WRITE ? VK_IMAGE_USAGE_TRANSFER_DST_BIT : 0);
	}

	VkQueueFlags getQueueFlags (const OperationContext& context) const
	{
		DE_UNREF(context);
		return m_requiredQueueFlags;
	}

	de::MovePtr<Operation> build (OperationContext& context, Resource& resource) const
	{
		if (m_type == TYPE_COPY)
			return de::MovePtr<Operation>(new CopyImplementation(context, resource, m_mode));
		else if (m_type == TYPE_BLIT)
			return de::MovePtr<Operation>(new BlitImplementation(context, resource, m_mode));
		else
			return de::MovePtr<Operation>(new ResolveImplementation(context, resource, m_mode));
	}

	de::MovePtr<Operation> build (OperationContext&, Resource&, Resource&) const
	{
		DE_ASSERT(0);
		return de::MovePtr<Operation>();
	}

private:
	const Type			m_type;
	const AccessMode	m_mode;
	VkQueueFlags		m_requiredQueueFlags;
};

class BlitCopyImplementation : public Operation
{
public:
	BlitCopyImplementation (OperationContext& context, Resource& inResource, Resource& outResource)
		: m_context			(context)
		, m_inResource		(inResource)
		, m_outResource		(outResource)
		, m_blitRegion		(makeBlitRegion(m_inResource))
	{
		DE_ASSERT(m_inResource.getType() == RESOURCE_TYPE_IMAGE);
		DE_ASSERT(m_outResource.getType() == RESOURCE_TYPE_IMAGE);

		const InstanceInterface&	vki				= m_context.getInstanceInterface();
		const VkPhysicalDevice		physDevice		= m_context.getPhysicalDevice();
		const auto&					imgResource		= m_inResource.getImage();
		const VkFormatProperties	formatProps		= getPhysicalDeviceFormatProperties(vki, physDevice, imgResource.format);
		const auto&					features		= ((imgResource.tiling == VK_IMAGE_TILING_LINEAR) ? formatProps.linearTilingFeatures : formatProps.optimalTilingFeatures);
		const VkFormatFeatureFlags	requiredFlags	= (VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT);

		// SRC and DST blit is required because both images are using the same format.
		if ((features & requiredFlags) != requiredFlags)
			TCU_THROW(NotSupportedError, "Format doesn't support blits");
	}

	void recordCommands (const VkCommandBuffer cmdBuffer)
	{
		const DeviceInterface&		vk						= m_context.getDeviceInterface();
		SynchronizationWrapperPtr	synchronizationWrapper	= getSynchronizationWrapper(m_context.getSynchronizationType(), vk, DE_FALSE);

		{
			const VkImageMemoryBarrier2KHR imageMemoryBarrier2 = makeImageMemoryBarrier2(
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,				// VkPipelineStageFlags2KHR			srcStageMask
				(VkAccessFlags2KHR)0,							// VkAccessFlags2KHR				srcAccessMask
				VK_PIPELINE_STAGE_TRANSFER_BIT,					// VkPipelineStageFlags2KHR			dstStageMask
				VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,				// VkAccessFlags2KHR				dstAccessMask
				VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout					oldLayout
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,			// VkImageLayout					newLayout
				m_outResource.getImage().handle,				// VkImage							image
				m_outResource.getImage().subresourceRange		// VkImageSubresourceRange			subresourceRange
			);
			VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, DE_NULL, &imageMemoryBarrier2);
			synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &dependencyInfo);
		}

		vk.cmdBlitImage(cmdBuffer,
						m_inResource.getImage().handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						m_outResource.getImage().handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						1u, &m_blitRegion, VK_FILTER_NEAREST);
	}

	SyncInfo getInSyncInfo (void) const
	{
		const SyncInfo		syncInfo	=
		{
			VK_PIPELINE_STAGE_TRANSFER_BIT,			// VkPipelineStageFlags		stageMask;
			VK_ACCESS_2_TRANSFER_READ_BIT_KHR,		// VkAccessFlags			accessMask;
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,	// VkImageLayout			imageLayout;
		};
		return syncInfo;
	}

	SyncInfo getOutSyncInfo (void) const
	{
		const SyncInfo		syncInfo	=
		{
			VK_PIPELINE_STAGE_TRANSFER_BIT,			// VkPipelineStageFlags		stageMask;
			VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,		// VkAccessFlags			accessMask;
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,	// VkImageLayout			imageLayout;
		};
		return syncInfo;
	}

	Data getData (void) const
	{
		Data data = { 0, DE_NULL };
		return data;
	}

	void setData (const Data&)
	{
		DE_ASSERT(0);
	}

private:
	OperationContext&	m_context;
	Resource&			m_inResource;
	Resource&			m_outResource;
	const VkImageBlit	m_blitRegion;
};

class CopyCopyImplementation : public Operation
{
public:
	CopyCopyImplementation (OperationContext& context, Resource& inResource, Resource& outResource)
		: m_context			(context)
		, m_inResource		(inResource)
		, m_outResource		(outResource)
		, m_imageCopyRegion	(makeImageRegion<VkImageCopy>(m_inResource))
	{
		DE_ASSERT(m_inResource.getType() == RESOURCE_TYPE_IMAGE);
		DE_ASSERT(m_outResource.getType() == RESOURCE_TYPE_IMAGE);
	}

	void recordCommands (const VkCommandBuffer cmdBuffer)
	{
		const DeviceInterface&		vk						= m_context.getDeviceInterface();
		SynchronizationWrapperPtr	synchronizationWrapper	= getSynchronizationWrapper(m_context.getSynchronizationType(), vk, DE_FALSE);

		{
			const VkImageMemoryBarrier2KHR imageMemoryBarrier2 = makeImageMemoryBarrier2(
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,				// VkPipelineStageFlags2KHR			srcStageMask
				(VkAccessFlags)0,								// VkAccessFlags2KHR				srcAccessMask
				VK_PIPELINE_STAGE_TRANSFER_BIT,					// VkPipelineStageFlags2KHR			dstStageMask
				VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,				// VkAccessFlags2KHR				dstAccessMask
				VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout					oldLayout
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,			// VkImageLayout					newLayout
				m_outResource.getImage().handle,				// VkImage							image
				m_outResource.getImage().subresourceRange		// VkImageSubresourceRange			subresourceRange
			);
			VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, DE_NULL, &imageMemoryBarrier2);
			synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &dependencyInfo);
		}

		vk.cmdCopyImage(cmdBuffer,
						m_inResource.getImage().handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						m_outResource.getImage().handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						1u, &m_imageCopyRegion);
	}

	SyncInfo getInSyncInfo (void) const
	{
		const SyncInfo		syncInfo	=
		{
			VK_PIPELINE_STAGE_TRANSFER_BIT,			// VkPipelineStageFlags		stageMask;
			VK_ACCESS_2_TRANSFER_READ_BIT_KHR,		// VkAccessFlags			accessMask;
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,	// VkImageLayout			imageLayout;
		};
		return syncInfo;
	}

	SyncInfo getOutSyncInfo (void) const
	{
		const SyncInfo		syncInfo	=
		{
			VK_PIPELINE_STAGE_TRANSFER_BIT,			// VkPipelineStageFlags		stageMask;
			VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,		// VkAccessFlags			accessMask;
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,	// VkImageLayout			imageLayout;
		};
		return syncInfo;
	}

	Data getData (void) const
	{
		Data data = { 0, DE_NULL };
		return data;
	}

	void setData (const Data&)
	{
		DE_ASSERT(0);
	}

private:
	OperationContext&	m_context;
	Resource&			m_inResource;
	Resource&			m_outResource;
	const VkImageCopy	m_imageCopyRegion;
};

class CopySupport : public OperationSupport
{
public:
	CopySupport (const ResourceDescription& resourceDesc, const Type type)
		: m_type				(type)
	{
		DE_ASSERT(resourceDesc.type == RESOURCE_TYPE_IMAGE);

		const bool isDepthStencil	= isDepthStencilFormat(resourceDesc.imageFormat);
		m_requiredQueueFlags		= (isDepthStencil || m_type == TYPE_BLIT ? VK_QUEUE_GRAPHICS_BIT : VK_QUEUE_TRANSFER_BIT);

		// Don't blit depth/stencil images.
		DE_ASSERT(m_type != TYPE_BLIT || !isDepthStencil);
	}

	deUint32 getInResourceUsageFlags (void) const
	{
		return VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	}

	deUint32 getOutResourceUsageFlags (void) const
	{
		return VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	}

	VkQueueFlags getQueueFlags (const OperationContext& context) const
	{
		DE_UNREF(context);
		return m_requiredQueueFlags;
	}

	de::MovePtr<Operation> build (OperationContext&, Resource&) const
	{
		DE_ASSERT(0);
		return de::MovePtr<Operation>();
	}

	de::MovePtr<Operation> build (OperationContext& context, Resource& inResource, Resource& outResource) const
	{
		if (m_type == TYPE_COPY)
			return de::MovePtr<Operation>(new CopyCopyImplementation(context, inResource, outResource));
		else
			return de::MovePtr<Operation>(new BlitCopyImplementation(context, inResource, outResource));
	}

private:
	const Type			m_type;
	VkQueueFlags		m_requiredQueueFlags;
};

} // CopyBlitImage ns

namespace ShaderAccess
{

enum DispatchCall
{
	DISPATCH_CALL_DISPATCH,
	DISPATCH_CALL_DISPATCH_INDIRECT,
};

class GraphicsPipeline : public Pipeline
{
public:
	GraphicsPipeline (OperationContext& context, const VkShaderStageFlagBits stage, const std::string& shaderPrefix, const VkDescriptorSetLayout descriptorSetLayout)
		: m_vertices	(context)
	{
		const DeviceInterface&		vk				= context.getDeviceInterface();
		const VkDevice				device			= context.getDevice();
		Allocator&					allocator		= context.getAllocator();
		const VkShaderStageFlags	requiredStages	= getRequiredStages(stage);

		// Color attachment

		m_colorFormat					= VK_FORMAT_R8G8B8A8_UNORM;
		m_colorImageSubresourceRange	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
		m_colorImageExtent				= makeExtent3D(16u, 16u, 1u);
		m_colorAttachmentImage			= de::MovePtr<Image>(new Image(vk, device, allocator,
			makeImageCreateInfo(VK_IMAGE_TYPE_2D, m_colorImageExtent, m_colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
								VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL),
			MemoryRequirement::Any));

		// Pipeline

		m_colorAttachmentView	= makeImageView		(vk, device, **m_colorAttachmentImage, VK_IMAGE_VIEW_TYPE_2D, m_colorFormat, m_colorImageSubresourceRange);
		m_renderPass			= makeRenderPass	(vk, device, m_colorFormat);
		m_framebuffer			= makeFramebuffer	(vk, device, *m_renderPass, *m_colorAttachmentView, m_colorImageExtent.width, m_colorImageExtent.height);
		m_pipelineLayout		= makePipelineLayout(vk, device, descriptorSetLayout);

		GraphicsPipelineBuilder pipelineBuilder;
		pipelineBuilder
			.setRenderSize					(tcu::IVec2(m_colorImageExtent.width, m_colorImageExtent.height))
			.setVertexInputSingleAttribute	(m_vertices.getVertexFormat(), m_vertices.getVertexStride())
			.setShader						(vk, device, VK_SHADER_STAGE_VERTEX_BIT,	context.getBinaryCollection().get(shaderPrefix + "vert"), DE_NULL)
			.setShader						(vk, device, VK_SHADER_STAGE_FRAGMENT_BIT,	context.getBinaryCollection().get(shaderPrefix + "frag"), DE_NULL);

		if (requiredStages & (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT))
			pipelineBuilder
				.setPatchControlPoints	(m_vertices.getNumVertices())
				.setShader				(vk, device, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,		context.getBinaryCollection().get(shaderPrefix + "tesc"), DE_NULL)
				.setShader				(vk, device, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,	context.getBinaryCollection().get(shaderPrefix + "tese"), DE_NULL);

		if (requiredStages & VK_SHADER_STAGE_GEOMETRY_BIT)
			pipelineBuilder
				.setShader	(vk, device, VK_SHADER_STAGE_GEOMETRY_BIT,	context.getBinaryCollection().get(shaderPrefix + "geom"), DE_NULL);

		m_pipeline = pipelineBuilder.build(vk, device, *m_pipelineLayout, *m_renderPass, context.getPipelineCacheData(), context.getResourceInterface());
	}

	void recordCommands (OperationContext& context, const VkCommandBuffer cmdBuffer, const VkDescriptorSet descriptorSet)
	{
		const DeviceInterface&		vk						= context.getDeviceInterface();
		SynchronizationWrapperPtr	synchronizationWrapper	= getSynchronizationWrapper(context.getSynchronizationType(), vk, DE_FALSE);

		// Change color attachment image layout
		{
			const VkImageMemoryBarrier2KHR imageMemoryBarrier2 = makeImageMemoryBarrier2(
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,				// VkPipelineStageFlags2KHR			srcStageMask
				(VkAccessFlags)0,								// VkAccessFlags2KHR				srcAccessMask
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,	// VkPipelineStageFlags2KHR			dstStageMask
				VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR,		// VkAccessFlags2KHR				dstAccessMask
				VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout					oldLayout
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,		// VkImageLayout					newLayout
				**m_colorAttachmentImage,						// VkImage							image
				m_colorImageSubresourceRange					// VkImageSubresourceRange			subresourceRange
			);
			VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, DE_NULL, &imageMemoryBarrier2);
			synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &dependencyInfo);
		}

		{
			const VkRect2D	renderArea	= makeRect2D(m_colorImageExtent);
			const tcu::Vec4	clearColor	= tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);

			beginRenderPass(vk, cmdBuffer, *m_renderPass, *m_framebuffer, renderArea, clearColor);
		}

		vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
		vk.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayout, 0u, 1u, &descriptorSet, 0u, DE_NULL);
		{
			const VkDeviceSize	vertexBufferOffset	= 0ull;
			const VkBuffer		vertexBuffer		= m_vertices.getVertexBuffer();
			vk.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer, &vertexBufferOffset);
		}

		vk.cmdDraw(cmdBuffer, m_vertices.getNumVertices(), 1u, 0u, 0u);
		endRenderPass(vk, cmdBuffer);
	}

private:
	const VertexGrid			m_vertices;
	VkFormat					m_colorFormat;
	de::MovePtr<Image>			m_colorAttachmentImage;
	Move<VkImageView>			m_colorAttachmentView;
	VkExtent3D					m_colorImageExtent;
	VkImageSubresourceRange		m_colorImageSubresourceRange;
	Move<VkRenderPass>			m_renderPass;
	Move<VkFramebuffer>			m_framebuffer;
	Move<VkPipelineLayout>		m_pipelineLayout;
	Move<VkPipeline>			m_pipeline;
};

class ComputePipeline : public Pipeline
{
public:
	ComputePipeline (OperationContext& context, const DispatchCall dispatchCall, const std::string& shaderPrefix, const VkDescriptorSetLayout descriptorSetLayout)
		: m_dispatchCall	(dispatchCall)
	{
		const DeviceInterface&	vk			= context.getDeviceInterface();
		const VkDevice			device		= context.getDevice();
		Allocator&				allocator	= context.getAllocator();

		if (m_dispatchCall == DISPATCH_CALL_DISPATCH_INDIRECT)
		{
			m_indirectBuffer = de::MovePtr<Buffer>(new Buffer(vk, device, allocator,
				makeBufferCreateInfo(sizeof(VkDispatchIndirectCommand), VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT), MemoryRequirement::HostVisible));

			const Allocation&					alloc				= m_indirectBuffer->getAllocation();
			VkDispatchIndirectCommand* const	pIndirectCommand	= static_cast<VkDispatchIndirectCommand*>(alloc.getHostPtr());

			pIndirectCommand->x	= 1u;
			pIndirectCommand->y = 1u;
			pIndirectCommand->z	= 1u;

			flushAlloc(vk, device, alloc);
		}

		const Unique<VkShaderModule> shaderModule(createShaderModule(vk, device, context.getBinaryCollection().get(shaderPrefix + "comp"), (VkShaderModuleCreateFlags)0));

		m_pipelineLayout = makePipelineLayout(vk, device, descriptorSetLayout);
		m_pipeline		 = makeComputePipeline(vk, device, *m_pipelineLayout, *shaderModule, DE_NULL, context.getPipelineCacheData(), context.getResourceInterface());
	}

	void recordCommands (OperationContext& context, const VkCommandBuffer cmdBuffer, const VkDescriptorSet descriptorSet)
	{
		const DeviceInterface&	vk	= context.getDeviceInterface();

		vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *m_pipeline);
		vk.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *m_pipelineLayout, 0u, 1u, &descriptorSet, 0u, DE_NULL);

		if (m_dispatchCall == DISPATCH_CALL_DISPATCH_INDIRECT)
			vk.cmdDispatchIndirect(cmdBuffer, **m_indirectBuffer, 0u);
		else
			vk.cmdDispatch(cmdBuffer, 1u, 1u, 1u);
	}

private:
	const DispatchCall			m_dispatchCall;
	de::MovePtr<Buffer>			m_indirectBuffer;
	Move<VkPipelineLayout>		m_pipelineLayout;
	Move<VkPipeline>			m_pipeline;
};

//! Read/write operation on a UBO/SSBO in graphics/compute pipeline.
class BufferImplementation : public Operation
{
public:
	BufferImplementation (OperationContext&				context,
						  Resource&						resource,
						  const VkShaderStageFlagBits	stage,
						  const BufferType				bufferType,
						  const std::string&			shaderPrefix,
						  const AccessMode				mode,
						  const bool					specializedAccess,
						  const PipelineType			pipelineType,
						  const DispatchCall			dispatchCall)
		: Operation				(specializedAccess)
		, m_context				(context)
		, m_resource			(resource)
		, m_stage				(stage)
		, m_pipelineStage		(pipelineStageFlagsFromShaderStageFlagBits(m_stage))
		, m_bufferType			(bufferType)
		, m_mode				(mode)
		, m_dispatchCall		(dispatchCall)
	{
		requireFeaturesForSSBOAccess (m_context, m_stage);

		const DeviceInterface&	vk			= m_context.getDeviceInterface();
		const VkDevice			device		= m_context.getDevice();
		Allocator&				allocator	= m_context.getAllocator();

		m_hostBuffer = de::MovePtr<Buffer>(new Buffer(
			vk, device, allocator, makeBufferCreateInfo(m_resource.getBuffer().size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), MemoryRequirement::HostVisible));

		// Init host buffer data
		{
			const Allocation& alloc = m_hostBuffer->getAllocation();
			if (m_mode == ACCESS_MODE_READ)
				deMemset(alloc.getHostPtr(), 0, static_cast<size_t>(m_resource.getBuffer().size));
			else
				fillPattern(alloc.getHostPtr(), m_resource.getBuffer().size);
			flushAlloc(vk, device, alloc);
		}
		// Prepare descriptors
		{
			VkDescriptorType	bufferDescriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

			if (m_bufferType == BUFFER_TYPE_UNIFORM)
				bufferDescriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			else if (m_bufferType == BUFFER_TYPE_UNIFORM_TEXEL)
				bufferDescriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;

			m_descriptorSetLayout = DescriptorSetLayoutBuilder()
				.addSingleBinding(bufferDescriptorType, m_stage)
				.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, m_stage)
				.build(vk, device);

			m_descriptorPool = DescriptorPoolBuilder()
				.addType(bufferDescriptorType)
				.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
				.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

			m_descriptorSet = makeDescriptorSet(vk, device, *m_descriptorPool, *m_descriptorSetLayout);

			if (m_mode == ACCESS_MODE_READ)
			{
				if ((m_bufferType == BUFFER_TYPE_UNIFORM) || (m_bufferType == BUFFER_TYPE_STORAGE))
				{
					const VkDescriptorBufferInfo  bufferInfo	 = makeDescriptorBufferInfo(m_resource.getBuffer().handle, m_resource.getBuffer().offset, m_resource.getBuffer().size);
					const VkDescriptorBufferInfo  hostBufferInfo = makeDescriptorBufferInfo(**m_hostBuffer, 0u, m_resource.getBuffer().size);
					DescriptorSetUpdateBuilder()
						.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), bufferDescriptorType, &bufferInfo)
						.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &hostBufferInfo)
						.update(vk, device);
				}
				else
				{
					m_pBufferView = vk::makeBufferView(vk, device, m_resource.getBuffer().handle, VK_FORMAT_R32G32B32A32_UINT, m_resource.getBuffer().offset, m_resource.getBuffer().size);
					const VkDescriptorBufferInfo  hostBufferInfo = makeDescriptorBufferInfo(**m_hostBuffer, 0u, m_resource.getBuffer().size);
					DescriptorSetUpdateBuilder()
						.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), bufferDescriptorType, &m_pBufferView.get())
						.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &hostBufferInfo)
						.update(vk, device);
				}
			}
			else
			{
				const VkDescriptorBufferInfo  bufferInfo	 = makeDescriptorBufferInfo(m_resource.getBuffer().handle, m_resource.getBuffer().offset, m_resource.getBuffer().size);
				const VkDescriptorBufferInfo  hostBufferInfo = makeDescriptorBufferInfo(**m_hostBuffer, 0u, m_resource.getBuffer().size);
				DescriptorSetUpdateBuilder()
					.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &hostBufferInfo)
					.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferInfo)
					.update(vk, device);
			}
		}

		// Create pipeline
		m_pipeline = (pipelineType == PIPELINE_TYPE_GRAPHICS ? de::MovePtr<Pipeline>(new GraphicsPipeline(context, stage, shaderPrefix, *m_descriptorSetLayout))
															 : de::MovePtr<Pipeline>(new ComputePipeline(context, m_dispatchCall, shaderPrefix, *m_descriptorSetLayout)));
	}

	void recordCommands (const VkCommandBuffer cmdBuffer)
	{
		m_pipeline->recordCommands(m_context, cmdBuffer, *m_descriptorSet);

		// Post draw/dispatch commands

		if (m_mode == ACCESS_MODE_READ)
		{
			const DeviceInterface&		vk						= m_context.getDeviceInterface();
			SynchronizationWrapperPtr	synchronizationWrapper	= getSynchronizationWrapper(m_context.getSynchronizationType(), vk, DE_FALSE);

			// Insert a barrier so data written by the shader is available to the host
			const VkBufferMemoryBarrier2KHR bufferMemoryBarrier2 = makeBufferMemoryBarrier2(
				m_pipelineStage,							// VkPipelineStageFlags2KHR			srcStageMask
				VK_ACCESS_2_SHADER_WRITE_BIT_KHR,			// VkAccessFlags2KHR				srcAccessMask
				VK_PIPELINE_STAGE_HOST_BIT,					// VkPipelineStageFlags2KHR			dstStageMask
				VK_ACCESS_2_HOST_READ_BIT_KHR,				// VkAccessFlags2KHR				dstAccessMask
				**m_hostBuffer,								// VkBuffer							buffer
				0u,											// VkDeviceSize						offset
				m_resource.getBuffer().size					// VkDeviceSize						size
			);
			VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, &bufferMemoryBarrier2);
			synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &dependencyInfo);
		}
	}

	SyncInfo getInSyncInfo (void) const
	{
		VkAccessFlags2KHR accessFlags = VK_ACCESS_2_NONE_KHR;

		if (m_mode == ACCESS_MODE_READ)
		{
			if (m_bufferType == BUFFER_TYPE_UNIFORM)
				accessFlags = VK_ACCESS_2_UNIFORM_READ_BIT_KHR;

			else if (m_bufferType == BUFFER_TYPE_UNIFORM_TEXEL)
			{
				if (m_specializedAccess)
					accessFlags = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT_KHR;
				else
					accessFlags = VK_ACCESS_2_SHADER_READ_BIT_KHR;
			}
			else
			{
				if (m_specializedAccess)
					accessFlags = VK_ACCESS_2_SHADER_STORAGE_READ_BIT_KHR;
				else
					accessFlags = VK_ACCESS_2_SHADER_READ_BIT_KHR;
			}
		}
		else
		{
			if (m_specializedAccess)
				accessFlags = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT_KHR;
			else
				accessFlags = VK_ACCESS_2_SHADER_WRITE_BIT_KHR;
		}

		const SyncInfo			syncInfo	=
		{
			m_pipelineStage,				// VkPipelineStageFlags		stageMask;
			accessFlags,					// VkAccessFlags			accessMask;
			VK_IMAGE_LAYOUT_UNDEFINED,		// VkImageLayout			imageLayout;
		};
		return syncInfo;
	}

	SyncInfo getOutSyncInfo (void) const
	{
		VkAccessFlags2KHR	accessFlags = VK_ACCESS_2_NONE_KHR;

		if (m_mode == ACCESS_MODE_WRITE)
		{
			if (m_specializedAccess)
				accessFlags = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT_KHR;
			else
				accessFlags = VK_ACCESS_2_SHADER_WRITE_BIT_KHR;
		}

		const SyncInfo		syncInfo	=
		{
			m_pipelineStage,				// VkPipelineStageFlags		stageMask;
			accessFlags,					// VkAccessFlags			accessMask;
			VK_IMAGE_LAYOUT_UNDEFINED,		// VkImageLayout			imageLayout;
		};
		return syncInfo;
	}

	Data getData (void) const
	{
		return getHostBufferData(m_context, *m_hostBuffer, m_resource.getBuffer().size);
	}

	void setData (const Data& data)
	{
		DE_ASSERT(m_mode == ACCESS_MODE_WRITE);
		setHostBufferData(m_context, *m_hostBuffer, data);
	}

private:
	OperationContext&			m_context;
	Resource&					m_resource;
	const VkShaderStageFlagBits	m_stage;
	const VkPipelineStageFlags	m_pipelineStage;
	const BufferType			m_bufferType;
	const AccessMode			m_mode;
	const DispatchCall			m_dispatchCall;
	de::MovePtr<Buffer>			m_hostBuffer;
	Move<VkDescriptorPool>		m_descriptorPool;
	Move<VkDescriptorSetLayout>	m_descriptorSetLayout;
	Move<VkDescriptorSet>		m_descriptorSet;
	de::MovePtr<Pipeline>		m_pipeline;
	Move<VkBufferView>			m_pBufferView;
};

class ImageImplementation : public Operation
{
public:
	ImageImplementation (OperationContext&				context,
						 Resource&						resource,
						 const VkShaderStageFlagBits	stage,
						 const std::string&				shaderPrefix,
						 const AccessMode				mode,
						 const bool						specializedAccess,
						 const PipelineType				pipelineType,
						 const DispatchCall				dispatchCall)
		: Operation				(specializedAccess)
		, m_context				(context)
		, m_resource			(resource)
		, m_stage				(stage)
		, m_pipelineStage		(pipelineStageFlagsFromShaderStageFlagBits(m_stage))
		, m_mode				(mode)
		, m_dispatchCall		(dispatchCall)
		, m_hostBufferSizeBytes	(getPixelBufferSize(m_resource.getImage().format, m_resource.getImage().extent))
	{
		const DeviceInterface&		vk			= m_context.getDeviceInterface();
		const InstanceInterface&	vki			= m_context.getInstanceInterface();
		const VkDevice				device		= m_context.getDevice();
		const VkPhysicalDevice		physDevice	= m_context.getPhysicalDevice();
		Allocator&					allocator	= m_context.getAllocator();

		// Image stores are always required, in either access mode.
		requireFeaturesForSSBOAccess(m_context, m_stage);

		// Some storage image formats may not be supported
		const auto& imgResource = m_resource.getImage();
		requireStorageImageSupport(vki, physDevice, imgResource.format, imgResource.tiling);

		m_hostBuffer = de::MovePtr<Buffer>(new Buffer(
			vk, device, allocator, makeBufferCreateInfo(m_hostBufferSizeBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
			MemoryRequirement::HostVisible));

		// Init host buffer data
		{
			const Allocation& alloc = m_hostBuffer->getAllocation();
			if (m_mode == ACCESS_MODE_READ)
				deMemset(alloc.getHostPtr(), 0, static_cast<size_t>(m_hostBufferSizeBytes));
			else
				fillPattern(alloc.getHostPtr(), m_hostBufferSizeBytes);
			flushAlloc(vk, device, alloc);
		}

		// Image resources
		{
			m_image = de::MovePtr<Image>(new Image(vk, device, allocator,
				makeImageCreateInfo(m_resource.getImage().imageType, m_resource.getImage().extent, m_resource.getImage().format,
									(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT),
									VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL),
				MemoryRequirement::Any));

			if (m_mode == ACCESS_MODE_READ)
			{
				m_srcImage = &m_resource.getImage().handle;
				m_dstImage = &(**m_image);
			}
			else
			{
				m_srcImage = &(**m_image);
				m_dstImage = &m_resource.getImage().handle;
			}

			const VkImageViewType viewType = getImageViewType(m_resource.getImage().imageType);

			m_srcImageView	= makeImageView(vk, device, *m_srcImage, viewType, m_resource.getImage().format, m_resource.getImage().subresourceRange);
			m_dstImageView	= makeImageView(vk, device, *m_dstImage, viewType, m_resource.getImage().format, m_resource.getImage().subresourceRange);
		}

		// Prepare descriptors
		{
			m_descriptorSetLayout = DescriptorSetLayoutBuilder()
				.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, m_stage)
				.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, m_stage)
				.build(vk, device);

			m_descriptorPool = DescriptorPoolBuilder()
				.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
				.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
				.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

			m_descriptorSet = makeDescriptorSet(vk, device, *m_descriptorPool, *m_descriptorSetLayout);

			const VkDescriptorImageInfo srcImageInfo = makeDescriptorImageInfo(DE_NULL, *m_srcImageView, VK_IMAGE_LAYOUT_GENERAL);
			const VkDescriptorImageInfo dstImageInfo = makeDescriptorImageInfo(DE_NULL, *m_dstImageView, VK_IMAGE_LAYOUT_GENERAL);

			DescriptorSetUpdateBuilder()
				.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &srcImageInfo)
				.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &dstImageInfo)
				.update(vk, device);
		}

		// Create pipeline
		m_pipeline = (pipelineType == PIPELINE_TYPE_GRAPHICS ? de::MovePtr<Pipeline>(new GraphicsPipeline(context, stage, shaderPrefix, *m_descriptorSetLayout))
															 : de::MovePtr<Pipeline>(new ComputePipeline(context, m_dispatchCall, shaderPrefix, *m_descriptorSetLayout)));
	}

	void recordCommands (const VkCommandBuffer cmdBuffer)
	{
		const DeviceInterface&		vk						= m_context.getDeviceInterface();
		const VkBufferImageCopy		bufferCopyRegion		= makeBufferImageCopy(m_resource.getImage().extent, m_resource.getImage().subresourceLayers);
		SynchronizationWrapperPtr	synchronizationWrapper	= getSynchronizationWrapper(m_context.getSynchronizationType(), vk, DE_FALSE);

		// Destination image layout
		{
			const VkImageMemoryBarrier2KHR imageMemoryBarrier2 = makeImageMemoryBarrier2(
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,			// VkPipelineStageFlags2KHR			srcStageMask
				(VkAccessFlags)0,							// VkAccessFlags2KHR				srcAccessMask
				m_pipelineStage,							// VkPipelineStageFlags2KHR			dstStageMask
				VK_ACCESS_2_SHADER_WRITE_BIT_KHR,			// VkAccessFlags2KHR				dstAccessMask
				VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout					oldLayout
				VK_IMAGE_LAYOUT_GENERAL,					// VkImageLayout					newLayout
				*m_dstImage,								// VkImage							image
				m_resource.getImage().subresourceRange		// VkImageSubresourceRange			subresourceRange
			);
			VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, DE_NULL, &imageMemoryBarrier2);
			synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &dependencyInfo);
		}

		// In write mode, source image must be filled with data.
		if (m_mode == ACCESS_MODE_WRITE)
		{
			// Layout for transfer
			{
				const VkImageMemoryBarrier2KHR imageMemoryBarrier2 = makeImageMemoryBarrier2(
					VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,			// VkPipelineStageFlags2KHR			srcStageMask
					(VkAccessFlags)0,							// VkAccessFlags2KHR				srcAccessMask
					VK_PIPELINE_STAGE_TRANSFER_BIT,				// VkPipelineStageFlags2KHR			dstStageMask
					VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,			// VkAccessFlags2KHR				dstAccessMask
					VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout					oldLayout
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout					newLayout
					*m_srcImage,								// VkImage							image
					m_resource.getImage().subresourceRange		// VkImageSubresourceRange			subresourceRange
				);
				VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, DE_NULL, &imageMemoryBarrier2);
				synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &dependencyInfo);
			}

			// Host buffer -> Src image
			vk.cmdCopyBufferToImage(cmdBuffer, **m_hostBuffer, *m_srcImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &bufferCopyRegion);

			// Layout for shader reading
			{
				const VkImageMemoryBarrier2KHR imageMemoryBarrier2 = makeImageMemoryBarrier2(
					VK_PIPELINE_STAGE_TRANSFER_BIT,				// VkPipelineStageFlags2KHR			srcStageMask
					VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,			// VkAccessFlags2KHR				srcAccessMask
					m_pipelineStage,							// VkPipelineStageFlags2KHR			dstStageMask
					VK_ACCESS_2_SHADER_READ_BIT_KHR,			// VkAccessFlags2KHR				dstAccessMask
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,		// VkImageLayout					oldLayout
					VK_IMAGE_LAYOUT_GENERAL,					// VkImageLayout					newLayout
					*m_srcImage,								// VkImage							image
					m_resource.getImage().subresourceRange		// VkImageSubresourceRange			subresourceRange
				);
				VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, DE_NULL, &imageMemoryBarrier2);
				synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &dependencyInfo);
			}
		}

		// Execute shaders

		m_pipeline->recordCommands(m_context, cmdBuffer, *m_descriptorSet);

		// Post draw/dispatch commands

		if (m_mode == ACCESS_MODE_READ)
		{
			// Layout for transfer
			{
				const VkImageMemoryBarrier2KHR imageMemoryBarrier2 = makeImageMemoryBarrier2(
					m_pipelineStage,							// VkPipelineStageFlags2KHR			srcStageMask
					VK_ACCESS_2_SHADER_WRITE_BIT_KHR,			// VkAccessFlags2KHR				srcAccessMask
					VK_PIPELINE_STAGE_TRANSFER_BIT,				// VkPipelineStageFlags2KHR			dstStageMask
					VK_ACCESS_2_TRANSFER_READ_BIT_KHR,			// VkAccessFlags2KHR				dstAccessMask
					VK_IMAGE_LAYOUT_GENERAL,					// VkImageLayout					oldLayout
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,		// VkImageLayout					newLayout
					*m_dstImage,								// VkImage							image
					m_resource.getImage().subresourceRange		// VkImageSubresourceRange			subresourceRange
				);
				VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, DE_NULL, &imageMemoryBarrier2);
				synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &dependencyInfo);
			}

			// Dst image -> Host buffer
			vk.cmdCopyImageToBuffer(cmdBuffer, *m_dstImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, **m_hostBuffer, 1u, &bufferCopyRegion);

			// Insert a barrier so data written by the shader is available to the host
			{
				const VkBufferMemoryBarrier2KHR bufferMemoryBarrier2 = makeBufferMemoryBarrier2(
					VK_PIPELINE_STAGE_TRANSFER_BIT,				// VkPipelineStageFlags2KHR			srcStageMask
					VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,			// VkAccessFlags2KHR				srcAccessMask
					VK_PIPELINE_STAGE_HOST_BIT,					// VkPipelineStageFlags2KHR			dstStageMask
					VK_ACCESS_2_HOST_READ_BIT_KHR,				// VkAccessFlags2KHR				dstAccessMask
					**m_hostBuffer,								// VkBuffer							buffer
					0u,											// VkDeviceSize						offset
					m_hostBufferSizeBytes						// VkDeviceSize						size
				);
				VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, &bufferMemoryBarrier2);
				synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &dependencyInfo);
			}
		}
	}

	SyncInfo getInSyncInfo (void) const
	{
		VkAccessFlags2KHR accessFlags = VK_ACCESS_2_NONE_KHR;

		if (m_mode == ACCESS_MODE_READ)
		{
			if (m_specializedAccess)
				accessFlags = VK_ACCESS_2_SHADER_STORAGE_READ_BIT_KHR;
			else
				accessFlags = VK_ACCESS_2_SHADER_READ_BIT_KHR;
		}

		const SyncInfo		syncInfo	=
		{
			m_pipelineStage,			// VkPipelineStageFlags		stageMask;
			accessFlags,				// VkAccessFlags			accessMask;
			VK_IMAGE_LAYOUT_GENERAL,	// VkImageLayout			imageLayout;
		};
		return syncInfo;
	}

	SyncInfo getOutSyncInfo (void) const
	{
		VkAccessFlags2KHR	accessFlags = VK_ACCESS_2_NONE_KHR;

		if (m_mode == ACCESS_MODE_WRITE)
		{
			if (m_specializedAccess)
				accessFlags = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT_KHR;
			else
				accessFlags = VK_ACCESS_2_SHADER_WRITE_BIT_KHR;
		}

		const SyncInfo		syncInfo	=
		{
			m_pipelineStage,			// VkPipelineStageFlags		stageMask;
			accessFlags,				// VkAccessFlags			accessMask;
			VK_IMAGE_LAYOUT_GENERAL,	// VkImageLayout			imageLayout;
		};
		return syncInfo;
	}

	Data getData (void) const
	{
		return getHostBufferData(m_context, *m_hostBuffer, m_hostBufferSizeBytes);
	}

	void setData (const Data& data)
	{
		DE_ASSERT(m_mode == ACCESS_MODE_WRITE);
		setHostBufferData(m_context, *m_hostBuffer, data);
	}

private:
	OperationContext&			m_context;
	Resource&					m_resource;
	const VkShaderStageFlagBits	m_stage;
	const VkPipelineStageFlags	m_pipelineStage;
	const AccessMode			m_mode;
	const DispatchCall			m_dispatchCall;
	const VkDeviceSize			m_hostBufferSizeBytes;
	de::MovePtr<Buffer>			m_hostBuffer;
	de::MovePtr<Image>			m_image;			//! Additional image used as src or dst depending on operation mode.
	const VkImage*				m_srcImage;
	const VkImage*				m_dstImage;
	Move<VkImageView>			m_srcImageView;
	Move<VkImageView>			m_dstImageView;
	Move<VkDescriptorPool>		m_descriptorPool;
	Move<VkDescriptorSetLayout>	m_descriptorSetLayout;
	Move<VkDescriptorSet>		m_descriptorSet;
	de::MovePtr<Pipeline>		m_pipeline;

};

//! Create generic passthrough shaders with bits of custom code inserted in a specific shader stage.
void initPassthroughPrograms (SourceCollections&			programCollection,
							  const std::string&			shaderPrefix,
							  const std::string&			declCode,
							  const std::string&			mainCode,
							  const VkShaderStageFlagBits	stage)
{
	const VkShaderStageFlags	requiredStages	= getRequiredStages(stage);

	if (requiredStages & VK_SHADER_STAGE_VERTEX_BIT)
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_440) << "\n"
			<< "\n"
			<< "layout(location = 0) in vec4 v_in_position;\n"
			<< "\n"
			<< "out " << s_perVertexBlock << ";\n"
			<< "\n"
			<< (stage & VK_SHADER_STAGE_VERTEX_BIT ? declCode + "\n" : "")
			<< "void main (void)\n"
			<< "{\n"
			<< "    gl_Position = v_in_position;\n"
			<< (stage & VK_SHADER_STAGE_VERTEX_BIT ? mainCode : "")
			<< "}\n";

		if (!programCollection.glslSources.contains(shaderPrefix + "vert"))
			programCollection.glslSources.add(shaderPrefix + "vert") << glu::VertexSource(src.str());
	}

	if (requiredStages & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_440) << "\n"
			<< "\n"
			<< "layout(vertices = 3) out;\n"
			<< "\n"
			<< "in " << s_perVertexBlock << " gl_in[gl_MaxPatchVertices];\n"
			<< "\n"
			<< "out " << s_perVertexBlock << " gl_out[];\n"
			<< "\n"
			<< (stage & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT ? declCode + "\n" : "")
			<< "void main (void)\n"
			<< "{\n"
			<< "    gl_TessLevelInner[0] = 1.0;\n"
			<< "    gl_TessLevelInner[1] = 1.0;\n"
			<< "\n"
			<< "    gl_TessLevelOuter[0] = 1.0;\n"
			<< "    gl_TessLevelOuter[1] = 1.0;\n"
			<< "    gl_TessLevelOuter[2] = 1.0;\n"
			<< "    gl_TessLevelOuter[3] = 1.0;\n"
			<< "\n"
			<< "    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
			<< (stage & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT ? "\n" + mainCode : "")
			<< "}\n";

		if (!programCollection.glslSources.contains(shaderPrefix + "tesc"))
			programCollection.glslSources.add(shaderPrefix + "tesc") << glu::TessellationControlSource(src.str());
	}

	if (requiredStages & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_440) << "\n"
			<< "\n"
			<< "layout(triangles, equal_spacing, ccw) in;\n"
			<< "\n"
			<< "in " << s_perVertexBlock << " gl_in[gl_MaxPatchVertices];\n"
			<< "\n"
			<< "out " << s_perVertexBlock << ";\n"
			<< "\n"
			<< (stage & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT ? declCode + "\n" : "")
			<< "void main (void)\n"
			<< "{\n"
			<< "    vec3 px = gl_TessCoord.x * gl_in[0].gl_Position.xyz;\n"
			<< "    vec3 py = gl_TessCoord.y * gl_in[1].gl_Position.xyz;\n"
			<< "    vec3 pz = gl_TessCoord.z * gl_in[2].gl_Position.xyz;\n"
			<< "    gl_Position = vec4(px + py + pz, 1.0);\n"
			<< (stage & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT ? mainCode : "")
			<< "}\n";

		if (!programCollection.glslSources.contains(shaderPrefix + "tese"))
			programCollection.glslSources.add(shaderPrefix + "tese") << glu::TessellationEvaluationSource(src.str());
	}

	if (requiredStages & VK_SHADER_STAGE_GEOMETRY_BIT)
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_440) << "\n"
			<< "\n"
			<< "layout(triangles) in;\n"
			<< "layout(triangle_strip, max_vertices = 3) out;\n"
			<< "\n"
			<< "in " << s_perVertexBlock << " gl_in[];\n"
			<< "\n"
			<< "out " << s_perVertexBlock << ";\n"
			<< "\n"
			<< (stage & VK_SHADER_STAGE_GEOMETRY_BIT ? declCode + "\n" : "")
			<< "void main (void)\n"
			<< "{\n"
			<< "    gl_Position = gl_in[0].gl_Position;\n"
			<< "    EmitVertex();\n"
			<< "\n"
			<< "    gl_Position = gl_in[1].gl_Position;\n"
			<< "    EmitVertex();\n"
			<< "\n"
			<< "    gl_Position = gl_in[2].gl_Position;\n"
			<< "    EmitVertex();\n"
			<< (stage & VK_SHADER_STAGE_GEOMETRY_BIT ? "\n" + mainCode : "")
			<< "}\n";

		if (!programCollection.glslSources.contains(shaderPrefix + "geom"))
			programCollection.glslSources.add(shaderPrefix + "geom") << glu::GeometrySource(src.str());
	}

	if (requiredStages & VK_SHADER_STAGE_FRAGMENT_BIT)
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_440) << "\n"
			<< "\n"
			<< "layout(location = 0) out vec4 o_color;\n"
			<< "\n"
			<< (stage & VK_SHADER_STAGE_FRAGMENT_BIT ? declCode + "\n" : "")
			<< "void main (void)\n"
			<< "{\n"
			<< "    o_color = vec4(1.0);\n"
			<< (stage & VK_SHADER_STAGE_FRAGMENT_BIT ? "\n" + mainCode : "")
			<< "}\n";

		if (!programCollection.glslSources.contains(shaderPrefix + "frag"))
			programCollection.glslSources.add(shaderPrefix + "frag") << glu::FragmentSource(src.str());
	}

	if (requiredStages & VK_SHADER_STAGE_COMPUTE_BIT)
	{
		std::ostringstream src;
		src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_440) << "\n"
			<< "\n"
			<< "layout(local_size_x = 1) in;\n"
			<< "\n"
			<< (stage & VK_SHADER_STAGE_COMPUTE_BIT ? declCode + "\n" : "")
			<< "void main (void)\n"
			<< "{\n"
			<< (stage & VK_SHADER_STAGE_COMPUTE_BIT ? mainCode : "")
			<< "}\n";

		if (!programCollection.glslSources.contains(shaderPrefix + "comp"))
			programCollection.glslSources.add(shaderPrefix + "comp") << glu::ComputeSource(src.str());
	}
}

class BufferSupport : public OperationSupport
{
public:
	BufferSupport (const ResourceDescription&	resourceDesc,
				   const BufferType				bufferType,
				   const AccessMode				mode,
				   const bool					specializedAccess,
				   const VkShaderStageFlagBits	stage,
				   const DispatchCall			dispatchCall = DISPATCH_CALL_DISPATCH)
		: OperationSupport		(specializedAccess)
		, m_resourceDesc		(resourceDesc)
		, m_bufferType			(bufferType)
		, m_mode				(mode)
		, m_stage				(stage)
		, m_shaderPrefix		(std::string(m_mode == ACCESS_MODE_READ ? "read_" : "write_") + (m_bufferType == BUFFER_TYPE_UNIFORM ? "ubo_" : (m_bufferType == BUFFER_TYPE_UNIFORM_TEXEL ? "ubo_texel_" : "ssbo_")))
		, m_dispatchCall		(dispatchCall)
	{
		DE_ASSERT(m_resourceDesc.type == RESOURCE_TYPE_BUFFER);
		DE_ASSERT(m_bufferType == BUFFER_TYPE_UNIFORM || m_bufferType == BUFFER_TYPE_STORAGE || m_bufferType == BUFFER_TYPE_UNIFORM_TEXEL);
		DE_ASSERT(m_mode == ACCESS_MODE_READ || m_mode == ACCESS_MODE_WRITE);
		DE_ASSERT(m_mode == ACCESS_MODE_READ || m_bufferType == BUFFER_TYPE_STORAGE);
		DE_ASSERT(m_bufferType != BUFFER_TYPE_UNIFORM || m_resourceDesc.size.x() <= MAX_UBO_RANGE);
		DE_ASSERT(m_dispatchCall == DISPATCH_CALL_DISPATCH || m_dispatchCall == DISPATCH_CALL_DISPATCH_INDIRECT);

		assertValidShaderStage(m_stage);
	}

	void initPrograms (SourceCollections& programCollection) const
	{
		DE_ASSERT((m_resourceDesc.size.x() % sizeof(tcu::UVec4)) == 0);

		std::string	bufferTypeStr = "";
		if (m_bufferType == BUFFER_TYPE_UNIFORM)
			bufferTypeStr = "uniform";
		else
		{
			if (m_bufferType == BUFFER_TYPE_UNIFORM_TEXEL)
				bufferTypeStr = "uniform utextureBuffer";
			else
				bufferTypeStr = "buffer";
		}

		const int numVecElements	= static_cast<int>(m_resourceDesc.size.x() / sizeof(tcu::UVec4));  // std140 must be aligned to a multiple of 16
		std::ostringstream declSrc;
		std::ostringstream copySrc;
		std::string outputBuff	=	"layout(set = 0, binding = 1, std140) writeonly buffer Output {\n"
									"    uvec4 data[" + std::to_string(numVecElements) + "];\n"
									"} b_out;\n";
		if ((m_bufferType == BUFFER_TYPE_UNIFORM) || (m_bufferType == BUFFER_TYPE_STORAGE))
		{
			declSrc << "layout(set = 0, binding = 0, std140) readonly " << bufferTypeStr << " Input {\n"
					<< "    uvec4 data[" << numVecElements << "];\n"
					<< "} b_in;\n"
					<< "\n"
					<< outputBuff;

			copySrc << "    for (int i = 0; i < " << numVecElements << "; ++i) {\n"
					<< "        b_out.data[i] = b_in.data[i];\n"
					<< "    }\n";
		}
		else if (m_bufferType == BUFFER_TYPE_UNIFORM_TEXEL)
		{
			declSrc << "layout(set = 0, binding = 0) " << bufferTypeStr << " Input;\n"
					<< "\n"
					<< outputBuff;

			copySrc << "    for (int i = 0; i < " << numVecElements << "; ++i) {\n"
					<< "        b_out.data[i] = texelFetch(Input, i);\n"
					<< "    }\n";
		}

		initPassthroughPrograms(programCollection, m_shaderPrefix, declSrc.str(), copySrc.str(), m_stage);
	}

	deUint32 getInResourceUsageFlags (void) const
	{
		if (m_bufferType == BUFFER_TYPE_UNIFORM)
			return m_mode == ACCESS_MODE_READ ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT : 0;
		else if (m_bufferType == BUFFER_TYPE_UNIFORM_TEXEL)
			return m_mode == ACCESS_MODE_READ ? VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT : 0;
		else
			return m_mode == ACCESS_MODE_READ ? VK_BUFFER_USAGE_STORAGE_BUFFER_BIT : 0;
	}

	deUint32 getOutResourceUsageFlags (void) const
	{
		if (m_bufferType == BUFFER_TYPE_UNIFORM)
			return m_mode == ACCESS_MODE_WRITE ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT : 0;
		else if (m_bufferType == BUFFER_TYPE_UNIFORM_TEXEL)
			return m_mode == ACCESS_MODE_WRITE ? 0 : VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
		else
			return m_mode == ACCESS_MODE_WRITE ? VK_BUFFER_USAGE_STORAGE_BUFFER_BIT : 0;
	}

	VkQueueFlags getQueueFlags (const OperationContext& context) const
	{
		DE_UNREF(context);
		return (m_stage == VK_SHADER_STAGE_COMPUTE_BIT ? VK_QUEUE_COMPUTE_BIT : VK_QUEUE_GRAPHICS_BIT);
	}

	de::MovePtr<Operation> build (OperationContext& context, Resource& resource) const
	{
		if (m_stage & VK_SHADER_STAGE_COMPUTE_BIT)
			return de::MovePtr<Operation>(new BufferImplementation(context, resource, m_stage, m_bufferType, m_shaderPrefix, m_mode, m_specializedAccess, PIPELINE_TYPE_COMPUTE, m_dispatchCall));
		else
			return de::MovePtr<Operation>(new BufferImplementation(context, resource, m_stage, m_bufferType, m_shaderPrefix, m_mode, m_specializedAccess, PIPELINE_TYPE_GRAPHICS, m_dispatchCall));
	}

	de::MovePtr<Operation> build (OperationContext&, Resource&, Resource&) const
	{
		DE_ASSERT(0);
		return de::MovePtr<Operation>();
	}

private:
	const ResourceDescription	m_resourceDesc;
	const BufferType			m_bufferType;
	const AccessMode			m_mode;
	const VkShaderStageFlagBits	m_stage;
	const std::string			m_shaderPrefix;
	const DispatchCall			m_dispatchCall;
};

class ImageSupport : public OperationSupport
{
public:
	ImageSupport (const ResourceDescription&	resourceDesc,
				  const AccessMode				mode,
				  const bool					specializedAccess,
				  const VkShaderStageFlagBits	stage,
				  const DispatchCall			dispatchCall = DISPATCH_CALL_DISPATCH)
		: OperationSupport		(specializedAccess)
		, m_resourceDesc		(resourceDesc)
		, m_mode				(mode)
		, m_stage				(stage)
		, m_shaderPrefix		(m_mode == ACCESS_MODE_READ ? "read_image_" : "write_image_")
		, m_dispatchCall		(dispatchCall)
	{
		DE_ASSERT(m_resourceDesc.type == RESOURCE_TYPE_IMAGE);
		DE_ASSERT(m_mode == ACCESS_MODE_READ || m_mode == ACCESS_MODE_WRITE);
		DE_ASSERT(m_dispatchCall == DISPATCH_CALL_DISPATCH || m_dispatchCall == DISPATCH_CALL_DISPATCH_INDIRECT);

		assertValidShaderStage(m_stage);
	}

	void initPrograms (SourceCollections& programCollection) const
	{
		const std::string	imageFormat	= getShaderImageFormatQualifier(m_resourceDesc.imageFormat);
		const std::string	imageType	= getShaderImageType(m_resourceDesc.imageFormat, m_resourceDesc.imageType);

		std::ostringstream declSrc;
		declSrc << "layout(set = 0, binding = 0, " << imageFormat << ") readonly  uniform " << imageType << " srcImg;\n"
				<< "layout(set = 0, binding = 1, " << imageFormat << ") writeonly uniform " << imageType << " dstImg;\n";

		std::ostringstream mainSrc;
		if (m_resourceDesc.imageType == VK_IMAGE_TYPE_1D)
			mainSrc << "    for (int x = 0; x < " << m_resourceDesc.size.x() << "; ++x)\n"
					<< "        imageStore(dstImg, x, imageLoad(srcImg, x));\n";
		else if (m_resourceDesc.imageType == VK_IMAGE_TYPE_2D)
			mainSrc << "    for (int y = 0; y < " << m_resourceDesc.size.y() << "; ++y)\n"
					<< "    for (int x = 0; x < " << m_resourceDesc.size.x() << "; ++x)\n"
					<< "        imageStore(dstImg, ivec2(x, y), imageLoad(srcImg, ivec2(x, y)));\n";
		else if (m_resourceDesc.imageType == VK_IMAGE_TYPE_3D)
			mainSrc << "    for (int z = 0; z < " << m_resourceDesc.size.z() << "; ++z)\n"
					<< "    for (int y = 0; y < " << m_resourceDesc.size.y() << "; ++y)\n"
					<< "    for (int x = 0; x < " << m_resourceDesc.size.x() << "; ++x)\n"
					<< "        imageStore(dstImg, ivec3(x, y, z), imageLoad(srcImg, ivec3(x, y, z)));\n";
		else
			DE_ASSERT(0);

		initPassthroughPrograms(programCollection, m_shaderPrefix, declSrc.str(), mainSrc.str(), m_stage);
	}

	deUint32 getInResourceUsageFlags (void) const
	{
		return VK_IMAGE_USAGE_STORAGE_BIT;
	}

	deUint32 getOutResourceUsageFlags (void) const
	{
		return VK_IMAGE_USAGE_STORAGE_BIT;
	}

	VkQueueFlags getQueueFlags (const OperationContext& context) const
	{
		DE_UNREF(context);
		return (m_stage == VK_SHADER_STAGE_COMPUTE_BIT ? VK_QUEUE_COMPUTE_BIT : VK_QUEUE_GRAPHICS_BIT);
	}

	de::MovePtr<Operation> build (OperationContext& context, Resource& resource) const
	{
		if (m_stage & VK_SHADER_STAGE_COMPUTE_BIT)
			return de::MovePtr<Operation>(new ImageImplementation(context, resource, m_stage, m_shaderPrefix, m_mode, m_specializedAccess, PIPELINE_TYPE_COMPUTE, m_dispatchCall));
		else
			return de::MovePtr<Operation>(new ImageImplementation(context, resource, m_stage, m_shaderPrefix, m_mode, m_specializedAccess, PIPELINE_TYPE_GRAPHICS, m_dispatchCall));
	}

	de::MovePtr<Operation> build (OperationContext&, Resource&, Resource&) const
	{
		DE_ASSERT(0);
		return de::MovePtr<Operation>();
	}

private:
	const ResourceDescription	m_resourceDesc;
	const AccessMode			m_mode;
	const VkShaderStageFlagBits	m_stage;
	const std::string			m_shaderPrefix;
	const DispatchCall			m_dispatchCall;
};

//! Copy operation on a UBO/SSBO in graphics/compute pipeline.
class BufferCopyImplementation : public Operation
{
public:
	BufferCopyImplementation (OperationContext&				context,
							  Resource&						inResource,
							  Resource&						outResource,
							  const VkShaderStageFlagBits	stage,
							  const BufferType				bufferType,
							  const std::string&			shaderPrefix,
							  const bool					specializedAccess,
							  const PipelineType			pipelineType,
							  const DispatchCall			dispatchCall)
		: Operation				(specializedAccess)
		, m_context				(context)
		, m_inResource			(inResource)
		, m_outResource			(outResource)
		, m_stage				(stage)
		, m_pipelineStage		(pipelineStageFlagsFromShaderStageFlagBits(m_stage))
		, m_bufferType			(bufferType)
		, m_dispatchCall		(dispatchCall)
	{
		requireFeaturesForSSBOAccess (m_context, m_stage);

		const DeviceInterface&	vk			= m_context.getDeviceInterface();
		const VkDevice			device		= m_context.getDevice();

		// Prepare descriptors
		{
			const VkDescriptorType	bufferDescriptorType	= (m_bufferType == BUFFER_TYPE_UNIFORM ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

			m_descriptorSetLayout = DescriptorSetLayoutBuilder()
				.addSingleBinding(bufferDescriptorType, m_stage)
				.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, m_stage)
				.build(vk, device);

			m_descriptorPool = DescriptorPoolBuilder()
				.addType(bufferDescriptorType)
				.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
				.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

			m_descriptorSet = makeDescriptorSet(vk, device, *m_descriptorPool, *m_descriptorSetLayout);

			const VkDescriptorBufferInfo  inBufferInfo = makeDescriptorBufferInfo(m_inResource.getBuffer().handle, m_inResource.getBuffer().offset, m_inResource.getBuffer().size);
			const VkDescriptorBufferInfo  outBufferInfo	 = makeDescriptorBufferInfo(m_outResource.getBuffer().handle, m_outResource.getBuffer().offset, m_outResource.getBuffer().size);

			DescriptorSetUpdateBuilder()
				.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &inBufferInfo)
				.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &outBufferInfo)
				.update(vk, device);
		}

		// Create pipeline
		m_pipeline = (pipelineType == PIPELINE_TYPE_GRAPHICS ? de::MovePtr<Pipeline>(new GraphicsPipeline(context, stage, shaderPrefix, *m_descriptorSetLayout))
															 : de::MovePtr<Pipeline>(new ComputePipeline(context, m_dispatchCall, shaderPrefix, *m_descriptorSetLayout)));
	}

	void recordCommands (const VkCommandBuffer cmdBuffer)
	{
		m_pipeline->recordCommands(m_context, cmdBuffer, *m_descriptorSet);
	}

	SyncInfo getInSyncInfo (void) const
	{
		VkAccessFlags2KHR	accessFlags = (m_specializedAccess ? VK_ACCESS_2_SHADER_STORAGE_READ_BIT_KHR : VK_ACCESS_2_SHADER_READ_BIT_KHR);
		const SyncInfo		syncInfo	=
		{
			m_pipelineStage,					// VkPipelineStageFlags		stageMask;
			accessFlags,						// VkAccessFlags			accessMask;
			VK_IMAGE_LAYOUT_UNDEFINED,			// VkImageLayout			imageLayout;
		};
		return syncInfo;
	}

	SyncInfo getOutSyncInfo (void) const
	{
		VkAccessFlags2KHR	accessFlags = (m_specializedAccess ? VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT_KHR : VK_ACCESS_2_SHADER_WRITE_BIT_KHR);
		const SyncInfo		syncInfo	=
		{
			m_pipelineStage,					// VkPipelineStageFlags		stageMask;
			accessFlags,						// VkAccessFlags			accessMask;
			VK_IMAGE_LAYOUT_UNDEFINED,			// VkImageLayout			imageLayout;
		};
		return syncInfo;
	}

	Data getData (void) const
	{
		Data data = { 0, DE_NULL };
		return data;
	}

	void setData (const Data&)
	{
		DE_ASSERT(0);
	}

private:
	OperationContext&			m_context;
	Resource&					m_inResource;
	Resource&					m_outResource;
	const VkShaderStageFlagBits	m_stage;
	const VkPipelineStageFlags	m_pipelineStage;
	const BufferType			m_bufferType;
	const DispatchCall			m_dispatchCall;
	Move<VkDescriptorPool>		m_descriptorPool;
	Move<VkDescriptorSetLayout>	m_descriptorSetLayout;
	Move<VkDescriptorSet>		m_descriptorSet;
	de::MovePtr<Pipeline>		m_pipeline;
};

class CopyBufferSupport : public OperationSupport
{
public:
	CopyBufferSupport (const ResourceDescription&	resourceDesc,
					   const BufferType				bufferType,
					   const bool					specializedAccess,
					   const VkShaderStageFlagBits	stage,
					   const DispatchCall			dispatchCall = DISPATCH_CALL_DISPATCH)
		: OperationSupport		(specializedAccess)
		, m_resourceDesc		(resourceDesc)
		, m_bufferType			(bufferType)
		, m_stage				(stage)
		, m_shaderPrefix		(std::string("copy_") + getShaderStageName(stage) + (m_bufferType == BUFFER_TYPE_UNIFORM ? "_ubo_" : "_ssbo_"))
		, m_dispatchCall		(dispatchCall)
	{
		DE_ASSERT(m_resourceDesc.type == RESOURCE_TYPE_BUFFER);
		DE_ASSERT(m_bufferType == BUFFER_TYPE_UNIFORM || m_bufferType == BUFFER_TYPE_STORAGE);
		DE_ASSERT(m_bufferType != BUFFER_TYPE_UNIFORM || m_resourceDesc.size.x() <= MAX_UBO_RANGE);
		DE_ASSERT(m_dispatchCall == DISPATCH_CALL_DISPATCH || m_dispatchCall == DISPATCH_CALL_DISPATCH_INDIRECT);

		assertValidShaderStage(m_stage);
	}

	void initPrograms (SourceCollections& programCollection) const
	{
		DE_ASSERT((m_resourceDesc.size.x() % sizeof(tcu::UVec4)) == 0);

		const std::string	bufferTypeStr	= (m_bufferType == BUFFER_TYPE_UNIFORM ? "uniform" : "buffer");
		const int			numVecElements	= static_cast<int>(m_resourceDesc.size.x() / sizeof(tcu::UVec4));  // std140 must be aligned to a multiple of 16

		std::ostringstream declSrc;
		declSrc << "layout(set = 0, binding = 0, std140) readonly " << bufferTypeStr << " Input {\n"
				<< "    uvec4 data[" << numVecElements << "];\n"
				<< "} b_in;\n"
				<< "\n"
				<< "layout(set = 0, binding = 1, std140) writeonly buffer Output {\n"
				<< "    uvec4 data[" << numVecElements << "];\n"
				<< "} b_out;\n";

		std::ostringstream copySrc;
		copySrc << "    for (int i = 0; i < " << numVecElements << "; ++i) {\n"
				<< "        b_out.data[i] = b_in.data[i];\n"
				<< "    }\n";

		initPassthroughPrograms(programCollection, m_shaderPrefix, declSrc.str(), copySrc.str(), m_stage);
	}

	deUint32 getInResourceUsageFlags (void) const
	{
		return (m_bufferType == BUFFER_TYPE_UNIFORM ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT : VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	}

	deUint32 getOutResourceUsageFlags (void) const
	{
		return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	}

	VkQueueFlags getQueueFlags (const OperationContext& context) const
	{
		DE_UNREF(context);
		return (m_stage == VK_SHADER_STAGE_COMPUTE_BIT ? VK_QUEUE_COMPUTE_BIT : VK_QUEUE_GRAPHICS_BIT);
	}

	de::MovePtr<Operation> build (OperationContext&, Resource&) const
	{
		DE_ASSERT(0);
		return de::MovePtr<Operation>();
	}

	de::MovePtr<Operation> build (OperationContext& context, Resource& inResource, Resource& outResource) const
	{
		if (m_stage & VK_SHADER_STAGE_COMPUTE_BIT)
			return de::MovePtr<Operation>(new BufferCopyImplementation(context, inResource, outResource, m_stage, m_bufferType, m_shaderPrefix, m_specializedAccess, PIPELINE_TYPE_COMPUTE, m_dispatchCall));
		else
			return de::MovePtr<Operation>(new BufferCopyImplementation(context, inResource, outResource, m_stage, m_bufferType, m_shaderPrefix, m_specializedAccess, PIPELINE_TYPE_GRAPHICS, m_dispatchCall));
	}

private:
	const ResourceDescription	m_resourceDesc;
	const BufferType			m_bufferType;
	const VkShaderStageFlagBits	m_stage;
	const std::string			m_shaderPrefix;
	const DispatchCall			m_dispatchCall;
};

class CopyImageImplementation : public Operation
{
public:
	CopyImageImplementation (OperationContext&				context,
							 Resource&						inResource,
							 Resource&						outResource,
							 const VkShaderStageFlagBits	stage,
							 const std::string&				shaderPrefix,
							 const bool						specializedAccess,
							 const PipelineType				pipelineType,
							 const DispatchCall				dispatchCall)
		: Operation				(specializedAccess)
		, m_context				(context)
		, m_inResource			(inResource)
		, m_outResource			(outResource)
		, m_stage				(stage)
		, m_pipelineStage		(pipelineStageFlagsFromShaderStageFlagBits(m_stage))
		, m_dispatchCall		(dispatchCall)
	{
		const DeviceInterface&		vk			= m_context.getDeviceInterface();
		const InstanceInterface&	vki			= m_context.getInstanceInterface();
		const VkDevice				device		= m_context.getDevice();
		const VkPhysicalDevice		physDevice	= m_context.getPhysicalDevice();

		// Image stores are always required, in either access mode.
		requireFeaturesForSSBOAccess(m_context, m_stage);

		// Some storage image formats may not be supported
		const auto& imgResource = m_inResource.getImage();
		requireStorageImageSupport(vki, physDevice, imgResource.format, imgResource.tiling);

		// Image resources
		{
			const VkImageViewType viewType = getImageViewType(m_inResource.getImage().imageType);

			m_srcImageView	= makeImageView(vk, device, m_inResource.getImage().handle, viewType, m_inResource.getImage().format, m_inResource.getImage().subresourceRange);
			m_dstImageView	= makeImageView(vk, device, m_outResource.getImage().handle, viewType, m_outResource.getImage().format, m_outResource.getImage().subresourceRange);
		}

		// Prepare descriptors
		{
			m_descriptorSetLayout = DescriptorSetLayoutBuilder()
				.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, m_stage)
				.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, m_stage)
				.build(vk, device);

			m_descriptorPool = DescriptorPoolBuilder()
				.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
				.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
				.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

			m_descriptorSet = makeDescriptorSet(vk, device, *m_descriptorPool, *m_descriptorSetLayout);

			const VkDescriptorImageInfo srcImageInfo = makeDescriptorImageInfo(DE_NULL, *m_srcImageView, VK_IMAGE_LAYOUT_GENERAL);
			const VkDescriptorImageInfo dstImageInfo = makeDescriptorImageInfo(DE_NULL, *m_dstImageView, VK_IMAGE_LAYOUT_GENERAL);

			DescriptorSetUpdateBuilder()
				.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &srcImageInfo)
				.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &dstImageInfo)
				.update(vk, device);
		}

		// Create pipeline
		m_pipeline = (pipelineType == PIPELINE_TYPE_GRAPHICS ? de::MovePtr<Pipeline>(new GraphicsPipeline(context, stage, shaderPrefix, *m_descriptorSetLayout))
															 : de::MovePtr<Pipeline>(new ComputePipeline(context, m_dispatchCall, shaderPrefix, *m_descriptorSetLayout)));
	}

	void recordCommands (const VkCommandBuffer cmdBuffer)
	{
		{
			const DeviceInterface&		vk						= m_context.getDeviceInterface();
			SynchronizationWrapperPtr	synchronizationWrapper	= getSynchronizationWrapper(m_context.getSynchronizationType(), vk, DE_FALSE);

			const VkImageMemoryBarrier2KHR imageMemoryBarriers2 = makeImageMemoryBarrier2(
					VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,				// VkPipelineStageFlags2KHR			srcStageMask
					(VkAccessFlags)0,								// VkAccessFlags2KHR				srcAccessMask
					m_pipelineStage,								// VkPipelineStageFlags2KHR			dstStageMask
					VK_ACCESS_2_SHADER_WRITE_BIT_KHR,				// VkAccessFlags2KHR				dstAccessMask
					VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout					oldLayout
					VK_IMAGE_LAYOUT_GENERAL,						// VkImageLayout					newLayout
					m_outResource.getImage().handle,				// VkImage							image
					m_outResource.getImage().subresourceRange		// VkImageSubresourceRange			subresourceRange
			);
			VkDependencyInfoKHR dependencyInfo
			{
				VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,				// VkStructureType					sType
				DE_NULL,											// const void*						pNext
				VK_DEPENDENCY_BY_REGION_BIT,						// VkDependencyFlags				dependencyFlags
				0u,													// deUint32							memoryBarrierCount
				DE_NULL,											// const VkMemoryBarrier2KHR*		pMemoryBarriers
				0u,													// deUint32							bufferMemoryBarrierCount
				DE_NULL,											// const VkBufferMemoryBarrier2KHR* pBufferMemoryBarriers
				1,													// deUint32							imageMemoryBarrierCount
				&imageMemoryBarriers2								// const VkImageMemoryBarrier2KHR*	pImageMemoryBarriers
			};
			synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &dependencyInfo);
		}

		// Execute shaders
		m_pipeline->recordCommands(m_context, cmdBuffer, *m_descriptorSet);
	}

	SyncInfo getInSyncInfo (void) const
	{
		VkAccessFlags2KHR	accessFlags = (m_specializedAccess ? VK_ACCESS_2_SHADER_STORAGE_READ_BIT_KHR : VK_ACCESS_2_SHADER_READ_BIT_KHR);
		const SyncInfo		syncInfo	=
		{
			m_pipelineStage,					// VkPipelineStageFlags		stageMask;
			accessFlags,						// VkAccessFlags			accessMask;
			VK_IMAGE_LAYOUT_GENERAL,			// VkImageLayout			imageLayout;
		};
		return syncInfo;
	}

	SyncInfo getOutSyncInfo (void) const
	{
		VkAccessFlags2KHR	accessFlags = (m_specializedAccess ? VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT_KHR : VK_ACCESS_2_SHADER_WRITE_BIT_KHR);
		const SyncInfo		syncInfo	=
		{
			m_pipelineStage,					// VkPipelineStageFlags		stageMask;
			accessFlags,						// VkAccessFlags			accessMask;
			VK_IMAGE_LAYOUT_GENERAL,			// VkImageLayout			imageLayout;
		};
		return syncInfo;
	}

	Data getData (void) const
	{
		Data data = { 0, DE_NULL };
		return data;
	}

	void setData (const Data&)
	{
		DE_ASSERT(0);
	}

private:
	OperationContext&			m_context;
	Resource&					m_inResource;
	Resource&					m_outResource;
	const VkShaderStageFlagBits	m_stage;
	const VkPipelineStageFlags	m_pipelineStage;
	const DispatchCall			m_dispatchCall;
	Move<VkImageView>			m_srcImageView;
	Move<VkImageView>			m_dstImageView;
	Move<VkDescriptorPool>		m_descriptorPool;
	Move<VkDescriptorSetLayout>	m_descriptorSetLayout;
	Move<VkDescriptorSet>		m_descriptorSet;
	de::MovePtr<Pipeline>		m_pipeline;
};

class CopyImageSupport : public OperationSupport
{
public:
	CopyImageSupport (const ResourceDescription&	resourceDesc,
					  const VkShaderStageFlagBits	stage,
					  const bool					specializedAccess,
					  const DispatchCall			dispatchCall = DISPATCH_CALL_DISPATCH)
		: OperationSupport		(specializedAccess)
		, m_resourceDesc		(resourceDesc)
		, m_stage				(stage)
		, m_shaderPrefix		(std::string("copy_image_") + getShaderStageName(stage) + "_")
		, m_dispatchCall		(dispatchCall)
	{
		DE_ASSERT(m_resourceDesc.type == RESOURCE_TYPE_IMAGE);
		DE_ASSERT(m_dispatchCall == DISPATCH_CALL_DISPATCH || m_dispatchCall == DISPATCH_CALL_DISPATCH_INDIRECT);

		assertValidShaderStage(m_stage);
	}

	void initPrograms (SourceCollections& programCollection) const
	{
		const std::string	imageFormat	= getShaderImageFormatQualifier(m_resourceDesc.imageFormat);
		const std::string	imageType	= getShaderImageType(m_resourceDesc.imageFormat, m_resourceDesc.imageType);

		std::ostringstream declSrc;
		declSrc << "layout(set = 0, binding = 0, " << imageFormat << ") readonly  uniform " << imageType << " srcImg;\n"
				<< "layout(set = 0, binding = 1, " << imageFormat << ") writeonly uniform " << imageType << " dstImg;\n";

		std::ostringstream mainSrc;
		if (m_resourceDesc.imageType == VK_IMAGE_TYPE_1D)
			mainSrc << "    for (int x = 0; x < " << m_resourceDesc.size.x() << "; ++x)\n"
					<< "        imageStore(dstImg, x, imageLoad(srcImg, x));\n";
		else if (m_resourceDesc.imageType == VK_IMAGE_TYPE_2D)
			mainSrc << "    for (int y = 0; y < " << m_resourceDesc.size.y() << "; ++y)\n"
					<< "    for (int x = 0; x < " << m_resourceDesc.size.x() << "; ++x)\n"
					<< "        imageStore(dstImg, ivec2(x, y), imageLoad(srcImg, ivec2(x, y)));\n";
		else if (m_resourceDesc.imageType == VK_IMAGE_TYPE_3D)
			mainSrc << "    for (int z = 0; z < " << m_resourceDesc.size.z() << "; ++z)\n"
					<< "    for (int y = 0; y < " << m_resourceDesc.size.y() << "; ++y)\n"
					<< "    for (int x = 0; x < " << m_resourceDesc.size.x() << "; ++x)\n"
					<< "        imageStore(dstImg, ivec3(x, y, z), imageLoad(srcImg, ivec3(x, y, z)));\n";
		else
			DE_ASSERT(0);

		initPassthroughPrograms(programCollection, m_shaderPrefix, declSrc.str(), mainSrc.str(), m_stage);
	}

	deUint32 getInResourceUsageFlags (void) const
	{
		return VK_IMAGE_USAGE_STORAGE_BIT;
	}

	deUint32 getOutResourceUsageFlags (void) const
	{
		return VK_IMAGE_USAGE_STORAGE_BIT;
	}

	VkQueueFlags getQueueFlags (const OperationContext& context) const
	{
		DE_UNREF(context);
		return (m_stage == VK_SHADER_STAGE_COMPUTE_BIT ? VK_QUEUE_COMPUTE_BIT : VK_QUEUE_GRAPHICS_BIT);
	}

	de::MovePtr<Operation> build (OperationContext&, Resource&) const
	{
		DE_ASSERT(0);
		return de::MovePtr<Operation>();
	}

	de::MovePtr<Operation> build (OperationContext& context, Resource& inResource, Resource& outResource) const
	{
		if (m_stage & VK_SHADER_STAGE_COMPUTE_BIT)
			return de::MovePtr<Operation>(new CopyImageImplementation(context, inResource, outResource, m_stage, m_shaderPrefix, m_specializedAccess, PIPELINE_TYPE_COMPUTE, m_dispatchCall));
		else
			return de::MovePtr<Operation>(new CopyImageImplementation(context, inResource, outResource, m_stage, m_shaderPrefix, m_specializedAccess, PIPELINE_TYPE_GRAPHICS, m_dispatchCall));
	}

private:
	const ResourceDescription	m_resourceDesc;
	const VkShaderStageFlagBits	m_stage;
	const std::string			m_shaderPrefix;
	const DispatchCall			m_dispatchCall;
};

class MSImageImplementation : public Operation
{
public:
	MSImageImplementation(OperationContext&	context,
						  Resource&			resource)
		: m_context		(context)
		, m_resource	(resource)
		, m_hostBufferSizeBytes(getPixelBufferSize(m_resource.getImage().format, m_resource.getImage().extent))
	{
		const DeviceInterface&			vk			= m_context.getDeviceInterface();
		const InstanceInterface&		vki			= m_context.getInstanceInterface();
		const VkDevice					device		= m_context.getDevice();
		const VkPhysicalDevice			physDevice	= m_context.getPhysicalDevice();
		const VkPhysicalDeviceFeatures	features	= getPhysicalDeviceFeatures(vki, physDevice);
		Allocator&						allocator	= m_context.getAllocator();

		const auto& imgResource = m_resource.getImage();
		requireStorageImageSupport(vki, physDevice, imgResource.format, imgResource.tiling);
		if (!features.shaderStorageImageMultisample)
			TCU_THROW(NotSupportedError, "Using multisample images as storage is not supported");

		VkBufferCreateInfo bufferCreateInfo = makeBufferCreateInfo(m_hostBufferSizeBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		m_hostBuffer = de::MovePtr<Buffer>(new Buffer(vk, device, allocator, bufferCreateInfo, MemoryRequirement::HostVisible));
		const Allocation& alloc = m_hostBuffer->getAllocation();
		fillPattern(alloc.getHostPtr(), m_hostBufferSizeBytes);
		flushAlloc(vk, device, alloc);

		const ImageResource&  image		= m_resource.getImage();
		const VkImageViewType viewType	= getImageViewType(image.imageType);
		m_imageView = makeImageView(vk, device, image.handle, viewType, image.format, image.subresourceRange);

		// Prepare descriptors
		{
			m_descriptorSetLayout = DescriptorSetLayoutBuilder()
				.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
				.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  VK_SHADER_STAGE_COMPUTE_BIT)
				.build(vk, device);

			m_descriptorPool = DescriptorPoolBuilder()
				.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
				.addType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
				.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

			m_descriptorSet = makeDescriptorSet(vk, device, *m_descriptorPool, *m_descriptorSetLayout);

			const VkDescriptorBufferInfo bufferInfo	= makeDescriptorBufferInfo(**m_hostBuffer, 0u, m_hostBufferSizeBytes);
			const VkDescriptorImageInfo  imageInfo	= makeDescriptorImageInfo(DE_NULL, *m_imageView, VK_IMAGE_LAYOUT_GENERAL);

			DescriptorSetUpdateBuilder()
				.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferInfo)
				.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(1u), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  &imageInfo)
				.update(vk, device);
		}

		// Create pipeline
		const Unique<VkShaderModule> shaderModule(createShaderModule(vk, device, context.getBinaryCollection().get("comp"), (VkShaderModuleCreateFlags)0));
		m_pipelineLayout	= makePipelineLayout (vk, device, *m_descriptorSetLayout);
		m_pipeline			= makeComputePipeline(vk, device, *m_pipelineLayout, *shaderModule, DE_NULL, context.getPipelineCacheData(), context.getResourceInterface());
	}

	void recordCommands(const VkCommandBuffer cmdBuffer)
	{
		const DeviceInterface&		vk						= m_context.getDeviceInterface();
		SynchronizationWrapperPtr	synchronizationWrapper	= getSynchronizationWrapper(m_context.getSynchronizationType(), vk, DE_FALSE);

		// change image layout
		{
			const VkImageMemoryBarrier2KHR imageMemoryBarrier2 = makeImageMemoryBarrier2(
				VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR,	// VkPipelineStageFlags2KHR			srcStageMask
				(VkAccessFlags)0,							// VkAccessFlags2KHR				srcAccessMask
				VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR,	// VkPipelineStageFlags2KHR			dstStageMask
				VK_ACCESS_2_SHADER_WRITE_BIT_KHR,			// VkAccessFlags2KHR				dstAccessMask
				VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout					oldLayout
				VK_IMAGE_LAYOUT_GENERAL,					// VkImageLayout					newLayout
				m_resource.getImage().handle,				// VkImage							image
				m_resource.getImage().subresourceRange		// VkImageSubresourceRange			subresourceRange
			);
			VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, DE_NULL, &imageMemoryBarrier2);
			synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &dependencyInfo);
		}

		// execute shader
		vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *m_pipeline);
		vk.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *m_pipelineLayout, 0u, 1u, &*m_descriptorSet, 0u, DE_NULL);
		vk.cmdDispatch(cmdBuffer, m_resource.getImage().extent.width, m_resource.getImage().extent.height, 1u);
	}

	SyncInfo getInSyncInfo(void) const
	{
		DE_ASSERT(false);
		return emptySyncInfo;
	}

	SyncInfo getOutSyncInfo(void) const
	{
		return
		{
			VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR,	// VkPipelineStageFlags		stageMask;
			VK_ACCESS_2_SHADER_WRITE_BIT_KHR,			// VkAccessFlags			accessMask;
			VK_IMAGE_LAYOUT_GENERAL,					// VkImageLayout			imageLayout;
		};
	}

	Data getData(void) const
	{
		return getHostBufferData(m_context, *m_hostBuffer, m_hostBufferSizeBytes);
	}

	void setData(const Data&)
	{
		DE_ASSERT(false);
	}

private:
	OperationContext&			m_context;
	Resource&					m_resource;
	Move<VkImageView>			m_imageView;

	const VkDeviceSize			m_hostBufferSizeBytes;
	de::MovePtr<Buffer>			m_hostBuffer;

	Move<VkDescriptorPool>		m_descriptorPool;
	Move<VkDescriptorSetLayout>	m_descriptorSetLayout;
	Move<VkDescriptorSet>		m_descriptorSet;
	Move<VkPipelineLayout>		m_pipelineLayout;
	Move<VkPipeline>			m_pipeline;
};

class MSImageSupport : public OperationSupport
{
public:
	MSImageSupport(const ResourceDescription& resourceDesc)
		: m_resourceDesc	(resourceDesc)
	{
		DE_ASSERT(m_resourceDesc.type == RESOURCE_TYPE_IMAGE);
	}

	void initPrograms (SourceCollections& programCollection) const
	{
		std::stringstream source;
		source <<
			"#version 440\n"
			"\n"
			"layout(local_size_x = 1) in;\n"
			"layout(set = 0, binding = 0, std430) readonly buffer Input {\n"
			"    uint data[];\n"
			"} inData;\n"
			"layout(set = 0, binding = 1, r32ui) writeonly uniform uimage2DMS msImage;\n"
			"\n"
			"void main (void)\n"
			"{\n"
			"  int  gx    = int(gl_GlobalInvocationID.x);\n"
			"  int  gy    = int(gl_GlobalInvocationID.y);\n"
			"  uint value = inData.data[gy * " << m_resourceDesc.size.x() << " + gx];\n"
			"  for (int sampleNdx = 0; sampleNdx < " << m_resourceDesc.imageSamples << "; ++sampleNdx)\n"
			"    imageStore(msImage, ivec2(gx, gy), sampleNdx, uvec4(value));\n"
			"}\n";
		programCollection.glslSources.add("comp") << glu::ComputeSource(source.str().c_str());
	}

	deUint32 getInResourceUsageFlags (void) const
	{
		return 0;
	}

	deUint32 getOutResourceUsageFlags (void) const
	{
		return VK_IMAGE_USAGE_STORAGE_BIT;
	}

	VkQueueFlags getQueueFlags (const OperationContext&) const
	{
		return VK_QUEUE_COMPUTE_BIT;
	}

	de::MovePtr<Operation> build (OperationContext& context, Resource& resource) const
	{
		return de::MovePtr<Operation>(new MSImageImplementation(context, resource));
	}

	de::MovePtr<Operation> build (OperationContext&, Resource&, Resource&) const
	{
		DE_ASSERT(0);
		return de::MovePtr<Operation>();
	}

private:
	const ResourceDescription	m_resourceDesc;
};

} // ShaderAccess ns

namespace CopyBufferToImage
{

class WriteImplementation : public Operation
{
public:
	WriteImplementation (OperationContext& context, Resource& resource)
		: m_context		(context)
		, m_resource	(resource)
		, m_bufferSize	(getPixelBufferSize(m_resource.getImage().format, m_resource.getImage().extent))
	{
		DE_ASSERT(m_resource.getType() == RESOURCE_TYPE_IMAGE);

		const DeviceInterface&	vk			= m_context.getDeviceInterface();
		const VkDevice			device		= m_context.getDevice();
		Allocator&				allocator	= m_context.getAllocator();

		m_hostBuffer = de::MovePtr<Buffer>(new Buffer(
			vk, device, allocator, makeBufferCreateInfo(m_bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT), MemoryRequirement::HostVisible));

		const Allocation& alloc = m_hostBuffer->getAllocation();
		fillPattern(alloc.getHostPtr(), m_bufferSize);
		flushAlloc(vk, device, alloc);
	}

	void recordCommands (const VkCommandBuffer cmdBuffer)
	{
		const DeviceInterface&		vk						= m_context.getDeviceInterface();
		const VkBufferImageCopy		copyRegion				= makeBufferImageCopy(m_resource.getImage().extent, m_resource.getImage().subresourceLayers);
		SynchronizationWrapperPtr	synchronizationWrapper	= getSynchronizationWrapper(m_context.getSynchronizationType(), vk, DE_FALSE);

		const VkImageMemoryBarrier2KHR imageMemoryBarrier2 = makeImageMemoryBarrier2(
			VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR,			// VkPipelineStageFlags2KHR			srcStageMask
			(VkAccessFlags)0,									// VkAccessFlags2KHR				srcAccessMask
			VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,				// VkPipelineStageFlags2KHR			dstStageMask
			VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,					// VkAccessFlags2KHR				dstAccessMask
			VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout					oldLayout
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,				// VkImageLayout					newLayout
			m_resource.getImage().handle,						// VkImage							image
			m_resource.getImage().subresourceRange				// VkImageSubresourceRange			subresourceRange
		);
		VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, DE_NULL, &imageMemoryBarrier2);
		synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &dependencyInfo);

		vk.cmdCopyBufferToImage(cmdBuffer, **m_hostBuffer, m_resource.getImage().handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &copyRegion);
	}

	SyncInfo getInSyncInfo (void) const
	{
		return emptySyncInfo;
	}

	SyncInfo getOutSyncInfo (void) const
	{
		const SyncInfo syncInfo =
		{
			VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,	// VkPipelineStageFlags		stageMask;
			VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,		// VkAccessFlags			accessMask;
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,	// VkImageLayout			imageLayout;
		};
		return syncInfo;
	}

	Data getData (void) const
	{
		return getHostBufferData(m_context, *m_hostBuffer, m_bufferSize);
	}

	void setData (const Data& data)
	{
		setHostBufferData(m_context, *m_hostBuffer, data);
	}

private:
	OperationContext&		m_context;
	Resource&				m_resource;
	de::MovePtr<Buffer>		m_hostBuffer;
	const VkDeviceSize		m_bufferSize;
};

class ReadImplementation : public Operation
{
public:
	ReadImplementation (OperationContext& context, Resource& resource)
		: m_context				(context)
		, m_resource			(resource)
		, m_subresourceRange	(makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u))
		, m_subresourceLayers	(makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u))
	{
		DE_ASSERT(m_resource.getType() == RESOURCE_TYPE_BUFFER);

		const DeviceInterface&	vk			= m_context.getDeviceInterface();
		const VkDevice			device		= m_context.getDevice();
		Allocator&				allocator	= m_context.getAllocator();
		const VkFormat			format		= VK_FORMAT_R8G8B8A8_UNORM;
		const deUint32			pixelSize	= tcu::getPixelSize(mapVkFormat(format));

		DE_ASSERT((m_resource.getBuffer().size % pixelSize) == 0);
		m_imageExtent = get2DImageExtentWithSize(m_resource.getBuffer().size, pixelSize);  // there may be some unused space at the end

		// Copy destination image.
		m_image = de::MovePtr<Image>(new Image(
			vk, device, allocator,
			makeImageCreateInfo(VK_IMAGE_TYPE_2D, m_imageExtent, format,
								(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT),
								VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL),
			MemoryRequirement::Any));

		// Image data will be copied here, so it can be read on the host.
		m_hostBuffer = de::MovePtr<Buffer>(new Buffer(
			vk, device, allocator, makeBufferCreateInfo(m_resource.getBuffer().size, VK_BUFFER_USAGE_TRANSFER_DST_BIT), MemoryRequirement::HostVisible));
	}

	void recordCommands (const VkCommandBuffer cmdBuffer)
	{
		const DeviceInterface&		vk						= m_context.getDeviceInterface();
		const VkBufferImageCopy		copyRegion				= makeBufferImageCopy(m_imageExtent, m_subresourceLayers);
		SynchronizationWrapperPtr	synchronizationWrapper	= getSynchronizationWrapper(m_context.getSynchronizationType(), vk, DE_FALSE);

		// Resource -> Image
		{
			const VkImageMemoryBarrier2KHR imageMemoryBarrier2 = makeImageMemoryBarrier2(
				VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR,			// VkPipelineStageFlags2KHR			srcStageMask
				(VkAccessFlags)0,									// VkAccessFlags2KHR				srcAccessMask
				VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,				// VkPipelineStageFlags2KHR			dstStageMask
				VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,					// VkAccessFlags2KHR				dstAccessMask
				VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout					oldLayout
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,				// VkImageLayout					newLayout
				**m_image,											// VkImage							image
				m_subresourceRange									// VkImageSubresourceRange			subresourceRange
			);
			VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, DE_NULL, &imageMemoryBarrier2);
			synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &dependencyInfo);

			vk.cmdCopyBufferToImage(cmdBuffer, m_resource.getBuffer().handle, **m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &copyRegion);
		}
		// Image -> Host buffer
		{
			const VkImageMemoryBarrier2KHR imageLayoutBarrier2 = makeImageMemoryBarrier2(
				VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,				// VkPipelineStageFlags2KHR			srcStageMask
				VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,					// VkAccessFlags2KHR				srcAccessMask
				VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,				// VkPipelineStageFlags2KHR			dstStageMask
				VK_ACCESS_2_TRANSFER_READ_BIT_KHR,					// VkAccessFlags2KHR				dstAccessMask
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,				// VkImageLayout					oldLayout
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,				// VkImageLayout					newLayout
				**m_image,											// VkImage							image
				m_subresourceRange									// VkImageSubresourceRange			subresourceRange
			);
			VkDependencyInfoKHR layoutDependencyInfo = makeCommonDependencyInfo(DE_NULL, DE_NULL, &imageLayoutBarrier2);
			synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &layoutDependencyInfo);

			vk.cmdCopyImageToBuffer(cmdBuffer, **m_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, **m_hostBuffer, 1u, &copyRegion);

			const VkBufferMemoryBarrier2KHR bufferMemoryBarrier2 = makeBufferMemoryBarrier2(
				VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,				// VkPipelineStageFlags2KHR			srcStageMask
				VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,					// VkAccessFlags2KHR				srcAccessMask
				VK_PIPELINE_STAGE_2_HOST_BIT_KHR,					// VkPipelineStageFlags2KHR			dstStageMask
				VK_ACCESS_2_HOST_READ_BIT_KHR,						// VkAccessFlags2KHR				dstAccessMask
				**m_hostBuffer,										// VkBuffer							buffer
				0u,													// VkDeviceSize						offset
				m_resource.getBuffer().size							// VkDeviceSize						size
			);
			VkDependencyInfoKHR bufferDependencyInfo = makeCommonDependencyInfo(DE_NULL, &bufferMemoryBarrier2);
			synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &bufferDependencyInfo);
		}
	}

	SyncInfo getInSyncInfo (void) const
	{
		const SyncInfo syncInfo =
		{
			VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,	// VkPipelineStageFlags		stageMask;
			VK_ACCESS_2_TRANSFER_READ_BIT_KHR,		// VkAccessFlags			accessMask;
			VK_IMAGE_LAYOUT_UNDEFINED,				// VkImageLayout			imageLayout;
		};
		return syncInfo;
	}

	SyncInfo getOutSyncInfo (void) const
	{
		return emptySyncInfo;
	}

	Data getData (void) const
	{
		return getHostBufferData(m_context, *m_hostBuffer, m_resource.getBuffer().size);
	}

	void setData (const Data& data)
	{
		setHostBufferData(m_context, *m_hostBuffer, data);
	}

private:
	OperationContext&				m_context;
	Resource&						m_resource;
	const VkImageSubresourceRange	m_subresourceRange;
	const VkImageSubresourceLayers	m_subresourceLayers;
	de::MovePtr<Buffer>				m_hostBuffer;
	de::MovePtr<Image>				m_image;
	VkExtent3D						m_imageExtent;
};

class Support : public OperationSupport
{
public:
	Support (const ResourceDescription& resourceDesc, const AccessMode mode)
		: m_mode				(mode)
		, m_resourceType		(resourceDesc.type)
		, m_requiredQueueFlags	(resourceDesc.type == RESOURCE_TYPE_IMAGE && isDepthStencilFormat(resourceDesc.imageFormat) ? VK_QUEUE_GRAPHICS_BIT : VK_QUEUE_TRANSFER_BIT)
	{
		// From spec:
		//   Because depth or stencil aspect buffer to image copies may require format conversions on some implementations,
		//   they are not supported on queues that do not support graphics.

		DE_ASSERT(m_mode == ACCESS_MODE_READ || m_mode == ACCESS_MODE_WRITE);
		DE_ASSERT(m_mode == ACCESS_MODE_READ || resourceDesc.type != RESOURCE_TYPE_BUFFER);
		DE_ASSERT(m_mode == ACCESS_MODE_WRITE || resourceDesc.type != RESOURCE_TYPE_IMAGE);
	}

	deUint32 getInResourceUsageFlags (void) const
	{
		if (m_resourceType == RESOURCE_TYPE_IMAGE)
			return m_mode == ACCESS_MODE_READ ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0;
		else
			return m_mode == ACCESS_MODE_READ ? VK_BUFFER_USAGE_TRANSFER_SRC_BIT : 0;
	}

	deUint32 getOutResourceUsageFlags (void) const
	{
		if (m_resourceType == RESOURCE_TYPE_IMAGE)
			return m_mode == ACCESS_MODE_WRITE ? VK_IMAGE_USAGE_TRANSFER_DST_BIT : 0;
		else
			return m_mode == ACCESS_MODE_WRITE ? VK_BUFFER_USAGE_TRANSFER_DST_BIT : 0;
	}

	VkQueueFlags getQueueFlags (const OperationContext& context) const
	{
		DE_UNREF(context);
		return m_requiredQueueFlags;
	}

	de::MovePtr<Operation> build (OperationContext& context, Resource& resource) const
	{
		if (m_mode == ACCESS_MODE_READ)
			return de::MovePtr<Operation>(new ReadImplementation(context, resource));
		else
			return de::MovePtr<Operation>(new WriteImplementation(context, resource));
	}

	de::MovePtr<Operation> build (OperationContext&, Resource&, Resource&) const
	{
		DE_ASSERT(0);
		return de::MovePtr<Operation>();
	}

private:
	const AccessMode			m_mode;
	const enum ResourceType		m_resourceType;
	const VkQueueFlags			m_requiredQueueFlags;
};

class CopyImplementation : public Operation
{
public:
	CopyImplementation (OperationContext& context, Resource& inResource, Resource& outResource)
		: m_context		(context)
		, m_inResource	(inResource)
		, m_outResource	(outResource)
	{
		DE_ASSERT(m_inResource.getType() == RESOURCE_TYPE_BUFFER);
		DE_ASSERT(m_outResource.getType() == RESOURCE_TYPE_IMAGE);
	}

	void recordCommands (const VkCommandBuffer cmdBuffer)
	{
		const DeviceInterface&		vk						= m_context.getDeviceInterface();
		const VkBufferImageCopy		copyRegion				= makeBufferImageCopy(m_outResource.getImage().extent, m_outResource.getImage().subresourceLayers);
		SynchronizationWrapperPtr	synchronizationWrapper	= getSynchronizationWrapper(m_context.getSynchronizationType(), vk, DE_FALSE);

		const VkBufferMemoryBarrier2KHR bufferMemoryBarrier2 = makeBufferMemoryBarrier2(
			VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR,		// VkPipelineStageFlags2KHR			srcStageMask
			(VkAccessFlags)0,								// VkAccessFlags2KHR				srcAccessMask
			VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,			// VkPipelineStageFlags2KHR			dstStageMask
			VK_ACCESS_2_TRANSFER_READ_BIT_KHR,				// VkAccessFlags2KHR				dstAccessMask
			m_inResource.getBuffer().handle,				// VkBuffer							buffer
			0u,												// VkDeviceSize						offset
			m_inResource.getBuffer().size					// VkDeviceSize						size
		);
		const VkImageMemoryBarrier2KHR imageMemoryBarrier2 = makeImageMemoryBarrier2(
			VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR,		// VkPipelineStageFlags2KHR			srcStageMask
			(VkAccessFlags)0,								// VkAccessFlags2KHR				srcAccessMask
			VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,			// VkPipelineStageFlags2KHR			dstStageMask
			VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,				// VkAccessFlags2KHR				dstAccessMask
			VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout					oldLayout
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,			// VkImageLayout					newLayout
			m_outResource.getImage().handle,				// VkImage							image
			m_outResource.getImage().subresourceRange		// VkImageSubresourceRange			subresourceRange
		);
		VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, &bufferMemoryBarrier2, &imageMemoryBarrier2);
		synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &dependencyInfo);

		vk.cmdCopyBufferToImage(cmdBuffer, m_inResource.getBuffer().handle, m_outResource.getImage().handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &copyRegion);
	}

	SyncInfo getInSyncInfo (void) const
	{
		const SyncInfo syncInfo =
		{
			VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,	// VkPipelineStageFlags		stageMask;
			VK_ACCESS_2_TRANSFER_READ_BIT_KHR,		// VkAccessFlags			accessMask;
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,	// VkImageLayout			imageLayout;
		};
		return syncInfo;
	}

	SyncInfo getOutSyncInfo (void) const
	{
		const SyncInfo syncInfo =
		{
			VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,	// VkPipelineStageFlags		stageMask;
			VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,		// VkAccessFlags			accessMask;
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,	// VkImageLayout			imageLayout;
		};
		return syncInfo;
	}

	Data getData (void) const
	{
		Data data = { 0, DE_NULL };
		return data;
	}

	void setData (const Data&)
	{
		DE_ASSERT(0);
	}

private:
	OperationContext&		m_context;
	Resource&				m_inResource;
	Resource&				m_outResource;
};

class CopySupport : public OperationSupport
{
public:
	CopySupport (const ResourceDescription& resourceDesc)
		: m_resourceType		(resourceDesc.type)
		, m_requiredQueueFlags	(resourceDesc.type == RESOURCE_TYPE_IMAGE && isDepthStencilFormat(resourceDesc.imageFormat) ? VK_QUEUE_GRAPHICS_BIT : VK_QUEUE_TRANSFER_BIT)
	{
	}

	deUint32 getInResourceUsageFlags (void) const
	{
		if (m_resourceType == RESOURCE_TYPE_IMAGE)
			return VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		else
			return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	}

	deUint32 getOutResourceUsageFlags (void) const
	{
		if (m_resourceType == RESOURCE_TYPE_IMAGE)
			return VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		else
			return VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	}

	VkQueueFlags getQueueFlags (const OperationContext& context) const
	{
		DE_UNREF(context);
		return m_requiredQueueFlags;
	}

	de::MovePtr<Operation> build (OperationContext&, Resource&) const
	{
		DE_ASSERT(0);
		return de::MovePtr<Operation>();
	}

	de::MovePtr<Operation> build (OperationContext& context, Resource& inResource, Resource& outResource) const
	{
		return de::MovePtr<Operation>(new CopyImplementation(context, inResource, outResource));
	}

private:
	const enum ResourceType		m_resourceType;
	const VkQueueFlags			m_requiredQueueFlags;
};

} // CopyBufferToImage ns

namespace CopyImageToBuffer
{

class WriteImplementation : public Operation
{
public:
	WriteImplementation (OperationContext& context, Resource& resource)
		: m_context				(context)
		, m_resource			(resource)
		, m_subresourceRange	(makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u))
		, m_subresourceLayers	(makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u))
	{
		DE_ASSERT(m_resource.getType() == RESOURCE_TYPE_BUFFER);

		const DeviceInterface&	vk			= m_context.getDeviceInterface();
		const VkDevice			device		= m_context.getDevice();
		Allocator&				allocator	= m_context.getAllocator();
		const VkFormat			format		= VK_FORMAT_R8G8B8A8_UNORM;
		const deUint32			pixelSize	= tcu::getPixelSize(mapVkFormat(format));

		DE_ASSERT((m_resource.getBuffer().size % pixelSize) == 0);
		m_imageExtent = get2DImageExtentWithSize(m_resource.getBuffer().size, pixelSize);

		// Source data staging buffer
		m_hostBuffer = de::MovePtr<Buffer>(new Buffer(
			vk, device, allocator, makeBufferCreateInfo(m_resource.getBuffer().size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT), MemoryRequirement::HostVisible));

		const Allocation& alloc = m_hostBuffer->getAllocation();
		fillPattern(alloc.getHostPtr(), m_resource.getBuffer().size);
		flushAlloc(vk, device, alloc);

		// Source data image
		m_image = de::MovePtr<Image>(new Image(
			vk, device, allocator,
			makeImageCreateInfo(VK_IMAGE_TYPE_2D, m_imageExtent, format, (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT),
								VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL),
			MemoryRequirement::Any));
	}

	void recordCommands (const VkCommandBuffer cmdBuffer)
	{
		const DeviceInterface&		vk						= m_context.getDeviceInterface();
		const VkBufferImageCopy		copyRegion				= makeBufferImageCopy(m_imageExtent, m_subresourceLayers);
		SynchronizationWrapperPtr	synchronizationWrapper	= getSynchronizationWrapper(m_context.getSynchronizationType(), vk, DE_FALSE);

		// Host buffer -> Image
		{
			const VkImageMemoryBarrier2KHR imageMemoryBarrier2 = makeImageMemoryBarrier2(
				VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR,		// VkPipelineStageFlags2KHR			srcStageMask
				(VkAccessFlags)0,								// VkAccessFlags2KHR				srcAccessMask
				VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,			// VkPipelineStageFlags2KHR			dstStageMask
				VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,				// VkAccessFlags2KHR				dstAccessMask
				VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout					oldLayout
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,			// VkImageLayout					newLayout
				**m_image,										// VkImage							image
				m_subresourceRange								// VkImageSubresourceRange			subresourceRange
			);
			VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, DE_NULL, &imageMemoryBarrier2);
			synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &dependencyInfo);

			vk.cmdCopyBufferToImage(cmdBuffer, **m_hostBuffer, **m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &copyRegion);
		}
		// Image -> Resource
		{
			const VkImageMemoryBarrier2KHR imageMemoryBarrier2 = makeImageMemoryBarrier2(
				VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,			// VkPipelineStageFlags2KHR			srcStageMask
				VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,				// VkAccessFlags2KHR				srcAccessMask
				VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,			// VkPipelineStageFlags2KHR			dstStageMask
				VK_ACCESS_2_TRANSFER_READ_BIT_KHR,				// VkAccessFlags2KHR				dstAccessMask
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,			// VkImageLayout					oldLayout
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,			// VkImageLayout					newLayout
				**m_image,										// VkImage							image
				m_subresourceRange								// VkImageSubresourceRange			subresourceRange
			);
			VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, DE_NULL, &imageMemoryBarrier2);
			synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &dependencyInfo);

			vk.cmdCopyImageToBuffer(cmdBuffer, **m_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_resource.getBuffer().handle, 1u, &copyRegion);
		}
	}

	SyncInfo getInSyncInfo (void) const
	{
		return emptySyncInfo;
	}

	SyncInfo getOutSyncInfo (void) const
	{
		const SyncInfo syncInfo =
		{
			VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,		// VkPipelineStageFlags		stageMask;
			VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,			// VkAccessFlags			accessMask;
			VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout			imageLayout;
		};
		return syncInfo;
	}

	Data getData (void) const
	{
		return getHostBufferData(m_context, *m_hostBuffer, m_resource.getBuffer().size);
	}

	void setData (const Data& data)
	{
		setHostBufferData(m_context, *m_hostBuffer, data);
	}

private:
	OperationContext&				m_context;
	Resource&						m_resource;
	const VkImageSubresourceRange	m_subresourceRange;
	const VkImageSubresourceLayers	m_subresourceLayers;
	de::MovePtr<Buffer>				m_hostBuffer;
	de::MovePtr<Image>				m_image;
	VkExtent3D						m_imageExtent;
};

class ReadImplementation : public Operation
{
public:
	ReadImplementation (OperationContext& context, Resource& resource)
		: m_context		(context)
		, m_resource	(resource)
		, m_bufferSize	(getPixelBufferSize(m_resource.getImage().format, m_resource.getImage().extent))
	{
		DE_ASSERT(m_resource.getType() == RESOURCE_TYPE_IMAGE);

		const DeviceInterface&	vk			= m_context.getDeviceInterface();
		const VkDevice			device		= m_context.getDevice();
		Allocator&				allocator	= m_context.getAllocator();

		m_hostBuffer = de::MovePtr<Buffer>(new Buffer(
			vk, device, allocator, makeBufferCreateInfo(m_bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT), MemoryRequirement::HostVisible));

		const Allocation& alloc = m_hostBuffer->getAllocation();
		deMemset(alloc.getHostPtr(), 0, static_cast<size_t>(m_bufferSize));
		flushAlloc(vk, device, alloc);
	}

	void recordCommands (const VkCommandBuffer cmdBuffer)
	{
		const DeviceInterface&		vk						= m_context.getDeviceInterface();
		const VkBufferImageCopy		copyRegion				= makeBufferImageCopy(m_resource.getImage().extent, m_resource.getImage().subresourceLayers);
		SynchronizationWrapperPtr	synchronizationWrapper	= getSynchronizationWrapper(m_context.getSynchronizationType(), vk, DE_FALSE);

		vk.cmdCopyImageToBuffer(cmdBuffer, m_resource.getImage().handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, **m_hostBuffer, 1u, &copyRegion);

		// Insert a barrier so data written by the transfer is available to the host
		{
			const VkBufferMemoryBarrier2KHR bufferMemoryBarrier2 = makeBufferMemoryBarrier2(
				VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,			// VkPipelineStageFlags2KHR			srcStageMask
				VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,				// VkAccessFlags2KHR				srcAccessMask
				VK_PIPELINE_STAGE_2_HOST_BIT_KHR,				// VkPipelineStageFlags2KHR			dstStageMask
				VK_ACCESS_2_HOST_READ_BIT_KHR,					// VkAccessFlags2KHR				dstAccessMask
				**m_hostBuffer,									// VkBuffer							buffer
				0u,												// VkDeviceSize						offset
				VK_WHOLE_SIZE									// VkDeviceSize						size
			);
			VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, &bufferMemoryBarrier2);
			synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &dependencyInfo);
		}
	}

	SyncInfo getInSyncInfo (void) const
	{
		const SyncInfo syncInfo =
		{
			VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,	// VkPipelineStageFlags		stageMask;
			VK_ACCESS_2_TRANSFER_READ_BIT_KHR,		// VkAccessFlags			accessMask;
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,	// VkImageLayout			imageLayout;
		};
		return syncInfo;
	}

	SyncInfo getOutSyncInfo (void) const
	{
		return emptySyncInfo;
	}

	Data getData (void) const
	{
		return getHostBufferData(m_context, *m_hostBuffer, m_bufferSize);
	}

	void setData (const Data&)
	{
		DE_ASSERT(0);
	}

private:
	OperationContext&		m_context;
	Resource&				m_resource;
	de::MovePtr<Buffer>		m_hostBuffer;
	const VkDeviceSize		m_bufferSize;
};

class CopyImplementation : public Operation
{
public:
	CopyImplementation (OperationContext& context, Resource& inResource, Resource& outResource)
		: m_context				(context)
		, m_inResource			(inResource)
		, m_outResource			(outResource)
		, m_subresourceRange	(makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u))
		, m_subresourceLayers	(makeImageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u))
	{
		DE_ASSERT(m_inResource.getType() == RESOURCE_TYPE_IMAGE);
		DE_ASSERT(m_outResource.getType() == RESOURCE_TYPE_BUFFER);
	}

	void recordCommands (const VkCommandBuffer cmdBuffer)
	{
		const DeviceInterface&		vk						= m_context.getDeviceInterface();
		const VkBufferImageCopy		copyRegion				= makeBufferImageCopy(m_inResource.getImage().extent, m_subresourceLayers);
		SynchronizationWrapperPtr	synchronizationWrapper	= getSynchronizationWrapper(m_context.getSynchronizationType(), vk, DE_FALSE);

		{
			const VkImageMemoryBarrier2KHR imageMemoryBarrier2 = makeImageMemoryBarrier2(
				VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR,		// VkPipelineStageFlags2KHR			srcStageMask
				(VkAccessFlags)0,								// VkAccessFlags2KHR				srcAccessMask
				VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,			// VkPipelineStageFlags2KHR			dstStageMask
				VK_ACCESS_2_TRANSFER_READ_BIT_KHR,				// VkAccessFlags2KHR				dstAccessMask
				VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout					oldLayout
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,			// VkImageLayout					newLayout
				m_inResource.getImage().handle,					// VkImage							image
				m_inResource.getImage().subresourceRange		// VkImageSubresourceRange			subresourceRange
			);
			const VkBufferMemoryBarrier2KHR bufferMemoryBarrier2 = makeBufferMemoryBarrier2(
				VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR,		// VkPipelineStageFlags2KHR			srcStageMask
				(VkAccessFlags)0,								// VkAccessFlags2KHR				srcAccessMask
				VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,			// VkPipelineStageFlags2KHR			dstStageMask
				VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,				// VkAccessFlags2KHR				dstAccessMask
				m_outResource.getBuffer().handle,				// VkBuffer							buffer
				0u,												// VkDeviceSize						offset
				m_outResource.getBuffer().size					// VkDeviceSize						size
			);
			VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, &bufferMemoryBarrier2, &imageMemoryBarrier2);
			synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &dependencyInfo);
		}

		vk.cmdCopyImageToBuffer(cmdBuffer, m_inResource.getImage().handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_outResource.getBuffer().handle, 1u, &copyRegion);
	}

	SyncInfo getInSyncInfo (void) const
	{
		const SyncInfo syncInfo =
		{
			VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,	// VkPipelineStageFlags		stageMask;
			VK_ACCESS_2_TRANSFER_READ_BIT_KHR,		// VkAccessFlags			accessMask;
			VK_IMAGE_LAYOUT_UNDEFINED,				// VkImageLayout			imageLayout;
		};
		return syncInfo;
	}

	SyncInfo getOutSyncInfo (void) const
	{
		const SyncInfo syncInfo =
		{
			VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,	// VkPipelineStageFlags		stageMask;
			VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,		// VkAccessFlags			accessMask;
			VK_IMAGE_LAYOUT_UNDEFINED,				// VkImageLayout			imageLayout;
		};
		return syncInfo;
	}

	Data getData (void) const
	{
		Data data = { 0, DE_NULL };
		return data;
	}

	void setData (const Data&)
	{
		DE_ASSERT(0);
	}

private:
	OperationContext&				m_context;
	Resource&						m_inResource;
	Resource&						m_outResource;
	const VkImageSubresourceRange	m_subresourceRange;
	const VkImageSubresourceLayers	m_subresourceLayers;
};

class Support : public OperationSupport
{
public:
	Support (const ResourceDescription& resourceDesc, const AccessMode mode)
		: m_mode				(mode)
		, m_requiredQueueFlags	(resourceDesc.type == RESOURCE_TYPE_IMAGE && isDepthStencilFormat(resourceDesc.imageFormat) ? VK_QUEUE_GRAPHICS_BIT : VK_QUEUE_TRANSFER_BIT)
	{
		DE_ASSERT(m_mode == ACCESS_MODE_READ || m_mode == ACCESS_MODE_WRITE);
		DE_ASSERT(m_mode == ACCESS_MODE_READ || resourceDesc.type != RESOURCE_TYPE_IMAGE);
		DE_ASSERT(m_mode == ACCESS_MODE_WRITE || resourceDesc.type != RESOURCE_TYPE_BUFFER);
	}

	deUint32 getInResourceUsageFlags (void) const
	{
		return m_mode == ACCESS_MODE_READ ? VK_BUFFER_USAGE_TRANSFER_SRC_BIT : 0;
	}

	deUint32 getOutResourceUsageFlags (void) const
	{
		return m_mode == ACCESS_MODE_WRITE ? VK_BUFFER_USAGE_TRANSFER_DST_BIT : 0;
	}

	VkQueueFlags getQueueFlags (const OperationContext& context) const
	{
		DE_UNREF(context);
		return m_requiredQueueFlags;
	}

	de::MovePtr<Operation> build (OperationContext& context, Resource& resource) const
	{
		if (m_mode == ACCESS_MODE_READ)
			return de::MovePtr<Operation>(new ReadImplementation(context, resource));
		else
			return de::MovePtr<Operation>(new WriteImplementation(context, resource));
	}

	de::MovePtr<Operation> build (OperationContext&, Resource&, Resource&) const
	{
		DE_ASSERT(0);
		return de::MovePtr<Operation>();
	}

private:
	const AccessMode			m_mode;
	const VkQueueFlags			m_requiredQueueFlags;
};

} // CopyImageToBuffer ns

namespace ClearImage
{

enum ClearMode
{
	CLEAR_MODE_COLOR,
	CLEAR_MODE_DEPTH_STENCIL,
};

class Implementation : public Operation
{
public:
	Implementation (OperationContext& context, Resource& resource, const ClearMode mode)
		: m_context		(context)
		, m_resource	(resource)
		, m_clearValue	(makeClearValue(m_resource.getImage().format))
		, m_mode		(mode)
	{
		const VkDeviceSize			size		= getPixelBufferSize(m_resource.getImage().format, m_resource.getImage().extent);
		const VkExtent3D&			extent		= m_resource.getImage().extent;
		const VkFormat				format		= m_resource.getImage().format;
		const tcu::TextureFormat	texFormat	= mapVkFormat(format);

		m_data.resize(static_cast<std::size_t>(size));
		tcu::PixelBufferAccess imagePixels(texFormat, extent.width, extent.height, extent.depth, &m_data[0]);
		clearPixelBuffer(imagePixels, m_clearValue);
	}

	void recordCommands (const VkCommandBuffer cmdBuffer)
	{
		const DeviceInterface&		vk						= m_context.getDeviceInterface();
		SynchronizationWrapperPtr	synchronizationWrapper	= getSynchronizationWrapper(m_context.getSynchronizationType(), vk, DE_FALSE);

		VkPipelineStageFlags2KHR dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR;
		if (m_context.getSynchronizationType() == SynchronizationType::SYNCHRONIZATION2)
			dstStageMask = VK_PIPELINE_STAGE_2_CLEAR_BIT_KHR;

		const VkImageMemoryBarrier2KHR imageMemoryBarrier2 = makeImageMemoryBarrier2(
			VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR,		// VkPipelineStageFlags2KHR			srcStageMask
			(VkAccessFlags)0,								// VkAccessFlags2KHR				srcAccessMask
			dstStageMask,									// VkPipelineStageFlags2KHR			dstStageMask
			VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,				// VkAccessFlags2KHR				dstAccessMask
			VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout					oldLayout
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,			// VkImageLayout					newLayout
			m_resource.getImage().handle,					// VkImage							image
			m_resource.getImage().subresourceRange			// VkImageSubresourceRange			subresourceRange
		);
		VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, DE_NULL, &imageMemoryBarrier2);
		synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &dependencyInfo);

		if (m_mode == CLEAR_MODE_COLOR)
			vk.cmdClearColorImage(cmdBuffer, m_resource.getImage().handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &m_clearValue.color, 1u, &m_resource.getImage().subresourceRange);
		else
			vk.cmdClearDepthStencilImage(cmdBuffer, m_resource.getImage().handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &m_clearValue.depthStencil, 1u, &m_resource.getImage().subresourceRange);
	}

	SyncInfo getInSyncInfo (void) const
	{
		return emptySyncInfo;
	}

	SyncInfo getOutSyncInfo (void) const
	{
		VkPipelineStageFlags2KHR stageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR;
		if (m_context.getSynchronizationType() == SynchronizationType::SYNCHRONIZATION2)
			stageMask = VK_PIPELINE_STAGE_2_CLEAR_BIT_KHR;

		return
		{
			stageMask,								// VkPipelineStageFlags		stageMask;
			VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,		// VkAccessFlags			accessMask;
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,	// VkImageLayout			imageLayout;
		};
	}

	Data getData (void) const
	{
		const Data data =
		{
			m_data.size(),		// std::size_t		size;
			&m_data[0],			// const deUint8*	data;
		};
		return data;
	}

	void setData (const Data&)
	{
		DE_ASSERT(0);
	}

private:
	OperationContext&		m_context;
	Resource&				m_resource;
	std::vector<deUint8>	m_data;
	const VkClearValue		m_clearValue;
	const ClearMode			m_mode;
};

class Support : public OperationSupport
{
public:
	Support (const ResourceDescription& resourceDesc, const ClearMode mode)
		: m_resourceDesc	(resourceDesc)
		, m_mode			(mode)
	{
		DE_ASSERT(m_mode == CLEAR_MODE_COLOR || m_mode == CLEAR_MODE_DEPTH_STENCIL);
		DE_ASSERT(m_resourceDesc.type == RESOURCE_TYPE_IMAGE);
		DE_ASSERT(m_resourceDesc.imageAspect == VK_IMAGE_ASPECT_COLOR_BIT || (m_mode != CLEAR_MODE_COLOR));
		DE_ASSERT((m_resourceDesc.imageAspect & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) || (m_mode != CLEAR_MODE_DEPTH_STENCIL));
	}

	deUint32 getInResourceUsageFlags (void) const
	{
		return 0;
	}

	deUint32 getOutResourceUsageFlags (void) const
	{
		return VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	}

	VkQueueFlags getQueueFlags (const OperationContext& context) const
	{
		DE_UNREF(context);
		if (m_mode == CLEAR_MODE_COLOR)
			return VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
		else
			return VK_QUEUE_GRAPHICS_BIT;
	}

	de::MovePtr<Operation> build (OperationContext& context, Resource& resource) const
	{
		return de::MovePtr<Operation>(new Implementation(context, resource, m_mode));
	}

	de::MovePtr<Operation> build (OperationContext&, Resource&, Resource&) const
	{
		DE_ASSERT(0);
		return de::MovePtr<Operation>();
	}

private:
	const ResourceDescription	m_resourceDesc;
	const ClearMode				m_mode;
};

} // ClearImage ns

namespace Draw
{

enum DrawCall
{
	DRAW_CALL_DRAW,
	DRAW_CALL_DRAW_INDEXED,
	DRAW_CALL_DRAW_INDIRECT,
	DRAW_CALL_DRAW_INDEXED_INDIRECT,
};

//! A write operation that is a result of drawing to an image.
//! \todo Add support for depth/stencil too?
class Implementation : public Operation
{
public:
	Implementation (OperationContext& context, Resource& resource, const DrawCall drawCall)
		: m_context		(context)
		, m_resource	(resource)
		, m_drawCall	(drawCall)
		, m_vertices	(context)
	{
		const DeviceInterface&		vk				= context.getDeviceInterface();
		const VkDevice				device			= context.getDevice();
		Allocator&					allocator		= context.getAllocator();

		// Indirect buffer

		if (m_drawCall == DRAW_CALL_DRAW_INDIRECT)
		{
			m_indirectBuffer = de::MovePtr<Buffer>(new Buffer(vk, device, allocator,
				makeBufferCreateInfo(sizeof(VkDrawIndirectCommand), VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT), MemoryRequirement::HostVisible));

			const Allocation&				alloc				= m_indirectBuffer->getAllocation();
			VkDrawIndirectCommand* const	pIndirectCommand	= static_cast<VkDrawIndirectCommand*>(alloc.getHostPtr());

			pIndirectCommand->vertexCount	= m_vertices.getNumVertices();
			pIndirectCommand->instanceCount	= 1u;
			pIndirectCommand->firstVertex	= 0u;
			pIndirectCommand->firstInstance	= 0u;

			flushAlloc(vk, device, alloc);
		}
		else if (m_drawCall == DRAW_CALL_DRAW_INDEXED_INDIRECT)
		{
			m_indirectBuffer = de::MovePtr<Buffer>(new Buffer(vk, device, allocator,
				makeBufferCreateInfo(sizeof(VkDrawIndexedIndirectCommand), VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT), MemoryRequirement::HostVisible));

			const Allocation&					alloc				= m_indirectBuffer->getAllocation();
			VkDrawIndexedIndirectCommand* const	pIndirectCommand	= static_cast<VkDrawIndexedIndirectCommand*>(alloc.getHostPtr());

			pIndirectCommand->indexCount	= m_vertices.getNumIndices();
			pIndirectCommand->instanceCount	= 1u;
			pIndirectCommand->firstIndex	= 0u;
			pIndirectCommand->vertexOffset	= 0u;
			pIndirectCommand->firstInstance	= 0u;

			flushAlloc(vk, device, alloc);
		}

		// Resource image is the color attachment

		m_colorFormat			= m_resource.getImage().format;
		m_colorSubresourceRange	= m_resource.getImage().subresourceRange;
		m_colorImage			= m_resource.getImage().handle;
		m_attachmentExtent		= m_resource.getImage().extent;

		// Pipeline

		m_colorAttachmentView	= makeImageView		(vk, device, m_colorImage, VK_IMAGE_VIEW_TYPE_2D, m_colorFormat, m_colorSubresourceRange);
		m_renderPass			= makeRenderPass	(vk, device, m_colorFormat);
		m_framebuffer			= makeFramebuffer	(vk, device, *m_renderPass, *m_colorAttachmentView, m_attachmentExtent.width, m_attachmentExtent.height);
		m_pipelineLayout		= makePipelineLayout(vk, device);

		GraphicsPipelineBuilder pipelineBuilder;
		pipelineBuilder
			.setRenderSize					(tcu::IVec2(m_attachmentExtent.width, m_attachmentExtent.height))
			.setVertexInputSingleAttribute	(m_vertices.getVertexFormat(), m_vertices.getVertexStride())
			.setShader						(vk, device, VK_SHADER_STAGE_VERTEX_BIT,	context.getBinaryCollection().get("draw_vert"), DE_NULL)
			.setShader						(vk, device, VK_SHADER_STAGE_FRAGMENT_BIT,	context.getBinaryCollection().get("draw_frag"), DE_NULL);

		m_pipeline = pipelineBuilder.build(vk, device, *m_pipelineLayout, *m_renderPass, context.getPipelineCacheData(), context.getResourceInterface() );

		// Set expected draw values

		m_expectedData.resize(static_cast<size_t>(getPixelBufferSize(m_resource.getImage().format, m_resource.getImage().extent)));
		tcu::PixelBufferAccess imagePixels(mapVkFormat(m_colorFormat), m_attachmentExtent.width, m_attachmentExtent.height, m_attachmentExtent.depth, &m_expectedData[0]);
		clearPixelBuffer(imagePixels, makeClearValue(m_colorFormat));
	}

	void recordCommands (const VkCommandBuffer cmdBuffer)
	{
		const DeviceInterface&		vk						= m_context.getDeviceInterface();
		SynchronizationWrapperPtr	synchronizationWrapper	= getSynchronizationWrapper(m_context.getSynchronizationType(), vk, DE_FALSE);

		// Change color attachment image layout
		{
			const VkImageMemoryBarrier2KHR imageMemoryBarrier2 = makeImageMemoryBarrier2(
				VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR,				// VkPipelineStageFlags2KHR			srcStageMask
				(VkAccessFlags)0,										// VkAccessFlags2KHR				srcAccessMask
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,	// VkPipelineStageFlags2KHR			dstStageMask
				VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR,				// VkAccessFlags2KHR				dstAccessMask
				VK_IMAGE_LAYOUT_UNDEFINED,								// VkImageLayout					oldLayout
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,				// VkImageLayout					newLayout
				m_colorImage,											// VkImage							image
				m_colorSubresourceRange									// VkImageSubresourceRange			subresourceRange
			);
			VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, DE_NULL, &imageMemoryBarrier2);
			synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &dependencyInfo);
		}

		{
			const VkRect2D	renderArea	= makeRect2D(m_attachmentExtent);
			const tcu::Vec4	clearColor	= tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);

			beginRenderPass(vk, cmdBuffer, *m_renderPass, *m_framebuffer, renderArea, clearColor);
		}

		vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
		{
			const VkDeviceSize	vertexBufferOffset	= 0ull;
			const VkBuffer		vertexBuffer		= m_vertices.getVertexBuffer();
			vk.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer, &vertexBufferOffset);
		}

		if (m_drawCall == DRAW_CALL_DRAW_INDEXED || m_drawCall == DRAW_CALL_DRAW_INDEXED_INDIRECT)
			vk.cmdBindIndexBuffer(cmdBuffer, m_vertices.getIndexBuffer(), 0u, m_vertices.getIndexType());

		switch (m_drawCall)
		{
			case DRAW_CALL_DRAW:
				vk.cmdDraw(cmdBuffer, m_vertices.getNumVertices(), 1u, 0u, 0u);
				break;

			case DRAW_CALL_DRAW_INDEXED:
				vk.cmdDrawIndexed(cmdBuffer, m_vertices.getNumIndices(), 1u, 0u, 0, 0u);
				break;

			case DRAW_CALL_DRAW_INDIRECT:
				vk.cmdDrawIndirect(cmdBuffer, **m_indirectBuffer, 0u, 1u, 0u);
				break;

			case DRAW_CALL_DRAW_INDEXED_INDIRECT:
				vk.cmdDrawIndexedIndirect(cmdBuffer, **m_indirectBuffer, 0u, 1u, 0u);
				break;
		}

		endRenderPass(vk, cmdBuffer);
	}

	SyncInfo getInSyncInfo (void) const
	{
		return emptySyncInfo;
	}

	SyncInfo getOutSyncInfo (void) const
	{
		const SyncInfo syncInfo =
		{
			VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,	// VkPipelineStageFlags		stageMask;
			VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR,				// VkAccessFlags			accessMask;
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,				// VkImageLayout			imageLayout;
		};
		return syncInfo;
	}

	Data getData (void) const
	{
		const Data data =
		{
			m_expectedData.size(),		// std::size_t		size;
			&m_expectedData[0],			// const deUint8*	data;
		};
		return data;
	}

	void setData (const Data& data)
	{
		DE_ASSERT(m_expectedData.size() == data.size);
		deMemcpy(&m_expectedData[0], data.data, data.size);
	}

private:
	OperationContext&			m_context;
	Resource&					m_resource;
	const DrawCall				m_drawCall;
	const VertexGrid			m_vertices;
	std::vector<deUint8>		m_expectedData;
	de::MovePtr<Buffer>			m_indirectBuffer;
	VkFormat					m_colorFormat;
	VkImage						m_colorImage;
	Move<VkImageView>			m_colorAttachmentView;
	VkImageSubresourceRange		m_colorSubresourceRange;
	VkExtent3D					m_attachmentExtent;
	Move<VkRenderPass>			m_renderPass;
	Move<VkFramebuffer>			m_framebuffer;
	Move<VkPipelineLayout>		m_pipelineLayout;
	Move<VkPipeline>			m_pipeline;
};

template<typename T, std::size_t N>
std::string toString (const T (&values)[N])
{
	std::ostringstream str;
	for (std::size_t i = 0; i < N; ++i)
		str << (i != 0 ? ", " : "") << values[i];
	return str.str();
}

class Support : public OperationSupport
{
public:
	Support (const ResourceDescription& resourceDesc, const DrawCall drawCall)
		: m_resourceDesc	(resourceDesc)
		, m_drawCall		(drawCall)
	{
		DE_ASSERT(m_resourceDesc.type == RESOURCE_TYPE_IMAGE && m_resourceDesc.imageType == VK_IMAGE_TYPE_2D);
		DE_ASSERT(!isDepthStencilFormat(m_resourceDesc.imageFormat));
	}

	void initPrograms (SourceCollections& programCollection) const
	{
		// Vertex
		{
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_440) << "\n"
				<< "\n"
				<< "layout(location = 0) in vec4 v_in_position;\n"
				<< "\n"
				<< "out " << s_perVertexBlock << ";\n"
				<< "\n"
				<< "void main (void)\n"
				<< "{\n"
				<< "    gl_Position = v_in_position;\n"
				<< "}\n";

			programCollection.glslSources.add("draw_vert") << glu::VertexSource(src.str());
		}

		// Fragment
		{
			const VkClearValue	clearValue		= makeClearValue(m_resourceDesc.imageFormat);
			const bool			isIntegerFormat = isIntFormat(m_resourceDesc.imageFormat) || isUintFormat(m_resourceDesc.imageFormat);
			const std::string	colorType		= (isIntegerFormat ? "uvec4" : "vec4");

			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_440) << "\n"
				<< "\n"
				<< "layout(location = 0) out " << colorType << " o_color;\n"
				<< "\n"
				<< "void main (void)\n"
				<< "{\n"
				<< "    o_color = " << colorType << "(" << (isIntegerFormat ? toString(clearValue.color.uint32) : toString(clearValue.color.float32)) << ");\n"
				<< "}\n";

			programCollection.glslSources.add("draw_frag") << glu::FragmentSource(src.str());
		}
	}

	deUint32 getInResourceUsageFlags (void) const
	{
		return 0;
	}

	deUint32 getOutResourceUsageFlags (void) const
	{
		return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	}

	VkQueueFlags getQueueFlags (const OperationContext& context) const
	{
		DE_UNREF(context);
		return VK_QUEUE_GRAPHICS_BIT;
	}

	de::MovePtr<Operation> build (OperationContext& context, Resource& resource) const
	{
		return de::MovePtr<Operation>(new Implementation(context, resource, m_drawCall));
	}

	de::MovePtr<Operation> build (OperationContext&, Resource&, Resource&) const
	{
		DE_ASSERT(0);
		return de::MovePtr<Operation>();
	}

private:
	const ResourceDescription	m_resourceDesc;
	const DrawCall				m_drawCall;
};

} // Draw ns

namespace ClearAttachments
{

class Implementation : public Operation
{
public:
	Implementation (OperationContext& context, Resource& resource)
		: m_context		(context)
		, m_resource	(resource)
		, m_clearValue	(makeClearValue(m_resource.getImage().format))
	{
		const DeviceInterface&		vk				= context.getDeviceInterface();
		const VkDevice				device			= context.getDevice();

		const VkDeviceSize			size		= getPixelBufferSize(m_resource.getImage().format, m_resource.getImage().extent);
		const VkExtent3D&			extent		= m_resource.getImage().extent;
		const VkFormat				format		= m_resource.getImage().format;
		const tcu::TextureFormat	texFormat	= mapVkFormat(format);
		const SyncInfo				syncInfo	= getOutSyncInfo();

		m_data.resize(static_cast<std::size_t>(size));
		tcu::PixelBufferAccess imagePixels(texFormat, extent.width, extent.height, extent.depth, &m_data[0]);
		clearPixelBuffer(imagePixels, m_clearValue);

		m_attachmentView = makeImageView(vk, device, m_resource.getImage().handle, getImageViewType(m_resource.getImage().imageType), m_resource.getImage().format, m_resource.getImage().subresourceRange);

		switch (m_resource.getImage().subresourceRange.aspectMask)
		{
			case VK_IMAGE_ASPECT_COLOR_BIT:
				m_renderPass = makeRenderPass(vk, device, m_resource.getImage().format, VK_FORMAT_UNDEFINED, VK_ATTACHMENT_LOAD_OP_DONT_CARE, syncInfo.imageLayout);
			break;
			case VK_IMAGE_ASPECT_STENCIL_BIT:
			case VK_IMAGE_ASPECT_DEPTH_BIT:
				m_renderPass = makeRenderPass(vk, device, VK_FORMAT_UNDEFINED, m_resource.getImage().format, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, syncInfo.imageLayout);
			break;
			default:
				DE_ASSERT(0);
			break;
		}

		m_frameBuffer	= makeFramebuffer(vk, device, *m_renderPass, *m_attachmentView, m_resource.getImage().extent.width, m_resource.getImage().extent.height);
	}

	void recordCommands (const VkCommandBuffer cmdBuffer)
	{
		const DeviceInterface&		vk						= m_context.getDeviceInterface();
		if ((m_resource.getImage().subresourceRange.aspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) != 0)
		{
			const VkImageMemoryBarrier imageBarrier =
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,				// sType
				DE_NULL,											// pNext
				0u,													// srcAccessMask
				VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,		// dstAccessMask
				VK_IMAGE_LAYOUT_UNDEFINED,							// oldLayout
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,	// newLayout
				VK_QUEUE_FAMILY_IGNORED,							// srcQueueFamilyIndex
				VK_QUEUE_FAMILY_IGNORED,							// dstQueueFamilyIndex
				m_resource.getImage().handle,						// image
				m_resource.getImage().subresourceRange				// subresourceRange
			};
			vk.cmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, 0u, 0u, DE_NULL, 0u, DE_NULL, 1u, &imageBarrier);
		}
		beginRenderPass(vk, cmdBuffer, *m_renderPass, *m_frameBuffer, makeRect2D(0 ,0, m_resource.getImage().extent.width, m_resource.getImage().extent.height), m_clearValue);

		const VkClearAttachment	clearAttachment	=
		{
			m_resource.getImage().subresourceRange.aspectMask,	// VkImageAspectFlags	aspectMask;
			0,													// deUint32				colorAttachment;
			m_clearValue										// VkClearValue			clearValue;
		};

		const VkRect2D			rect2D			= makeRect2D(m_resource.getImage().extent);

		const VkClearRect		clearRect		=
		{
			rect2D,												// VkRect2D	rect;
			0u,													// deUint32	baseArrayLayer;
			m_resource.getImage().subresourceLayers.layerCount	// deUint32	layerCount;
		};

		vk.cmdClearAttachments(cmdBuffer, 1, &clearAttachment, 1, &clearRect);

		endRenderPass(vk, cmdBuffer);
	}

	SyncInfo getInSyncInfo (void) const
	{
		return emptySyncInfo;
	}

	SyncInfo getOutSyncInfo (void) const
	{
		SyncInfo syncInfo;
		syncInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR;

		switch (m_resource.getImage().subresourceRange.aspectMask)
		{
			case VK_IMAGE_ASPECT_COLOR_BIT:
				syncInfo.accessMask		= VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR;
				syncInfo.imageLayout	= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			break;
			case VK_IMAGE_ASPECT_STENCIL_BIT:
			case VK_IMAGE_ASPECT_DEPTH_BIT:
				syncInfo.accessMask		= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT_KHR;
				syncInfo.imageLayout	= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			break;
			default:
				DE_ASSERT(0);
			break;
		}

		return syncInfo;
	}

	Data getData (void) const
	{
		const Data data =
		{
			m_data.size(),	// std::size_t		size;
			&m_data[0],		// const deUint8*	data;
		};
		return data;
	}

	void setData (const Data&)
	{
		DE_ASSERT(0);
	}

private:
	OperationContext&		m_context;
	Resource&				m_resource;
	std::vector<deUint8>	m_data;
	const VkClearValue		m_clearValue;
	Move<VkImageView>		m_attachmentView;
	Move<VkRenderPass>		m_renderPass;
	Move<VkFramebuffer>		m_frameBuffer;
};

class Support : public OperationSupport
{
public:
	Support (const ResourceDescription& resourceDesc)
		: m_resourceDesc (resourceDesc)
	{
		DE_ASSERT(m_resourceDesc.type == RESOURCE_TYPE_IMAGE);
	}

	deUint32 getInResourceUsageFlags (void) const
	{
		return 0;
	}

	deUint32 getOutResourceUsageFlags (void) const
	{
		switch (m_resourceDesc.imageAspect)
		{
			case VK_IMAGE_ASPECT_COLOR_BIT:
				return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			case VK_IMAGE_ASPECT_STENCIL_BIT:
			case VK_IMAGE_ASPECT_DEPTH_BIT:
				return VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			default:
				DE_ASSERT(0);
		}
		return 0u;
	}

	VkQueueFlags getQueueFlags (const OperationContext& context) const
	{
		DE_UNREF(context);
		return VK_QUEUE_GRAPHICS_BIT;
	}

	de::MovePtr<Operation> build (OperationContext& context, Resource& resource) const
	{
		return de::MovePtr<Operation>(new Implementation(context, resource));
	}

	de::MovePtr<Operation> build (OperationContext&, Resource&, Resource&) const
	{
		DE_ASSERT(0);
		return de::MovePtr<Operation>();
	}

private:
	const ResourceDescription	m_resourceDesc;
};

} // ClearAttachments

namespace IndirectBuffer
{

class GraphicsPipeline : public Pipeline
{
public:
	GraphicsPipeline (OperationContext&				context,
					  const ResourceType			resourceType,
					  const VkBuffer				indirectBuffer,
					  const std::string&			shaderPrefix,
					  const VkDescriptorSetLayout	descriptorSetLayout)
		: m_resourceType	(resourceType)
		, m_indirectBuffer	(indirectBuffer)
		, m_vertices		(context)
	{
		const DeviceInterface&		vk				= context.getDeviceInterface();
		const VkDevice				device			= context.getDevice();
		Allocator&					allocator		= context.getAllocator();

		// Color attachment

		m_colorFormat					= VK_FORMAT_R8G8B8A8_UNORM;
		m_colorImageSubresourceRange	= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
		m_colorImageExtent				= makeExtent3D(16u, 16u, 1u);
		m_colorAttachmentImage			= de::MovePtr<Image>(new Image(vk, device, allocator,
			makeImageCreateInfo(VK_IMAGE_TYPE_2D, m_colorImageExtent, m_colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
								VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL),
			MemoryRequirement::Any));

		// Pipeline

		m_colorAttachmentView	= makeImageView		(vk, device, **m_colorAttachmentImage, VK_IMAGE_VIEW_TYPE_2D, m_colorFormat, m_colorImageSubresourceRange);
		m_renderPass			= makeRenderPass	(vk, device, m_colorFormat);
		m_framebuffer			= makeFramebuffer	(vk, device, *m_renderPass, *m_colorAttachmentView, m_colorImageExtent.width, m_colorImageExtent.height);
		m_pipelineLayout		= makePipelineLayout(vk, device, descriptorSetLayout);

		GraphicsPipelineBuilder pipelineBuilder;
		pipelineBuilder
			.setRenderSize					(tcu::IVec2(m_colorImageExtent.width, m_colorImageExtent.height))
			.setVertexInputSingleAttribute	(m_vertices.getVertexFormat(), m_vertices.getVertexStride())
			.setShader						(vk, device, VK_SHADER_STAGE_VERTEX_BIT,	context.getBinaryCollection().get(shaderPrefix + "vert"), DE_NULL)
			.setShader						(vk, device, VK_SHADER_STAGE_FRAGMENT_BIT,	context.getBinaryCollection().get(shaderPrefix + "frag"), DE_NULL);

		m_pipeline = pipelineBuilder.build(vk, device, *m_pipelineLayout, *m_renderPass, context.getPipelineCacheData(), context.getResourceInterface());
	}

	void recordCommands (OperationContext& context, const VkCommandBuffer cmdBuffer, const VkDescriptorSet descriptorSet)
	{
		const DeviceInterface&		vk						= context.getDeviceInterface();
		SynchronizationWrapperPtr	synchronizationWrapper	= getSynchronizationWrapper(context.getSynchronizationType(), vk, DE_FALSE);

		// Change color attachment image layout
		{
			const VkImageMemoryBarrier2KHR imageMemoryBarrier2 = makeImageMemoryBarrier2(
				VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR,				// VkPipelineStageFlags2KHR			srcStageMask
				(VkAccessFlags)0,										// VkAccessFlags2KHR				srcAccessMask
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,	// VkPipelineStageFlags2KHR			dstStageMask
				VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR,				// VkAccessFlags2KHR				dstAccessMask
				VK_IMAGE_LAYOUT_UNDEFINED,								// VkImageLayout					oldLayout
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,				// VkImageLayout					newLayout
				**m_colorAttachmentImage,								// VkImage							image
				m_colorImageSubresourceRange							// VkImageSubresourceRange			subresourceRange
			);
			VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, DE_NULL, &imageMemoryBarrier2);
			synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &dependencyInfo);
		}

		{
			const VkRect2D	renderArea	= makeRect2D(m_colorImageExtent);
			const tcu::Vec4	clearColor	= tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);

			beginRenderPass(vk, cmdBuffer, *m_renderPass, *m_framebuffer, renderArea, clearColor);
		}

		vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
		vk.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayout, 0u, 1u, &descriptorSet, 0u, DE_NULL);
		{
			const VkDeviceSize	vertexBufferOffset	= 0ull;
			const VkBuffer		vertexBuffer		= m_vertices.getVertexBuffer();
			vk.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &vertexBuffer, &vertexBufferOffset);
		}

		switch (m_resourceType)
		{
			case RESOURCE_TYPE_INDIRECT_BUFFER_DRAW:
				vk.cmdDrawIndirect(cmdBuffer, m_indirectBuffer, 0u, 1u, 0u);
				break;

			case RESOURCE_TYPE_INDIRECT_BUFFER_DRAW_INDEXED:
				vk.cmdBindIndexBuffer(cmdBuffer, m_vertices.getIndexBuffer(), 0u, m_vertices.getIndexType());
				vk.cmdDrawIndexedIndirect(cmdBuffer, m_indirectBuffer, 0u, 1u, 0u);
				break;

			default:
				DE_ASSERT(0);
				break;
		}
		endRenderPass(vk, cmdBuffer);
	}

private:
	const ResourceType			m_resourceType;
	const VkBuffer				m_indirectBuffer;
	const VertexGrid			m_vertices;
	VkFormat					m_colorFormat;
	de::MovePtr<Image>			m_colorAttachmentImage;
	Move<VkImageView>			m_colorAttachmentView;
	VkExtent3D					m_colorImageExtent;
	VkImageSubresourceRange		m_colorImageSubresourceRange;
	Move<VkRenderPass>			m_renderPass;
	Move<VkFramebuffer>			m_framebuffer;
	Move<VkPipelineLayout>		m_pipelineLayout;
	Move<VkPipeline>			m_pipeline;
};

class ComputePipeline : public Pipeline
{
public:
	ComputePipeline (OperationContext&				context,
					 const VkBuffer					indirectBuffer,
					 const std::string&				shaderPrefix,
					 const VkDescriptorSetLayout	descriptorSetLayout)
		: m_indirectBuffer	(indirectBuffer)
	{
		const DeviceInterface&	vk		= context.getDeviceInterface();
		const VkDevice			device	= context.getDevice();

		const Unique<VkShaderModule> shaderModule(createShaderModule(vk, device, context.getBinaryCollection().get(shaderPrefix + "comp"), (VkShaderModuleCreateFlags)0));

		m_pipelineLayout = makePipelineLayout(vk, device, descriptorSetLayout);
		m_pipeline		 = makeComputePipeline(vk, device, *m_pipelineLayout, *shaderModule, DE_NULL, context.getPipelineCacheData(), context.getResourceInterface());
	}

	void recordCommands (OperationContext& context, const VkCommandBuffer cmdBuffer, const VkDescriptorSet descriptorSet)
	{
		const DeviceInterface&	vk	= context.getDeviceInterface();

		vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *m_pipeline);
		vk.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *m_pipelineLayout, 0u, 1u, &descriptorSet, 0u, DE_NULL);
		vk.cmdDispatchIndirect(cmdBuffer, m_indirectBuffer, 0u);
	}

private:
	const VkBuffer				m_indirectBuffer;
	Move<VkPipelineLayout>		m_pipelineLayout;
	Move<VkPipeline>			m_pipeline;
};

//! Read indirect buffer by executing an indirect draw or dispatch command.
class ReadImplementation : public Operation
{
public:
	ReadImplementation (OperationContext& context, Resource& resource)
		: m_context				(context)
		, m_resource			(resource)
		, m_stage				(resource.getType() == RESOURCE_TYPE_INDIRECT_BUFFER_DISPATCH ? VK_SHADER_STAGE_COMPUTE_BIT : VK_SHADER_STAGE_VERTEX_BIT)
		, m_pipelineStage		(pipelineStageFlagsFromShaderStageFlagBits(m_stage))
		, m_hostBufferSizeBytes	(sizeof(deUint32))
	{
		requireFeaturesForSSBOAccess (m_context, m_stage);

		const DeviceInterface&	vk			= m_context.getDeviceInterface();
		const VkDevice			device		= m_context.getDevice();
		Allocator&				allocator	= m_context.getAllocator();

		m_hostBuffer = de::MovePtr<Buffer>(new Buffer(
			vk, device, allocator, makeBufferCreateInfo(m_hostBufferSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), MemoryRequirement::HostVisible));

		// Init host buffer data
		{
			const Allocation& alloc = m_hostBuffer->getAllocation();
			deMemset(alloc.getHostPtr(), 0, static_cast<size_t>(m_hostBufferSizeBytes));
			flushAlloc(vk, device, alloc);
		}

		// Prepare descriptors
		{
			m_descriptorSetLayout = DescriptorSetLayoutBuilder()
				.addSingleBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, m_stage)
				.build(vk, device);

			m_descriptorPool = DescriptorPoolBuilder()
				.addType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
				.build(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

			m_descriptorSet = makeDescriptorSet(vk, device, *m_descriptorPool, *m_descriptorSetLayout);

			const VkDescriptorBufferInfo  hostBufferInfo = makeDescriptorBufferInfo(**m_hostBuffer, 0u, m_hostBufferSizeBytes);

			DescriptorSetUpdateBuilder()
				.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &hostBufferInfo)
				.update(vk, device);
		}

		// Create pipeline
		m_pipeline = (m_resource.getType() == RESOURCE_TYPE_INDIRECT_BUFFER_DISPATCH
			? de::MovePtr<Pipeline>(new ComputePipeline(context, m_resource.getBuffer().handle, "read_ib_", *m_descriptorSetLayout))
			: de::MovePtr<Pipeline>(new GraphicsPipeline(context, m_resource.getType(), m_resource.getBuffer().handle, "read_ib_", *m_descriptorSetLayout)));
	}

	void recordCommands (const VkCommandBuffer cmdBuffer)
	{
		const DeviceInterface&		vk						= m_context.getDeviceInterface();
		SynchronizationWrapperPtr	synchronizationWrapper	= getSynchronizationWrapper(m_context.getSynchronizationType(), vk, DE_FALSE);

		m_pipeline->recordCommands(m_context, cmdBuffer, *m_descriptorSet);

		// Insert a barrier so data written by the shader is available to the host
		const VkBufferMemoryBarrier2KHR bufferMemoryBarrier2 = makeBufferMemoryBarrier2(
			m_pipelineStage,							// VkPipelineStageFlags2KHR			srcStageMask
			VK_ACCESS_2_SHADER_WRITE_BIT_KHR,			// VkAccessFlags2KHR				srcAccessMask
			VK_PIPELINE_STAGE_2_HOST_BIT_KHR,			// VkPipelineStageFlags2KHR			dstStageMask
			VK_ACCESS_2_HOST_READ_BIT_KHR,				// VkAccessFlags2KHR				dstAccessMask
			**m_hostBuffer,								// VkBuffer							buffer
			0u,											// VkDeviceSize						offset
			m_hostBufferSizeBytes						// VkDeviceSize						size
		);
		VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, &bufferMemoryBarrier2);
		synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &dependencyInfo);
	}

	SyncInfo getInSyncInfo (void) const
	{
		const SyncInfo syncInfo =
		{
			VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT_KHR,	// VkPipelineStageFlags		stageMask;
			VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT_KHR,	// VkAccessFlags			accessMask;
			VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout			imageLayout;
		};
		return syncInfo;
	}

	SyncInfo getOutSyncInfo (void) const
	{
		return emptySyncInfo;
	}

	Data getData (void) const
	{
		return getHostBufferData(m_context, *m_hostBuffer, m_hostBufferSizeBytes);
	}

	void setData (const Data&)
	{
		DE_ASSERT(0);
	}

private:
	OperationContext&			m_context;
	Resource&					m_resource;
	const VkShaderStageFlagBits	m_stage;
	const VkPipelineStageFlags	m_pipelineStage;
	const VkDeviceSize			m_hostBufferSizeBytes;
	de::MovePtr<Buffer>			m_hostBuffer;
	Move<VkDescriptorPool>		m_descriptorPool;
	Move<VkDescriptorSetLayout>	m_descriptorSetLayout;
	Move<VkDescriptorSet>		m_descriptorSet;
	de::MovePtr<Pipeline>		m_pipeline;
};

//! Prepare indirect buffer for a draw/dispatch call.
class WriteImplementation : public Operation
{
public:
	WriteImplementation (OperationContext& context, Resource& resource)
		: m_context			(context)
		, m_resource		(resource)
	{
		switch (m_resource.getType())
		{
			case RESOURCE_TYPE_INDIRECT_BUFFER_DRAW:
			{
				m_drawIndirect.vertexCount		= 6u;
				m_drawIndirect.instanceCount	= 1u;
				m_drawIndirect.firstVertex		= 0u;
				m_drawIndirect.firstInstance	= 0u;

				m_indirectData					= reinterpret_cast<deUint32*>(&m_drawIndirect);
				m_expectedValue					= 6u;
			}
			break;

			case RESOURCE_TYPE_INDIRECT_BUFFER_DRAW_INDEXED:
			{
				m_drawIndexedIndirect.indexCount	= 6u;
				m_drawIndexedIndirect.instanceCount	= 1u;
				m_drawIndexedIndirect.firstIndex	= 0u;
				m_drawIndexedIndirect.vertexOffset	= 0u;
				m_drawIndexedIndirect.firstInstance	= 0u;

				m_indirectData						= reinterpret_cast<deUint32*>(&m_drawIndexedIndirect);
				m_expectedValue						= 6u;
			}
			break;

			case RESOURCE_TYPE_INDIRECT_BUFFER_DISPATCH:
			{
				m_dispatchIndirect.x	= 7u;
				m_dispatchIndirect.y	= 2u;
				m_dispatchIndirect.z	= 1u;

				m_indirectData			= reinterpret_cast<deUint32*>(&m_dispatchIndirect);
				m_expectedValue			= 14u;
			}
			break;

			default:
				DE_ASSERT(0);
				break;
		}
	}

	void recordCommands (const VkCommandBuffer cmdBuffer)
	{
		const DeviceInterface&	vk	= m_context.getDeviceInterface();

		vk.cmdUpdateBuffer(cmdBuffer, m_resource.getBuffer().handle, m_resource.getBuffer().offset, m_resource.getBuffer().size, m_indirectData);
	}

	SyncInfo getInSyncInfo (void) const
	{
		return emptySyncInfo;
	}

	SyncInfo getOutSyncInfo (void) const
	{
		const SyncInfo syncInfo =
		{
			VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,	// VkPipelineStageFlags		stageMask;
			VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,		// VkAccessFlags			accessMask;
			VK_IMAGE_LAYOUT_UNDEFINED,				// VkImageLayout			imageLayout;
		};
		return syncInfo;
	}

	Data getData (void) const
	{
		const Data data =
		{
			sizeof(deUint32),									// std::size_t		size;
			reinterpret_cast<const deUint8*>(&m_expectedValue),	// const deUint8*	data;
		};
		return data;
	}

	void setData (const Data&)
	{
		DE_ASSERT(0);
	}

private:
	OperationContext&				m_context;
	Resource&						m_resource;
	VkDrawIndirectCommand			m_drawIndirect;
	VkDrawIndexedIndirectCommand	m_drawIndexedIndirect;
	VkDispatchIndirectCommand		m_dispatchIndirect;
	deUint32*						m_indirectData;
	deUint32						m_expectedValue;	//! Side-effect value expected to be computed by a read (draw/dispatch) command.
};

class ReadSupport : public OperationSupport
{
public:
	ReadSupport (const ResourceDescription& resourceDesc)
		: m_resourceDesc	(resourceDesc)
	{
		DE_ASSERT(isIndirectBuffer(m_resourceDesc.type));
	}

	void initPrograms (SourceCollections& programCollection) const
	{
		std::ostringstream decl;
		decl << "layout(set = 0, binding = 0, std140) coherent buffer Data {\n"
			 << "    uint value;\n"
			 << "} sb_out;\n";

		std::ostringstream main;
		main << "    atomicAdd(sb_out.value, 1u);\n";

		// Vertex
		{
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_440) << "\n"
				<< "\n"
				<< "layout(location = 0) in vec4 v_in_position;\n"
				<< "\n"
				<< "out " << s_perVertexBlock << ";\n"
				<< "\n"
				<< decl.str()
				<< "\n"
				<< "void main (void)\n"
				<< "{\n"
				<< "    gl_Position = v_in_position;\n"
				<< main.str()
				<< "}\n";

			programCollection.glslSources.add("read_ib_vert") << glu::VertexSource(src.str());
		}

		// Fragment
		{
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_440) << "\n"
				<< "\n"
				<< "layout(location = 0) out vec4 o_color;\n"
				<< "\n"
				<< "void main (void)\n"
				<< "{\n"
				<< "    o_color = vec4(1.0);\n"
				<< "}\n";

			programCollection.glslSources.add("read_ib_frag") << glu::FragmentSource(src.str());
		}

		// Compute
		{
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_440) << "\n"
				<< "\n"
				<< "layout(local_size_x = 1) in;\n"
				<< "\n"
				<< decl.str()
				<< "\n"
				<< "void main (void)\n"
				<< "{\n"
				<< main.str()
				<< "}\n";

			programCollection.glslSources.add("read_ib_comp") << glu::ComputeSource(src.str());
		}
	}

	deUint32 getInResourceUsageFlags (void) const
	{
		return VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
	}

	deUint32 getOutResourceUsageFlags (void) const
	{
		return 0;
	}

	VkQueueFlags getQueueFlags (const OperationContext& context) const
	{
		DE_UNREF(context);
		return (m_resourceDesc.type == RESOURCE_TYPE_INDIRECT_BUFFER_DISPATCH ? VK_QUEUE_COMPUTE_BIT : VK_QUEUE_GRAPHICS_BIT);
	}

	de::MovePtr<Operation> build (OperationContext& context, Resource& resource) const
	{
		return de::MovePtr<Operation>(new ReadImplementation(context, resource));
	}

	de::MovePtr<Operation> build (OperationContext&, Resource&, Resource&) const
	{
		DE_ASSERT(0);
		return de::MovePtr<Operation>();
	}

private:
	const ResourceDescription	m_resourceDesc;
};


class WriteSupport : public OperationSupport
{
public:
	WriteSupport (const ResourceDescription& resourceDesc)
	{
		DE_ASSERT(isIndirectBuffer(resourceDesc.type));
		DE_UNREF(resourceDesc);
	}

	deUint32 getInResourceUsageFlags (void) const
	{
		return 0;
	}

	deUint32 getOutResourceUsageFlags (void) const
	{
		return VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	}

	VkQueueFlags getQueueFlags (const OperationContext& context) const
	{
		DE_UNREF(context);
		return VK_QUEUE_TRANSFER_BIT;
	}

	de::MovePtr<Operation> build (OperationContext& context, Resource& resource) const
	{
		return de::MovePtr<Operation>(new WriteImplementation(context, resource));
	}

	de::MovePtr<Operation> build (OperationContext&, Resource&, Resource&) const
	{
		DE_ASSERT(0);
		return de::MovePtr<Operation>();
	}
};

} // IndirectBuffer ns

namespace VertexInput
{

enum DrawMode
{
	DRAW_MODE_VERTEX	= 0,
	DRAW_MODE_INDEXED,
};

class Implementation : public Operation
{
public:
	Implementation (OperationContext& context, Resource& resource, DrawMode drawMode)
		: m_context		(context)
		, m_resource	(resource)
		, m_drawMode	(drawMode)
	{
		requireFeaturesForSSBOAccess (m_context, VK_SHADER_STAGE_VERTEX_BIT);

		const DeviceInterface&		vk				= context.getDeviceInterface();
		const VkDevice				device			= context.getDevice();
		Allocator&					allocator		= context.getAllocator();
		VkFormat					attributeFormat = VK_FORMAT_R32G32B32A32_UINT;
		const VkDeviceSize			dataSizeBytes	= m_resource.getBuffer().size;

		// allocate ssbo that will store data used for verification
		{
			m_outputBuffer = de::MovePtr<Buffer>(new Buffer(vk, device, allocator,
				makeBufferCreateInfo(dataSizeBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), MemoryRequirement::HostVisible));

			const Allocation& alloc = m_outputBuffer->getAllocation();
			deMemset(alloc.getHostPtr(), 0, static_cast<size_t>(dataSizeBytes));
			flushAlloc(vk, device, alloc);
		}

		// allocate buffer that will be used for vertex attributes when we use resource for indices
		if (m_drawMode == DRAW_MODE_INDEXED)
		{
			attributeFormat = VK_FORMAT_R32_UINT;

			m_inputBuffer = de::MovePtr<Buffer>(new Buffer(vk, device, allocator,
				makeBufferCreateInfo(dataSizeBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT), MemoryRequirement::HostVisible));

			const Allocation& alloc = m_inputBuffer->getAllocation();
			fillPattern(alloc.getHostPtr(), dataSizeBytes, true);
			flushAlloc(vk, device, alloc);
		}

		m_descriptorSetLayout = DescriptorSetLayoutBuilder()
			.addSingleBinding	(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
			.build				(vk, device);

		m_descriptorPool = DescriptorPoolBuilder()
			.addType	(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
			.build		(vk, device, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 1u);

		m_descriptorSet = makeDescriptorSet(vk, device, *m_descriptorPool, *m_descriptorSetLayout);

		const VkDescriptorBufferInfo outputBufferDescriptorInfo = makeDescriptorBufferInfo(m_outputBuffer->get(), 0ull, dataSizeBytes);
		DescriptorSetUpdateBuilder()
			.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0u), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &outputBufferDescriptorInfo)
			.update		(vk, device);

		// Color attachment
		m_colorFormat						= VK_FORMAT_R8G8B8A8_UNORM;
		m_colorImageSubresourceRange		= makeImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u);
		m_colorImageExtent					= makeExtent3D(16u, 16u, 1u);
		m_colorAttachmentImage				= de::MovePtr<Image>(new Image(vk, device, allocator,
			makeImageCreateInfo(VK_IMAGE_TYPE_2D, m_colorImageExtent, m_colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
								VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL),
			MemoryRequirement::Any));

		// Pipeline
		m_colorAttachmentView	= makeImageView		(vk, device, **m_colorAttachmentImage, VK_IMAGE_VIEW_TYPE_2D, m_colorFormat, m_colorImageSubresourceRange);
		m_renderPass			= makeRenderPass	(vk, device, m_colorFormat);
		m_framebuffer			= makeFramebuffer	(vk, device, *m_renderPass, *m_colorAttachmentView, m_colorImageExtent.width, m_colorImageExtent.height);
		m_pipelineLayout		= makePipelineLayout(vk, device, *m_descriptorSetLayout);

		m_pipeline = GraphicsPipelineBuilder()
			.setPrimitiveTopology			(VK_PRIMITIVE_TOPOLOGY_POINT_LIST)
			.setRenderSize					(tcu::IVec2(static_cast<int>(m_colorImageExtent.width), static_cast<int>(m_colorImageExtent.height)))
			.setVertexInputSingleAttribute	(attributeFormat, tcu::getPixelSize(mapVkFormat(attributeFormat)))
			.setShader						(vk, device, VK_SHADER_STAGE_VERTEX_BIT,	context.getBinaryCollection().get("input_vert"), DE_NULL)
			.setShader						(vk, device, VK_SHADER_STAGE_FRAGMENT_BIT,	context.getBinaryCollection().get("input_frag"), DE_NULL)
			.build							(vk, device, *m_pipelineLayout, *m_renderPass, context.getPipelineCacheData(), context.getResourceInterface());
	}

	void recordCommands (const VkCommandBuffer cmdBuffer)
	{
		const DeviceInterface&		vk						= m_context.getDeviceInterface();
		const VkDeviceSize			dataSizeBytes			= m_resource.getBuffer().size;
		SynchronizationWrapperPtr	synchronizationWrapper	= getSynchronizationWrapper(m_context.getSynchronizationType(), vk, DE_FALSE);

		// Change color attachment image layout
		{
			const VkImageMemoryBarrier2KHR imageMemoryBarrier2 = makeImageMemoryBarrier2(
				VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR,				// VkPipelineStageFlags2KHR			srcStageMask
				(VkAccessFlags)0,										// VkAccessFlags2KHR				srcAccessMask
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,	// VkPipelineStageFlags2KHR			dstStageMask
				VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR,				// VkAccessFlags2KHR				dstAccessMask
				VK_IMAGE_LAYOUT_UNDEFINED,								// VkImageLayout					oldLayout
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,				// VkImageLayout					newLayout
				**m_colorAttachmentImage,								// VkImage							image
				m_colorImageSubresourceRange							// VkImageSubresourceRange			subresourceRange
			);
			VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, DE_NULL, &imageMemoryBarrier2);
			synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &dependencyInfo);
		}

		{
			const VkRect2D	renderArea	= makeRect2D(m_colorImageExtent);
			const tcu::Vec4	clearColor	= tcu::Vec4(0.0f, 0.0f, 0.0f, 1.0f);

			beginRenderPass(vk, cmdBuffer, *m_renderPass, *m_framebuffer, renderArea, clearColor);
		}

		vk.cmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipeline);
		vk.cmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayout, 0u, 1u, &m_descriptorSet.get(), 0u, DE_NULL);

		const VkDeviceSize vertexBufferOffset	= 0ull;
		if (m_drawMode == DRAW_MODE_VERTEX)
		{
			const deUint32 count = static_cast<deUint32>(dataSizeBytes / sizeof(tcu::UVec4));
			vk.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &m_resource.getBuffer().handle, &vertexBufferOffset);
			vk.cmdDraw(cmdBuffer, count, 1u, 0u, 0u);
		}
		else // (m_drawMode == DRAW_MODE_INDEXED)
		{
			const deUint32 count = static_cast<deUint32>(dataSizeBytes / sizeof(deUint32));
			vk.cmdBindVertexBuffers(cmdBuffer, 0u, 1u, &**m_inputBuffer, &vertexBufferOffset);
			vk.cmdBindIndexBuffer(cmdBuffer, m_resource.getBuffer().handle, 0u, VK_INDEX_TYPE_UINT32);
			vk.cmdDrawIndexed(cmdBuffer, count, 1, 0, 0, 0);
		}

		endRenderPass(vk, cmdBuffer);

		// Insert a barrier so data written by the shader is available to the host
		{
			const VkBufferMemoryBarrier2KHR bufferMemoryBarrier2 = makeBufferMemoryBarrier2(
				VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT_KHR,		// VkPipelineStageFlags2KHR			srcStageMask
				VK_ACCESS_2_SHADER_WRITE_BIT_KHR,				// VkAccessFlags2KHR				srcAccessMask
				VK_PIPELINE_STAGE_2_HOST_BIT_KHR,				// VkPipelineStageFlags2KHR			dstStageMask
				VK_ACCESS_2_HOST_READ_BIT_KHR,					// VkAccessFlags2KHR				dstAccessMask
				**m_outputBuffer,								// VkBuffer							buffer
				0u,												// VkDeviceSize						offset
				m_resource.getBuffer().size						// VkDeviceSize						size
			);
			VkDependencyInfoKHR dependencyInfo = makeCommonDependencyInfo(DE_NULL, &bufferMemoryBarrier2);
			synchronizationWrapper->cmdPipelineBarrier(cmdBuffer, &dependencyInfo);
		}
	}

	SyncInfo getInSyncInfo (void) const
	{
		const bool					usingIndexedDraw	= (m_drawMode == DRAW_MODE_INDEXED);
		VkPipelineStageFlags2KHR	stageMask			= VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT_KHR;
		VkAccessFlags2KHR			accessMask			= usingIndexedDraw ? VK_ACCESS_2_INDEX_READ_BIT_KHR
																		   : VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT_KHR;

		if (m_context.getSynchronizationType() == SynchronizationType::SYNCHRONIZATION2)
		{
			stageMask = usingIndexedDraw ? VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT_KHR
										 : VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT_KHR;
		}

		const SyncInfo syncInfo =
		{
			stageMask,									// VkPipelineStageFlags		stageMask;
			accessMask,									// VkAccessFlags			accessMask;
			VK_IMAGE_LAYOUT_UNDEFINED,					// VkImageLayout			imageLayout;
		};
		return syncInfo;
	}

	SyncInfo getOutSyncInfo (void) const
	{
		return emptySyncInfo;
	}

	Data getData (void) const
	{
		return getHostBufferData(m_context, *m_outputBuffer, m_resource.getBuffer().size);
	}

	void setData (const Data& data)
	{
		setHostBufferData(m_context, *m_outputBuffer, data);
	}

private:
	OperationContext&			m_context;
	Resource&					m_resource;
	DrawMode					m_drawMode;
	de::MovePtr<Buffer>			m_inputBuffer;
	de::MovePtr<Buffer>			m_outputBuffer;
	Move<VkRenderPass>			m_renderPass;
	Move<VkFramebuffer>			m_framebuffer;
	Move<VkPipelineLayout>		m_pipelineLayout;
	Move<VkPipeline>			m_pipeline;
	VkFormat					m_colorFormat;
	de::MovePtr<Image>			m_colorAttachmentImage;
	Move<VkImageView>			m_colorAttachmentView;
	VkExtent3D					m_colorImageExtent;
	VkImageSubresourceRange		m_colorImageSubresourceRange;
	Move<VkDescriptorPool>		m_descriptorPool;
	Move<VkDescriptorSetLayout>	m_descriptorSetLayout;
	Move<VkDescriptorSet>		m_descriptorSet;
};

class Support : public OperationSupport
{
public:
	Support(const ResourceDescription& resourceDesc, DrawMode drawMode)
		: m_resourceDesc	(resourceDesc)
		, m_drawMode		(drawMode)
	{
		DE_ASSERT(m_resourceDesc.type == RESOURCE_TYPE_BUFFER || m_resourceDesc.type == RESOURCE_TYPE_INDEX_BUFFER);
	}

	void initPrograms (SourceCollections& programCollection) const
	{
		// Vertex
		{
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_440) << "\n";
			if (m_drawMode == DRAW_MODE_VERTEX)
			{
				src << "layout(location = 0) in uvec4 v_in_data;\n"
					<< "layout(set = 0, binding = 0, std140) writeonly buffer Output {\n"
					<< "    uvec4 data[" << m_resourceDesc.size.x() / sizeof(tcu::UVec4) << "];\n"
					<< "} b_out;\n"
					<< "\n"
					<< "void main (void)\n"
					<< "{\n"
					<< "    b_out.data[gl_VertexIndex] = v_in_data;\n"
					<< "    gl_PointSize = 1.0f;\n"
					<< "}\n";
			}
			else // DRAW_MODE_INDEXED
			{
				src << "layout(location = 0) in uint v_in_data;\n"
					<< "layout(set = 0, binding = 0, std430) writeonly buffer Output {\n"
					<< "    uint data[" << m_resourceDesc.size.x() / sizeof(deUint32) << "];\n"
					<< "} b_out;\n"
					<< "\n"
					<< "void main (void)\n"
					<< "{\n"
					<< "    b_out.data[gl_VertexIndex] = v_in_data;\n"
					<< "    gl_PointSize = 1.0f;\n"
					<< "}\n";
			}
			programCollection.glslSources.add("input_vert") << glu::VertexSource(src.str());
		}

		// Fragment
		{
			std::ostringstream src;
			src << glu::getGLSLVersionDeclaration(glu::GLSL_VERSION_440) << "\n"
				<< "\n"
				<< "layout(location = 0) out vec4 o_color;\n"
				<< "\n"
				<< "void main (void)\n"
				<< "{\n"
				<< "    o_color = vec4(1.0);\n"
				<< "}\n";
			programCollection.glslSources.add("input_frag") << glu::FragmentSource(src.str());
		}
	}

	deUint32 getInResourceUsageFlags (void) const
	{
		return (m_drawMode == DRAW_MODE_VERTEX) ? VK_BUFFER_USAGE_VERTEX_BUFFER_BIT : VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	}

	deUint32 getOutResourceUsageFlags (void) const
	{
		return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	}

	VkQueueFlags getQueueFlags (const OperationContext&) const
	{
		return VK_QUEUE_GRAPHICS_BIT;
	}

	de::MovePtr<Operation> build (OperationContext& context, Resource& resource) const
	{
		return de::MovePtr<Operation>(new Implementation(context, resource, m_drawMode));
	}

	de::MovePtr<Operation> build (OperationContext&, Resource&, Resource&) const
	{
		DE_ASSERT(0);
		return de::MovePtr<Operation>();
	}

private:
	const ResourceDescription	m_resourceDesc;
	const DrawMode				m_drawMode;
};

} // VertexInput

} // anonymous ns

OperationContext::OperationContext (Context& context, SynchronizationType syncType, PipelineCacheData& pipelineCacheData)
	: m_context				(context)
	, m_syncType			(syncType)
	, m_vki					(context.getInstanceInterface())
	, m_vk					(context.getDeviceInterface())
	, m_physicalDevice		(context.getPhysicalDevice())
	, m_device				(context.getDevice())
	, m_allocator			(context.getDefaultAllocator())
	, m_progCollection		(context.getBinaryCollection())
	, m_pipelineCacheData	(pipelineCacheData)
{
}

OperationContext::OperationContext (Context&				context,
									SynchronizationType		syncType,
									const DeviceInterface&	vk,
									const VkDevice			device,
									vk::Allocator&			allocator,
									PipelineCacheData&		pipelineCacheData)
	: m_context				(context)
	, m_syncType			(syncType)
	, m_vki					(context.getInstanceInterface())
	, m_vk					(vk)
	, m_physicalDevice		(context.getPhysicalDevice())
	, m_device				(device)
	, m_allocator			(allocator)
	, m_progCollection		(context.getBinaryCollection())
	, m_pipelineCacheData	(pipelineCacheData)
{
}

OperationContext::OperationContext (Context&						context,
									SynchronizationType				syncType,
									const vk::InstanceInterface&	vki,
									const vk::DeviceInterface&		vkd,
									vk::VkPhysicalDevice			physicalDevice,
									vk::VkDevice					device,
									vk::Allocator&					allocator,
									vk::BinaryCollection&			programCollection,
									PipelineCacheData&				pipelineCacheData)
	: m_context				(context)
	, m_syncType			(syncType)
	, m_vki					(vki)
	, m_vk					(vkd)
	, m_physicalDevice		(physicalDevice)
	, m_device				(device)
	, m_allocator			(allocator)
	, m_progCollection		(programCollection)
	, m_pipelineCacheData	(pipelineCacheData)
{
}

Resource::Resource (OperationContext& context, const ResourceDescription& desc, const deUint32 usage, const vk::VkSharingMode sharingMode, const std::vector<deUint32>& queueFamilyIndex)
	: m_type	(desc.type)
{
	const DeviceInterface&		vk			= context.getDeviceInterface();
	const InstanceInterface&	vki			= context.getInstanceInterface();
	const VkDevice				device		= context.getDevice();
	const VkPhysicalDevice		physDevice	= context.getPhysicalDevice();
	Allocator&					allocator	= context.getAllocator();

	if (m_type == RESOURCE_TYPE_BUFFER || m_type == RESOURCE_TYPE_INDEX_BUFFER || isIndirectBuffer(m_type))
	{
		m_bufferData = de::MovePtr<BufferResource>(new BufferResource(DE_NULL, 0u, static_cast<VkDeviceSize>(desc.size.x())));
		VkBufferCreateInfo bufferCreateInfo = makeBufferCreateInfo(m_bufferData->size, usage);
		bufferCreateInfo.sharingMode		= sharingMode;
		if (queueFamilyIndex.size() > 0)
		{
			bufferCreateInfo.queueFamilyIndexCount	= static_cast<deUint32>(queueFamilyIndex.size());
			bufferCreateInfo.pQueueFamilyIndices	= &queueFamilyIndex[0];
		}
		m_buffer				= de::MovePtr<Buffer>(new Buffer(vk, device, allocator, bufferCreateInfo, MemoryRequirement::Any));
		m_bufferData->handle	= **m_buffer;
	}
	else if (m_type == RESOURCE_TYPE_IMAGE)
	{
		m_imageData = de::MovePtr<ImageResource>(new ImageResource(
			DE_NULL,
			makeExtent3D(desc.size.x(), std::max(1, desc.size.y()), std::max(1, desc.size.z())),
			desc.imageType,
			desc.imageFormat,
			makeImageSubresourceRange(desc.imageAspect, 0u, 1u, 0u, 1u),
			makeImageSubresourceLayers(desc.imageAspect, 0u, 0u, 1u),
			vk::VK_IMAGE_TILING_OPTIMAL
		));
		VkImageCreateInfo imageInfo		= makeImageCreateInfo(m_imageData->imageType, m_imageData->extent, m_imageData->format, usage, desc.imageSamples, m_imageData->tiling);
		imageInfo.sharingMode			= sharingMode;
		if (queueFamilyIndex.size() > 0)
		{
			imageInfo.queueFamilyIndexCount	= static_cast<deUint32>(queueFamilyIndex.size());
			imageInfo.pQueueFamilyIndices	= &queueFamilyIndex[0];
		}

		VkImageFormatProperties	imageFormatProperties;
		const VkResult formatResult		= vki.getPhysicalDeviceImageFormatProperties(physDevice, imageInfo.format, imageInfo.imageType, imageInfo.tiling, imageInfo.usage, imageInfo.flags, &imageFormatProperties);

		if (formatResult != VK_SUCCESS)
			TCU_THROW(NotSupportedError, "Image format is not supported");

		if ((imageFormatProperties.sampleCounts & desc.imageSamples) != desc.imageSamples)
			TCU_THROW(NotSupportedError, "Requested sample count is not supported");

		m_image							= de::MovePtr<Image>(new Image(vk, device, allocator, imageInfo, MemoryRequirement::Any));
		m_imageData->handle				= **m_image;
	}
	else
		DE_ASSERT(0);
}

Resource::Resource (ResourceType				type,
					vk::Move<vk::VkBuffer>		buffer,
					de::MovePtr<vk::Allocation>	allocation,
					vk::VkDeviceSize			offset,
					vk::VkDeviceSize			size)
	: m_type		(type)
	, m_buffer		(new Buffer(buffer, allocation))
	, m_bufferData	(de::MovePtr<BufferResource>(new BufferResource(m_buffer->get(), offset, size)))
{
	DE_ASSERT(type != RESOURCE_TYPE_IMAGE);
}

Resource::Resource (vk::Move<vk::VkImage>			image,
					de::MovePtr<vk::Allocation>		allocation,
					const vk::VkExtent3D&			extent,
					vk::VkImageType					imageType,
					vk::VkFormat					format,
					vk::VkImageSubresourceRange		subresourceRange,
					vk::VkImageSubresourceLayers	subresourceLayers,
					vk::VkImageTiling				tiling)
	: m_type		(RESOURCE_TYPE_IMAGE)
	, m_image		(new Image(image, allocation))
	, m_imageData	(de::MovePtr<ImageResource>(new ImageResource(m_image->get(), extent, imageType, format, subresourceRange, subresourceLayers, tiling)))
{
}

vk::VkDeviceMemory Resource::getMemory (void) const
{
	if (m_type == RESOURCE_TYPE_IMAGE)
		return m_image->getAllocation().getMemory();
	else
		return m_buffer->getAllocation().getMemory();
}

//! \note This function exists for performance reasons. We're creating a lot of tests and checking requirements here
//!       before creating an OperationSupport object is faster.
bool isResourceSupported (const OperationName opName, const ResourceDescription& resourceDesc)
{
	switch (opName)
	{
		case OPERATION_NAME_WRITE_FILL_BUFFER:
		case OPERATION_NAME_WRITE_COPY_BUFFER:
		case OPERATION_NAME_WRITE_COPY_IMAGE_TO_BUFFER:
		case OPERATION_NAME_WRITE_SSBO_VERTEX:
		case OPERATION_NAME_WRITE_SSBO_TESSELLATION_CONTROL:
		case OPERATION_NAME_WRITE_SSBO_TESSELLATION_EVALUATION:
		case OPERATION_NAME_WRITE_SSBO_GEOMETRY:
		case OPERATION_NAME_WRITE_SSBO_FRAGMENT:
		case OPERATION_NAME_WRITE_SSBO_COMPUTE:
		case OPERATION_NAME_WRITE_SSBO_COMPUTE_INDIRECT:
		case OPERATION_NAME_READ_COPY_BUFFER:
		case OPERATION_NAME_READ_COPY_BUFFER_TO_IMAGE:
		case OPERATION_NAME_READ_SSBO_VERTEX:
		case OPERATION_NAME_READ_SSBO_TESSELLATION_CONTROL:
		case OPERATION_NAME_READ_SSBO_TESSELLATION_EVALUATION:
		case OPERATION_NAME_READ_SSBO_GEOMETRY:
		case OPERATION_NAME_READ_SSBO_FRAGMENT:
		case OPERATION_NAME_READ_SSBO_COMPUTE:
		case OPERATION_NAME_READ_SSBO_COMPUTE_INDIRECT:
		case OPERATION_NAME_READ_VERTEX_INPUT:
			return resourceDesc.type == RESOURCE_TYPE_BUFFER;

		case OPERATION_NAME_WRITE_INDIRECT_BUFFER_DRAW:
		case OPERATION_NAME_READ_INDIRECT_BUFFER_DRAW:
			return resourceDesc.type == RESOURCE_TYPE_INDIRECT_BUFFER_DRAW;

		case OPERATION_NAME_WRITE_INDIRECT_BUFFER_DRAW_INDEXED:
		case OPERATION_NAME_READ_INDIRECT_BUFFER_DRAW_INDEXED:
			return resourceDesc.type == RESOURCE_TYPE_INDIRECT_BUFFER_DRAW_INDEXED;

		case OPERATION_NAME_WRITE_INDIRECT_BUFFER_DISPATCH:
		case OPERATION_NAME_READ_INDIRECT_BUFFER_DISPATCH:
			return resourceDesc.type == RESOURCE_TYPE_INDIRECT_BUFFER_DISPATCH;

		case OPERATION_NAME_WRITE_UPDATE_INDEX_BUFFER:
		case OPERATION_NAME_READ_INDEX_INPUT:
			return resourceDesc.type == RESOURCE_TYPE_INDEX_BUFFER;

		case OPERATION_NAME_WRITE_UPDATE_BUFFER:
			return resourceDesc.type == RESOURCE_TYPE_BUFFER && resourceDesc.size.x() <= MAX_UPDATE_BUFFER_SIZE;

		case OPERATION_NAME_WRITE_COPY_IMAGE:
		case OPERATION_NAME_WRITE_COPY_BUFFER_TO_IMAGE:
		case OPERATION_NAME_READ_COPY_IMAGE:
		case OPERATION_NAME_READ_COPY_IMAGE_TO_BUFFER:
			return resourceDesc.type == RESOURCE_TYPE_IMAGE && resourceDesc.imageSamples == VK_SAMPLE_COUNT_1_BIT;

		case OPERATION_NAME_WRITE_CLEAR_ATTACHMENTS:
			return resourceDesc.type == RESOURCE_TYPE_IMAGE && resourceDesc.imageType != VK_IMAGE_TYPE_3D
				&& resourceDesc.imageSamples == VK_SAMPLE_COUNT_1_BIT;

		case OPERATION_NAME_WRITE_IMAGE_COMPUTE_MULTISAMPLE:
		case OPERATION_NAME_READ_RESOLVE_IMAGE:
			return resourceDesc.type == RESOURCE_TYPE_IMAGE && resourceDesc.imageAspect == VK_IMAGE_ASPECT_COLOR_BIT
				&& resourceDesc.imageSamples != VK_SAMPLE_COUNT_1_BIT;

		case OPERATION_NAME_WRITE_BLIT_IMAGE:
		case OPERATION_NAME_READ_BLIT_IMAGE:
		case OPERATION_NAME_WRITE_IMAGE_VERTEX:
		case OPERATION_NAME_WRITE_IMAGE_TESSELLATION_CONTROL:
		case OPERATION_NAME_WRITE_IMAGE_TESSELLATION_EVALUATION:
		case OPERATION_NAME_WRITE_IMAGE_GEOMETRY:
		case OPERATION_NAME_WRITE_IMAGE_FRAGMENT:
		case OPERATION_NAME_WRITE_IMAGE_COMPUTE:
		case OPERATION_NAME_WRITE_IMAGE_COMPUTE_INDIRECT:
		case OPERATION_NAME_READ_IMAGE_VERTEX:
		case OPERATION_NAME_READ_IMAGE_TESSELLATION_CONTROL:
		case OPERATION_NAME_READ_IMAGE_TESSELLATION_EVALUATION:
		case OPERATION_NAME_READ_IMAGE_GEOMETRY:
		case OPERATION_NAME_READ_IMAGE_FRAGMENT:
		case OPERATION_NAME_READ_IMAGE_COMPUTE:
		case OPERATION_NAME_READ_IMAGE_COMPUTE_INDIRECT:
			return resourceDesc.type == RESOURCE_TYPE_IMAGE && resourceDesc.imageAspect == VK_IMAGE_ASPECT_COLOR_BIT
				&& resourceDesc.imageSamples == VK_SAMPLE_COUNT_1_BIT;

		case OPERATION_NAME_READ_UBO_VERTEX:
		case OPERATION_NAME_READ_UBO_TESSELLATION_CONTROL:
		case OPERATION_NAME_READ_UBO_TESSELLATION_EVALUATION:
		case OPERATION_NAME_READ_UBO_GEOMETRY:
		case OPERATION_NAME_READ_UBO_FRAGMENT:
		case OPERATION_NAME_READ_UBO_COMPUTE:
		case OPERATION_NAME_READ_UBO_COMPUTE_INDIRECT:
		case OPERATION_NAME_READ_UBO_TEXEL_VERTEX:
		case OPERATION_NAME_READ_UBO_TEXEL_TESSELLATION_CONTROL:
		case OPERATION_NAME_READ_UBO_TEXEL_TESSELLATION_EVALUATION:
		case OPERATION_NAME_READ_UBO_TEXEL_GEOMETRY:
		case OPERATION_NAME_READ_UBO_TEXEL_FRAGMENT:
		case OPERATION_NAME_READ_UBO_TEXEL_COMPUTE:
		case OPERATION_NAME_READ_UBO_TEXEL_COMPUTE_INDIRECT:
			return resourceDesc.type == RESOURCE_TYPE_BUFFER && resourceDesc.size.x() <= MAX_UBO_RANGE;

		case OPERATION_NAME_WRITE_CLEAR_COLOR_IMAGE:
			return resourceDesc.type == RESOURCE_TYPE_IMAGE && resourceDesc.imageAspect == VK_IMAGE_ASPECT_COLOR_BIT
				&& resourceDesc.imageSamples == VK_SAMPLE_COUNT_1_BIT;

		case OPERATION_NAME_WRITE_CLEAR_DEPTH_STENCIL_IMAGE:
			return resourceDesc.type == RESOURCE_TYPE_IMAGE && (resourceDesc.imageAspect & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
				&& resourceDesc.imageSamples == VK_SAMPLE_COUNT_1_BIT;

		case OPERATION_NAME_WRITE_DRAW:
		case OPERATION_NAME_WRITE_DRAW_INDEXED:
		case OPERATION_NAME_WRITE_DRAW_INDIRECT:
		case OPERATION_NAME_WRITE_DRAW_INDEXED_INDIRECT:
			return resourceDesc.type == RESOURCE_TYPE_IMAGE && resourceDesc.imageType == VK_IMAGE_TYPE_2D
				&& (resourceDesc.imageAspect & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) == 0
				&& resourceDesc.imageSamples == VK_SAMPLE_COUNT_1_BIT;

		case OPERATION_NAME_COPY_BUFFER:
		case OPERATION_NAME_COPY_SSBO_VERTEX:
		case OPERATION_NAME_COPY_SSBO_TESSELLATION_CONTROL:
		case OPERATION_NAME_COPY_SSBO_TESSELLATION_EVALUATION:
		case OPERATION_NAME_COPY_SSBO_GEOMETRY:
		case OPERATION_NAME_COPY_SSBO_FRAGMENT:
		case OPERATION_NAME_COPY_SSBO_COMPUTE:
		case OPERATION_NAME_COPY_SSBO_COMPUTE_INDIRECT:
			return resourceDesc.type == RESOURCE_TYPE_BUFFER;

		case OPERATION_NAME_COPY_IMAGE:
		case OPERATION_NAME_BLIT_IMAGE:
		case OPERATION_NAME_COPY_IMAGE_VERTEX:
		case OPERATION_NAME_COPY_IMAGE_TESSELLATION_CONTROL:
		case OPERATION_NAME_COPY_IMAGE_TESSELLATION_EVALUATION:
		case OPERATION_NAME_COPY_IMAGE_GEOMETRY:
		case OPERATION_NAME_COPY_IMAGE_FRAGMENT:
		case OPERATION_NAME_COPY_IMAGE_COMPUTE:
		case OPERATION_NAME_COPY_IMAGE_COMPUTE_INDIRECT:
			return resourceDesc.type == RESOURCE_TYPE_IMAGE && resourceDesc.imageAspect == VK_IMAGE_ASPECT_COLOR_BIT
				&& resourceDesc.imageSamples == VK_SAMPLE_COUNT_1_BIT;

		default:
			DE_ASSERT(0);
			return false;
	}
}

std::string getOperationName (const OperationName opName)
{
	switch (opName)
	{
		case OPERATION_NAME_WRITE_FILL_BUFFER:						return "write_fill_buffer";
		case OPERATION_NAME_WRITE_UPDATE_BUFFER:					return "write_update_buffer";
		case OPERATION_NAME_WRITE_COPY_BUFFER:						return "write_copy_buffer";
		case OPERATION_NAME_WRITE_COPY_BUFFER_TO_IMAGE:				return "write_copy_buffer_to_image";
		case OPERATION_NAME_WRITE_COPY_IMAGE_TO_BUFFER:				return "write_copy_image_to_buffer";
		case OPERATION_NAME_WRITE_COPY_IMAGE:						return "write_copy_image";
		case OPERATION_NAME_WRITE_BLIT_IMAGE:						return "write_blit_image";
		case OPERATION_NAME_WRITE_SSBO_VERTEX:						return "write_ssbo_vertex";
		case OPERATION_NAME_WRITE_SSBO_TESSELLATION_CONTROL:		return "write_ssbo_tess_control";
		case OPERATION_NAME_WRITE_SSBO_TESSELLATION_EVALUATION:		return "write_ssbo_tess_eval";
		case OPERATION_NAME_WRITE_SSBO_GEOMETRY:					return "write_ssbo_geometry";
		case OPERATION_NAME_WRITE_SSBO_FRAGMENT:					return "write_ssbo_fragment";
		case OPERATION_NAME_WRITE_SSBO_COMPUTE:						return "write_ssbo_compute";
		case OPERATION_NAME_WRITE_SSBO_COMPUTE_INDIRECT:			return "write_ssbo_compute_indirect";
		case OPERATION_NAME_WRITE_IMAGE_VERTEX:						return "write_image_vertex";
		case OPERATION_NAME_WRITE_IMAGE_TESSELLATION_CONTROL:		return "write_image_tess_control";
		case OPERATION_NAME_WRITE_IMAGE_TESSELLATION_EVALUATION:	return "write_image_tess_eval";
		case OPERATION_NAME_WRITE_IMAGE_GEOMETRY:					return "write_image_geometry";
		case OPERATION_NAME_WRITE_IMAGE_FRAGMENT:					return "write_image_fragment";
		case OPERATION_NAME_WRITE_IMAGE_COMPUTE:					return "write_image_compute";
		case OPERATION_NAME_WRITE_IMAGE_COMPUTE_MULTISAMPLE:		return "write_image_compute_multisample";
		case OPERATION_NAME_WRITE_IMAGE_COMPUTE_INDIRECT:			return "write_image_compute_indirect";
		case OPERATION_NAME_WRITE_CLEAR_COLOR_IMAGE:				return "write_clear_color_image";
		case OPERATION_NAME_WRITE_CLEAR_DEPTH_STENCIL_IMAGE:		return "write_clear_depth_stencil_image";
		case OPERATION_NAME_WRITE_DRAW:								return "write_draw";
		case OPERATION_NAME_WRITE_DRAW_INDEXED:						return "write_draw_indexed";
		case OPERATION_NAME_WRITE_DRAW_INDIRECT:					return "write_draw_indirect";
		case OPERATION_NAME_WRITE_DRAW_INDEXED_INDIRECT:			return "write_draw_indexed_indirect";
		case OPERATION_NAME_WRITE_CLEAR_ATTACHMENTS:				return "write_clear_attachments";
		case OPERATION_NAME_WRITE_INDIRECT_BUFFER_DRAW:				return "write_indirect_buffer_draw";
		case OPERATION_NAME_WRITE_INDIRECT_BUFFER_DRAW_INDEXED:		return "write_indirect_buffer_draw_indexed";
		case OPERATION_NAME_WRITE_INDIRECT_BUFFER_DISPATCH:			return "write_indirect_buffer_dispatch";
		case OPERATION_NAME_WRITE_UPDATE_INDEX_BUFFER:				return "write_update_index_buffer";

		case OPERATION_NAME_READ_COPY_BUFFER:						return "read_copy_buffer";
		case OPERATION_NAME_READ_COPY_BUFFER_TO_IMAGE:				return "read_copy_buffer_to_image";
		case OPERATION_NAME_READ_COPY_IMAGE_TO_BUFFER:				return "read_copy_image_to_buffer";
		case OPERATION_NAME_READ_COPY_IMAGE:						return "read_copy_image";
		case OPERATION_NAME_READ_BLIT_IMAGE:						return "read_blit_image";
		case OPERATION_NAME_READ_RESOLVE_IMAGE:						return "read_resolve_image";
		case OPERATION_NAME_READ_UBO_VERTEX:						return "read_ubo_vertex";
		case OPERATION_NAME_READ_UBO_TESSELLATION_CONTROL:			return "read_ubo_tess_control";
		case OPERATION_NAME_READ_UBO_TESSELLATION_EVALUATION:		return "read_ubo_tess_eval";
		case OPERATION_NAME_READ_UBO_GEOMETRY:						return "read_ubo_geometry";
		case OPERATION_NAME_READ_UBO_FRAGMENT:						return "read_ubo_fragment";
		case OPERATION_NAME_READ_UBO_COMPUTE:						return "read_ubo_compute";
		case OPERATION_NAME_READ_UBO_COMPUTE_INDIRECT:				return "read_ubo_compute_indirect";
		case OPERATION_NAME_READ_UBO_TEXEL_VERTEX:					return "read_ubo_texel_vertex";
		case OPERATION_NAME_READ_UBO_TEXEL_TESSELLATION_CONTROL:	return "read_ubo_texel_tess_control";
		case OPERATION_NAME_READ_UBO_TEXEL_TESSELLATION_EVALUATION:	return "read_ubo_texel_tess_eval";
		case OPERATION_NAME_READ_UBO_TEXEL_GEOMETRY:				return "read_ubo_texel_geometry";
		case OPERATION_NAME_READ_UBO_TEXEL_FRAGMENT:				return "read_ubo_texel_fragment";
		case OPERATION_NAME_READ_UBO_TEXEL_COMPUTE:					return "read_ubo_texel_compute";
		case OPERATION_NAME_READ_UBO_TEXEL_COMPUTE_INDIRECT:		return "read_ubo_texel_compute_indirect";
		case OPERATION_NAME_READ_SSBO_VERTEX:						return "read_ssbo_vertex";
		case OPERATION_NAME_READ_SSBO_TESSELLATION_CONTROL:			return "read_ssbo_tess_control";
		case OPERATION_NAME_READ_SSBO_TESSELLATION_EVALUATION:		return "read_ssbo_tess_eval";
		case OPERATION_NAME_READ_SSBO_GEOMETRY:						return "read_ssbo_geometry";
		case OPERATION_NAME_READ_SSBO_FRAGMENT:						return "read_ssbo_fragment";
		case OPERATION_NAME_READ_SSBO_COMPUTE:						return "read_ssbo_compute";
		case OPERATION_NAME_READ_SSBO_COMPUTE_INDIRECT:				return "read_ssbo_compute_indirect";
		case OPERATION_NAME_READ_IMAGE_VERTEX:						return "read_image_vertex";
		case OPERATION_NAME_READ_IMAGE_TESSELLATION_CONTROL:		return "read_image_tess_control";
		case OPERATION_NAME_READ_IMAGE_TESSELLATION_EVALUATION:		return "read_image_tess_eval";
		case OPERATION_NAME_READ_IMAGE_GEOMETRY:					return "read_image_geometry";
		case OPERATION_NAME_READ_IMAGE_FRAGMENT:					return "read_image_fragment";
		case OPERATION_NAME_READ_IMAGE_COMPUTE:						return "read_image_compute";
		case OPERATION_NAME_READ_IMAGE_COMPUTE_INDIRECT:			return "read_image_compute_indirect";
		case OPERATION_NAME_READ_INDIRECT_BUFFER_DRAW:				return "read_indirect_buffer_draw";
		case OPERATION_NAME_READ_INDIRECT_BUFFER_DRAW_INDEXED:		return "read_indirect_buffer_draw_indexed";
		case OPERATION_NAME_READ_INDIRECT_BUFFER_DISPATCH:			return "read_indirect_buffer_dispatch";
		case OPERATION_NAME_READ_VERTEX_INPUT:						return "read_vertex_input";
		case OPERATION_NAME_READ_INDEX_INPUT:						return "read_index_input";

		case OPERATION_NAME_COPY_BUFFER:							return "copy_buffer";
		case OPERATION_NAME_COPY_IMAGE:								return "copy_image";
		case OPERATION_NAME_BLIT_IMAGE:								return "blit_image";
		case OPERATION_NAME_COPY_SSBO_VERTEX:						return "copy_buffer_vertex";
		case OPERATION_NAME_COPY_SSBO_TESSELLATION_CONTROL:			return "copy_ssbo_tess_control";
		case OPERATION_NAME_COPY_SSBO_TESSELLATION_EVALUATION:		return "copy_ssbo_tess_eval";
		case OPERATION_NAME_COPY_SSBO_GEOMETRY:						return "copy_ssbo_geometry";
		case OPERATION_NAME_COPY_SSBO_FRAGMENT:						return "copy_ssbo_fragment";
		case OPERATION_NAME_COPY_SSBO_COMPUTE:						return "copy_ssbo_compute";
		case OPERATION_NAME_COPY_SSBO_COMPUTE_INDIRECT:				return "copy_ssbo_compute_indirect";
		case OPERATION_NAME_COPY_IMAGE_VERTEX:						return "copy_image_vertex";
		case OPERATION_NAME_COPY_IMAGE_TESSELLATION_CONTROL:		return "copy_image_tess_control";
		case OPERATION_NAME_COPY_IMAGE_TESSELLATION_EVALUATION:		return "copy_image_tess_eval";
		case OPERATION_NAME_COPY_IMAGE_GEOMETRY:					return "copy_image_geometry";
		case OPERATION_NAME_COPY_IMAGE_FRAGMENT:					return "copy_image_fragment";
		case OPERATION_NAME_COPY_IMAGE_COMPUTE:						return "copy_image_compute";
		case OPERATION_NAME_COPY_IMAGE_COMPUTE_INDIRECT:			return "copy_image_compute_indirect";
		default:
			DE_ASSERT(0);
			return "";
	}
}

bool isSpecializedAccessFlagSupported (const OperationName opName)
{
	switch (opName)
	{
		case OPERATION_NAME_WRITE_SSBO_VERTEX:
		case OPERATION_NAME_WRITE_SSBO_TESSELLATION_CONTROL:
		case OPERATION_NAME_WRITE_SSBO_TESSELLATION_EVALUATION:
		case OPERATION_NAME_WRITE_SSBO_GEOMETRY:
		case OPERATION_NAME_WRITE_SSBO_FRAGMENT:
		case OPERATION_NAME_WRITE_SSBO_COMPUTE:
		case OPERATION_NAME_WRITE_SSBO_COMPUTE_INDIRECT:
		case OPERATION_NAME_WRITE_IMAGE_VERTEX:
		case OPERATION_NAME_WRITE_IMAGE_TESSELLATION_CONTROL:
		case OPERATION_NAME_WRITE_IMAGE_TESSELLATION_EVALUATION:
		case OPERATION_NAME_WRITE_IMAGE_GEOMETRY:
		case OPERATION_NAME_WRITE_IMAGE_FRAGMENT:
		case OPERATION_NAME_WRITE_IMAGE_COMPUTE:
		case OPERATION_NAME_WRITE_IMAGE_COMPUTE_INDIRECT:
		case OPERATION_NAME_READ_UBO_VERTEX:
		case OPERATION_NAME_READ_UBO_TESSELLATION_CONTROL:
		case OPERATION_NAME_READ_UBO_TESSELLATION_EVALUATION:
		case OPERATION_NAME_READ_UBO_GEOMETRY:
		case OPERATION_NAME_READ_UBO_FRAGMENT:
		case OPERATION_NAME_READ_UBO_COMPUTE:
		case OPERATION_NAME_READ_UBO_COMPUTE_INDIRECT:
		case OPERATION_NAME_READ_UBO_TEXEL_VERTEX:
		case OPERATION_NAME_READ_UBO_TEXEL_TESSELLATION_CONTROL:
		case OPERATION_NAME_READ_UBO_TEXEL_TESSELLATION_EVALUATION:
		case OPERATION_NAME_READ_UBO_TEXEL_GEOMETRY:
		case OPERATION_NAME_READ_UBO_TEXEL_FRAGMENT:
		case OPERATION_NAME_READ_UBO_TEXEL_COMPUTE:
		case OPERATION_NAME_READ_UBO_TEXEL_COMPUTE_INDIRECT:
		case OPERATION_NAME_READ_SSBO_VERTEX:
		case OPERATION_NAME_READ_SSBO_TESSELLATION_CONTROL:
		case OPERATION_NAME_READ_SSBO_TESSELLATION_EVALUATION:
		case OPERATION_NAME_READ_SSBO_GEOMETRY:
		case OPERATION_NAME_READ_SSBO_FRAGMENT:
		case OPERATION_NAME_READ_SSBO_COMPUTE:
		case OPERATION_NAME_READ_SSBO_COMPUTE_INDIRECT:
		case OPERATION_NAME_READ_IMAGE_VERTEX:
		case OPERATION_NAME_READ_IMAGE_TESSELLATION_CONTROL:
		case OPERATION_NAME_READ_IMAGE_TESSELLATION_EVALUATION:
		case OPERATION_NAME_READ_IMAGE_GEOMETRY:
		case OPERATION_NAME_READ_IMAGE_FRAGMENT:
		case OPERATION_NAME_READ_IMAGE_COMPUTE:
		case OPERATION_NAME_READ_IMAGE_COMPUTE_INDIRECT:
		case OPERATION_NAME_COPY_SSBO_VERTEX:
		case OPERATION_NAME_COPY_SSBO_TESSELLATION_CONTROL:
		case OPERATION_NAME_COPY_SSBO_GEOMETRY:
		case OPERATION_NAME_COPY_SSBO_FRAGMENT:
		case OPERATION_NAME_COPY_SSBO_COMPUTE:
		case OPERATION_NAME_COPY_SSBO_COMPUTE_INDIRECT:
		case OPERATION_NAME_COPY_IMAGE_VERTEX:
		case OPERATION_NAME_COPY_IMAGE_TESSELLATION_CONTROL:
		case OPERATION_NAME_COPY_IMAGE_TESSELLATION_EVALUATION:
		case OPERATION_NAME_COPY_IMAGE_GEOMETRY:
		case OPERATION_NAME_COPY_IMAGE_FRAGMENT:
		case OPERATION_NAME_COPY_IMAGE_COMPUTE:
		case OPERATION_NAME_COPY_IMAGE_COMPUTE_INDIRECT:
			return true;
		default:
			return false;

	}
}
de::MovePtr<OperationSupport> makeOperationSupport (const OperationName opName, const ResourceDescription& resourceDesc, const bool specializedAccess)
{
	switch (opName)
	{
		case OPERATION_NAME_WRITE_FILL_BUFFER:						return de::MovePtr<OperationSupport>(new FillUpdateBuffer	::Support		(resourceDesc, FillUpdateBuffer::BUFFER_OP_FILL));
		case OPERATION_NAME_WRITE_UPDATE_BUFFER:					return de::MovePtr<OperationSupport>(new FillUpdateBuffer	::Support		(resourceDesc, FillUpdateBuffer::BUFFER_OP_UPDATE));
		case OPERATION_NAME_WRITE_COPY_BUFFER:						return de::MovePtr<OperationSupport>(new CopyBuffer			::Support		(resourceDesc, ACCESS_MODE_WRITE));
		case OPERATION_NAME_WRITE_COPY_BUFFER_TO_IMAGE:				return de::MovePtr<OperationSupport>(new CopyBufferToImage	::Support		(resourceDesc, ACCESS_MODE_WRITE));
		case OPERATION_NAME_WRITE_COPY_IMAGE_TO_BUFFER:				return de::MovePtr<OperationSupport>(new CopyImageToBuffer	::Support		(resourceDesc, ACCESS_MODE_WRITE));
		case OPERATION_NAME_WRITE_COPY_IMAGE:						return de::MovePtr<OperationSupport>(new CopyBlitResolveImage	::Support	(resourceDesc, CopyBlitResolveImage::TYPE_COPY, ACCESS_MODE_WRITE));
		case OPERATION_NAME_WRITE_BLIT_IMAGE:						return de::MovePtr<OperationSupport>(new CopyBlitResolveImage	::Support	(resourceDesc, CopyBlitResolveImage::TYPE_BLIT, ACCESS_MODE_WRITE));
		case OPERATION_NAME_WRITE_SSBO_VERTEX:						return de::MovePtr<OperationSupport>(new ShaderAccess		::BufferSupport	(resourceDesc, BUFFER_TYPE_STORAGE, ACCESS_MODE_WRITE, specializedAccess, VK_SHADER_STAGE_VERTEX_BIT));
		case OPERATION_NAME_WRITE_SSBO_TESSELLATION_CONTROL:		return de::MovePtr<OperationSupport>(new ShaderAccess		::BufferSupport	(resourceDesc, BUFFER_TYPE_STORAGE, ACCESS_MODE_WRITE, specializedAccess, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT));
		case OPERATION_NAME_WRITE_SSBO_TESSELLATION_EVALUATION:		return de::MovePtr<OperationSupport>(new ShaderAccess		::BufferSupport	(resourceDesc, BUFFER_TYPE_STORAGE, ACCESS_MODE_WRITE, specializedAccess, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT));
		case OPERATION_NAME_WRITE_SSBO_GEOMETRY:					return de::MovePtr<OperationSupport>(new ShaderAccess		::BufferSupport	(resourceDesc, BUFFER_TYPE_STORAGE, ACCESS_MODE_WRITE, specializedAccess, VK_SHADER_STAGE_GEOMETRY_BIT));
		case OPERATION_NAME_WRITE_SSBO_FRAGMENT:					return de::MovePtr<OperationSupport>(new ShaderAccess		::BufferSupport	(resourceDesc, BUFFER_TYPE_STORAGE, ACCESS_MODE_WRITE, specializedAccess, VK_SHADER_STAGE_FRAGMENT_BIT));
		case OPERATION_NAME_WRITE_SSBO_COMPUTE:						return de::MovePtr<OperationSupport>(new ShaderAccess		::BufferSupport	(resourceDesc, BUFFER_TYPE_STORAGE, ACCESS_MODE_WRITE, specializedAccess, VK_SHADER_STAGE_COMPUTE_BIT));
		case OPERATION_NAME_WRITE_SSBO_COMPUTE_INDIRECT:			return de::MovePtr<OperationSupport>(new ShaderAccess		::BufferSupport	(resourceDesc, BUFFER_TYPE_STORAGE, ACCESS_MODE_WRITE, specializedAccess, VK_SHADER_STAGE_COMPUTE_BIT, ShaderAccess::DISPATCH_CALL_DISPATCH_INDIRECT));
		case OPERATION_NAME_WRITE_IMAGE_VERTEX:						return de::MovePtr<OperationSupport>(new ShaderAccess		::ImageSupport	(resourceDesc, ACCESS_MODE_WRITE, specializedAccess, VK_SHADER_STAGE_VERTEX_BIT));
		case OPERATION_NAME_WRITE_IMAGE_TESSELLATION_CONTROL:		return de::MovePtr<OperationSupport>(new ShaderAccess		::ImageSupport	(resourceDesc, ACCESS_MODE_WRITE, specializedAccess, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT));
		case OPERATION_NAME_WRITE_IMAGE_TESSELLATION_EVALUATION:	return de::MovePtr<OperationSupport>(new ShaderAccess		::ImageSupport	(resourceDesc, ACCESS_MODE_WRITE, specializedAccess, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT));
		case OPERATION_NAME_WRITE_IMAGE_GEOMETRY:					return de::MovePtr<OperationSupport>(new ShaderAccess		::ImageSupport	(resourceDesc, ACCESS_MODE_WRITE, specializedAccess, VK_SHADER_STAGE_GEOMETRY_BIT));
		case OPERATION_NAME_WRITE_IMAGE_FRAGMENT:					return de::MovePtr<OperationSupport>(new ShaderAccess		::ImageSupport	(resourceDesc, ACCESS_MODE_WRITE, specializedAccess, VK_SHADER_STAGE_FRAGMENT_BIT));
		case OPERATION_NAME_WRITE_IMAGE_COMPUTE:					return de::MovePtr<OperationSupport>(new ShaderAccess		::ImageSupport	(resourceDesc, ACCESS_MODE_WRITE, specializedAccess, VK_SHADER_STAGE_COMPUTE_BIT));
		case OPERATION_NAME_WRITE_IMAGE_COMPUTE_INDIRECT:			return de::MovePtr<OperationSupport>(new ShaderAccess		::ImageSupport	(resourceDesc, ACCESS_MODE_WRITE, specializedAccess, VK_SHADER_STAGE_COMPUTE_BIT, ShaderAccess::DISPATCH_CALL_DISPATCH_INDIRECT));
		case OPERATION_NAME_WRITE_IMAGE_COMPUTE_MULTISAMPLE:		return de::MovePtr<OperationSupport>(new ShaderAccess		::MSImageSupport(resourceDesc));
		case OPERATION_NAME_WRITE_CLEAR_COLOR_IMAGE:				return de::MovePtr<OperationSupport>(new ClearImage			::Support		(resourceDesc, ClearImage::CLEAR_MODE_COLOR));
		case OPERATION_NAME_WRITE_CLEAR_DEPTH_STENCIL_IMAGE:		return de::MovePtr<OperationSupport>(new ClearImage			::Support		(resourceDesc, ClearImage::CLEAR_MODE_DEPTH_STENCIL));
		case OPERATION_NAME_WRITE_DRAW:								return de::MovePtr<OperationSupport>(new Draw				::Support		(resourceDesc, Draw::DRAW_CALL_DRAW));
		case OPERATION_NAME_WRITE_DRAW_INDEXED:						return de::MovePtr<OperationSupport>(new Draw				::Support		(resourceDesc, Draw::DRAW_CALL_DRAW_INDEXED));
		case OPERATION_NAME_WRITE_DRAW_INDIRECT:					return de::MovePtr<OperationSupport>(new Draw				::Support		(resourceDesc, Draw::DRAW_CALL_DRAW_INDIRECT));
		case OPERATION_NAME_WRITE_DRAW_INDEXED_INDIRECT:			return de::MovePtr<OperationSupport>(new Draw				::Support		(resourceDesc, Draw::DRAW_CALL_DRAW_INDEXED_INDIRECT));
		case OPERATION_NAME_WRITE_CLEAR_ATTACHMENTS:				return de::MovePtr<OperationSupport>(new ClearAttachments	::Support		(resourceDesc));
		case OPERATION_NAME_WRITE_INDIRECT_BUFFER_DRAW:				return de::MovePtr<OperationSupport>(new IndirectBuffer		::WriteSupport	(resourceDesc));
		case OPERATION_NAME_WRITE_INDIRECT_BUFFER_DRAW_INDEXED:		return de::MovePtr<OperationSupport>(new IndirectBuffer		::WriteSupport	(resourceDesc));
		case OPERATION_NAME_WRITE_INDIRECT_BUFFER_DISPATCH:			return de::MovePtr<OperationSupport>(new IndirectBuffer		::WriteSupport	(resourceDesc));
		case OPERATION_NAME_WRITE_UPDATE_INDEX_BUFFER:				return de::MovePtr<OperationSupport>(new FillUpdateBuffer	::Support		(resourceDesc, FillUpdateBuffer::BUFFER_OP_UPDATE_WITH_INDEX_PATTERN));

		case OPERATION_NAME_READ_COPY_BUFFER:						return de::MovePtr<OperationSupport>(new CopyBuffer			::Support		(resourceDesc, ACCESS_MODE_READ));
		case OPERATION_NAME_READ_COPY_BUFFER_TO_IMAGE:				return de::MovePtr<OperationSupport>(new CopyBufferToImage	::Support		(resourceDesc, ACCESS_MODE_READ));
		case OPERATION_NAME_READ_COPY_IMAGE_TO_BUFFER:				return de::MovePtr<OperationSupport>(new CopyImageToBuffer	::Support		(resourceDesc, ACCESS_MODE_READ));
		case OPERATION_NAME_READ_COPY_IMAGE:						return de::MovePtr<OperationSupport>(new CopyBlitResolveImage::Support		(resourceDesc, CopyBlitResolveImage::TYPE_COPY,		ACCESS_MODE_READ));
		case OPERATION_NAME_READ_BLIT_IMAGE:						return de::MovePtr<OperationSupport>(new CopyBlitResolveImage::Support		(resourceDesc, CopyBlitResolveImage::TYPE_BLIT,		ACCESS_MODE_READ));
		case OPERATION_NAME_READ_RESOLVE_IMAGE:						return de::MovePtr<OperationSupport>(new CopyBlitResolveImage::Support		(resourceDesc, CopyBlitResolveImage::TYPE_RESOLVE,	ACCESS_MODE_READ));
		case OPERATION_NAME_READ_UBO_VERTEX:						return de::MovePtr<OperationSupport>(new ShaderAccess		::BufferSupport	(resourceDesc, BUFFER_TYPE_UNIFORM, ACCESS_MODE_READ, specializedAccess, VK_SHADER_STAGE_VERTEX_BIT));
		case OPERATION_NAME_READ_UBO_TESSELLATION_CONTROL:			return de::MovePtr<OperationSupport>(new ShaderAccess		::BufferSupport	(resourceDesc, BUFFER_TYPE_UNIFORM, ACCESS_MODE_READ, specializedAccess, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT));
		case OPERATION_NAME_READ_UBO_TESSELLATION_EVALUATION:		return de::MovePtr<OperationSupport>(new ShaderAccess		::BufferSupport	(resourceDesc, BUFFER_TYPE_UNIFORM, ACCESS_MODE_READ, specializedAccess, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT));
		case OPERATION_NAME_READ_UBO_GEOMETRY:						return de::MovePtr<OperationSupport>(new ShaderAccess		::BufferSupport	(resourceDesc, BUFFER_TYPE_UNIFORM, ACCESS_MODE_READ, specializedAccess, VK_SHADER_STAGE_GEOMETRY_BIT));
		case OPERATION_NAME_READ_UBO_FRAGMENT:						return de::MovePtr<OperationSupport>(new ShaderAccess		::BufferSupport	(resourceDesc, BUFFER_TYPE_UNIFORM, ACCESS_MODE_READ, specializedAccess, VK_SHADER_STAGE_FRAGMENT_BIT));
		case OPERATION_NAME_READ_UBO_COMPUTE:						return de::MovePtr<OperationSupport>(new ShaderAccess		::BufferSupport	(resourceDesc, BUFFER_TYPE_UNIFORM, ACCESS_MODE_READ, specializedAccess, VK_SHADER_STAGE_COMPUTE_BIT));
		case OPERATION_NAME_READ_UBO_COMPUTE_INDIRECT:				return de::MovePtr<OperationSupport>(new ShaderAccess		::BufferSupport	(resourceDesc, BUFFER_TYPE_UNIFORM, ACCESS_MODE_READ, specializedAccess, VK_SHADER_STAGE_COMPUTE_BIT, ShaderAccess::DISPATCH_CALL_DISPATCH_INDIRECT));
		case OPERATION_NAME_READ_UBO_TEXEL_VERTEX:					return de::MovePtr<OperationSupport>(new ShaderAccess		::BufferSupport	(resourceDesc, BUFFER_TYPE_UNIFORM_TEXEL, ACCESS_MODE_READ, specializedAccess, VK_SHADER_STAGE_VERTEX_BIT));
		case OPERATION_NAME_READ_UBO_TEXEL_TESSELLATION_CONTROL:	return de::MovePtr<OperationSupport>(new ShaderAccess		::BufferSupport	(resourceDesc, BUFFER_TYPE_UNIFORM_TEXEL, ACCESS_MODE_READ, specializedAccess, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT));
		case OPERATION_NAME_READ_UBO_TEXEL_TESSELLATION_EVALUATION:	return de::MovePtr<OperationSupport>(new ShaderAccess		::BufferSupport	(resourceDesc, BUFFER_TYPE_UNIFORM_TEXEL, ACCESS_MODE_READ, specializedAccess, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT));
		case OPERATION_NAME_READ_UBO_TEXEL_GEOMETRY:				return de::MovePtr<OperationSupport>(new ShaderAccess		::BufferSupport	(resourceDesc, BUFFER_TYPE_UNIFORM_TEXEL, ACCESS_MODE_READ, specializedAccess, VK_SHADER_STAGE_GEOMETRY_BIT));
		case OPERATION_NAME_READ_UBO_TEXEL_FRAGMENT:				return de::MovePtr<OperationSupport>(new ShaderAccess		::BufferSupport	(resourceDesc, BUFFER_TYPE_UNIFORM_TEXEL, ACCESS_MODE_READ, specializedAccess, VK_SHADER_STAGE_FRAGMENT_BIT));
		case OPERATION_NAME_READ_UBO_TEXEL_COMPUTE:					return de::MovePtr<OperationSupport>(new ShaderAccess		::BufferSupport	(resourceDesc, BUFFER_TYPE_UNIFORM_TEXEL, ACCESS_MODE_READ, specializedAccess, VK_SHADER_STAGE_COMPUTE_BIT));
		case OPERATION_NAME_READ_UBO_TEXEL_COMPUTE_INDIRECT:		return de::MovePtr<OperationSupport>(new ShaderAccess		::BufferSupport	(resourceDesc, BUFFER_TYPE_UNIFORM_TEXEL, ACCESS_MODE_READ, specializedAccess, VK_SHADER_STAGE_COMPUTE_BIT, ShaderAccess::DISPATCH_CALL_DISPATCH_INDIRECT));
		case OPERATION_NAME_READ_SSBO_VERTEX:						return de::MovePtr<OperationSupport>(new ShaderAccess		::BufferSupport	(resourceDesc, BUFFER_TYPE_STORAGE, ACCESS_MODE_READ, specializedAccess, VK_SHADER_STAGE_VERTEX_BIT));
		case OPERATION_NAME_READ_SSBO_TESSELLATION_CONTROL:			return de::MovePtr<OperationSupport>(new ShaderAccess		::BufferSupport	(resourceDesc, BUFFER_TYPE_STORAGE, ACCESS_MODE_READ, specializedAccess, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT));
		case OPERATION_NAME_READ_SSBO_TESSELLATION_EVALUATION:		return de::MovePtr<OperationSupport>(new ShaderAccess		::BufferSupport	(resourceDesc, BUFFER_TYPE_STORAGE, ACCESS_MODE_READ, specializedAccess, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT));
		case OPERATION_NAME_READ_SSBO_GEOMETRY:						return de::MovePtr<OperationSupport>(new ShaderAccess		::BufferSupport	(resourceDesc, BUFFER_TYPE_STORAGE, ACCESS_MODE_READ, specializedAccess, VK_SHADER_STAGE_GEOMETRY_BIT));
		case OPERATION_NAME_READ_SSBO_FRAGMENT:						return de::MovePtr<OperationSupport>(new ShaderAccess		::BufferSupport	(resourceDesc, BUFFER_TYPE_STORAGE, ACCESS_MODE_READ, specializedAccess, VK_SHADER_STAGE_FRAGMENT_BIT));
		case OPERATION_NAME_READ_SSBO_COMPUTE:						return de::MovePtr<OperationSupport>(new ShaderAccess		::BufferSupport	(resourceDesc, BUFFER_TYPE_STORAGE, ACCESS_MODE_READ, specializedAccess, VK_SHADER_STAGE_COMPUTE_BIT));
		case OPERATION_NAME_READ_SSBO_COMPUTE_INDIRECT:				return de::MovePtr<OperationSupport>(new ShaderAccess		::BufferSupport	(resourceDesc, BUFFER_TYPE_STORAGE, ACCESS_MODE_READ, specializedAccess, VK_SHADER_STAGE_COMPUTE_BIT, ShaderAccess::DISPATCH_CALL_DISPATCH_INDIRECT));
		case OPERATION_NAME_READ_IMAGE_VERTEX:						return de::MovePtr<OperationSupport>(new ShaderAccess		::ImageSupport	(resourceDesc, ACCESS_MODE_READ, specializedAccess, VK_SHADER_STAGE_VERTEX_BIT));
		case OPERATION_NAME_READ_IMAGE_TESSELLATION_CONTROL:		return de::MovePtr<OperationSupport>(new ShaderAccess		::ImageSupport	(resourceDesc, ACCESS_MODE_READ, specializedAccess, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT));
		case OPERATION_NAME_READ_IMAGE_TESSELLATION_EVALUATION:		return de::MovePtr<OperationSupport>(new ShaderAccess		::ImageSupport	(resourceDesc, ACCESS_MODE_READ, specializedAccess, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT));
		case OPERATION_NAME_READ_IMAGE_GEOMETRY:					return de::MovePtr<OperationSupport>(new ShaderAccess		::ImageSupport	(resourceDesc, ACCESS_MODE_READ, specializedAccess, VK_SHADER_STAGE_GEOMETRY_BIT));
		case OPERATION_NAME_READ_IMAGE_FRAGMENT:					return de::MovePtr<OperationSupport>(new ShaderAccess		::ImageSupport	(resourceDesc, ACCESS_MODE_READ, specializedAccess, VK_SHADER_STAGE_FRAGMENT_BIT));
		case OPERATION_NAME_READ_IMAGE_COMPUTE:						return de::MovePtr<OperationSupport>(new ShaderAccess		::ImageSupport	(resourceDesc, ACCESS_MODE_READ, specializedAccess, VK_SHADER_STAGE_COMPUTE_BIT));
		case OPERATION_NAME_READ_IMAGE_COMPUTE_INDIRECT:			return de::MovePtr<OperationSupport>(new ShaderAccess		::ImageSupport	(resourceDesc, ACCESS_MODE_READ, specializedAccess, VK_SHADER_STAGE_COMPUTE_BIT, ShaderAccess::DISPATCH_CALL_DISPATCH_INDIRECT));
		case OPERATION_NAME_READ_INDIRECT_BUFFER_DRAW:				return de::MovePtr<OperationSupport>(new IndirectBuffer		::ReadSupport	(resourceDesc));
		case OPERATION_NAME_READ_INDIRECT_BUFFER_DRAW_INDEXED:		return de::MovePtr<OperationSupport>(new IndirectBuffer		::ReadSupport	(resourceDesc));
		case OPERATION_NAME_READ_INDIRECT_BUFFER_DISPATCH:			return de::MovePtr<OperationSupport>(new IndirectBuffer		::ReadSupport	(resourceDesc));
		case OPERATION_NAME_READ_VERTEX_INPUT:						return de::MovePtr<OperationSupport>(new VertexInput		::Support		(resourceDesc, VertexInput::DRAW_MODE_VERTEX));
		case OPERATION_NAME_READ_INDEX_INPUT:						return de::MovePtr<OperationSupport>(new VertexInput		::Support		(resourceDesc, VertexInput::DRAW_MODE_INDEXED));

		case OPERATION_NAME_COPY_BUFFER:							return de::MovePtr<OperationSupport>(new CopyBuffer			::CopySupport		(resourceDesc));
		case OPERATION_NAME_COPY_IMAGE:								return de::MovePtr<OperationSupport>(new CopyBlitResolveImage::CopySupport		(resourceDesc, CopyBlitResolveImage::TYPE_COPY));
		case OPERATION_NAME_BLIT_IMAGE:								return de::MovePtr<OperationSupport>(new CopyBlitResolveImage::CopySupport		(resourceDesc, CopyBlitResolveImage::TYPE_BLIT));
		case OPERATION_NAME_COPY_SSBO_VERTEX:						return de::MovePtr<OperationSupport>(new ShaderAccess		::CopyBufferSupport	(resourceDesc, BUFFER_TYPE_STORAGE, specializedAccess, VK_SHADER_STAGE_VERTEX_BIT));
		case OPERATION_NAME_COPY_SSBO_TESSELLATION_CONTROL:			return de::MovePtr<OperationSupport>(new ShaderAccess		::CopyBufferSupport	(resourceDesc, BUFFER_TYPE_STORAGE, specializedAccess, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT));
		case OPERATION_NAME_COPY_SSBO_TESSELLATION_EVALUATION:		return de::MovePtr<OperationSupport>(new ShaderAccess		::CopyBufferSupport	(resourceDesc, BUFFER_TYPE_STORAGE, specializedAccess, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT));
		case OPERATION_NAME_COPY_SSBO_GEOMETRY:						return de::MovePtr<OperationSupport>(new ShaderAccess		::CopyBufferSupport	(resourceDesc, BUFFER_TYPE_STORAGE, specializedAccess, VK_SHADER_STAGE_GEOMETRY_BIT));
		case OPERATION_NAME_COPY_SSBO_FRAGMENT:						return de::MovePtr<OperationSupport>(new ShaderAccess		::CopyBufferSupport	(resourceDesc, BUFFER_TYPE_STORAGE, specializedAccess, VK_SHADER_STAGE_FRAGMENT_BIT));
		case OPERATION_NAME_COPY_SSBO_COMPUTE:						return de::MovePtr<OperationSupport>(new ShaderAccess		::CopyBufferSupport	(resourceDesc, BUFFER_TYPE_STORAGE, specializedAccess, VK_SHADER_STAGE_COMPUTE_BIT));
		case OPERATION_NAME_COPY_SSBO_COMPUTE_INDIRECT:				return de::MovePtr<OperationSupport>(new ShaderAccess		::CopyBufferSupport	(resourceDesc, BUFFER_TYPE_STORAGE, specializedAccess, VK_SHADER_STAGE_COMPUTE_BIT, ShaderAccess::DISPATCH_CALL_DISPATCH_INDIRECT));
		case OPERATION_NAME_COPY_IMAGE_VERTEX:						return de::MovePtr<OperationSupport>(new ShaderAccess		::CopyImageSupport	(resourceDesc,  VK_SHADER_STAGE_VERTEX_BIT, specializedAccess));
		case OPERATION_NAME_COPY_IMAGE_TESSELLATION_CONTROL:		return de::MovePtr<OperationSupport>(new ShaderAccess		::CopyImageSupport	(resourceDesc,  VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, specializedAccess));
		case OPERATION_NAME_COPY_IMAGE_TESSELLATION_EVALUATION:		return de::MovePtr<OperationSupport>(new ShaderAccess		::CopyImageSupport	(resourceDesc, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, specializedAccess));
		case OPERATION_NAME_COPY_IMAGE_GEOMETRY:					return de::MovePtr<OperationSupport>(new ShaderAccess		::CopyImageSupport	(resourceDesc, VK_SHADER_STAGE_GEOMETRY_BIT, specializedAccess));
		case OPERATION_NAME_COPY_IMAGE_FRAGMENT:					return de::MovePtr<OperationSupport>(new ShaderAccess		::CopyImageSupport	(resourceDesc, VK_SHADER_STAGE_FRAGMENT_BIT, specializedAccess));
		case OPERATION_NAME_COPY_IMAGE_COMPUTE:						return de::MovePtr<OperationSupport>(new ShaderAccess		::CopyImageSupport	(resourceDesc, VK_SHADER_STAGE_COMPUTE_BIT, specializedAccess));
		case OPERATION_NAME_COPY_IMAGE_COMPUTE_INDIRECT:			return de::MovePtr<OperationSupport>(new ShaderAccess		::CopyImageSupport	(resourceDesc, VK_SHADER_STAGE_COMPUTE_BIT, specializedAccess, ShaderAccess::DISPATCH_CALL_DISPATCH_INDIRECT));

		default:
			DE_ASSERT(0);
			return de::MovePtr<OperationSupport>();
	}
}

} // synchronization
} // vkt
